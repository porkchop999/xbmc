/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "utils/EGLUtils.h"

namespace KODI
{
namespace WINDOWING
{
namespace LINUX
{

class CWinSystemEGL
{
public:
  CWinSystemEGL(EGLenum platform, std::string const& platformExtension);
  ~CWinSystemEGL() = default;

  bool SupportsDmaBufImportModifiers() { return m_eglContext.SupportsDmaBufImportModifiers(); }
  bool IsFormatSupported(uint32_t format) { return m_eglContext.IsFormatSupported(format); }
  std::vector<EGLuint64KHR>* GetModifiersForFormat(uint32_t format)
  {
    return m_eglContext.GetModifiersForFormat(format);
  }

  EGLDisplay GetEGLDisplay() const;
  EGLSurface GetEGLSurface() const;
  EGLContext GetEGLContext() const;
  EGLConfig GetEGLConfig() const;

protected:
  CEGLContextUtils m_eglContext;
};

} // namespace LINUX
} // namespace WINDOWING
} // namespace KODI
