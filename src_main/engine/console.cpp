// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "client_pch.h"

#include "console.h"

#include <ctime>
#ifndef SWDS
#include "draw.h"
#include "io.h"
#include "keys.h"
#endif
#include "MapReslistGenerator.h"
#include "ivideomode.h"
#include "server.h"
#include "sv_main.h"
#include "tier0/include/icommandline.h"
#include "tier0/include/vcrmode.h"
#include "zone.h"

#include "tier0/include/memdbgon.h"

#define MAXPRINTMSG 16384

bool con_debuglog = false;
bool con_initialized = false;
bool con_debuglogmapprefixed = false;

static ConVar con_timestamp("con_timestamp", "0", 0,
                            "Prefix console.log entries with timestamps");

void ConsoleLogFileCallback(IConVar *var, const ch *pOldValue,
                            float flOldValue) {
  ConVarRef ref(var->GetName());
  const ch *log_file = ref.GetString();

  if (!COM_IsValidPath(log_file)) {
    con_debuglog = CommandLine()->FindParm("-condebug") != 0;
  } else {
    con_debuglog = true;
  }
}

ConVar con_logfile("con_logfile", "", 0,
                   "Console output gets written to this file", false, 0.0f,
                   false, 0.0f, ConsoleLogFileCallback);

const ch *GetConsoleLogFilename() {
  const ch *log_file = con_logfile.GetString();
  return !COM_IsValidPath(log_file) ? "console.log" : log_file;
}

static const ch *GetTimestampString() {
  static ch timestamp[128];

  tm today;
  VCRHook_LocalTime(&today);
  Q_snprintf(timestamp, std::size(timestamp), "%02i/%02i/%04i - %02i:%02i:%02i",
             today.tm_mon + 1, today.tm_mday, 1900 + today.tm_year,
             today.tm_hour, today.tm_min, today.tm_sec);

  return timestamp;
}

#ifndef SWDS
static ConVar con_trace("con_trace", "0", 0,
                        "Print console text to low level printout.");
static ConVar con_notifytime("con_notifytime", "8", 0,
                             "How long to display recent console text to the "
                             "upper part of the game window");
static ConVar con_times("contimes", "8", 0,
                        "Number of console lines to overlay for debugging.");
static ConVar con_drawnotify(
    "con_drawnotify", "1", 0,
    "Disables drawing of notification area (for taking screenshots).");
static ConVar con_enable("con_enable", "0", FCVAR_ARCHIVE,
                         "Allows the console to be activated.");
static ConVar con_filter_enable("con_filter_enable", "0", 0,
                                "Filters console output based on the setting "
                                "of con_filter_text. 1 filters completely, 2 "
                                "displays filtered text brighter than other "
                                "text.");
static ConVar con_filter_text("con_filter_text", "", 0,
                              "Text with which to filter console spew. Set "
                              "con_filter_enable 1 or 2 to activate.");
static ConVar con_filter_text_out("con_filter_text_out", "", 0,
                                  "Text with which to filter OUT of console "
                                  "spew. Set con_filter_enable 1 or 2 to "
                                  "activate.");

// Purpose: Implements the console using VGUI.
class CConPanel : public CBasePanel {
  typedef CBasePanel BaseClass;

 public:
  enum { MAX_NOTIFY_TEXT_LINE = 256 };

  CConPanel(vgui::Panel *parent);
  virtual ~CConPanel();

  virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

  // Draws the text
  virtual void Paint();
  // Draws the background image
  virtual void PaintBackground();

  // Draw notify area
  virtual void DrawNotify(void);
  // Draws debug ( Con_NXPrintf ) areas
  virtual void DrawDebugAreas(void);

  int ProcessNotifyLines(int &left, int &top, int &right, int &bottom,
                         bool bDraw);

  // Draw helpers
  virtual int DrawText(vgui::HFont font, int x, int y, wchar_t *fmt, ...);

  virtual bool ShouldDraw(void);

