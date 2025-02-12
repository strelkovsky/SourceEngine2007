// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#include "include/vgui_mat_sys_app.h"

#include "FileSystem.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "base/include/windows/windows_light.h"
#include "filesystem_init.h"
#include "inputsystem/iinputsystem.h"
#include "materialsystem/IMaterialSystem.h"
#include "materialsystem/MaterialSystem_Config.h"
#include "tier0/include/dbg.h"
#include "tier0/include/icommandline.h"
#include "tier3/tier3.h"
#include "vgui/ILocalize.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui_controls/controls.h"

#include "tier0/include/memdbgon.h"

// Create all singleton systems
bool CVguiMatSysApp::Create() {
  AppSystemInfo_t app_systems[] = {
      {"inputsystem.dll", INPUTSYSTEM_INTERFACE_VERSION},
      {"materialsystem.dll", MATERIAL_SYSTEM_INTERFACE_VERSION},

      // NOTE: This has to occur before vgui2.dll so it replaces vgui2's surface
      // implementation
      {"vguimatsurface.dll", VGUI_SURFACE_INTERFACE_VERSION},
      {"vgui2.dll", VGUI_IVGUI_INTERFACE_VERSION},

      // Required to terminate the list
      {"", ""}};

  if (!AddSystems(app_systems)) return false;

  auto *material_system =
      FindSystem<IMaterialSystem>(MATERIAL_SYSTEM_INTERFACE_VERSION);

  if (!material_system) {
    Warning(
        "CVguiMatSysApp::Create: Unable to connect to necessary interface!\n");
    return false;
  }

  material_system->SetShaderAPI("shaderapidx9.dll");
  return true;
}

void CVguiMatSysApp::Destroy() {}

// Window management
HWND CVguiMatSysApp::CreateAppWindow(ch const *pTitle, bool bWindowed, i32 w,
                                     i32 h) {
  WNDCLASSEX wc{sizeof(wc)};
  wc.style = CS_OWNDC | CS_DBLCLKS;
  wc.lpfnWndProc = DefWindowProc;
  wc.hInstance = (HINSTANCE)GetAppInstance();
  wc.lpszClassName = "Valve002";
  wc.hIcon =
      nullptr;  // LoadIcon( s_HInstance, MAKEINTRESOURCE( IDI_LAUNCHER ) );
  wc.hIconSm = wc.hIcon;

  RegisterClassEx(&wc);

  // Note, it's hidden
  DWORD style = WS_POPUP | WS_CLIPSIBLINGS;

  if (bWindowed) {
    // Give it a frame
    style |= WS_OVERLAPPEDWINDOW;
    style &= ~WS_THICKFRAME;
  }

  // Never a max box
  style &= ~WS_MAXIMIZEBOX;

  RECT windowRect;
  windowRect.top = 0;
  windowRect.left = 0;
  windowRect.right = w;
  windowRect.bottom = h;

  // Compute rect needed for that size client area based on window style
  AdjustWindowRectEx(&windowRect, style, FALSE, 0);

  // Create the window
  HWND hWnd = CreateWindow(wc.lpszClassName, pTitle, style, 0, 0,
                           windowRect.right - windowRect.left,
                           windowRect.bottom - windowRect.top, nullptr, nullptr,
                           (HINSTANCE)GetAppInstance(), nullptr);

  if (!hWnd) return nullptr;

  i32 CenterX, CenterY;

  CenterX = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
  CenterY = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
  CenterX = (CenterX < 0) ? 0 : CenterX;
  CenterY = (CenterY < 0) ? 0 : CenterY;

  // In VCR modes, keep it in the upper left so mouse coordinates are always
  // relative to the window.
  SetWindowPos(hWnd, nullptr, CenterX, CenterY, 0, 0,
               SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);

  return hWnd;
}

// Pump messages
void CVguiMatSysApp::AppPumpMessages() { g_pInputSystem->PollInputState(); }

// Sets up the game path
bool CVguiMatSysApp::SetupSearchPaths(const ch *pStartingDir,
                                      bool bOnlyUseStartingDir, bool bIsTool) {
  if (!BaseClass::SetupSearchPaths(pStartingDir, bOnlyUseStartingDir, bIsTool))
    return false;

  g_pFullFileSystem->AddSearchPath(GetGameInfoPath(), "SKIN", PATH_ADD_TO_HEAD);
  return true;
}

