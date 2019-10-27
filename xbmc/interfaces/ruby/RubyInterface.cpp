/*
 *  Copyright (C) 2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "RubyInterface.h"

#include "RubyInvoker.h"
#include "utils/log.h"

using namespace RUBY;

#include <ruby.h>

CRubyInterface::~CRubyInterface()
{
  CLog::Log(LOGINFO, "CRubyInterface: Uninitializing Ruby VM");
  ruby_finalize();
  m_initialized = false;
}

bool CRubyInterface::Initialize()
{
  CSingleLock lock(m_critical);

  if (!m_initialized)
  {
    CLog::Log(LOGINFO, "CRubyInterface: Initializing Ruby VM");

    if (ruby_setup() != 0)
    {
      CLog::Log(LOGERROR, "CRubyInterface: Failed to create Ruby VM");
      return false;
    }

    m_initialized = true;
  }

  return m_initialized;
}

ILanguageInvoker* CRubyInterface::CreateInvoker()
{
  return new CRubyInvoker(this);
}