  void Con_NPrintf(int idx, const ch *msg);
  void Con_NXPrintf(const struct con_nprint_s *info, const ch *msg);

  void AddToNotify(const Color &clr, ch const *msg);
  void ClearNofify();

 private:
  // Console font
  vgui::HFont m_hFont;
  vgui::HFont m_hFontFixed;

  struct CNotifyText {
    Color clr;
    float liferemaining;
    wchar_t text[MAX_NOTIFY_TEXT_LINE];
  };

  CUtlVector<CNotifyText> m_NotifyText;

  enum {
    MAX_DBG_NOTIFY = 128,
    DBG_NOTIFY_TIMEOUT = 4,
  };

  float da_default_color[3];

  struct da_notify_t {
    wchar_t szNotify[MAX_NOTIFY_TEXT_LINE];
    float expire;
    float color[3];
    bool fixed_width_font;
  };

  da_notify_t da_notify[MAX_DBG_NOTIFY];
  bool m_bDrawDebugAreas;
};

static CConPanel *g_pConPanel = nullptr;

void Con_HideConsole_f() {
  if (EngineVGui()->IsConsoleVisible()) {
    // hide the console
    EngineVGui()->HideConsole();
  }
}

void Con_ShowConsole_f() {
  if (vgui::input()->GetAppModalSurface()) {
    // If a dialog has modal, it probably has grabbed keyboard focus, so showing
    // the console would be a bad idea.
    return;
  }

  // make sure we're allowed to see the console
  if (con_enable.GetBool() || developer.GetInt() ||
      CommandLine()->CheckParm("-console") ||
      CommandLine()->CheckParm("-rpt")) {
    // show the console
    EngineVGui()->ShowConsole();

    // remove any loading screen
    SCR_EndLoadingPlaque();
  }
}

// Purpose: Toggles the console.
void Con_ToggleConsole_f() {
  if (EngineVGui()->IsConsoleVisible()) {
    Con_HideConsole_f();

    // If we hide the console, we also hide the game UI
    EngineVGui()->HideGameUI();
  } else {
    Con_ShowConsole_f();
  }
}

// Purpose: Clears the console.
void Con_Clear_f() {
  EngineVGui()->ClearConsole();
  Con_ClearNotify();
}

void Con_ClearNotify() {
  if (g_pConPanel) g_pConPanel->ClearNofify();
}
#endif  // SWDS

void Con_Init() {
#ifdef OS_POSIX
  con_debuglog = false;  // the dedicated server's console will handle this
  con_debuglogmapprefixed = false;
#else
  const bool bRPTClient = CommandLine()->FindParm("-rpt") != 0;
  con_debuglog = bRPTClient || (CommandLine()->FindParm("-condebug") != 0);
  con_debuglogmapprefixed = CommandLine()->FindParm("-makereslists") != 0;

  if (con_debuglog) {
    con_logfile.SetValue("console.log");
    if (bRPTClient || (CommandLine()->FindParm("-conclearlog"))) {
      g_pFileSystem->RemoveFile(GetConsoleLogFilename(), "GAME");
    }
  }
#endif  // !OS_POSIX

  con_initialized = true;
}

void Con_Shutdown() { con_initialized = false; }

void Con_DebugLog(const ch *fmt, ...) {
  va_list argptr;
  ch data[MAXPRINTMSG];

  va_start(argptr, fmt);
  Q_vsnprintf(data, sizeof(data), fmt, argptr);
  va_end(argptr);

  const ch *file = GetConsoleLogFilename();

  FileHandle_t fh = g_pFileSystem->Open(file, "a");
  if (fh != FILESYSTEM_INVALID_HANDLE) {
    if (con_debuglogmapprefixed) {
      ch const *prefix = MapReslistGenerator().LogPrefix();
      if (prefix) {
        g_pFileSystem->Write(prefix, strlen(prefix), fh);
      }
    }

    if (con_timestamp.GetBool()) {
      static bool needTimestamp =
          true;  // Start the first line with a timestamp
      if (needTimestamp) {
        const ch *timestamp = GetTimestampString();
        g_pFileSystem->Write(timestamp, strlen(timestamp), fh);
        g_pFileSystem->Write(": ", 2, fh);
      }
      needTimestamp = V_stristr(data, "\n");
    }

    g_pFileSystem->Write(data, strlen(data), fh);
    g_pFileSystem->Close(fh);
  }
}