// Init, shutdown
bool CVguiMatSysApp::PreInit() {
  if (!BaseClass::PreInit()) return false;

  if (!g_pFullFileSystem || !g_pMaterialSystem || !g_pInputSystem ||
      !g_pMatSystemSurface) {
    Warning(
        "CVguiMatSysApp::PreInit: Unable to connect to necessary interface!\n");
    return false;
  }

  // Add paths...
  if (!SetupSearchPaths(nullptr, false, true)) return false;

  const ch *pArg;
  i32 iWidth = 1024;
  i32 iHeight = 768;
  bool bWindowed = (CommandLine()->CheckParm("-fullscreen") == nullptr);
  if (CommandLine()->CheckParm("-width", &pArg)) {
    iWidth = atoi(pArg);
  }
  if (CommandLine()->CheckParm("-height", &pArg)) {
    iHeight = atoi(pArg);
  }

  m_nWidth = iWidth;
  m_nHeight = iHeight;
  m_HWnd = CreateAppWindow(GetAppName(), bWindowed, iWidth, iHeight);
  if (!m_HWnd) return false;

  g_pInputSystem->AttachToWindow(m_HWnd);
  g_pMatSystemSurface->AttachToWindow(m_HWnd);

  // NOTE: If we specifically wanted to use a particular shader DLL, we set it
  // here...
  // m_pMaterialSystem->SetShaderAPI( "shaderapidx8" );

  // Get the adapter from the command line....
  const ch *pAdapterString;
  i32 adapter = 0;
  if (CommandLine()->CheckParm("-adapter", &pAdapterString)) {
    adapter = atoi(pAdapterString);
  }

  i32 adapterFlags = 0;
  if (CommandLine()->CheckParm("-ref")) {
    adapterFlags |= MATERIAL_INIT_REFERENCE_RASTERIZER;
  }
  if (AppUsesReadPixels()) {
    adapterFlags |= MATERIAL_INIT_ALLOCATE_FULLSCREEN_TEXTURE;
  }

  g_pMaterialSystem->SetAdapter(adapter, adapterFlags);

  return true;
}

void CVguiMatSysApp::PostShutdown() {
  if (g_pMatSystemSurface && g_pInputSystem) {
    g_pMatSystemSurface->AttachToWindow(nullptr);
    g_pInputSystem->DetachFromWindow();
  }

  BaseClass::PostShutdown();
}

// Gets the window size
i32 CVguiMatSysApp::GetWindowWidth() const { return m_nWidth; }

i32 CVguiMatSysApp::GetWindowHeight() const { return m_nHeight; }

// Returns the window
void *CVguiMatSysApp::GetAppWindow() { return m_HWnd; }

// Sets the video mode
bool CVguiMatSysApp::SetVideoMode() {
  MaterialSystem_Config_t config;
  if (CommandLine()->CheckParm("-fullscreen")) {
    config.SetFlag(MATSYS_VIDCFG_FLAGS_WINDOWED, false);
  } else {
    config.SetFlag(MATSYS_VIDCFG_FLAGS_WINDOWED, true);
  }

  if (CommandLine()->CheckParm("-resizing")) {
    config.SetFlag(MATSYS_VIDCFG_FLAGS_RESIZING, true);
  }

  if (CommandLine()->CheckParm("-mat_vsync")) {
    config.SetFlag(MATSYS_VIDCFG_FLAGS_NO_WAIT_FOR_VSYNC, false);
  }

  config.m_nAASamples = CommandLine()->ParmValue("-mat_antialias", 1);
  config.m_nAAQuality = CommandLine()->ParmValue("-mat_aaquality", 0);

  config.m_VideoMode.m_Width = config.m_VideoMode.m_Height = 0;
  config.m_VideoMode.m_Format = IMAGE_FORMAT_BGRX8888;
  config.m_VideoMode.m_RefreshRate = 0;
  bool modeSet = g_pMaterialSystem->SetMode(m_HWnd, config);
  if (!modeSet) {
    Error("Unable to set mode\n");
    return false;
  }

  g_pMaterialSystem->OverrideConfig(config, false);
  return true;
}
