// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#include "cvar.h"

#include "gl_cvars.h"

#include <ctype.h>
#include "GameEventManager.h"
#include "client.h"
#include "demo.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "netmessages.h"
#include "server.h"
#include "sv_main.h"
#include "tier1/convar.h"
#ifdef OS_POSIX
#include <wctype.h>
#endif

#ifndef SWDS
#include <vgui/ILocalize.h>
#include <vgui_controls/Controls.h>
#endif

#include "tier0/include/memdbgon.h"

//-----------------------------------------------------------------------------
// Singleton CCvarUtilities
//-----------------------------------------------------------------------------
static CCvarUtilities g_CvarUtilities;
CCvarUtilities *cv = &g_CvarUtilities;

//-----------------------------------------------------------------------------
// Purpose: Update clients/server when FCVAR_REPLICATED etc vars change
//-----------------------------------------------------------------------------
static void ConVarNetworkChangeCallback(IConVar *pConVar, const char *pOldValue,
                                        float flOldValue) {
  ConVarRef var(pConVar);
  if (!pOldValue) {
    if (var.GetFloat() == flOldValue) return;
  } else {
    if (!Q_strcmp(var.GetString(), pOldValue)) return;
  }

  if (var.IsFlagSet(FCVAR_USERINFO)) {
    // Are we not a server, but a client and have a change?
    if (cl.IsConnected()) {
      // send changed cvar to server
      NET_SetConVar convar(var.GetName(), var.GetString());
      cl.m_NetChannel->SendNetMsg(convar);
    }
  }

  // Log changes to server variables

  // Print to clients
  if (var.IsFlagSet(FCVAR_NOTIFY)) {
    IGameEvent *event = g_GameEventManager.CreateEvent("server_cvar");

    if (event) {
      event->SetString("cvarname", var.GetName());

      if (var.IsFlagSet(FCVAR_PROTECTED)) {
        event->SetString("cvarvalue", "***PROTECTED***");
      } else {
        event->SetString("cvarvalue", var.GetString());
      }

      g_GameEventManager.FireEvent(event);
    }
  }

  // Force changes down to clients (if running server)
  if (var.IsFlagSet(FCVAR_REPLICATED) && sv.IsActive()) {
    SV_ReplicateConVarChange(static_cast<ConVar *>(pConVar), var.GetString());
  }
}

//-----------------------------------------------------------------------------
// Implementation of the ICvarQuery interface
//-----------------------------------------------------------------------------
class CCvarQuery : public CBaseAppSystem<ICvarQuery> {
 public:
  virtual bool Connect(CreateInterfaceFn factory) {
    ICvar *pCVar = (ICvar *)factory(CVAR_INTERFACE_VERSION, 0);
    if (!pCVar) return false;

    pCVar->InstallCVarQuery(this);
    return true;
  }

  virtual InitReturnVal_t Init() {
    // If the value has changed, notify clients/server based on ConVar flags.
    // NOTE: this will only happen for non-FCVAR_NEVER_AS_STRING vars.
    // Also, this happened in SetDirect for older clients that don't have the
    // callback interface.
    g_pCVar->InstallGlobalChangeCallback(ConVarNetworkChangeCallback);
    return INIT_OK;
  }

  virtual void Shutdown() {
    g_pCVar->RemoveGlobalChangeCallback(ConVarNetworkChangeCallback);
  }

  virtual void *QueryInterface(const char *pInterfaceName) {
    if (!Q_stricmp(pInterfaceName, CVAR_QUERY_INTERFACE_VERSION))
      return (ICvarQuery *)this;
    return NULL;
  }

