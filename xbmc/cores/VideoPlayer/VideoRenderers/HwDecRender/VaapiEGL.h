/*
 *      Copyright (C) 2007-2017 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include <array>

#if defined(HAS_GL)
#include <GL/gl.h>
#elif defined(HAS_GLES)
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#endif

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <va/va.h>
#include <va/va_drmcommon.h>

#include "guilib/Geometry.h"
#include "utils/posix/FileHandle.h"

namespace VAAPI
{

class CVaapiRenderPicture;

struct InteropInfo
{
  EGLDisplay eglDisplay = nullptr;
  PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
  PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
  PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
  GLenum textureTarget;
};

class CVaapiTexture
{
public:
  CVaapiTexture() = default;
  virtual ~CVaapiTexture() = default;

  virtual void Init(InteropInfo &interop) = 0;
  virtual bool Map(CVaapiRenderPicture *pic) = 0;
  virtual void Unmap() = 0;

  virtual GLuint GetTextureY() = 0;
  virtual GLuint GetTextureVU() = 0;
  virtual CSizeInt GetTextureSize() = 0;

  static void TestInterop(VADisplay vaDpy, EGLDisplay eglDisplay, bool &general, bool &hevc);

  static CVaapiTexture* CreateTexture(VADisplay vaDpy, EGLDisplay eglDisplay);
};

class CVaapi1Texture : public CVaapiTexture
{
public:
  CVaapi1Texture();

  bool Map(CVaapiRenderPicture *pic) override;
  void Unmap() override;
  void Init(InteropInfo &interop) override;

  GLuint GetTextureY() override;
  GLuint GetTextureVU() override;
  CSizeInt GetTextureSize() override;

  static void TestInterop(VADisplay vaDpy, EGLDisplay eglDisplay, bool &general, bool &hevc);

  GLuint m_texture = 0;
  GLuint m_textureY = 0;
  GLuint m_textureVU = 0;
  int m_texWidth = 0;
  int m_texHeight = 0;

protected:
  static bool TestInteropHevc(VADisplay vaDpy, EGLDisplay eglDisplay);

  InteropInfo m_interop;
  CVaapiRenderPicture *m_vaapiPic = nullptr;
  struct GLSurface
  {
    VAImage vaImage;
    VABufferInfo vBufInfo;
    EGLImageKHR eglImage;
    EGLImageKHR eglImageY, eglImageVU;
  } m_glSurface;
};

#if VA_CHECK_VERSION(1, 1, 0)
class CVaapi2Texture : public CVaapiTexture
{
public:
  bool Map(CVaapiRenderPicture *pic) override;
  void Unmap() override;
  void Init(InteropInfo &interop) override;

  GLuint GetTextureY() override;
  GLuint GetTextureVU() override;
  CSizeInt GetTextureSize() override;

  static void TestInterop(VADisplay vaDpy, EGLDisplay eglDisplay, bool &general, bool &hevc);

private:
  static bool TestEsh(VADisplay vaDpy, EGLDisplay eglDisplay, std::uint32_t rtFormat, std::int32_t pixelFormat);

  struct MappedTexture
  {
    EGLImageKHR eglImage{EGL_NO_IMAGE_KHR};
    GLuint glTexture{0};
  };

  GLuint ImportImageToTexture(EGLImageKHR image);
  void Unmap(MappedTexture& texture);

  InteropInfo m_interop;
  CVaapiRenderPicture* m_vaapiPic{};
  bool m_hasPlaneModifiers{false};
  std::array<KODI::UTILS::POSIX::CFileHandle, 4> m_drmFDs;
  MappedTexture m_y, m_vu;
  CSizeInt m_textureSize;
};
#endif

}

