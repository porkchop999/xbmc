/*
 *  Copyright (C) 2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "cores/VideoPlayer/Interface/StreamInfo.h"
#include "windowing/gbm/VideoLayerBridge.h"

#include <memory>

class CDRMUtils;
class CVideoBufferDRMPRIME;

class CVideoLayerBridgeDRMPRIME
  : public CVideoLayerBridge
{
public:
  CVideoLayerBridgeDRMPRIME(std::shared_ptr<CDRMUtils> drm);
  ~CVideoLayerBridgeDRMPRIME();
  void Disable() override;

  virtual void Configure(CVideoBufferDRMPRIME* buffer);
  virtual void SetVideoPlane(CVideoBufferDRMPRIME* buffer, const CRect& destRect);

protected:
  std::shared_ptr<CDRMUtils> m_DRM;

private:
  void Acquire(CVideoBufferDRMPRIME* buffer);
  void Release(CVideoBufferDRMPRIME* buffer);
  bool Map(CVideoBufferDRMPRIME* buffer);
  void Unmap(CVideoBufferDRMPRIME* buffer);

  CVideoBufferDRMPRIME* m_buffer = nullptr;
  CVideoBufferDRMPRIME* m_prev_buffer = nullptr;
};

enum drm_color_encoding {
  DRM_COLOR_YCBCR_BT601,
  DRM_COLOR_YCBCR_BT709,
  DRM_COLOR_YCBCR_BT2020,
};

enum drm_color_range {
  DRM_COLOR_YCBCR_LIMITED_RANGE,
  DRM_COLOR_YCBCR_FULL_RANGE,
};
