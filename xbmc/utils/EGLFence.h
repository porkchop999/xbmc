/*
 *  Copyright (C) 2017-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace KODI
{
namespace UTILS
{
namespace EGL
{

class CEGLFence
{
public:
  explicit CEGLFence(EGLDisplay display);
  CEGLFence(CEGLFence const& other) = delete;
  CEGLFence& operator=(CEGLFence const& other) = delete;

  void CreateFence();
  void DestroyFence();
  bool IsSignaled();

#if defined(EGL_ANDROID_native_fence_sync)
  void CreateKMSFence(int fd);
  void CreateGPUFence();
  EGLint FlushFence();
  void WaitSyncGPU();
  void WaitSyncCPU();
#endif

private:
  EGLSyncKHR CreateFence(int fd);

  EGLDisplay m_display{nullptr};
  EGLSyncKHR m_fence{nullptr};

  EGLSyncKHR m_gpuFence{EGL_NO_SYNC_KHR};
  EGLSyncKHR m_kmsFence{EGL_NO_SYNC_KHR};

  PFNEGLCREATESYNCKHRPROC m_eglCreateSyncKHR{nullptr};
  PFNEGLDESTROYSYNCKHRPROC m_eglDestroySyncKHR{nullptr};
  PFNEGLGETSYNCATTRIBKHRPROC m_eglGetSyncAttribKHR{nullptr};

#if defined(EGL_ANDROID_native_fence_sync)
  PFNEGLDUPNATIVEFENCEFDANDROIDPROC m_eglDupNativeFenceFDANDROID{nullptr};
  PFNEGLCLIENTWAITSYNCKHRPROC m_eglClientWaitSyncKHR{nullptr};
  PFNEGLWAITSYNCKHRPROC m_eglWaitSyncKHR{nullptr};
#endif
};

}
}
}
