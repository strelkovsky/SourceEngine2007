// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASESTATUSBAR_H
#define BASESTATUSBAR_H

#include "datamodel/dmehandle.h"
#include "vgui_controls/EditablePanel.h"

class CDmeClip;
class CMovieDoc;
class CConsolePage;
namespace vgui {
class Label;
}

class CBaseStatusBar : public vgui::EditablePanel {
  DECLARE_CLASS_SIMPLE(CBaseStatusBar, vgui::EditablePanel)
 public:
  CBaseStatusBar(vgui::Panel *parent, char const *panelName);

 private:
  void UpdateMemoryUsage(float mbUsed);
  virtual void PerformLayout();
  virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

  virtual void OnThink();

  CConsolePage *m_pConsole;
  vgui::Label *m_pLabel;
  vgui::Label *m_pMemory;
  vgui::Label *m_pFPS;
  vgui::Label *m_pGameTime;
  float m_flLastFPSSnapShot;
};

#endif  // BASESTATUSBAR_H
