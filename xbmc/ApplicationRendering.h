/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "guilib/GUIMessage.h"
#include "guilib/IMsgTargetCallback.h"
#include "guilib/IWindowManagerCallback.h"
#include "messaging/IMessageTarget.h"
#include "threads/CriticalSection.h"
#include "threads/Thread.h"
#include "utils/GlobalsHandling.h"
#include "utils/Stopwatch.h"
#include "windowing/Resolution.h"
#include "windowing/XBMC_events.h"

#include <deque>
#include <memory>

class CAppInboundProtocol;
class CAppParamParser;
class CGUIComponent;
class CWinSystemBase;

class CApplicationRendering : CThread,
                              public IWindowManagerCallback,
                              public IMsgTargetCallback,
                              public KODI::MESSAGING::IMessageTarget
{
  friend class CAppInboundProtocol;

public:
  CApplicationRendering();
  CApplicationRendering(const CApplicationRendering& right) = delete;

  ~CApplicationRendering();

  void Start(const CAppParamParser& params);

  void FrameMove(bool processEvents, bool processGUI = true) override;
  void Render() override;

  bool OnMessage(CGUIMessage& message) override;

  void SetRenderGUI(bool renderGUI);

  bool CreateGUI();
  bool InitWindow(RESOLUTION res = RES_INVALID);

  void CleanUp();

  bool Initialize();
  void ReloadSkin(bool confirm = false);

  bool LoadSkin(const std::string& skinID);
  void UnloadSkin();

  bool LoadCustomWindows();

  bool IsStopping() { return m_bStop; }

  bool IsCurrentThread() { return CThread::IsCurrentThread(); }

  int GetMessageMask() override;
  void OnApplicationMessage(KODI::MESSAGING::ThreadMessage* pMsg) override;

  // inbound protocol
  bool OnEvent(XBMC_Event& newEvent);
  void HandlePortEvents();

  void ProcessCallBack();

  bool GetRenderGUI() const override { return m_renderGUI; };

protected:
  // virtual void OnStartup(){};
  // virtual void OnExit(){};
  void Process() override;

private:
  std::atomic<bool> m_renderGUI;

  bool m_skipGuiRender;

  CCriticalSection m_criticalSection;

  CCriticalSection m_frameMoveGuard;

  CStopWatch m_frameTime;

  XbmcThreads::EndTime m_guiRefreshTimer;

  unsigned int m_lastRenderTime = 0;

  std::atomic_uint m_WaitingExternalCalls;

  unsigned int m_ProcessedExternalCalls = 0;
  unsigned int m_ProcessedExternalDecay = 0;

  std::unique_ptr<CGUIComponent> m_pGUI;
  std::unique_ptr<CWinSystemBase> m_pWinSystem;

  bool m_confirmSkinChange = true;
  bool m_ignoreSkinSettingChanges = false;

  bool m_saveSkinOnUnloading = true;

  bool m_bInitializing = true;

  std::string m_windowing;

  std::shared_ptr<CAppInboundProtocol> m_pAppPort;
  std::deque<XBMC_Event> m_portEvents;
  CCriticalSection m_portSection;
};

XBMC_GLOBAL_REF(CApplicationRendering, g_applicationRendering);
#define g_applicationRendering XBMC_GLOBAL_USE(CApplicationRendering)
