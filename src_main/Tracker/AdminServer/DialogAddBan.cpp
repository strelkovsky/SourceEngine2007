// Copyright © 1996-2001, Valve LLC, All rights reserved.

#include "DialogAddBan.h"

#include <cstdio>
#include "tier1/KeyValues.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/MessageBox.h"
#include "vgui_controls/TextEntry.h"

using namespace vgui;

CDialogAddBan::CDialogAddBan(vgui::Panel *parent)
    : Frame(parent, "DialogAddBan") {
  SetSize(320, 200);
  SetTitle("#Game_Ban_Add_Title", false);

  m_pIDTextEntry = new TextEntry(this, "IDTextEntry");

  m_pOkayButton = new Button(this, "OkayButton", "#Okay_Button");

  m_pPermBanRadio =
      new RadioButton(this, "PermBanRadio", "#Add_Ban_Time_Permanent");
  m_pTempBanRadio =
      new RadioButton(this, "TempBanRadio", "#Add_Ban_Time_Temporary");
  m_pPermBanRadio->SetSelected(true);

  m_pTimeTextEntry = new TextEntry(this, "TimeTextEntry");
  m_pTimeCombo = new ComboBox(this, "TimeCombo", 3, false);
  int defaultItem = m_pTimeCombo->AddItem("#Add_Ban_Period_Minutes", nullptr);
  m_pTimeCombo->AddItem("#Add_Ban_Period_Hours", nullptr);
  m_pTimeCombo->AddItem("#Add_Ban_Period_Days", nullptr);
  m_pTimeCombo->ActivateItem(defaultItem);

  LoadControlSettings("Admin\\DialogAddBan.res", "PLATFORM");

  SetTitle("#Add_Ban_Title", true);
  SetSizeable(false);

  // set our initial position in the middle of the workspace
  MoveToCenterOfScreen();
}

CDialogAddBan::~CDialogAddBan() {}

// Purpose: initializes the dialog and brings it to the foreground
void CDialogAddBan::Activate(const ch *type, const ch *player,
                             const ch *authid) {
  m_cType = type;

  m_pOkayButton->SetAsDefaultButton(true);
  MakePopup();
  MoveToFront();

  RequestFocus();
  m_pIDTextEntry->RequestFocus();
  SetVisible(true);

  SetTextEntry("PlayerTextEntry", player);
  SetTextEntry("IDTextEntry", authid);

  BaseClass::Activate();
}

// Purpose: Sets the text of a labell by name
void CDialogAddBan::SetLabelText(const ch *textEntryName, const ch *text) {
  Label *entry = dynamic_cast<Label *>(FindChildByName(textEntryName));
  if (entry) entry->SetText(text);
}

// Purpose: Sets the text of a labell by name
void CDialogAddBan::SetTextEntry(const ch *textEntryName, const ch *text) {
  TextEntry *entry = dynamic_cast<TextEntry *>(FindChildByName(textEntryName));
  if (entry) entry->SetText(text);
}

bool CDialogAddBan::IsIPCheck() {
  char buf[64];
  int dotCount = 0;
  m_pIDTextEntry->GetText(buf, sizeof(buf) - 1);

  for (usize i = 0; i < strlen(buf); i++) {
    if (buf[i] == '.') dotCount++;
  }

  return dotCount > 0;
}

void CDialogAddBan::OnCommand(const ch *command) {
  bool bClose = false;

  if (!_stricmp(command, "Okay")) {
    KeyValues *msg = new KeyValues("AddBanValue");
    char buf[64], idbuf[64];
    float time;
    m_pIDTextEntry->GetText(idbuf, sizeof(idbuf));
    m_pTimeTextEntry->GetText(buf, 64);

    if (strlen(idbuf) <= 0) {
      MessageBox *dlg = new MessageBox("#Add_Ban_Error", "#Add_Ban_ID_Invalid");
      dlg->DoModal();
      bClose = false;
    } else if (strlen(buf) <= 0 && !m_pPermBanRadio->IsSelected()) {
      MessageBox *dlg = new MessageBox("#Add_Ban_Error", "#Add_Ban_Time_Empty");
      dlg->DoModal();
      bClose = false;
    } else {
      if (m_pPermBanRadio->IsSelected()) {
        time = 0;
      } else {
        sscanf_s(buf, "%f", &time);
        m_pTimeCombo->GetText(buf, 64);
        if (strstr(buf, "hour")) {
          time *= 60;
        } else if (strstr(buf, "day")) {
          time *= (60 * 24);
        }
        if (time < 0) {
          MessageBox *dlg =
              new MessageBox("#Add_Ban_Error", "#Add_Ban_Time_Invalid");
          dlg->DoModal();
          bClose = false;
        }
      }

      if (time >= 0) {
        msg->SetFloat("time", time);
        msg->SetString("id", idbuf);
        msg->SetString("type", m_cType);
        msg->SetInt("ipcheck", IsIPCheck());

        PostActionSignal(msg);

        bClose = true;
      }
    }
  } else if (!_stricmp(command, "Close")) {
    bClose = true;
  } else {
    BaseClass::OnCommand(command);
  }

  if (bClose) Close();
}

void CDialogAddBan::PerformLayout() { BaseClass::PerformLayout(); }

// Purpose: deletes the dialog on close
void CDialogAddBan::OnClose() {
  BaseClass::OnClose();
  MarkForDeletion();
}

// Purpose: called when the perm/temp ban time radio buttons are pressed
void CDialogAddBan::OnButtonToggled(Panel *panel) {
  if (panel == m_pPermBanRadio) {
    m_pTimeTextEntry->SetEnabled(false);
    m_pTimeCombo->SetEnabled(false);
  } else {
    m_pTimeTextEntry->SetEnabled(true);
    m_pTimeCombo->SetEnabled(true);
  }

  Repaint();
}
