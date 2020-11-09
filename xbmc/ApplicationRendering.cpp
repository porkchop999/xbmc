/*
 *  Copyright (C) 2005-2020 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ApplicationRendering.h"

#include "AppInboundProtocol.h"
#include "AppParamParser.h"
#include "Application.h"
#include "GUIInfoManager.h"
#include "GUILargeTextureManager.h"
#include "GUIPassword.h"
#include "GUIUserMessages.h"
#include "PlayListPlayer.h"
#include "ServiceBroker.h"
#include "TextureCache.h"
#include "addons/AddonManager.h"
#include "addons/Skin.h"
#include "dialogs/GUIDialogButtonMenu.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "dialogs/GUIDialogSubMenu.h"
#include "filesystem/Directory.h"
#include "filesystem/DirectoryCache.h"
#include "guilib/GUIAudioManager.h"
#include "guilib/GUIColorManager.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIFontManager.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/LocalizeStrings.h"
#include "guilib/StereoscopicsManager.h"
#include "input/InputManager.h"
#include "messaging/ApplicationMessenger.h"
#include "messaging/helpers/DialogHelper.h"
#include "platform/MessagePrinter.h"
#include "profiles/ProfileManager.h"
#include "settings/AdvancedSettings.h"
#include "settings/DisplaySettings.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "settings/SkinSettings.h"
#include "settings/lib/Setting.h"
#include "threads/SingleLock.h"
#include "utils/CPUInfo.h"
#include "utils/SystemInfo.h"
#include "utils/TimeUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"
#include "video/dialogs/GUIDialogFullScreenInfo.h"
#include "windowing/GraphicContext.h"
#include "windowing/WindowSystemFactory.h"

CApplicationRendering::CApplicationRendering()
  : CThread("Application Rendering"), m_pGUI(nullptr), m_pWinSystem(nullptr)
{
  m_lastRenderTime = XbmcThreads::SystemClockMillis();
}

CApplicationRendering::~CApplicationRendering() = default;

void CApplicationRendering::SetRenderGUI(bool renderGUI)
{
  CSingleLock lock(m_criticalSection);

  m_renderGUI = renderGUI;
}

int CApplicationRendering::GetMessageMask()
{
  return TMSG_MASK_APPLICATION;
}

void CApplicationRendering::OnApplicationMessage(KODI::MESSAGING::ThreadMessage* pMsg)
{
  auto gui = CServiceBroker::GetGUI();

  uint32_t msg = pMsg->dwMessage;

  switch (msg)
  {
    case TMSG_QUIT:
    {
      m_bStop = true;
      break;
    }
    case TMSG_VIDEORESIZE:
    {
      XBMC_Event newEvent;
      memset(&newEvent, 0, sizeof(newEvent));
      newEvent.type = XBMC_VIDEORESIZE;
      newEvent.resize.w = pMsg->param1;
      newEvent.resize.h = pMsg->param2;
      OnEvent(newEvent);

      if (gui)
        gui->GetWindowManager().MarkDirty();

      break;
    }
    case TMSG_EVENT:
    {
      if (pMsg->lpVoid)
      {
        XBMC_Event* event = static_cast<XBMC_Event*>(pMsg->lpVoid);
        OnEvent(*event);
        delete event;
      }

      break;
    }
    default:
      CLog::Log(LOGERROR, "%s: Unhandled threadmessage sent, %u", __FUNCTION__, msg);
      break;
  }
}

void CApplicationRendering::CleanUp()
{
  StopThread(true);

  {
    // close inbound port
    CServiceBroker::UnregisterAppPort();
    XbmcThreads::EndTime timer(1000);
    while (m_pAppPort.use_count() > 1)
    {
      KODI::TIME::Sleep(100);
      if (timer.IsTimePast())
      {
        CLog::Log(LOGERROR, "CApplicationRendering::Stop - CAppPort still in use, app may crash");
        break;
      }
    }
    m_pAppPort.reset();
  }

  CLog::Log(LOGINFO, "unload skin");
  g_applicationRendering.UnloadSkin();

  CRenderSystemBase* renderSystem = CServiceBroker::GetRenderSystem();
  if (renderSystem)
    renderSystem->DestroyRenderSystem();

  CWinSystemBase* winSystem = CServiceBroker::GetWinSystem();
  if (winSystem)
    winSystem->DestroyWindow();

  if (m_pGUI)
    m_pGUI->GetWindowManager().DestroyWindows();

  if (m_pGUI)
  {
    m_pGUI->Deinit();
    m_pGUI.reset();
  }

  if (winSystem)
  {
    winSystem->DestroyWindowSystem();
    CServiceBroker::UnregisterWinSystem();
    winSystem = nullptr;
    m_pWinSystem.reset();
  }
}

void CApplicationRendering::Start(const CAppParamParser& params)
{
  m_windowing = params.m_windowing;

  Create(true);
}

void CApplicationRendering::Process()
{
  KODI::MESSAGING::CApplicationMessenger::GetInstance().RegisterReceiver(this);
  KODI::MESSAGING::CApplicationMessenger::GetInstance().SetGUIThread(GetCurrentThreadId());

  if (!CreateGUI())
  {
    CMessagePrinter::DisplayError("ERROR: Unable to create GUI. Exiting");
    StopThread();
    return;
  }

  if (!Initialize())
  {
    CMessagePrinter::DisplayError("ERROR: Unable to initialize GUI. Exiting");
    StopThread();
    return;
  }

  while (!m_bStop)
  {
    if (!m_bStop)
    {
      FrameMove(true, m_renderGUI);
    }

    if (m_renderGUI && !m_bStop)
    {
      Render();
    }

    if (!m_renderGUI)
      KODI::TIME::Sleep(20); //! @todo: arbritrary
  }
}

void CApplicationRendering::ProcessCallBack()
{
  auto gui = CServiceBroker::GetGUI();
  auto winSystem = CServiceBroker::GetWinSystem();

  // dispatch the messages generated by python or other threads to the current window
  if (gui)
    gui->GetWindowManager().DispatchThreadMessages();

  // process messages which have to be send to the gui
  // (this can only be done after CServiceBroker::GetGUI()->GetWindowManager().Render())
  KODI::MESSAGING::CApplicationMessenger::GetInstance().ProcessWindowMessages();

  // handle any active scripts

  // {
  //   // Allow processing of script threads to let them shut down properly.
  //   if (winSystem)
  //   {
  //     CSingleExit ex(winSystem->GetGfxContext());
  //     m_frameMoveGuard.unlock();
  //     CScriptInvocationManager::GetInstance().Process();
  //     m_frameMoveGuard.lock();
  //   }
  // }
}

bool CApplicationRendering::OnEvent(XBMC_Event& newEvent)
{
  CSingleLock lock(m_portSection);
  m_portEvents.push_back(newEvent);
  return true;
}

void CApplicationRendering::HandlePortEvents()
{
  CSingleLock lock(m_portSection);

  auto gui = CServiceBroker::GetGUI();

  while (!m_portEvents.empty())
  {
    auto newEvent = m_portEvents.front();
    m_portEvents.pop_front();
    CSingleExit lock(m_portSection);
    switch (newEvent.type)
    {
      case XBMC_QUIT:
        if (!m_bStop)
          KODI::MESSAGING::CApplicationMessenger::GetInstance().PostMsg(TMSG_QUIT);
        break;
      case XBMC_VIDEORESIZE:
        if (gui)
        {
          if (gui->GetWindowManager().Initialized())
          {
            if (!CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_fullScreen)
            {
              CServiceBroker::GetWinSystem()->GetGfxContext().ApplyWindowResize(newEvent.resize.w,
                                                                                newEvent.resize.h);

              const std::shared_ptr<CSettings> settings =
                  CServiceBroker::GetSettingsComponent()->GetSettings();
              settings->SetInt(CSettings::SETTING_WINDOW_WIDTH, newEvent.resize.w);
              settings->SetInt(CSettings::SETTING_WINDOW_HEIGHT, newEvent.resize.h);
              settings->Save();
            }
#ifdef TARGET_WINDOWS
            else
            {
              // this may occurs when OS tries to resize application window
              //CDisplaySettings::GetInstance().SetCurrentResolution(RES_DESKTOP, true);
              //auto& gfxContext = CServiceBroker::GetWinSystem()->GetGfxContext();
              //gfxContext.SetVideoResolution(gfxContext.GetVideoResolution(), true);
              // try to resize window back to it's full screen size
              auto& res_info = CDisplaySettings::GetInstance().GetResolutionInfo(RES_DESKTOP);
              CServiceBroker::GetWinSystem()->ResizeWindow(res_info.iScreenWidth,
                                                           res_info.iScreenHeight, 0, 0);
            }
#endif
          }
        }
        break;
      case XBMC_VIDEOMOVE:
      {
        CServiceBroker::GetWinSystem()->OnMove(newEvent.move.x, newEvent.move.y);
      }
      break;
      case XBMC_MODECHANGE:
        CServiceBroker::GetWinSystem()->GetGfxContext().ApplyModeChange(newEvent.mode.res);
        break;
      case XBMC_USEREVENT:
        KODI::MESSAGING::CApplicationMessenger::GetInstance().PostMsg(
            static_cast<uint32_t>(newEvent.user.code));
        break;
      case XBMC_SETFOCUS:
        // Reset the screensaver
        // ResetScreenSaver();
        // WakeUpScreenSaverAndDPMS();
        // Send a mouse motion event with no dx,dy for getting the current guiitem selected
        // OnAction(CAction(ACTION_MOUSE_MOVE, 0, static_cast<float>(newEvent.focus.x), static_cast<float>(newEvent.focus.y), 0, 0));
        break;
      default:
        CServiceBroker::GetInputManager().OnEvent(newEvent);
    }
  }
}

void CApplicationRendering::FrameMove(bool processEvents, bool processGUI)
{
  auto gui = CServiceBroker::GetGUI();
  auto winSystem = CServiceBroker::GetWinSystem();

  if (processEvents)
  {
    // // currently we calculate the repeat time (ie time from last similar keypress) just global as fps
    // float frameTime = m_frameTime.GetElapsedSeconds();
    // m_frameTime.StartZero();
    // // never set a frametime less than 2 fps to avoid problems when debugging and on breaks
    // if (frameTime > 0.5)
    //   frameTime = 0.5;

    if (gui && winSystem && processGUI && m_renderGUI)
    {
      CSingleLock lock(winSystem->GetGfxContext());
      // check if there are notifications to display
      CGUIDialogKaiToast* toast =
          gui->GetWindowManager().GetWindow<CGUIDialogKaiToast>(WINDOW_DIALOG_KAI_TOAST);
      if (toast && toast->DoWork())
      {
        if (!toast->IsDialogRunning())
        {
          toast->Open();
        }
      }
    }

    HandlePortEvents();

    // if (gui)
    //   CServiceBroker::GetInputManager().Process(gui->GetWindowManager().GetActiveWindowOrDialog(),
    //                                             frameTime);
    // else
    //   CServiceBroker::GetInputManager().Process(WINDOW_INVALID, frameTime);

    if (processGUI && m_renderGUI)
    {
      // m_pInertialScrollingHandler->ProcessInertialScroll(frameTime);
      g_application.GetAppPlayer().GetSeekHandler().FrameMove();
    }

    // Open the door for external calls e.g python exactly here.
    // Window size can be between 2 and 10ms and depends on number of continuous requests
    if (m_WaitingExternalCalls && winSystem)
    {
      CSingleExit ex(winSystem->GetGfxContext());
      m_frameMoveGuard.unlock();

      // Calculate a window size between 2 and 10ms, 4 continuous requests let the window grow by 1ms
      // When not playing video we allow it to increase to 80ms
      unsigned int max_sleep = 10;
      if (!g_application.GetAppPlayer().IsPlayingVideo() ||
          g_application.GetAppPlayer().IsPausedPlayback())
        max_sleep = 80;
      unsigned int sleepTime = std::max(static_cast<unsigned int>(2),
                                        std::min(m_ProcessedExternalCalls >> 2, max_sleep));
      KODI::TIME::Sleep(sleepTime);
      m_frameMoveGuard.lock();
      m_ProcessedExternalDecay = 5;
    }
    if (m_ProcessedExternalDecay && --m_ProcessedExternalDecay == 0)
      m_ProcessedExternalCalls = 0;
  }

  if (gui && processGUI && m_renderGUI)
  {
    m_skipGuiRender = false;

    /*! @todo look into the possibility to use this for GBM
    int fps = 0;

    // This code reduces rendering fps of the GUI layer when playing videos in fullscreen mode
    // it makes only sense on architectures with multiple layers
    if (CServiceBroker::GetWinSystem()->GetGfxContext().IsFullScreenVideo() && !m_appPlayer.IsPausedPlayback() && m_appPlayer.IsRenderingVideoLayer())
      fps = CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(CSettings::SETTING_VIDEOPLAYER_LIMITGUIUPDATE);

    unsigned int now = XbmcThreads::SystemClockMillis();
    unsigned int frameTime = now - m_lastRenderTime;
    if (fps > 0 && frameTime * fps < 1000)
      m_skipGuiRender = true;
    */

    if (CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_guiSmartRedraw &&
        m_guiRefreshTimer.IsTimePast())
    {
      gui->GetWindowManager().SendMessage(GUI_MSG_REFRESH_TIMER, 0, 0);
      m_guiRefreshTimer.Set(500);
    }

    if (!m_bStop)
    {
      if (!m_skipGuiRender)
        gui->GetWindowManager().Process(CTimeUtils::GetFrameTime());
    }

    gui->GetWindowManager().FrameMove();
  }

  g_application.GetAppPlayer().FrameMove();

  // this will go away when render systems gets its own thread
  if (winSystem)
    winSystem->DriveRenderLoop();
}

