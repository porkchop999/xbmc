/*
 *  Copyright (C) 2017-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VideoLayerBridgeDRMPRIME.h"

#include "cores/VideoPlayer/DVDCodecs/Video/DVDVideoCodecDRMPRIME.h"
#include "utils/log.h"
#include "windowing/gbm/DRMUtils.h"

CVideoLayerBridgeDRMPRIME::CVideoLayerBridgeDRMPRIME(std::shared_ptr<CDRMUtils> drm)
  : m_DRM(drm)
{
}

CVideoLayerBridgeDRMPRIME::~CVideoLayerBridgeDRMPRIME()
{
  Release(m_prev_buffer);
  Release(m_buffer);
}

void CVideoLayerBridgeDRMPRIME::Disable()
{
  struct connector* connector = m_DRM->GetConnector();
  if (m_DRM->HasProperty(connector, "content type"))
  {
    m_DRM->AddProperty(connector, "content type", m_DRM->GetContentType(false));
    m_DRM->SetActive(true);
  }

  // disable video plane
  struct plane* plane = m_DRM->GetPrimaryPlane();
  m_DRM->AddProperty(plane, "FB_ID", 0);
  m_DRM->AddProperty(plane, "CRTC_ID", 0);
}

void CVideoLayerBridgeDRMPRIME::Acquire(CVideoBufferDRMPRIME* buffer)
{
  // release the buffer that is no longer presented on screen
  Release(m_prev_buffer);

  // release the buffer currently being presented next call
  m_prev_buffer = m_buffer;

  // reference count the buffer that is going to be presented on screen
  m_buffer = buffer;
  m_buffer->Acquire();
}

void CVideoLayerBridgeDRMPRIME::Release(CVideoBufferDRMPRIME* buffer)
{
  if (!buffer)
    return;

  Unmap(buffer);
  buffer->Release();
}

bool CVideoLayerBridgeDRMPRIME::Map(CVideoBufferDRMPRIME* buffer)
{
  if (buffer->m_fb_id)
    return true;

  AVDRMFrameDescriptor* descriptor = buffer->GetDescriptor();
  uint32_t handles[4] = {0}, pitches[4] = {0}, offsets[4] = {0}, flags = 0;
  uint64_t modifier[4] = {0};
  int ret;

  // convert Prime FD to GEM handle
  for (int object = 0; object < descriptor->nb_objects; object++)
  {
    ret = drmPrimeFDToHandle(m_DRM->GetFileDescriptor(), descriptor->objects[object].fd, &buffer->m_handles[object]);
    if (ret < 0)
    {
      CLog::Log(LOGERROR, "CVideoLayerBridgeDRMPRIME::%s - failed to convert prime fd %d to gem handle %u, ret = %d",
                __FUNCTION__, descriptor->objects[object].fd, buffer->m_handles[object], ret);
      return false;
    }
  }

  AVDRMLayerDescriptor* layer = &descriptor->layers[0];

  for (int plane = 0; plane < layer->nb_planes; plane++)
  {
    int object = layer->planes[plane].object_index;
    uint32_t handle = buffer->m_handles[object];
    if (handle && layer->planes[plane].pitch)
    {
      handles[plane] = handle;
      pitches[plane] = layer->planes[plane].pitch;
      offsets[plane] = layer->planes[plane].offset;
      modifier[plane] = descriptor->objects[object].format_modifier;
    }
  }

  if (modifier[0] && modifier[0] != DRM_FORMAT_MOD_INVALID)
    flags = DRM_MODE_FB_MODIFIERS;

  // add the video frame FB
  ret = drmModeAddFB2WithModifiers(m_DRM->GetFileDescriptor(), buffer->GetWidth(), buffer->GetHeight(), layer->format,
                                   handles, pitches, offsets, modifier, &buffer->m_fb_id, flags);
  if (ret < 0)
  {
    CLog::Log(LOGERROR, "CVideoLayerBridgeDRMPRIME::%s - failed to add fb %d, ret = %d", __FUNCTION__, buffer->m_fb_id, ret);
    return false;
  }

  Acquire(buffer);
  return true;
}

void CVideoLayerBridgeDRMPRIME::Unmap(CVideoBufferDRMPRIME* buffer)
{
  if (buffer->m_fb_id)
  {
    drmModeRmFB(m_DRM->GetFileDescriptor(), buffer->m_fb_id);
    buffer->m_fb_id = 0;
  }

  for (int i = 0; i < AV_DRM_MAX_PLANES; i++)
  {
    if (buffer->m_handles[i])
    {
      struct drm_gem_close gem_close = { .handle = buffer->m_handles[i] };
      drmIoctl(m_DRM->GetFileDescriptor(), DRM_IOCTL_GEM_CLOSE, &gem_close);
      buffer->m_handles[i] = 0;
    }
  }
}

static int GetColorEncoding(CVideoBufferDRMPRIME* buffer)
{
  AVFrame* frame = buffer->GetFrame();
  if (frame->colorspace == AVCOL_SPC_BT2020_NCL ||
      frame->colorspace == AVCOL_SPC_BT2020_CL ||
      frame->color_primaries == AVCOL_PRI_BT2020 ||
      frame->color_trc == AVCOL_TRC_SMPTE2084 ||
      frame->color_trc == AVCOL_TRC_BT2020_10)
    return DRM_COLOR_YCBCR_BT2020;

  if (frame->colorspace == AVCOL_SPC_SMPTE170M ||
      frame->color_primaries == AVCOL_PRI_SMPTE170M)
    return DRM_COLOR_YCBCR_BT601;

  return DRM_COLOR_YCBCR_BT709;
}

static int GetColorRange(CVideoBufferDRMPRIME* buffer)
{
  AVFrame* frame = buffer->GetFrame();
  if (frame->color_range == AVCOL_RANGE_JPEG)
    return DRM_COLOR_YCBCR_FULL_RANGE;

  return DRM_COLOR_YCBCR_LIMITED_RANGE;
}

void CVideoLayerBridgeDRMPRIME::Configure(CVideoBufferDRMPRIME* buffer)
{
  struct connector* connector = m_DRM->GetConnector();
  if (m_DRM->HasProperty(connector, "content type"))
  {
    m_DRM->AddProperty(connector, "content type", m_DRM->GetContentType(true));
    m_DRM->SetActive(true);
  }

  struct plane* plane = m_DRM->GetPrimaryPlane();
  if (m_DRM->HasProperty(plane, "COLOR_ENCODING") &&
      m_DRM->HasProperty(plane, "COLOR_RANGE"))
  {
    m_DRM->AddProperty(plane, "COLOR_ENCODING", GetColorEncoding(buffer));
    m_DRM->AddProperty(plane, "COLOR_RANGE", GetColorRange(buffer));
  }
}

void CVideoLayerBridgeDRMPRIME::SetVideoPlane(CVideoBufferDRMPRIME* buffer, const CRect& destRect)
{
  if (!Map(buffer))
  {
    Unmap(buffer);
    return;
  }

  struct plane* plane = m_DRM->GetPrimaryPlane();
  m_DRM->AddProperty(plane, "FB_ID", buffer->m_fb_id);
  m_DRM->AddProperty(plane, "CRTC_ID", m_DRM->GetCrtc()->crtc->crtc_id);
  m_DRM->AddProperty(plane, "SRC_X", 0);
  m_DRM->AddProperty(plane, "SRC_Y", 0);
  m_DRM->AddProperty(plane, "SRC_W", buffer->GetWidth() << 16);
  m_DRM->AddProperty(plane, "SRC_H", buffer->GetHeight() << 16);
  m_DRM->AddProperty(plane, "CRTC_X", static_cast<int32_t>(destRect.x1) & ~1);
  m_DRM->AddProperty(plane, "CRTC_Y", static_cast<int32_t>(destRect.y1) & ~1);
  m_DRM->AddProperty(plane, "CRTC_W", (static_cast<uint32_t>(destRect.Width()) + 1) & ~1);
  m_DRM->AddProperty(plane, "CRTC_H", (static_cast<uint32_t>(destRect.Height()) + 1) & ~1);
}
