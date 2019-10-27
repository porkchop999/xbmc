/*
 *  Copyright (C) 2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "RubyInvoker.h"

#include "filesystem/File.h"
#include "filesystem/SpecialProtocol.h"
#include "threads/SingleLock.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

using namespace RUBY;

#include <ruby.h>

// Time before Ruby programs are terminated
namespace
{
const int RUBY_TIMEOUT_MS{1000};
}

CRubyInvoker::CRubyInvoker(ILanguageInvocationHandler* invocationHandler)
  : ILanguageInvoker(invocationHandler)
{
}

CRubyInvoker::~CRubyInvoker()
{
  // Nothing to do for the default invoker used for registration with the
  // CScriptInvocationManager
  if (GetId() < 0)
    return;

  Stop(true);
  pulseGlobalEvent();

  onExecutionFinalized();
}

bool CRubyInvoker::Execute(
    const std::string& script,
    const std::vector<std::string>& arguments /* = std::vector<std::string>() */)
{
  if (script.empty())
    return false;

  if (!XFILE::CFile::Exists(script))
  {
    CLog::Log(LOGERROR, "CRubyInvoker({}): File '{}' does not exist", GetId(),
              CSpecialProtocol::TranslatePath(script));
    return false;
  }

  if (!onExecutionInitialized())
    return false;

  return ILanguageInvoker::Execute(script, arguments);
}

bool CRubyInvoker::IsStopping() const
{
  bool stopping;
  {
    CSingleLock lock(m_critical);
    stopping = m_stop;
  }

  return stopping || ILanguageInvoker::IsStopping();
}

bool CRubyInvoker::execute(const std::string& script, const std::vector<std::string>& arguments)
{
  m_sourceFile = script;

  bool stopping = false;
  {
    CSingleLock lock(m_critical);
    stopping = m_stop;
  }

  if (stopping)
  {
    CLog::Log(LOGDEBUG, "CRubyInvoker({}, {}): Failed to execute script: Ruby VM is stopped",
              GetId(), m_sourceFile);
    return false;
  }

  CLog::Log(LOGDEBUG, "CRubyInvoker({}, {}): Start processing", GetId(), m_sourceFile);

  VALUE rubyScript = rb_str_new_cstr(m_sourceFile.c_str());

  rb_load_protect(rubyScript, 0, &m_state);

  if (m_state)
  {
    VALUE exception = rb_errinfo();
    rb_set_errinfo(Qnil);

    if (RTEST(exception))
    {
      VALUE error = rb_funcall(exception, rb_intern("full_message"), 0);
      CLog::Log(LOGERROR, "CRubyInvoker({}): {}", GetId(), StringValueCStr(error));
      return false;
    }
  }

  return true;
}

bool CRubyInvoker::stop(bool abort)
{
  CSingleLock lock(m_critical);
  m_stop = true;

  if (!IsRunning())
    return false;

  XbmcThreads::EndTime timeout(RUBY_TIMEOUT_MS);

  while (!m_stoppedEvent.WaitMSec(15))
  {
    if (timeout.IsTimePast())
      CLog::Log(LOGERROR,
                "CRubyInvoker({}, {}): Waited {} seconds to terminate Ruby VM - let's kill it",
                GetId(), m_sourceFile, RUBY_TIMEOUT_MS / 1000);

    break;
  }

  if (!timeout.IsTimePast())
    CLog::Log(LOGDEBUG, "CRubyInvoker({}, {}): Ruby VM termination took {}ms", GetId(),
              m_sourceFile, RUBY_TIMEOUT_MS - timeout.MillisLeft());

  return true;
}

void CRubyInvoker::onExecutionDone()
{
  m_stoppedEvent.Set();

  ILanguageInvoker::onExecutionDone();
}

void CRubyInvoker::onError(const std::string& exceptionType /* = "" */,
                           const std::string& exceptionValue /* = "" */,
                           const std::string& exceptionTraceback /* = "" */)
{
  /* TODO
  CSingleLock gc(CServiceBroker::GetWinSystem()->GetGfxContext());
  CGUIDialogKaiToast *pDlgToast = CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogKaiToast>(WINDOW_DIALOG_KAI_TOAST);
  if (pDlgToast != NULL)
  {
    std::string message;
    if (m_addon && !m_addon->Name().empty())
      message = StringUtils::Format(g_localizeStrings.Get(2102).c_str(), m_addon->Name().c_str());
    else if (m_sourceFile == CSpecialProtocol::TranslatePath("special://profile/autoexec.py"))
      message = StringUtils::Format(g_localizeStrings.Get(2102).c_str(), "autoexec.py");
    else
       message = g_localizeStrings.Get(2103);
    pDlgToast->QueueNotification(CGUIDialogKaiToast::Error, message, g_localizeStrings.Get(2104));
  }
  */
}