void CApplicationRendering::Render()
{
  // do not render if we are stopped or in background
  if (m_bStop)
    return;

  bool hasRendered = false;

  // Whether externalplayer is playing and we're unfocused
  bool extPlayerActive =
      g_application.GetAppPlayer().IsExternalPlaying(); //! @todo: && !m_AppFocused;

  if (!extPlayerActive && CServiceBroker::GetWinSystem()->GetGfxContext().IsFullScreenVideo() &&
      !g_application.GetAppPlayer().IsPausedPlayback())
  {
    // ResetScreenSaver();
  }

  auto gui = CServiceBroker::GetGUI();

  if (!CServiceBroker::GetRenderSystem()->BeginRender())
    return;

  // render gui layer
  if (gui && m_renderGUI && !m_skipGuiRender)
  {
    if (CServiceBroker::GetWinSystem()->GetGfxContext().GetStereoMode())
    {
      CServiceBroker::GetWinSystem()->GetGfxContext().SetStereoView(RENDER_STEREO_VIEW_LEFT);
      hasRendered |= gui->GetWindowManager().Render();

      if (CServiceBroker::GetWinSystem()->GetGfxContext().GetStereoMode() !=
          RENDER_STEREO_MODE_MONO)
      {
        CServiceBroker::GetWinSystem()->GetGfxContext().SetStereoView(RENDER_STEREO_VIEW_RIGHT);
        hasRendered |= gui->GetWindowManager().Render();
      }
      CServiceBroker::GetWinSystem()->GetGfxContext().SetStereoView(RENDER_STEREO_VIEW_OFF);
    }
    else
    {
      hasRendered |= gui->GetWindowManager().Render();
    }
    // execute post rendering actions (finalize window closing)
    gui->GetWindowManager().AfterRender();

    m_lastRenderTime = XbmcThreads::SystemClockMillis();
  }

  // render video layer
  if (gui)
    gui->GetWindowManager().RenderEx();

  CServiceBroker::GetRenderSystem()->EndRender();

  // reset our info cache - we do this at the end of Render so that it is
  // fresh for the next process(), or after a windowclose animation (where process()
  // isn't called)
  if (gui)
  {
    CGUIInfoManager& infoMgr = gui->GetInfoManager();
    infoMgr.ResetCache();
    infoMgr.GetInfoProviders().GetGUIControlsInfoProvider().ResetContainerMovingCache();

    if (hasRendered)
    {
      infoMgr.GetInfoProviders().GetSystemInfoProvider().UpdateFPS();
    }
  }

  CServiceBroker::GetWinSystem()->GetGfxContext().Flip(
      hasRendered, g_application.GetAppPlayer().IsRenderingVideoLayer());

  CTimeUtils::UpdateFrameTime(hasRendered);
}