static bool g_fIsDebugPrint = false;

#ifndef SWDS
// Handles cursor positioning, line wrapping, etc
static bool g_fColorPrintf = false;
static bool g_bInColorPrint = false;
extern thread_local bool g_bInSpew;

void Con_Printf(const ch *fmt, ...);

void Con_ColorPrint(const Color &clr, ch const *msg) {
  if (g_bInColorPrint) return;

  int nCon_Filter_Enable = con_filter_enable.GetInt();
  if (nCon_Filter_Enable > 0) {
    const ch *pszText = con_filter_text.GetString();
    const ch *pszIgnoreText = con_filter_text_out.GetString();

    switch (nCon_Filter_Enable) {
      case 1:
        // if line does not contain keyword do not print the line
        if (pszText && (*pszText != '\0') && (Q_stristr(msg, pszText) == NULL))
          return;
        if (pszIgnoreText && *pszIgnoreText &&
            (Q_stristr(msg, pszIgnoreText) != NULL))
          return;
        break;

      case 2:
        if (pszIgnoreText && *pszIgnoreText &&
            (Q_stristr(msg, pszIgnoreText) != NULL))
          return;
        // if line does not contain keyword print it in a darker color
        if (pszText && (*pszText != '\0') &&
            (Q_stristr(msg, pszText) == NULL)) {
          Color mycolor(200, 200, 200, 150);
          g_pCVar->ConsoleColorPrintf(mycolor, msg);
          return;
        }
        break;

      default:
        // by default do no filtering
        break;
    }
  }

  g_bInColorPrint = true;

  // also echo to debugging console
  if (Plat_IsInDebugSession() && !con_trace.GetInt()) {
    Plat_DebugString(msg);
  }

  if (sv.IsDedicated()) {
    g_bInColorPrint = false;
    return;  // no graphics mode
  }

  bool convisible = Con_IsVisible();
  bool indeveloper = (developer.GetInt() > 0);
  bool debugprint = g_fIsDebugPrint;

  if (g_fColorPrintf) {
    g_pCVar->ConsoleColorPrintf(clr, "%s", msg);
  } else {
    // write it out to the vgui console no matter what
    if (g_fIsDebugPrint) {
      // Don't spew debug stuff to actual console once in game, unless console
      // isn't up
      if (!cl.IsActive() || !convisible) {
        g_pCVar->ConsoleDPrintf("%s", msg);
      }
    } else {
      g_pCVar->ConsolePrintf("%s", msg);
    }
  }

  // Make sure we "spew" if this wan't generated from the spew system
  if (!g_bInSpew) {
    Msg("%s", msg);
  }

  // Only write to notify if it's non-debug or we are running with developer
  // set > 0 Buf it it's debug then make sure we don't have the console down
  if ((!debugprint || indeveloper) && !(debugprint && convisible)) {
    if (g_pConPanel) {
      g_pConPanel->AddToNotify(clr, msg);
    }
  }
  g_bInColorPrint = false;
}
#endif

// returns false if the print function shouldn't continue
bool HandleRedirectAndDebugLog(const ch *msg) {
  // Add to redirected message
  if (SV_RedirectActive()) {
    SV_RedirectAddText(msg);
    return false;
  }

  // log all messages to file
  if (con_debuglog) Con_DebugLog("%s", msg);

  return con_initialized;
}

void Con_Print(const ch *msg) {
  if (!msg || !msg[0]) return;

  if (!HandleRedirectAndDebugLog(msg)) {
    return;
  }

#ifdef SWDS
  Msg("%s", msg);
#else
  if (sv.IsDedicated()) {
    Msg("%s", msg);
  } else {
#if !defined(_X360)
    Color clr(255, 255, 255, 255);
#else
    Color clr(0, 0, 0, 255);
#endif
    Con_ColorPrint(clr, msg);
  }
#endif
}