  // Purpose: Returns true if the commands can be aliased to one another
  //  Either game/client .dll shared with engine,
  //  or game and client dll shared and marked FCVAR_REPLICATED
  virtual bool AreConVarsLinkable(const ConVar *child, const ConVar *parent) {
    // Both parent and child must be marked replicated for this to work
    bool repchild = child->IsFlagSet(FCVAR_REPLICATED);
    bool repparent = parent->IsFlagSet(FCVAR_REPLICATED);

    char sz[512];

    if (repchild && repparent) {
      // Never on protected vars
      if (child->IsFlagSet(FCVAR_PROTECTED) ||
          parent->IsFlagSet(FCVAR_PROTECTED)) {
        Q_snprintf(sz, sizeof(sz),
                   "FCVAR_REPLICATED can't also be FCVAR_PROTECTED (%s)\n",
                   child->GetName());
        ConMsg(sz);
        return false;
      }

      // Only on ConVars
      if (child->IsCommand() || parent->IsCommand()) {
        Q_snprintf(sz, sizeof(sz),
                   "FCVAR_REPLICATED not valid on ConCommands (%s)\n",
                   child->GetName());
        ConMsg(sz);
        return false;
      }

      // One must be in client .dll and the other in the game .dll, or both in
      // the engine
      if (child->IsFlagSet(FCVAR_GAMEDLL) &&
          !parent->IsFlagSet(FCVAR_CLIENTDLL)) {
        Q_snprintf(sz, sizeof(sz),
                   "For FCVAR_REPLICATED, ConVar must be defined in client and "
                   "game .dlls (%s)\n",
                   child->GetName());
        ConMsg(sz);
        return false;
      }

      if (child->IsFlagSet(FCVAR_CLIENTDLL) &&
          !parent->IsFlagSet(FCVAR_GAMEDLL)) {
        Q_snprintf(sz, sizeof(sz),
                   "For FCVAR_REPLICATED, ConVar must be defined in client and "
                   "game .dlls (%s)\n",
                   child->GetName());
        ConMsg(sz);
        return false;
      }

      // Allowable
      return true;
    }

    // Otherwise need both to allow linkage
    if (repchild || repparent) {
      Q_snprintf(sz, sizeof(sz),
                 "Both ConVars must be marked FCVAR_REPLICATED for linkage to "
                 "work (%s)\n",
                 child->GetName());
      ConMsg(sz);
      return false;
    }

    if (parent->IsFlagSet(FCVAR_CLIENTDLL)) {
      Q_snprintf(sz, sizeof(sz), "Parent cvar in client.dll not allowed (%s)\n",
                 child->GetName());
      ConMsg(sz);
      return false;
    }

    if (parent->IsFlagSet(FCVAR_GAMEDLL)) {
      Q_snprintf(sz, sizeof(sz), "Parent cvar in server.dll not allowed (%s)\n",
                 child->GetName());
      ConMsg(sz);
      return false;
    }

    return true;
  }
};

//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CCvarQuery s_CvarQuery;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CCvarQuery, ICvarQuery,
                                  CVAR_QUERY_INTERFACE_VERSION, s_CvarQuery);

