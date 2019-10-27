/*
 *  Copyright (C) 2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "interfaces/generic/ILanguageInvocationHandler.h"
#include "threads/CriticalSection.h"

namespace RUBY
{
class CRubyInterface : public ILanguageInvocationHandler
{
public:
  ~CRubyInterface() override;

  // Implementation of ILanguageInvocationHandler
  bool Initialize() override;
  //void Uninitialize() override;
  //bool OnScriptInitialized(ILanguageInvoker *invoker) override;
  //void OnScriptFinalized(ILanguageInvoker *invoker) override;
  ILanguageInvoker* CreateInvoker() override;

private:
  mutable CCriticalSection m_critical;
  bool m_initialized{false};
};
} // namespace RUBY