void Con_Printf(const ch *fmt, ...) {
  va_list argptr;
  ch msg[MAXPRINTMSG];
  static bool inupdate;

  va_start(argptr, fmt);
  Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
  va_end(argptr);

#ifndef NO_VCR
  // Normally, we shouldn't need to write this data to the file, but it can help
  // catch out-of-sync errors earlier.
  if (vcr_verbose.GetInt()) {
    VCRGenericString("Con_Printf", msg);
  }
#endif

  if (!HandleRedirectAndDebugLog(msg)) {
    return;
  }

#ifdef SWDS
  Msg("%s", msg);
#else
  if (sv.IsDedicated()) {
    Msg("%s", msg);
  } else {
#if !defined(_X360)
    Color clr(255, 255, 255, 255);
#else
    Color clr(0, 0, 0, 255);
#endif
    Con_ColorPrint(clr, msg);
  }
#endif
}

#ifndef SWDS
void Con_ColorPrintf(const Color &clr, const ch *fmt, ...) {
  va_list argptr;
  ch msg[MAXPRINTMSG];
  static bool inupdate;

  va_start(argptr, fmt);
  Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
  va_end(argptr);

  LOCAL_THREAD_LOCK();
  if (!HandleRedirectAndDebugLog(msg)) {
    return;
  }

  g_fColorPrintf = true;
  Con_ColorPrint(clr, msg);
  g_fColorPrintf = false;
}
#endif

// A Con_Printf that only shows up if the "developer" cvar is set
void Con_DPrintf(const ch *fmt, ...) {
  va_list argptr;
  ch msg[MAXPRINTMSG];

  va_start(argptr, fmt);
  Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
  va_end(argptr);

  g_fIsDebugPrint = true;

#ifdef SWDS
  DevMsg("%s", msg);
#else
  if (sv.IsDedicated()) {
    DevMsg("%s", msg);
  } else {
    Color clr(196, 181, 80, 255);
    Con_ColorPrint(clr, msg);
  }
#endif

  g_fIsDebugPrint = false;
}

// Okay to call even when the screen can't be updated
void Con_SafePrintf(const ch *fmt, ...) {
  va_list argptr;
  ch msg[MAXPRINTMSG];

  va_start(argptr, fmt);
  Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
  va_end(argptr);

#ifndef SWDS
  int temp = scr_disabled_for_loading;
  scr_disabled_for_loading = true;
#endif
  g_fIsDebugPrint = true;
  Con_Printf("%s", msg);
  g_fIsDebugPrint = false;
#ifndef SWDS
  scr_disabled_for_loading = temp;
#endif
}

#ifndef SWDS
bool Con_IsVisible() { return (EngineVGui()->IsConsoleVisible()); }

void Con_NPrintf(int idx, const ch *fmt, ...) {
  va_list argptr;
  ch outtext[MAXPRINTMSG];

  va_start(argptr, fmt);
  Q_vsnprintf(outtext, sizeof(outtext), fmt, argptr);
  va_end(argptr);

  g_pConPanel->Con_NPrintf(idx, outtext);
}

void Con_NXPrintf(const struct con_nprint_s *info, const ch *fmt, ...) {
  va_list argptr;
  ch outtext[MAXPRINTMSG];

  va_start(argptr, fmt);
  Q_vsnprintf(outtext, sizeof(outtext), fmt, argptr);
  va_end(argptr);

  g_pConPanel->Con_NXPrintf(info, outtext);
}

// Purpose: Creates the console panel.
CConPanel::CConPanel(vgui::Panel *parent) : CBasePanel(parent, "CConPanel") {
  // Full screen assumed
  SetSize(videomode->GetModeWidth(), videomode->GetModeHeight());
  SetPos(0, 0);
  SetVisible(true);
  SetCursor(0);

  da_default_color[0] = 1.0;
  da_default_color[1] = 1.0;
  da_default_color[2] = 1.0;

  m_bDrawDebugAreas = false;

  g_pConPanel = this;
  memset(da_notify, 0, sizeof(da_notify));
}