//-----------------------------------------------------------------------------
//
// CVar utilities begins here
//
//-----------------------------------------------------------------------------
static bool IsAllSpaces(const wchar_t *str) {
  const wchar_t *p = str;
  while (p && *p) {
    if (!iswspace(*p)) return false;

    ++p;
  }

  return true;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *var -
//			*value -
//-----------------------------------------------------------------------------
void CCvarUtilities::SetDirect(ConVar *var, const char *value) {
  const char *pszValue;
  char szNew[1024];

  // Bail early if we're trying to set a FCVAR_USERINFO cvar on a dedicated
  // server
  if (var->IsFlagSet(FCVAR_USERINFO)) {
    if (sv.IsDedicated()) {
      return;
    }
  }

  pszValue = value;
  // This cvar's string must only contain printable characters.
  // Strip out any other crap.
  // We'll fill in "empty" if nothing is left
  if (var->IsFlagSet(FCVAR_PRINTABLEONLY)) {
    wchar_t unicode[512];
#ifndef SWDS
    if (sv.IsDedicated()) {
      // Dedicated servers don't have g_pVGuiLocalize, so fall back
      mbstowcs(unicode, pszValue, SOURCE_ARRAYSIZE(unicode));
    } else {
      g_pVGuiLocalize->ConvertANSIToUnicode(pszValue, unicode, sizeof(unicode));
    }
#else
    mbstowcs(unicode, pszValue, SOURCE_ARRAYSIZE(unicode));
#endif
    wchar_t newUnicode[512];

    const wchar_t *pS;
    wchar_t *pD;

    // Clear out new string
    newUnicode[0] = L'\0';

    pS = unicode;
    pD = newUnicode;

    // Step through the string, only copying back in characters that are
    // printable
    while (*pS) {
      if (iswcntrl(*pS) || *pS == '~') {
        pS++;
        continue;
      }

      *pD++ = *pS++;
    }

    // Terminate the new string
    *pD = L'\0';

    // If it's empty or all spaces, then insert a marker string
    if (!wcslen(newUnicode) || IsAllSpaces(newUnicode)) {
      wcsncpy(newUnicode, L"#empty",
              (sizeof(newUnicode) / sizeof(wchar_t)) - 1);
      newUnicode[(sizeof(newUnicode) / sizeof(wchar_t)) - 1] = L'\0';
    }

#ifndef SWDS
    if (sv.IsDedicated()) {
      wcstombs(szNew, newUnicode, sizeof(szNew));
    } else {
      g_pVGuiLocalize->ConvertUnicodeToANSI(newUnicode, szNew, sizeof(szNew));
    }
#else
    wcstombs(szNew, newUnicode, sizeof(szNew));
#endif
    // Point the value here.
    pszValue = szNew;
  }

  if (var->IsFlagSet(FCVAR_NEVER_AS_STRING)) {
    var->SetValue((float)atof(pszValue));
  } else {
    var->SetValue(pszValue);
  }
}

//-----------------------------------------------------------------------------
// Purpose:
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------

// If you are changing this, please take a look at IsValidToggleCommand()
bool CCvarUtilities::IsCommand(const CCommand &args) {
  int c = args.ArgC();
  if (c == 0) return false;

  ConVar *v;

  // check variables
  v = g_pCVar->FindVar(args[0]);
  if (!v) return false;

  // NOTE: Not checking for 'HIDDEN' here so we can actually set hidden convars
  if (v->IsFlagSet(FCVAR_DEVELOPMENTONLY)) return false;

  // perform a variable print or set
  if (c == 1) {
    ConVar_PrintDescription(v);
    return true;
  }

  if (v->IsFlagSet(FCVAR_SPONLY)) {
#ifndef SWDS
    // Connected to server?
    if (cl.IsConnected()) {
      // Is it not a single player game?
      if (cl.m_nMaxClients > 1) {
        ConMsg("Can't set %s in multiplayer\n", v->GetName());
        return true;
      }
    }
#endif
  }

  if (v->IsFlagSet(FCVAR_NOT_CONNECTED)) {
#ifndef SWDS
    // Connected to server?
    if (cl.IsConnected()) {
      ConMsg("Can't set %s when connected\n", v->GetName());
      return true;
    }
#endif
  }

  // Allow cheat commands in singleplayer, debug, or multiplayer with sv_cheats
  // on
  if (v->IsFlagSet(FCVAR_CHEAT)) {
    if (!Host_IsSinglePlayerGame() && !CanCheat()
#if !defined(SWDS)
        && !cl.ishltv && !demoplayer->IsPlayingBack()
#endif
    ) {
      ConMsg(
          "Can't use cheat cvar %s in multiplayer, unless the server has "
          "sv_cheats set to 1.\n",
          v->GetName());
      return true;
    }
  }

  // Text invoking the command was typed into the console, decide what to do
  // with it
  //  if this is a replicated ConVar, except don't worry about restrictions if
  //  playing a .dem file
  if (v->IsFlagSet(FCVAR_REPLICATED)
#if !defined(SWDS)
      && !demoplayer->IsPlayingBack()
#endif
  ) {
    // If not running a server but possibly connected as a client, then
    //  if the message came from console, don't process the command
    if (!sv.IsActive() && !sv.IsLoading() && (cmd_source == src_command) &&
        cl.IsConnected()) {
      ConMsg(
          "Can't change replicated ConVar %s from console of client, only "
          "server operator can change its value\n",
          v->GetName());
      return true;
    }

    // TODO(d.rattman):  Do we need a case where cmd_source == src_client?
    Assert(cmd_source != src_client);
  }

  // Note that we don't want the tokenized list, send down the entire string
  // except for surrounding quotes
  char remaining[1024];
  const char *pArgS = args.ArgS();
  int nLen = Q_strlen(pArgS);
  bool bIsQuoted = pArgS[0] == '\"';
  if (!bIsQuoted) {
    Q_strncpy(remaining, args.ArgS(), sizeof(remaining));
  } else {
    --nLen;
    Q_strncpy(remaining, &pArgS[1], sizeof(remaining));
  }

  // Now strip off any trailing spaces
  char *p = remaining + nLen - 1;
  while (p >= remaining) {
    if (*p > ' ') break;

    *p-- = 0;
  }

  // Strip off ending quote
  if (bIsQuoted && p >= remaining) {
    if (*p == '\"') {
      *p = 0;
    }
  }

  SetDirect(v, remaining);
  return true;
}

// This is a band-aid copied directly from IsCommand().
bool CCvarUtilities::IsValidToggleCommand(const char *cmd) {
  ConVar *v;

  // check variables
  v = g_pCVar->FindVar(cmd);
  if (!v) {
    ConMsg("%s is not a valid cvar\n", cmd);
    return false;
  }

  if (v->IsFlagSet(FCVAR_DEVELOPMENTONLY) || v->IsFlagSet(FCVAR_HIDDEN))
    return false;

  if (v->IsFlagSet(FCVAR_SPONLY)) {
#ifndef SWDS
    // Connected to server?
    if (cl.IsConnected()) {
      // Is it not a single player game?
      if (cl.m_nMaxClients > 1) {
        ConMsg("Can't set %s in multiplayer\n", v->GetName());
        return false;
      }
    }
#endif
  }

  if (v->IsFlagSet(FCVAR_NOT_CONNECTED)) {
#ifndef SWDS
    // Connected to server?
    if (cl.IsConnected()) {
      ConMsg("Can't set %s when connected\n", v->GetName());
      return false;
    }
#endif
  }

  // Allow cheat commands in singleplayer, debug, or multiplayer with sv_cheats
  // on
  if (v->IsFlagSet(FCVAR_CHEAT)) {
    if (!Host_IsSinglePlayerGame() && !CanCheat()
#if !defined(SWDS) && !defined(_XBOX)
        && !demoplayer->IsPlayingBack()
#endif
    ) {
      ConMsg(
          "Can't use cheat cvar %s in multiplayer, unless the server has "
          "sv_cheats set to 1.\n",
          v->GetName());
      return false;
    }
  }

  // Text invoking the command was typed into the console, decide what to do
  // with it
  //  if this is a replicated ConVar, except don't worry about restrictions if
  //  playing a .dem file
  if (v->IsFlagSet(FCVAR_REPLICATED)
#if !defined(SWDS) && !defined(_XBOX)
      && !demoplayer->IsPlayingBack()
#endif
  ) {
    // If not running a server but possibly connected as a client, then
    //  if the message came from console, don't process the command
    if (!sv.IsActive() && !sv.IsLoading() && (cmd_source == src_command) &&
        cl.IsConnected()) {
      ConMsg(
          "Can't change replicated ConVar %s from console of client, only "
          "server operator can change its value\n",
          v->GetName());
      return false;
    }
  }

  // TODO(d.rattman):  Do we need a case where cmd_source == src_client?
  Assert(cmd_source != src_client);
  return true;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *f -
//-----------------------------------------------------------------------------
void CCvarUtilities::WriteVariables(CUtlBuffer &buff) {
  const ConCommandBase *var;

  for (var = g_pCVar->GetCommands(); var; var = var->GetNext()) {
    if (var->IsCommand()) continue;

    bool archive =
        var->IsFlagSet(IsX360() ? FCVAR_ARCHIVE_XBOX : FCVAR_ARCHIVE);
    if (archive) {
      buff.Printf("%s \"%s\"\n", var->GetName(), ((ConVar *)var)->GetString());
    }
  }
}

static char *StripTabsAndReturns(const char *inbuffer, char *outbuffer,
                                 int outbufferSize) {
  char *out = outbuffer;
  const char *i = inbuffer;
  char *o = out;

  out[0] = 0;

  while (*i && o - out < outbufferSize - 1) {
    if (*i == '\n' || *i == '\r' || *i == '\t') {
      *o++ = ' ';
      i++;
      continue;
    }
    if (*i == '\"') {
      *o++ = '\'';
      i++;
      continue;
    }

    *o++ = *i++;
  }

  *o = '\0';

  return out;
}

static char *StripQuotes(const char *inbuffer, char *outbuffer,
                         int outbufferSize) {
  char *out = outbuffer;
  const char *i = inbuffer;
  char *o = out;

  out[0] = 0;

  while (*i && o - out < outbufferSize - 1) {
    if (*i == '\"') {
      *o++ = '\'';
      i++;
      continue;
    }

    *o++ = *i++;
  }

  *o = '\0';

  return out;
}

struct ConVarFlags_t {
  int bit;
  const char *desc;
  const char *shortdesc;
};

#define CONVARFLAG(x, y) \
  { FCVAR_##x, #x, #y }

static ConVarFlags_t g_ConVarFlags[] = {
    //	CONVARFLAG( UNREGISTERED, "u" ),
    CONVARFLAG(ARCHIVE, "a"),
    CONVARFLAG(SPONLY, "sp"),
    CONVARFLAG(GAMEDLL, "sv"),
    CONVARFLAG(CHEAT, "cheat"),
    CONVARFLAG(USERINFO, "user"),
    CONVARFLAG(NOTIFY, "nf"),
    CONVARFLAG(PROTECTED, "prot"),
    CONVARFLAG(PRINTABLEONLY, "print"),
    CONVARFLAG(UNLOGGED, "log"),
    CONVARFLAG(NEVER_AS_STRING, "numeric"),
    CONVARFLAG(REPLICATED, "rep"),
    CONVARFLAG(DEMO, "demo"),
    CONVARFLAG(DONTRECORD, "norecord"),
    CONVARFLAG(SERVER_CAN_EXECUTE, "server_can_execute"),
    CONVARFLAG(CLIENTCMD_CAN_EXECUTE, "clientcmd_can_execute"),
    CONVARFLAG(CLIENTDLL, "cl"),
};

static void PrintListHeader(FileHandle_t &f) {
  char csvflagstr[1024];

  csvflagstr[0] = 0;

  int c = SOURCE_ARRAYSIZE(g_ConVarFlags);
  for (int i = 0; i < c; ++i) {
    char csvf[64];

    ConVarFlags_t &entry = g_ConVarFlags[i];
    Q_snprintf(csvf, sizeof(csvf), "\"%s\",", entry.desc);
    Q_strncat(csvflagstr, csvf, sizeof(csvflagstr), COPY_ALL_CHARACTERS);
  }

  g_pFileSystem->FPrintf(f, "\"%s\",\"%s\",%s,\"%s\"\n", "Name", "Value",
                         csvflagstr, "Help Text");
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *var -
//			*f -
//-----------------------------------------------------------------------------
static void PrintCvar(const ConVar *var, bool logging, FileHandle_t &f) {
  char flagstr[128];
  char csvflagstr[1024];

  flagstr[0] = 0;
  csvflagstr[0] = 0;

  size_t c = SOURCE_ARRAYSIZE(g_ConVarFlags);
  for (size_t i = 0; i < c; ++i) {
    char fl[32];
    char csvf[64];

    ConVarFlags_t &entry = g_ConVarFlags[i];
    if (var->IsFlagSet(entry.bit)) {
      Q_snprintf(fl, SOURCE_ARRAYSIZE(fl), ", %s", entry.shortdesc);
      Q_strncat(flagstr, fl, SOURCE_ARRAYSIZE(flagstr), COPY_ALL_CHARACTERS);
      Q_snprintf(csvf, SOURCE_ARRAYSIZE(csvf), "\"%s\",", entry.desc);
    } else {
      Q_snprintf(csvf, SOURCE_ARRAYSIZE(csvf), ",");
    }

    Q_strncat(csvflagstr, csvf, SOURCE_ARRAYSIZE(csvflagstr), COPY_ALL_CHARACTERS);
  }

  char valstr[32];
  char tempbuff[128];

  // Clean up integers
  if (var->GetInt() == (int)var->GetFloat()) {
    Q_snprintf(valstr, SOURCE_ARRAYSIZE(valstr), "%-8i", var->GetInt());
  } else {
    Q_snprintf(valstr, SOURCE_ARRAYSIZE(valstr), "%-8.3f", var->GetFloat());
  }

  // Print to console
  ConMsg(
      "%-40s : %-8s : %-16s : %s\n", var->GetName(), valstr, flagstr,
      StripTabsAndReturns(var->GetHelpText(), tempbuff, SOURCE_ARRAYSIZE(tempbuff)));
  if (logging) {
    g_pFileSystem->FPrintf(
        f, "\"%s\",\"%s\",%s,\"%s\"\n", var->GetName(), valstr, csvflagstr,
        StripQuotes(var->GetHelpText(), tempbuff, SOURCE_ARRAYSIZE(tempbuff)));
  }
}

static void PrintCommand(const ConCommand *cmd, bool logging, FileHandle_t &f) {
  // Print to console
  char tempbuff[128];
  ConMsg("%-40s : %-8s : %-16s : %s\n", cmd->GetName(), "cmd", "",
         StripTabsAndReturns(cmd->GetHelpText(), tempbuff, sizeof(tempbuff)));
  if (logging) {
    char emptyflags[256];

    emptyflags[0] = 0;

    int c = SOURCE_ARRAYSIZE(g_ConVarFlags);
    for (int i = 0; i < c; ++i) {
      char csvf[64];
      Q_snprintf(csvf, sizeof(csvf), ",");
      Q_strncat(emptyflags, csvf, sizeof(emptyflags), COPY_ALL_CHARACTERS);
    }
    // Names staring with +/- need to be wrapped in single quotes
    char name[256];
    Q_snprintf(name, sizeof(name), "%s", cmd->GetName());
    if (name[0] == '+' || name[0] == '-') {
      Q_snprintf(name, sizeof(name), "'%s'", cmd->GetName());
    }
    char tempbuff2[128];
    g_pFileSystem->FPrintf(
        f, "\"%s\",\"%s\",%s,\"%s\"\n", name, "cmd", emptyflags,
        StripQuotes(cmd->GetHelpText(), tempbuff2, sizeof(tempbuff2)));
  }
}

static bool ConCommandBaseLessFunc(const ConCommandBase *const &lhs,
                                   const ConCommandBase *const &rhs) {
  const char *left = lhs->GetName();
  const char *right = rhs->GetName();

  if (*left == '-' || *left == '+') left++;
  if (*right == '-' || *right == '+') right++;

  return (Q_stricmp(left, right) < 0);
}

//-----------------------------------------------------------------------------
// Purpose:
// Output : void CCvar::CvarList_f
//-----------------------------------------------------------------------------
void CCvarUtilities::CvarList(const CCommand &args) {
  const ConCommandBase *var;   // Temporary Pointer to cvars
  int iArgs;                   // Argument count
  const char *partial = NULL;  // Partial cvar to search for...
                               // E.eg
  int ipLen = 0;               // Length of the partial cvar

  FileHandle_t f = FILESYSTEM_INVALID_HANDLE;  // FilePointer for logging
  bool bLogging = false;
  // Are we logging?
  iArgs = args.ArgC();  // Get count

  // Print usage?
  if (iArgs == 2 && !Q_strcasecmp(args[1], "?")) {
    ConMsg("cvarlist:  [log logfile] [ partial ]\n");
    return;
  }

  if (!Q_strcasecmp(args[1], "log") && iArgs >= 3) {
    char fn[256];
    Q_snprintf(fn, sizeof(fn), "%s", args[2]);
    f = g_pFileSystem->Open(fn, "wb");
    if (f) {
      bLogging = true;
    } else {
      ConMsg("Couldn't open '%s' for writing!\n", fn);
      return;
    }

    if (iArgs == 4) {
      partial = args[3];
      ipLen = Q_strlen(partial);
    }
  } else {
    partial = args[1];
    ipLen = Q_strlen(partial);
  }

  // Banner
  ConMsg("cvar list\n--------------\n");

  CUtlRBTree<const ConCommandBase *> sorted(0, 0, ConCommandBaseLessFunc);

  // Loop through cvars...
  for (var = g_pCVar->GetCommands(); var; var = var->GetNext()) {
    bool print = false;

    if (var->IsFlagSet(FCVAR_DEVELOPMENTONLY) || var->IsFlagSet(FCVAR_HIDDEN))
      continue;

    if (partial)  // Partial string searching?
    {
      if (!Q_strncasecmp(var->GetName(), partial, ipLen)) {
        print = true;
      }
    } else {
      print = true;
    }

    if (!print) continue;

    sorted.Insert(var);
  }

  if (bLogging) {
    PrintListHeader(f);
  }
  for (int i = sorted.FirstInorder(); i != sorted.InvalidIndex();
       i = sorted.NextInorder(i)) {
    var = sorted[i];
    if (var->IsCommand()) {
      PrintCommand((ConCommand *)var, bLogging, f);
    } else {
      PrintCvar((ConVar *)var, bLogging, f);
    }
  }

  // Show total and syntax help...
  if (partial && partial[0]) {
    ConMsg("--------------\n%3i convars/concommands for [%s]\n", sorted.Count(),
           partial);
  } else {
    ConMsg("--------------\n%3i total convars/concommands\n", sorted.Count());
  }

  if (bLogging) {
    g_pFileSystem->Close(f);
  }
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
// Output : int
//-----------------------------------------------------------------------------
int CCvarUtilities::CountVariablesWithFlags(int flags) {
  int i = 0;
  const ConCommandBase *var;

  for (var = g_pCVar->GetCommands(); var; var = var->GetNext()) {
    if (var->IsCommand()) continue;

    if (var->IsFlagSet(flags)) {
      i++;
    }
  }

  return i;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCvarUtilities::CvarHelp(const CCommand &args) {
  const char *search;
  const ConCommandBase *var;

  if (args.ArgC() != 2) {
    ConMsg("Usage:  help <cvarname>\n");
    return;
  }

  // Get name of var to find
  search = args[1];

  // Search for it
  var = g_pCVar->FindCommandBase(search);
  if (!var) {
    ConMsg("help:  no cvar or command named %s\n", search);
    return;
  }

  // Show info
  ConVar_PrintDescription(var);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCvarUtilities::CvarDifferences(const CCommand &args) {
  const ConCommandBase *var;

  // Loop through vars and print out findings
  for (var = g_pCVar->GetCommands(); var; var = var->GetNext()) {
    if (var->IsCommand()) continue;
    if (var->IsFlagSet(FCVAR_DEVELOPMENTONLY) || var->IsFlagSet(FCVAR_HIDDEN))
      continue;

    if (!Q_stricmp(((ConVar *)var)->GetDefault(), ((ConVar *)var)->GetString()))
      continue;

    ConVar_PrintDescription((ConVar *)var);
  }
}

//-----------------------------------------------------------------------------
// Purpose: Toggles a cvar on/off, or cycles through a set of values
//-----------------------------------------------------------------------------
void CCvarUtilities::CvarToggle(const CCommand &args) {
  int i;

  int c = args.ArgC();
  if (c < 2) {
    ConMsg("Usage:  toggle <cvarname> [value1] [value2] [value3]...\n");
    return;
  }

  ConVar *var = g_pCVar->FindVar(args[1]);

  if (!IsValidToggleCommand(args[1])) {
    return;
  }

  if (c == 2) {
    // just toggle it on and off
    var->SetValue(!var->GetBool());
    ConVar_PrintDescription(var);
  } else {
    // look for the current value in the command arguments
    for (i = 2; i < c; i++) {
      if (!Q_strcmp(var->GetString(), args[i])) break;
    }

    // choose the next one
    i++;

    // if we didn't find it, or were at the last value in the command arguments,
    // use the 1st argument
    if (i >= c) {
      i = 2;
    }

    var->SetValue(args[i]);
    ConVar_PrintDescription(var);
  }
}

void CCvarUtilities::CvarFindFlags_f(const CCommand &args) {
  if (args.ArgC() < 2) {
    ConMsg("Usage:  findflags <string>\n");
    ConMsg("Available flags to search for: \n");

    for (int i = 0; i < SOURCE_ARRAYSIZE(g_ConVarFlags); i++) {
      ConMsg("   - %s\n", g_ConVarFlags[i].desc);
    }
    return;
  }

  // Get substring to find
  const char *search = args[1];
  const ConCommandBase *var;

  // Loop through vars and print out findings
  for (var = g_pCVar->GetCommands(); var; var = var->GetNext()) {
    if (var->IsFlagSet(FCVAR_DEVELOPMENTONLY) || var->IsFlagSet(FCVAR_HIDDEN))
      continue;

    for (int i = 0; i < SOURCE_ARRAYSIZE(g_ConVarFlags); i++) {
      if (!var->IsFlagSet(g_ConVarFlags[i].bit)) continue;

      if (!V_stristr(g_ConVarFlags[i].desc, search)) continue;

      ConVar_PrintDescription(var);
    }
  }
}

//-----------------------------------------------------------------------------
// Purpose: Hook to command
//-----------------------------------------------------------------------------
CON_COMMAND(findflags, "Find concommands by flags.") {
  cv->CvarFindFlags_f(args);
}

//-----------------------------------------------------------------------------
// Purpose: Hook to command
//-----------------------------------------------------------------------------
CON_COMMAND(cvarlist, "Show the list of convars/concommands.") {
  cv->CvarList(args);
}

//-----------------------------------------------------------------------------
// Purpose: Print help text for cvar
//-----------------------------------------------------------------------------
CON_COMMAND(help, "Find help about a convar/concommand.") {
  cv->CvarHelp(args);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CON_COMMAND(differences,
            "Show all convars which are not at their default values.") {
  cv->CvarDifferences(args);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CON_COMMAND(toggle,
            "Toggles a convar on or off, or cycles through a set of values.") {
  cv->CvarToggle(args);
}

//-----------------------------------------------------------------------------
// Purpose: Send the cvars to VXConsole
//-----------------------------------------------------------------------------
#if defined(_X360)
CON_COMMAND(getcvars, "") { g_pCVar->PublishToVXConsole(); }
#endif
