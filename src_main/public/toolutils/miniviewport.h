// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#ifndef MINIVIEWPORT_H
#define MINIVIEWPORT_H

#include "vgui_controls/ToolWindow.h"

class CMiniViewportPropertyPage;

class CMiniViewport : public vgui::ToolWindow {
  DECLARE_CLASS_SIMPLE(CMiniViewport, vgui::ToolWindow);

 public:
  CMiniViewport(vgui::Panel* parent, bool contextLabel,
                vgui::IToolWindowFactory* factory = 0, vgui::Panel* page = NULL,
                char const* title = NULL, bool contextMenu = false);

  void GetViewport(bool& enabled, int& x, int& y, int& width, int& height);

  void GetEngineBounds(int& x, int& y, int& w, int& h);

  void RenderFrameBegin();

  // Sets text to draw over the window
  void SetOverlayText(const char* pText);

  void ReleaseLayoffTexture();

 private:
  vgui::DHANDLE<CMiniViewportPropertyPage> m_hPage;
};

#endif  // MINIVIEWPORT_H