CConPanel::~CConPanel() {}

void CConPanel::Con_NPrintf(int idx, const ch *msg) {
  if (idx < 0 || idx >= MAX_DBG_NOTIFY) return;

  swprintf_s(da_notify[idx].szNotify, L"%S", msg);

  // Reset values
  da_notify[idx].expire = realtime + DBG_NOTIFY_TIMEOUT;
  VectorCopy(da_default_color, da_notify[idx].color);
  da_notify[idx].fixed_width_font = false;
  m_bDrawDebugAreas = true;
}

void CConPanel::Con_NXPrintf(const struct con_nprint_s *info, const ch *msg) {
  if (!info) return;

  if (info->index < 0 || info->index >= MAX_DBG_NOTIFY) return;

  swprintf_s(da_notify[info->index].szNotify, L"%S", msg);

  // Reset values
  // -1 is special marker means to just draw it once
  da_notify[info->index].expire =
      info->time_to_live == -1 ? -1 : realtime + info->time_to_live;

  VectorCopy(info->color, da_notify[info->index].color);
  da_notify[info->index].fixed_width_font = info->fixed_width_font;
  m_bDrawDebugAreas = true;
}

static void safestrncat(wchar_t *text, int maxCharactersWithNullTerminator,
                        wchar_t const *add, int addchars) {
  int maxCharactersWithoutTerminator = maxCharactersWithNullTerminator - 1;

  int curlen = wcslen(text);
  if (curlen >= maxCharactersWithoutTerminator) return;

  wchar_t *p = text + curlen;
  while (curlen++ < maxCharactersWithoutTerminator && --addchars >= 0) {
    *p++ = *add++;
  }
  *p = 0;
}

void CConPanel::AddToNotify(const Color &clr, ch const *msg) {
  if (!host_initialized) return;

  // notify area only ever draws in developer mode - it should never be used for
  // game messages
  if (!developer.GetBool()) return;

  // skip any special characters
  if (msg[0] == 1 || msg[0] == 2) {
    msg++;
  }

  // Nothing left
  if (!msg[0]) return;

  CNotifyText *current = NULL;

  int slot = m_NotifyText.Count() - 1;
  if (slot < 0) {
    slot = m_NotifyText.AddToTail();
    current = &m_NotifyText[slot];
    current->clr = clr;
    current->text[0] = 0;
    current->liferemaining = con_notifytime.GetFloat();
    ;
  } else {
    current = &m_NotifyText[slot];
    current->clr = clr;
  }

  Assert(current);

  wchar_t unicode[1024];
  g_pVGuiLocalize->ConvertANSIToUnicode(msg, unicode, sizeof(unicode));

  wchar_t const *p = unicode;
  while (*p) {
    const wchar_t *nextreturn = wcschr(p, L'\n');
    if (nextreturn != nullptr) {
      int copysize = nextreturn - p + 1;
      safestrncat(current->text, MAX_NOTIFY_TEXT_LINE, p, copysize);

      // Add a new notify, but don't add a new one if the previous one was
      // empty...
      if (current->text[0] && current->text[0] != L'\n') {
        slot = m_NotifyText.AddToTail();
        current = &m_NotifyText[slot];
      }
      // Clear it
      current->clr = clr;
      current->text[0] = 0;
      current->liferemaining = con_notifytime.GetFloat();
      // Skip return character
      p += copysize;
      continue;
    }

    // Append it
    safestrncat(current->text, MAX_NOTIFY_TEXT_LINE, p, wcslen(p));
    current->clr = clr;
    current->liferemaining = con_notifytime.GetFloat();
    break;
  }

  while (m_NotifyText.Count() > 0 &&
         (m_NotifyText.Count() >= con_times.GetInt())) {
    m_NotifyText.Remove(0);
  }
}

