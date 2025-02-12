// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#include "SavegameDialog.h"

#include "BasePanel.h"
#include "BaseSaveGameDialog.h"
#include "EngineInterface.h"
#include "GameUI_Interface.h"
#include "ModInfo.h"
#include "SaveGameBrowserDialog.h"
#include "filesystem.h"
#include "tier1/keyvalues.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "vgui_controls/AnimationController.h"
#include "vgui_controls/ImagePanel.h"
#include "vstdlib/random.h"
#include "base/include/windows/windows_light.h"  // FILETIME

 
#include "tier0/include/memdbgon.h"

#include "tier1/UtlVector.h"
#include "vgui_controls/Frame.h"

extern const char *COM_GetModDirectory();

using namespace vgui;

CSaveGameDialogXbox::CSaveGameDialogXbox(vgui::Panel *parent)
    : BaseClass(parent), m_bGameSaving(false), m_bNewSaveAvailable(false) {
  m_bFilterAutosaves = true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CSaveGameDialogXbox::PerformSelectedAction(void) {
  BaseClass::PerformSelectedAction();

  // If there are no panels, don't allow this
  if (GetNumPanels() == 0) return;

  SetControlDisabled(true);

  // Decide if this is an overwrite or a new save game
  bool bNewSave = (GetActivePanelIndex() == 0) && m_bNewSaveAvailable;
  if (bNewSave) {
    OnCommand("SaveGame");
  } else {
    BasePanel()->ShowMessageDialog(MD_SAVE_OVERWRITE, this);
  }
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : bNewSaveSelected -
//-----------------------------------------------------------------------------
void CSaveGameDialogXbox::UpdateFooterOptions(void) {
  CFooterPanel *pFooter = GetFooterPanel();

  // Show available buttons
  pFooter->ClearButtons();

  bool bSavePanelsActive = (GetNumPanels() != 0);
  if (bSavePanelsActive) {
    bool bNewSaveSelected = (GetActivePanelIndex() == 0) && m_bNewSaveAvailable;
    if (bNewSaveSelected) {
      pFooter->AddNewButtonLabel("#GameUI_SaveGame_NewSave",
                                 "#GameUI_Icons_A_BUTTON");
    } else {
      pFooter->AddNewButtonLabel("#GameUI_SaveGame_Overwrite",
                                 "#GameUI_Icons_A_BUTTON");
    }
  }

  // Always available
  pFooter->AddNewButtonLabel("#GameUI_Close", "#GameUI_Icons_B_BUTTON");
  pFooter->AddNewButtonLabel("#GameUI_Console_StorageChange",
                             "#GameUI_Icons_Y_BUTTON");
}

//-----------------------------------------------------------------------------
// Purpose: perfrom the save on a separate thread
//-----------------------------------------------------------------------------
class CAsyncCtxSaveGame : public CBasePanel::CAsyncJobContext {
 public:
  explicit CAsyncCtxSaveGame(CSaveGameDialogXbox *pDlg);
  ~CAsyncCtxSaveGame() {}

 public:
  virtual void ExecuteAsync();
  virtual void Completed();

 public:
  char m_szFilename[SOURCE_MAX_PATH];
  CSaveGameDialogXbox *m_pSaveGameDlg;
};

CAsyncCtxSaveGame::CAsyncCtxSaveGame(CSaveGameDialogXbox *pDlg)
    : CBasePanel::CAsyncJobContext(
          3.0f),  // Storage device info for at least 3 seconds
      m_pSaveGameDlg(pDlg) {
  NULL;
}

void CAsyncCtxSaveGame::ExecuteAsync() {
  // Sit and wait for the async save to finish
  for (;;) {
    if (!engine->IsSaveInProgress())
      // Save operation is no longer in progress
      break;
    else
      ThreadSleep(50);
  }
}

void CAsyncCtxSaveGame::Completed() { m_pSaveGameDlg->SaveCompleted(this); }

//-----------------------------------------------------------------------------
// Purpose: kicks off the async save (called on the main thread)
//-----------------------------------------------------------------------------
void CSaveGameDialogXbox::InitiateSaving() {
  // Determine whether this is a new save or overwrite
  bool bNewSave = (GetActivePanelIndex() == 0) && m_bNewSaveAvailable;

  // Allocate the async context for saving
  CAsyncCtxSaveGame *pAsyncCtx = new CAsyncCtxSaveGame(this);

  // If this is an overwrite then there was an overwrite warning displayed
  if (!bNewSave) BasePanel()->CloseMessageDialog(DIALOG_STACK_IDX_WARNING);
  // Now display the saving warning
  BasePanel()->ShowMessageDialog(MD_SAVING_WARNING, this);

  // Kick off saving
  char *szFilename = pAsyncCtx->m_szFilename;
  const int maxFilenameLen = sizeof(pAsyncCtx->m_szFilename);
  char szCmd[SOURCE_MAX_PATH];

  // See if this is the "new save game" slot
  if (bNewSave) {
    // Create a new save game (name is created from the current time, which
    // should be pretty unique)
    FILETIME currentTime;
    GetSystemTimeAsFileTime(&currentTime);

    Q_snprintf(szFilename, maxFilenameLen, "%s_%u", COM_GetModDirectory(),
               currentTime);
    Q_snprintf(szCmd, sizeof(szCmd), "xsave %s", szFilename);
    engine->ExecuteClientCmd(szCmd);
    Q_strncat(szFilename, ".360.sav", maxFilenameLen);
  } else {
    const SaveGameDescription_t *pDesc = GetActivePanelSaveDescription();
    Q_strncpy(szFilename, pDesc->szShortName, maxFilenameLen);
    Q_snprintf(szCmd, sizeof(szCmd), "xsave %s", szFilename);
    engine->ExecuteClientCmd(szCmd);
  }

  // Enqueue waiting
  BasePanel()->ExecuteAsync(pAsyncCtx);
}

//-----------------------------------------------------------------------------
// Purpose: handles the end of async save (called on the main thread)
//-----------------------------------------------------------------------------
void CSaveGameDialogXbox::SaveCompleted(CAsyncCtxSaveGame *pCtx) {
  char const *szFilename = pCtx->m_szFilename;

  // We should now be saved so get the new desciption back from the file
  char szDirectory[SOURCE_MAX_PATH];
  Q_snprintf(szDirectory, sizeof(szDirectory), "%s:/%s", COM_GetModDirectory(),
             szFilename);

  ParseSaveData(szDirectory, szFilename, &m_NewSaveDesc);

  // Close the progress dialog
  BasePanel()->CloseMessageDialog(DIALOG_STACK_IDX_WARNING);

  bool bNewSave = (GetActivePanelIndex() == 0) && m_bNewSaveAvailable;
  if (bNewSave) {
    AnimateInsertNewPanel(&m_NewSaveDesc);
  } else {
    AnimateOverwriteActivePanel(&m_NewSaveDesc);
  }

  m_bGameSaving = false;
}

//-----------------------------------------------------------------------------
// Purpose: handles button commands
//-----------------------------------------------------------------------------
void CSaveGameDialogXbox::OnCommand(const char *command) {
  m_KeyRepeat.Reset();

  if (!Q_stricmp(command, "SaveGame")) {
    if (m_bGameSaving) return;
    m_bGameSaving = true;

    SetControlDisabled(true);

    // Initiate the saving operation
    InitiateSaving();
  } else if (!Q_stricmp(command, "SaveSuccess")) {
    vgui::surface()->PlaySound("UI/buttonclick.wav");
    GameUI().SetSavedThisMenuSession(true);
  } else if (!Q_stricmp(command, "CloseAndSelectResume")) {
    BasePanel()->ArmFirstMenuItem();
    OnCommand("Close");
  } else if (!Q_stricmp(command, "OverwriteGameCancelled")) {
    SetControlDisabled(false);
  } else if (!Q_stricmp(command, "RefreshSaveGames")) {
    RefreshSaveGames();
  } else if (!Q_stricmp(command, "ReleaseModalWindow")) {
    vgui::surface()->RestrictPaintToSinglePanel(NULL);
  } else if (!m_bGameSaving) {
    BaseClass::OnCommand(command);
  }
}

//-----------------------------------------------------------------------------
// Purpose: On completion of scanning, prepend a utility slot on the stack
//-----------------------------------------------------------------------------
void CSaveGameDialogXbox::OnDoneScanningSaveGames(void) {
  m_bNewSaveAvailable = false;
}