bool CApplicationRendering::CreateGUI()
{
  m_frameMoveGuard.lock();

  m_renderGUI = true;

  auto windowSystems = KODI::WINDOWING::CWindowSystemFactory::GetWindowSystems();

  if (!m_windowing.empty())
    windowSystems = {m_windowing};

  for (auto& windowSystem : windowSystems)
  {
    CLog::Log(LOGDEBUG, "CApplication::{} - trying to init {} windowing system", __FUNCTION__,
              windowSystem);
    m_pWinSystem = KODI::WINDOWING::CWindowSystemFactory::CreateWindowSystem(windowSystem);

    if (!m_pWinSystem)
      continue;

    if (!m_windowing.empty() && m_windowing != windowSystem)
      continue;

    CServiceBroker::RegisterWinSystem(m_pWinSystem.get());

    if (!m_pWinSystem->InitWindowSystem())
    {
      CLog::Log(LOGDEBUG, "CApplication::{} - unable to init {} windowing system", __FUNCTION__,
                windowSystem);
      m_pWinSystem->DestroyWindowSystem();
      m_pWinSystem.reset();
      CServiceBroker::UnregisterWinSystem();
      continue;
    }
    else
    {
      CLog::Log(LOGINFO, "CApplication::{} - using the {} windowing system", __FUNCTION__,
                windowSystem);
      break;
    }
  }

  if (!m_pWinSystem)
  {
    CLog::Log(LOGFATAL, "CApplication::{} - unable to init windowing system", __FUNCTION__);
    CServiceBroker::UnregisterWinSystem();
    return false;
  }

  // Retrieve the matching resolution based on GUI settings
  bool sav_res = false;
  CDisplaySettings::GetInstance().SetCurrentResolution(
      CDisplaySettings::GetInstance().GetDisplayResolution());
  CLog::Log(LOGINFO, "Checking resolution %i",
            CDisplaySettings::GetInstance().GetCurrentResolution());
  if (!CServiceBroker::GetWinSystem()->GetGfxContext().IsValidResolution(
          CDisplaySettings::GetInstance().GetCurrentResolution()))
  {
    CLog::Log(LOGINFO, "Setting safe mode %i", RES_DESKTOP);
    // defer saving resolution after window was created
    CDisplaySettings::GetInstance().SetCurrentResolution(RES_DESKTOP);
    sav_res = true;
  }

  // update the window resolution
  const std::shared_ptr<CSettings> settings = CServiceBroker::GetSettingsComponent()->GetSettings();
  CServiceBroker::GetWinSystem()->SetWindowResolution(
      settings->GetInt(CSettings::SETTING_WINDOW_WIDTH),
      settings->GetInt(CSettings::SETTING_WINDOW_HEIGHT));

  if (CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_startFullScreen &&
      CDisplaySettings::GetInstance().GetCurrentResolution() == RES_WINDOW)
  {
    // defer saving resolution after window was created
    CDisplaySettings::GetInstance().SetCurrentResolution(RES_DESKTOP);
    sav_res = true;
  }

  if (!CServiceBroker::GetWinSystem()->GetGfxContext().IsValidResolution(
          CDisplaySettings::GetInstance().GetCurrentResolution()))
  {
    // Oh uh - doesn't look good for starting in their wanted screenmode
    CLog::Log(LOGERROR, "The screen resolution requested is not valid, resetting to a valid mode");
    CDisplaySettings::GetInstance().SetCurrentResolution(RES_DESKTOP);
    sav_res = true;
  }

  if (!InitWindow())
  {
    return false;
  }

  // Set default screen saver mode
  auto screensaverModeSetting = std::static_pointer_cast<CSettingString>(
      settings->GetSetting(CSettings::SETTING_SCREENSAVER_MODE));
  // Can only set this after windowing has been initialized since it depends on it
  if (CServiceBroker::GetWinSystem()->GetOSScreenSaver())
  {
    // If OS has a screen saver, use it by default
    screensaverModeSetting->SetDefault("");
  }
  else
  {
    // If OS has no screen saver, use Kodi one by default
    screensaverModeSetting->SetDefault("screensaver.xbmc.builtin.dim");
  }

  if (sav_res)
    CDisplaySettings::GetInstance().SetCurrentResolution(RES_DESKTOP, true);

  m_pGUI.reset(new CGUIComponent());
  m_pGUI->Init();

  // Splash requires gui component!!
  CServiceBroker::GetRenderSystem()->ShowSplash("");

  // The key mappings may already have been loaded by a peripheral
  CLog::Log(LOGINFO, "load keymapping");
  if (!CServiceBroker::GetInputManager().LoadKeymaps())
    return false;

  RESOLUTION_INFO info = CServiceBroker::GetWinSystem()->GetGfxContext().GetResInfo();
  CLog::Log(LOGINFO, "GUI format %ix%i, Display %s", info.iWidth, info.iHeight,
            info.strMode.c_str());

  // application inbound service
  m_pAppPort = std::make_shared<CAppInboundProtocol>(*this);
  CServiceBroker::RegisterAppPort(m_pAppPort);

  return true;
}

