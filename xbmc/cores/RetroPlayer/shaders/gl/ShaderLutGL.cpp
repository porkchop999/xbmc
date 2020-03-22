/*
 *  Copyright (C) 2019 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ShaderLutGL.h"
#include "ShaderTextureGL.h"
#include "ShaderUtilsGL.h"
#include "cores/RetroPlayer/rendering/RenderContext.h"
#include "cores/RetroPlayer/shaders/IShaderPreset.h"
#include "rendering/gl/RenderSystemGL.h"
#include "utils/log.h"

#include <utility>
#define MAX_FLOAT 3.402823466E+38

using namespace KODI;
using namespace SHADER;

CShaderLutGL::CShaderLutGL(const std::string &id, const std::string &path)
  : IShaderLut(id, path)
{
}

CShaderLutGL::~CShaderLutGL() = default;

bool CShaderLutGL::Create(RETRO::CRenderContext &context, const ShaderLut &lut)
{
  std::unique_ptr<IShaderTexture> lutTexture(CreateLUTTexture(context, lut));
  if (!lutTexture)
  {
    CLog::Log(LOGWARNING, "%s - Couldn't create a LUT texture for LUT %s", __FUNCTION__, lut.strId);
    return false;
  }

  m_texture = std::move(lutTexture);

  return true;
}

std::unique_ptr<IShaderTexture> CShaderLutGL::CreateLUTTexture(RETRO::CRenderContext &context, const KODI::SHADER::ShaderLut &lut)
{
  auto wrapType = CShaderUtilsGL::TranslateWrapType(lut.wrap);
  auto filterType = lut.filter ? GL_LINEAR : GL_NEAREST;

  CGLTexture* texture = static_cast<CGLTexture*>(CGLTexture::LoadFromFile(lut.path));

  if (!texture)
  {
    CLog::Log(LOGERROR, "Couldn't open LUT %s", lut.path);
    return std::unique_ptr<IShaderTexture>();
  }

  texture->CreateTextureObject();
  glBindTexture(GL_TEXTURE_2D, texture->getMTexture());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterType);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterType);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapType);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapType);
#if defined(HAS_GL)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, wrapType);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_NEVER);
#elif defined(HAS_GLES)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R_OES, wrapType);
#endif
#if defined(HAS_GL)
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0.0);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, MAX_FLOAT);
#endif

  GLfloat blackBorder[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
#if defined(HAS_GL)
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, blackBorder);
#elif defined(HAS_GLES)
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR_EXT, blackBorder);
#endif
  if (lut.mipmap)
    texture->SetMipmapping();

  return std::unique_ptr<IShaderTexture>(new CShaderTextureGL(texture));
}
