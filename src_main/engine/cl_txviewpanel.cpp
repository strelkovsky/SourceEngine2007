// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#include "client_pch.h"

#include "cl_txviewpanel.h"

#include <vgui/IInput.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <vgui_controls/BuildGroup.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/FileOpenDialog.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/ListViewPanel.h>
#include <vgui_controls/ProgressBar.h>
#include <vgui_controls/PropertySheet.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/TextImage.h>
#include <vgui_controls/Tooltip.h>
#include "filesystem.h"
#include "tier1/keyvalues.h"

#include "tier0/include/memdbgon.h"

using namespace vgui;

TxViewPanel *g_pTxViewPanel = NULL;

void TxViewPanel::Install(vgui::Panel *parent) {
  if (g_pTxViewPanel) return;

  g_pTxViewPanel = new TxViewPanel(parent);
  Assert(g_pTxViewPanel);
}

TxViewPanel::TxViewPanel(vgui::Panel *parent)
    : vgui::Frame(parent, "TxViewPanel") {
  m_pRefresh = new vgui::Button(this, "Refresh", "Refresh");
  m_pView = new vgui::ListViewPanel(this, "Textures");

  vgui::ivgui()->AddTickSignal(GetVPanel(), 0);

  LoadControlSettings("Resource\\TxViewPanel.res");

  SetVisible(false);
  SetSizeable(true);
  SetMoveable(true);
}

TxViewPanel::~TxViewPanel() { ; }

void TxViewPanel::OnTick() {
  BaseClass::OnTick();

  if (!IsVisible()) return;

  ;
}

void TxViewPanel::OnCommand(const ch *command) {
  if (!Q_strcasecmp(command, "refresh")) {
    ;
  } else {
    BaseClass::OnCommand(command);
  }
}

void TxViewPanel::OnMessage(const KeyValues *params, vgui::VPANEL fromPanel) {
  BaseClass::OnMessage(params, fromPanel);
}

void TxViewPanel::OnFileSelected(const ch *fullpath) {
  if (!fullpath || !fullpath[0]) return;

  ;
}

void TxView_f() {
  if (!g_pTxViewPanel) return;

  if (g_pTxViewPanel->IsVisible()) {
    g_pTxViewPanel->Close();
  } else {
    g_pTxViewPanel->Activate();
  }
}

// static ConCommand txview( "txview", TxView_f, "Show/hide the internal texture
// viewer.", FCVAR_DONTRECORD );