bool CApplicationRendering::InitWindow(RESOLUTION res)
{
  if (res == RES_INVALID)
    res = CDisplaySettings::GetInstance().GetCurrentResolution();

  bool bFullScreen = res != RES_WINDOW;
  if (!CServiceBroker::GetWinSystem()->CreateNewWindow(
          CSysInfo::GetAppName(), bFullScreen,
          CDisplaySettings::GetInstance().GetResolutionInfo(res)))
  {
    CLog::Log(LOGFATAL, "CApplication::Create: Unable to create window");
    return false;
  }

  if (!CServiceBroker::GetRenderSystem()->InitRenderSystem())
  {
    CLog::Log(LOGFATAL, "CApplication::Create: Unable to init rendering system");
    return false;
  }
  // set GUI res and force the clear of the screen
  CServiceBroker::GetWinSystem()->GetGfxContext().SetVideoResolution(res, false);
  return true;
}

bool CApplicationRendering::Initialize()
{
  bool uiInitializationFinished = false;

  const auto profileManager = CServiceBroker::GetSettingsComponent()->GetProfileManager();
  auto renderSystem = CServiceBroker::GetRenderSystem();

  auto gui = CServiceBroker::GetGUI();
  if (gui)
  {
    if (gui->GetWindowManager().Initialized())
    {
      const std::shared_ptr<CSettings> settings =
          CServiceBroker::GetSettingsComponent()->GetSettings();

      gui->GetWindowManager().CreateWindows();

      m_confirmSkinChange = false;

      // Start splashscreen and load skin
      if (renderSystem)
        renderSystem->ShowSplash("");
      m_confirmSkinChange = true;

      std::string defaultSkin = std::static_pointer_cast<const CSettingString>(
                                    settings->GetSetting(CSettings::SETTING_LOOKANDFEEL_SKIN))
                                    ->GetDefault();
      if (!LoadSkin(settings->GetString(CSettings::SETTING_LOOKANDFEEL_SKIN)))
      {
        CLog::Log(LOGERROR, "Failed to load skin '%s'",
                  settings->GetString(CSettings::SETTING_LOOKANDFEEL_SKIN).c_str());
        if (!LoadSkin(defaultSkin))
        {
          CLog::Log(LOGFATAL, "Default skin '%s' could not be loaded! Terminating..",
                    defaultSkin.c_str());
          return false;
        }
      }

      // initialize splash window after splash screen disappears
      // because we need a real window in the background which gets
      // rendered while we load the main window or enter the master lock key

      if (gui)
        gui->GetWindowManager().ActivateWindow(WINDOW_SPLASH);

      if (settings->GetBool(CSettings::SETTING_MASTERLOCK_STARTUPLOCK) &&
          profileManager->GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE &&
          !profileManager->GetMasterProfile().getLockCode().empty())
      {
        g_passwordManager.CheckStartUpLock();
      }

      // check if we should use the login screen
      if (profileManager->UsingLoginScreen())
      {
        if (gui)
          gui->GetWindowManager().ActivateWindow(WINDOW_LOGIN_SCREEN);
      }
      else
      {
        // activate the configured start window
        int firstWindow = g_SkinInfo->GetFirstWindow();
        if (gui)
        {
          gui->GetWindowManager().ActivateWindow(firstWindow);

          if (gui->GetWindowManager().IsWindowActive(WINDOW_STARTUP_ANIM))
          {
            CLog::Log(LOGWARNING, "CApplication::Initialize - startup.xml taints init process");
          }
        }
        // the startup window is considered part of the initialization as it most likely switches to the final window
        uiInitializationFinished = firstWindow != WINDOW_STARTUP_ANIM;
      }
    }
  }
  else //No GUI Created
  {
    uiInitializationFinished = true;
  }

  // if the user interfaces has been fully initialized let everyone know
  if (uiInitializationFinished)
  {
    if (gui)
    {
      CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UI_READY);
      gui->GetWindowManager().SendThreadMessage(msg);
    }
  }

  return true;
}

