// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Core Movie Maker UI API

#include "toolutils/toolswitchmenubutton.h"

#include "tier1/keyvalues.h"
#include "toolframework/ienginetool.h"
#include "toolutils/enginetools_int.h"
#include "toolutils/toolmenubutton.h"
#include "vgui_controls/menu.h"
#include "vgui_controls/panel.h"

#include "tier0/include/memdbgon.h"

//-----------------------------------------------------------------------------
// Menu to switch between tools
//-----------------------------------------------------------------------------
class CToolSwitchMenuButton : public CToolMenuButton {
  DECLARE_CLASS_SIMPLE(CToolSwitchMenuButton, CToolMenuButton);

 public:
  CToolSwitchMenuButton(vgui::Panel *parent, const char *panelName,
                        const char *text, vgui::Panel *pActionTarget);
  virtual void OnShowMenu(vgui::Menu *menu);
};

//-----------------------------------------------------------------------------
// Global function to create the switch menu
//-----------------------------------------------------------------------------
CToolMenuButton *CreateToolSwitchMenuButton(vgui::Panel *parent,
                                            const char *panelName,
                                            const char *text,
                                            vgui::Panel *pActionTarget) {
  return new CToolSwitchMenuButton(parent, panelName, text, pActionTarget);
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CToolSwitchMenuButton::CToolSwitchMenuButton(vgui::Panel *parent,
                                             const char *panelName,
                                             const char *text,
                                             vgui::Panel *pActionTarget)
    : BaseClass(parent, panelName, text, pActionTarget) {
  SetMenu(m_pMenu);
}

//-----------------------------------------------------------------------------
// Is called when the menu is made visible
//-----------------------------------------------------------------------------
void CToolSwitchMenuButton::OnShowMenu(vgui::Menu *menu) {
  BaseClass::OnShowMenu(menu);

  Reset();

  int c = enginetools->GetToolCount();
  for (int i = 0; i < c; ++i) {
    char const *toolname = enginetools->GetToolName(i);

    char toolcmd[32];
    Q_snprintf(toolcmd, sizeof(toolcmd), "OnTool%i", i);

    int id = AddCheckableMenuItem(toolname, toolname,
                                  new KeyValues("Command", "command", toolcmd),
                                  m_pActionTarget);
    m_pMenu->SetItemEnabled(id, true);
    m_pMenu->SetMenuItemChecked(
        id, enginetools->IsTopmostTool(enginetools->GetToolSystem(i)));
  }
}