void CConPanel::ClearNofify() { m_NotifyText.RemoveAll(); }

void CConPanel::ApplySchemeSettings(vgui::IScheme *pScheme) {
  BaseClass::ApplySchemeSettings(pScheme);

  // Console font
  m_hFont = pScheme->GetFont("DefaultSmallDropShadow", false);
  m_hFontFixed = pScheme->GetFont("DefaultFixedDropShadow", false);
}

int CConPanel::DrawText(vgui::HFont font, int x, int y, wchar_t *fmt, ...) {
  va_list argptr;
  wchar_t data[1024];
  int len;

  va_start(argptr, fmt);
  swprintf_s(data, fmt, argptr);
  va_end(argptr);

  len = DrawColoredText(font, x, y, 255, 255, 255, 255, data);

  return len;
}

// Called when we're ticked...
bool CConPanel::ShouldDraw() {
  bool bVisible = false;

  if (m_bDrawDebugAreas) {
    bVisible = true;
  }

  // Should be invisible if there's no notifys and the console is up.
  // and if the launcher isn't active
  if (!Con_IsVisible()) {
    int c = m_NotifyText.Count();
    for (int i = c - 1; i >= 0; i--) {
      CNotifyText *notify = &m_NotifyText[i];

      notify->liferemaining -= host_frametime;

      if (notify->liferemaining <= 0.0f) {
        m_NotifyText.Remove(i);
        continue;
      }

      bVisible = true;
    }
  } else {
    bVisible = true;
  }

  return bVisible;
}

void CConPanel::DrawNotify() {
  if (!m_hFontFixed) return;

  // notify area only draws in developer mode
  if (!developer.GetBool()) return;

  // don't render notify area into movies, either
  if (cl_movieinfo.IsRecording()) {
    return;
  }

  vgui::surface()->DrawSetTextFont(m_hFontFixed);

  int fontTall = vgui::surface()->GetFontTall(m_hFontFixed) + 1;

  Color clr;
  int x = 8, y = 5;

  int c = m_NotifyText.Count();
  for (int i = 0; i < c; i++) {
    CNotifyText *notify = &m_NotifyText[i];

    float timeleft = notify->liferemaining;

    clr = notify->clr;

    if (timeleft < .5f) {
      float f = std::clamp(timeleft, 0.0f, .5f) / .5f;

      clr[3] = (int)(f * 255.0f);

      if (i == 0 && f < 0.2f) {
        y -= fontTall * (1.0f - f / 0.2f);
      }
    } else {
      clr[3] = 255;
    }

    DrawColoredText(m_hFontFixed, x, y, clr[0], clr[1], clr[2], clr[3],
                    notify->text);

    y += fontTall;
  }
}

ConVar con_nprint_bgalpha("con_nprint_bgalpha", "50", 0,
                          "Con_NPrint background alpha.");
ConVar con_nprint_bgborder("con_nprint_bgborder", "5", 0,
                           "Con_NPrint border size.");

void CConPanel::DrawDebugAreas() {
  if (!m_bDrawDebugAreas) return;

  // Find the top and bottom of all the nprint text so we can draw a box behind
  // it.
  int left = 99999, top = 99999, right = -99999, bottom = -99999;
  if (con_nprint_bgalpha.GetInt()) {
    // First, figure out the bounds of all the con_nprint text.
    if (ProcessNotifyLines(left, top, right, bottom, false)) {
      int b = con_nprint_bgborder.GetInt();

      // Now draw a box behind it.
      vgui::surface()->DrawSetColor(0, 0, 0, con_nprint_bgalpha.GetInt());
      vgui::surface()->DrawFilledRect(left - b, top - b, right + b, bottom + b);
    }
  }

  // Now draw the text.
  if (ProcessNotifyLines(left, top, right, bottom, true) == 0) {
    // Have all notifies expired?
    m_bDrawDebugAreas = false;
  }
}