void CApplicationRendering::ReloadSkin(bool confirm /*=false*/)
{
  if (!g_SkinInfo || m_bInitializing)
    return; // Don't allow reload before skin is loaded by system

  std::string oldSkin = g_SkinInfo->ID();

  auto gui = CServiceBroker::GetGUI();

  if (gui)
  {
    CGUIMessage msg(GUI_MSG_LOAD_SKIN, -1, gui->GetWindowManager().GetActiveWindow());
    gui->GetWindowManager().SendMessage(msg);
  }

  const std::shared_ptr<CSettings> settings = CServiceBroker::GetSettingsComponent()->GetSettings();
  std::string newSkin = settings->GetString(CSettings::SETTING_LOOKANDFEEL_SKIN);
  if (LoadSkin(newSkin))
  {
    /* The Reset() or SetString() below will cause recursion, so the m_confirmSkinChange boolean is set so as to not prompt the
       user as to whether they want to keep the current skin. */
    if (confirm && m_confirmSkinChange)
    {
      if (KODI::MESSAGING::HELPERS::ShowYesNoDialogText(CVariant{13123}, CVariant{13111},
                                                        CVariant{""}, CVariant{""}, 10000) !=
          KODI::MESSAGING::HELPERS::DialogResponse::YES)
      {
        m_confirmSkinChange = false;
        settings->SetString(CSettings::SETTING_LOOKANDFEEL_SKIN, oldSkin);
      }
      else
      {
        if (gui)
          gui->GetWindowManager().ActivateWindow(WINDOW_STARTUP_ANIM);
      }
    }
  }
  else
  {
    // skin failed to load - we revert to the default only if we didn't fail loading the default
    std::string defaultSkin = std::static_pointer_cast<CSettingString>(
                                  settings->GetSetting(CSettings::SETTING_LOOKANDFEEL_SKIN))
                                  ->GetDefault();
    if (newSkin != defaultSkin)
    {
      m_confirmSkinChange = false;
      settings->GetSetting(CSettings::SETTING_LOOKANDFEEL_SKIN)->Reset();
      CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Error, g_localizeStrings.Get(24102),
                                            g_localizeStrings.Get(24103));
    }
  }
  m_confirmSkinChange = true;
}

