/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GBMBufferObject.h"

#include "BufferObjectFactory.h"
#include "ServiceBroker.h"
#include "windowing/gbm/WinSystemGbmEGLContext.h"

#include <cstring>

#include <gbm.h>

using namespace KODI::WINDOWING::GBM;

std::unique_ptr<CBufferObject> CGBMBufferObject::Create()
{
  return std::make_unique<CGBMBufferObject>();
}

void CGBMBufferObject::Register()
{
  CBufferObjectFactory::RegisterBufferObject(CGBMBufferObject::Create);
}

CGBMBufferObject::CGBMBufferObject()
{
  m_device =
      static_cast<CWinSystemGbmEGLContext*>(CServiceBroker::GetWinSystem())->GetGBMDevice()->Get();
}

CGBMBufferObject::~CGBMBufferObject()
{
  ReleaseMemory();
  DestroyBufferObject();
}

bool CGBMBufferObject::CreateBufferObject(uint32_t format, uint32_t width, uint32_t height)
{
  if (m_fd >= 0)
    return true;

  m_width = width;
  m_height = height;

  m_bo = gbm_bo_create(m_device, m_width, m_height, format, GBM_BO_USE_RENDERING);

  if (!m_bo)
    return false;

  m_fd = gbm_bo_get_fd(m_bo);

  return true;
}

void CGBMBufferObject::DestroyBufferObject()
{
  close(m_fd);

  if (m_bo)
    gbm_bo_destroy(m_bo);

  m_bo = nullptr;
  m_fd = -1;
}

uint8_t* CGBMBufferObject::GetMemory()
{
  if (m_bo)
  {
    m_map = static_cast<uint8_t*>(gbm_bo_map(m_bo, 0, 0, m_width, m_height,
                                             GBM_BO_TRANSFER_READ_WRITE, &m_stride, &m_map_data));
    if (m_map)
      return m_map;
  }

  return nullptr;
}

void CGBMBufferObject::ReleaseMemory()
{
  if (m_bo && m_map)
  {
    gbm_bo_unmap(m_bo, m_map_data);
    m_map_data = nullptr;
    m_map = nullptr;
  }
}

uint64_t CGBMBufferObject::GetModifier()
{
#if defined(HAS_GBM_MODIFIERS)
  return gbm_bo_get_modifier(m_bo);
#else
  return 0;
#endif
}

bool CGBMBufferObject::ImportBufferObject(uint32_t width,
                                          uint32_t height,
                                          uint32_t format,
                                          uint32_t planeCount,
                                          int* fds,
                                          int* strides,
                                          int* offsets,
                                          uint64_t modifier)
{
  m_width = width;
  m_height = height;

  gbm_import_fd_modifier_data data;
  data.width = m_width;
  data.height = m_height;
  data.format = format;
  data.num_fds = planeCount;
  data.modifier = modifier;

  std::memcpy(data.fds, fds, sizeof(data.fds));
  std::memcpy(data.strides, strides, sizeof(data.strides));
  std::memcpy(data.offsets, offsets, sizeof(data.offsets));

  m_bo = gbm_bo_import(m_device, GBM_BO_IMPORT_FD_MODIFIER, &data, GBM_BO_USE_RENDERING);
  if (!m_bo)
    return false;

  return true;
}