int CConPanel::ProcessNotifyLines(int &left, int &top, int &right, int &bottom,
                                  bool bDraw) {
  int count = 0;
  int y = 20;

  for (int i = 0; i < MAX_DBG_NOTIFY; i++) {
    if (realtime < da_notify[i].expire || da_notify[i].expire == -1) {
      // If it's marked this way, only draw it once.
      if (da_notify[i].expire == -1 && bDraw) {
        da_notify[i].expire = realtime - 1;
      }

      int len;
      int x;

      vgui::HFont font = da_notify[i].fixed_width_font ? m_hFontFixed : m_hFont;

      int fontTall = vgui::surface()->GetFontTall(m_hFontFixed) + 1;

      len = DrawTextLen(font, da_notify[i].szNotify);
      x = videomode->GetModeWidth() - 10 - len;

      if (y + fontTall > videomode->GetModeHeight() - 20) return count;

      count++;
      int y = 20 + 10 * i;

      if (bDraw) {
        DrawColoredText(font, x, y, da_notify[i].color[0] * 255,
                        da_notify[i].color[1] * 255,
                        da_notify[i].color[2] * 255, 255,
                        da_notify[i].szNotify);
      }

      if (da_notify[i].szNotify[0]) {
        // Extend the bounds.
        left = std::min(left, x);
        top = std::min(top, y);
        right = std::max(right, x + len);
        bottom = std::max(bottom, y + fontTall);
      }

      y += fontTall;
    }
  }

  return count;
}

void CConPanel::Paint() {
  VPROF("CConPanel::Paint");

  DrawDebugAreas();

  DrawNotify();  // only draw notify in game
}

void CConPanel::PaintBackground() {
  if (!Con_IsVisible()) return;

  ch version[100];
  Q_snprintf(version, std::size(version), "Source Engine %i (build %d)",
             PROTOCOL_VERSION, build_number());
  wchar_t unicode_version[200];
  g_pVGuiLocalize->ConvertANSIToUnicode(version, unicode_version,
                                        std::size(unicode_version));

  vgui::surface()->DrawSetTextColor(Color(255, 255, 255, 255));

  const int wide = GetWide();
  const int x = wide - DrawTextLen(m_hFont, unicode_version) - 2;

  DrawText(m_hFont, x, 0, unicode_version);

  if (cl.IsActive()) {
    if (cl.m_NetChannel->IsLoopback()) {
      Q_snprintf(version, std::size(version), "Map '%s'",
                 cl.m_szLevelNameShort);
    } else {
      Q_snprintf(version, std::size(version), "Server '%s' Map '%s'",
                 cl.m_NetChannel->GetRemoteAddress().ToString(),
                 cl.m_szLevelNameShort);
    }

    wchar_t unicode_map_and_server[200];
    g_pVGuiLocalize->ConvertANSIToUnicode(version, unicode_map_and_server,
                                          std::size(unicode_map_and_server));

    const int tall = vgui::surface()->GetFontTall(m_hFont);
    const int x = wide - DrawTextLen(m_hFont, unicode_map_and_server) - 2;

    DrawText(m_hFont, x, tall + 1, unicode_map_and_server);
  }
}

// Purpose: Creates the Console VGUI object
static CConPanel *conPanel = nullptr;

void Con_CreateConsolePanel(vgui::Panel *parent) {
  conPanel = new CConPanel(parent);
  if (conPanel) {
    conPanel->SetVisible(false);
  }
}

vgui::Panel *Con_GetConsolePanel() { return conPanel; }

static ConCommand toggleconsole("toggleconsole", Con_ToggleConsole_f,
                                "Show/hide the console.", FCVAR_DONTRECORD);
static ConCommand hideconsole("hideconsole", Con_HideConsole_f,
                              "Hide the console.", FCVAR_DONTRECORD);
static ConCommand showconsole("showconsole", Con_ShowConsole_f,
                              "Show the console.", FCVAR_DONTRECORD);
static ConCommand clear("clear", Con_Clear_f, "Clear all console output.",
                        FCVAR_DONTRECORD);

#endif  // SWDS