bool CApplicationRendering::LoadSkin(const std::string& skinID)
{
  ADDON::SkinPtr skin;
  {
    ADDON::AddonPtr addon;
    if (!CServiceBroker::GetAddonMgr().GetAddon(skinID, addon, ADDON::ADDON_SKIN))
      return false;
    skin = std::static_pointer_cast<ADDON::CSkinInfo>(addon);
  }

  // store player and rendering state
  bool bPreviousPlayingState = false;

  enum class RENDERING_STATE
  {
    NONE,
    VIDEO,
    GAME,
  } previousRenderingState = RENDERING_STATE::NONE;

  auto gui = CServiceBroker::GetGUI();

  if (g_application.GetAppPlayer().IsPlayingVideo())
  {
    bPreviousPlayingState = !g_application.GetAppPlayer().IsPausedPlayback();
    if (bPreviousPlayingState)
      g_application.GetAppPlayer().Pause();
    g_application.GetAppPlayer().FlushRenderer();

    if (gui)
    {
      if (gui->GetWindowManager().GetActiveWindow() == WINDOW_FULLSCREEN_VIDEO)
      {
        gui->GetWindowManager().ActivateWindow(WINDOW_HOME);
        previousRenderingState = RENDERING_STATE::VIDEO;
      }
      else if (gui->GetWindowManager().GetActiveWindow() == WINDOW_FULLSCREEN_GAME)
      {
        gui->GetWindowManager().ActivateWindow(WINDOW_HOME);
        previousRenderingState = RENDERING_STATE::GAME;
      }
    }
  }

  CSingleLock lock(CServiceBroker::GetWinSystem()->GetGfxContext());

  // store current active window with its focused control
  int currentWindowID;
  int currentFocusedControlID = -1;

  if (gui)
  {
    currentWindowID = gui->GetWindowManager().GetActiveWindow();
    if (currentWindowID != WINDOW_INVALID)
    {
      CGUIWindow* pWindow = gui->GetWindowManager().GetWindow(currentWindowID);
      if (pWindow)
        currentFocusedControlID = pWindow->GetFocusedControlID();
    }
  }

  UnloadSkin();

  skin->Start();

  // migrate any skin-specific settings that are still stored in guisettings.xml
  CSkinSettings::GetInstance().MigrateSettings(skin);

  // check if the skin has been properly loaded and if it has a Home.xml
  if (!skin->HasSkinFile("Home.xml"))
  {
    CLog::Log(LOGERROR, "failed to load requested skin '%s'", skin->ID().c_str());
    return false;
  }

  CLog::Log(LOGINFO, "  load skin from: %s (version: %s)", skin->Path().c_str(),
            skin->Version().asString().c_str());
  g_SkinInfo = skin;

  CLog::Log(LOGINFO, "  load fonts for skin...");
  CServiceBroker::GetWinSystem()->GetGfxContext().SetMediaDir(skin->Path());
  g_directoryCache.ClearSubPaths(skin->Path());


  const std::shared_ptr<CSettings> settings = CServiceBroker::GetSettingsComponent()->GetSettings();
  if (gui)
    gui->GetColorManager().Load(settings->GetString(CSettings::SETTING_LOOKANDFEEL_SKINCOLORS));

  g_SkinInfo->LoadIncludes();

  g_fontManager.LoadFonts(settings->GetString(CSettings::SETTING_LOOKANDFEEL_FONT));

  // load in the skin strings
  std::string langPath = URIUtils::AddFileToFolder(skin->Path(), "language");
  URIUtils::AddSlashAtEnd(langPath);

  g_localizeStrings.LoadSkinStrings(langPath,
                                    settings->GetString(CSettings::SETTING_LOCALE_LANGUAGE));


  int64_t start;
  start = CurrentHostCounter();

  CLog::Log(LOGINFO, "  load new skin...");

  // Load custom windows
  LoadCustomWindows();

  int64_t end, freq;
  end = CurrentHostCounter();
  freq = CurrentHostFrequency();
  CLog::Log(LOGDEBUG, "Load Skin XML: %.2fms", 1000.f * (end - start) / freq);

  CLog::Log(LOGINFO, "  initialize new skin...");
  if (gui)
  {
    gui->GetWindowManager().AddMsgTarget(this);
    gui->GetWindowManager().AddMsgTarget(&CServiceBroker::GetPlaylistPlayer());
    gui->GetWindowManager().AddMsgTarget(&g_fontManager);
    gui->GetWindowManager().AddMsgTarget(&gui->GetStereoscopicsManager());
    gui->GetWindowManager().SetCallback(*this);
    //@todo should be done by GUIComponents
    gui->GetWindowManager().Initialize();
    CTextureCache::GetInstance().Initialize();
    gui->GetAudioManager().Enable(true);
    gui->GetAudioManager().Load();

    if (g_SkinInfo->HasSkinFile("DialogFullScreenInfo.xml"))
      gui->GetWindowManager().Add(new CGUIDialogFullScreenInfo);
  }

  CLog::Log(LOGINFO, "  skin loaded...");

  // leave the graphics lock
  lock.Leave();

  // restore active window
  if (gui)
  {
    if (currentWindowID != WINDOW_INVALID)
    {
      gui->GetWindowManager().ActivateWindow(currentWindowID);
      if (currentFocusedControlID != -1)
      {
        CGUIWindow* pWindow = gui->GetWindowManager().GetWindow(currentWindowID);
        if (pWindow && pWindow->HasSaveLastControl())
        {
          CGUIMessage msg(GUI_MSG_SETFOCUS, currentWindowID, currentFocusedControlID, 0);
          pWindow->OnMessage(msg);
        }
      }
    }

    // restore player and rendering state
    if (g_application.GetAppPlayer().IsPlayingVideo())
    {
      if (bPreviousPlayingState)
        g_application.GetAppPlayer().Pause();

      switch (previousRenderingState)
      {
        case RENDERING_STATE::VIDEO:
          gui->GetWindowManager().ActivateWindow(WINDOW_FULLSCREEN_VIDEO);
          break;
        case RENDERING_STATE::GAME:
          gui->GetWindowManager().ActivateWindow(WINDOW_FULLSCREEN_GAME);
          break;
        default:
          break;
      }
    }
  }

  return true;
}

void CApplicationRendering::UnloadSkin()
{
  if (g_SkinInfo != nullptr && m_saveSkinOnUnloading)
    g_SkinInfo->SaveSettings();
  else if (!m_saveSkinOnUnloading)
    m_saveSkinOnUnloading = true;

  CGUIComponent* gui = CServiceBroker::GetGUI();
  if (gui)
  {
    gui->GetAudioManager().Enable(false);

    gui->GetWindowManager().DeInitialize();
    CTextureCache::GetInstance().Deinitialize();

    // remove the skin-dependent window
    gui->GetWindowManager().Delete(WINDOW_DIALOG_FULLSCREEN_INFO);

    gui->GetTextureManager().Cleanup();
    gui->GetLargeTextureManager().CleanupUnusedImages(true);

    g_fontManager.Clear();

    gui->GetColorManager().Clear();

    gui->GetInfoManager().Clear();
  }

  //  The g_SkinInfo shared_ptr ought to be reset here
  // but there are too many places it's used without checking for NULL
  // and as a result a race condition on exit can cause a crash.

  CLog::Log(LOGINFO, "Unloaded skin");
}

bool CApplicationRendering::LoadCustomWindows()
{
  // Start from wherever home.xml is
  std::vector<std::string> vecSkinPath;
  g_SkinInfo->GetSkinPaths(vecSkinPath);

  for (const auto& skinPath : vecSkinPath)
  {
    CLog::Log(LOGINFO, "Loading custom window XMLs from skin path %s", skinPath.c_str());

    CFileItemList items;
    if (XFILE::CDirectory::GetDirectory(skinPath, items, ".xml",
                                        XFILE::DIR_FLAG::DIR_FLAG_NO_FILE_DIRS))
    {
      for (const auto& item : items)
      {
        if (item->m_bIsFolder)
          continue;

        std::string skinFile = URIUtils::GetFileName(item->GetPath());
        if (StringUtils::StartsWithNoCase(skinFile, "custom"))
        {
          CXBMCTinyXML xmlDoc;
          if (!xmlDoc.LoadFile(item->GetPath()))
          {
            CLog::Log(LOGERROR, "Unable to load custom window XML %s. Line %d\n%s",
                      item->GetPath().c_str(), xmlDoc.ErrorRow(), xmlDoc.ErrorDesc());
            continue;
          }

          // Root element should be <window>
          TiXmlElement* pRootElement = xmlDoc.RootElement();
          std::string strValue = pRootElement->Value();
          if (!StringUtils::EqualsNoCase(strValue, "window"))
          {
            CLog::Log(LOGERROR, "No <window> root element found for custom window in %s",
                      skinFile.c_str());
            continue;
          }

          int id = WINDOW_INVALID;

          // Read the type attribute or element to get the window type to create
          // If no type is specified, create a CGUIWindow as default
          std::string strType;
          if (pRootElement->Attribute("type"))
            strType = pRootElement->Attribute("type");
          else
          {
            const TiXmlNode* pType = pRootElement->FirstChild("type");
            if (pType && pType->FirstChild())
              strType = pType->FirstChild()->Value();
          }

          // Read the id attribute or element to get the window id
          if (!pRootElement->Attribute("id", &id))
          {
            const TiXmlNode* pType = pRootElement->FirstChild("id");
            if (pType && pType->FirstChild())
              id = atol(pType->FirstChild()->Value());
          }

          int windowId = id + WINDOW_HOME;

          auto gui = CServiceBroker::GetGUI();
          if (gui)
          {
            if (id == WINDOW_INVALID || gui->GetWindowManager().GetWindow(windowId))
            {
              // No id specified or id already in use
              CLog::Log(LOGERROR, "No id specified or id already in use for custom window in %s",
                        skinFile.c_str());
              continue;
            }
          }

          CGUIWindow* pWindow = NULL;
          bool hasVisibleCondition = false;

          if (StringUtils::EqualsNoCase(strType, "dialog"))
          {
            hasVisibleCondition = pRootElement->FirstChildElement("visible") != nullptr;
            pWindow = new CGUIDialog(windowId, skinFile);
          }
          else if (StringUtils::EqualsNoCase(strType, "submenu"))
          {
            pWindow = new CGUIDialogSubMenu(windowId, skinFile);
          }
          else if (StringUtils::EqualsNoCase(strType, "buttonmenu"))
          {
            pWindow = new CGUIDialogButtonMenu(windowId, skinFile);
          }
          else
          {
            pWindow = new CGUIWindow(windowId, skinFile);
          }

          if (!pWindow)
          {
            CLog::Log(LOGERROR, "Failed to create custom window from %s", skinFile.c_str());
            continue;
          }

          pWindow->SetCustom(true);

          // Determining whether our custom dialog is modeless (visible condition is present)
          // will be done on load. Therefore we need to initialize the custom dialog on gui init.
          pWindow->SetLoadType(hasVisibleCondition ? CGUIWindow::LOAD_ON_GUI_INIT
                                                   : CGUIWindow::KEEP_IN_MEMORY);

          if (gui)
            gui->GetWindowManager().AddCustomWindow(pWindow);
        }
      }
    }
  }
  return true;
}

bool CApplicationRendering::OnMessage(CGUIMessage& message)
{
  auto gui = CServiceBroker::GetGUI();

  switch (message.GetMessage())
  {
    case GUI_MSG_NOTIFY_ALL:
    {
      if (message.GetParam1() == GUI_MSG_UI_READY)
      {
        if (gui)
          gui->GetWindowManager().Delete(WINDOW_SPLASH);

        m_bInitializing = false;
      }
    }
    break;

    case GUI_MSG_EXECUTE:
    {
      if (message.GetNumStringParams())
        return g_application.ExecuteXBMCAction(message.GetStringParam(), message.GetItem());
      break;
    }

    default:
      break;
  }
  return false;
}
