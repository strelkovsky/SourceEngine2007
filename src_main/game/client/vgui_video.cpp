// Copyright � 1996-2007, Valve Corporation, All rights reserved.
//
// Purpose: VGUI panel which can play back video, in-engine

#include "cbase.h"

#include "vgui_video.h"

#include "engine/ienginesound.h"
#include "iclientmode.h"
#include "ienginevgui.h"
#include "vgui/IInput.h"
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"

 
#include "tier0/include/memdbgon.h"

VideoPanel::VideoPanel(unsigned int nXPos, unsigned int nYPos,
                       unsigned int nHeight, unsigned int nWidth)
    : BaseClass(NULL, "VideoPanel"),
      m_BIKHandle(BIKHANDLE_INVALID),
      m_nPlaybackWidth(0),
      m_nPlaybackHeight(0) {
  vgui::VPANEL pParent = enginevgui->GetPanel(PANEL_GAMEUIDLL);
  SetParent(pParent);
  SetVisible(false);

  // Must be passed in, off by default
  m_szExitCommand[0] = '\0';

  m_bBlackBackground = true;

  SetKeyBoardInputEnabled(true);
  SetMouseInputEnabled(false);

  SetProportional(false);
  SetVisible(true);
  SetPaintBackgroundEnabled(false);
  SetPaintBorderEnabled(false);

  // Set us up
  SetTall(nHeight);
  SetWide(nWidth);
  SetPos(nXPos, nYPos);

  SetScheme(vgui::scheme()->LoadSchemeFromFile("resource/VideoPanelScheme.res",
                                               "VideoPanelScheme"));
  LoadControlSettings("resource/UI/VideoPanel.res");
}

//-----------------------------------------------------------------------------
// Properly shutdown out materials
//-----------------------------------------------------------------------------
VideoPanel::~VideoPanel(void) {
  SetParent((vgui::Panel *)NULL);

  // Shut down this video
  if (m_BIKHandle != BIKHANDLE_INVALID) {
    bik->DestroyMaterial(m_BIKHandle);
    m_BIKHandle = BIKHANDLE_INVALID;
  }
}

//-----------------------------------------------------------------------------
// Purpose: Begins playback of a movie
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool VideoPanel::BeginPlayback(const char *pFilename) {
  // Destroy any previously allocated video
  if (m_BIKHandle != BIKHANDLE_INVALID) {
    bik->DestroyMaterial(m_BIKHandle);
    m_BIKHandle = BIKHANDLE_INVALID;
  }

  // Load and create our BINK video
  m_BIKHandle = bik->CreateMaterial("VideoBIKMaterial", pFilename, "GAME");
  if (m_BIKHandle == BIKHANDLE_INVALID) return false;

  // We want to be the sole audio source
  // TODO(d.rattman): This may not always be true!
  enginesound->NotifyBeginMoviePlayback();

  int nWidth, nHeight;
  bik->GetFrameSize(m_BIKHandle, &nWidth, &nHeight);
  bik->GetTexCoordRange(m_BIKHandle, &m_flU, &m_flV);
  m_pMaterial = bik->GetMaterial(m_BIKHandle);

  float flFrameRatio = ((float)GetWide() / (float)GetTall());
  float flVideoRatio = ((float)nWidth / (float)nHeight);

  if (flVideoRatio > flFrameRatio) {
    m_nPlaybackWidth = GetWide();
    m_nPlaybackHeight = (GetWide() / flVideoRatio);
  } else if (flVideoRatio < flFrameRatio) {
    m_nPlaybackWidth = (GetTall() * flVideoRatio);
    m_nPlaybackHeight = GetTall();
  } else {
    m_nPlaybackWidth = GetWide();
    m_nPlaybackHeight = GetTall();
  }

  return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VideoPanel::Activate(void) {
  MoveToFront();
  RequestFocus();
  SetVisible(true);
  SetEnabled(true);

  vgui::surface()->SetMinimized(GetVPanel(), false);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VideoPanel::DoModal(void) {
  MakePopup();
  Activate();

  vgui::input()->SetAppModalSurface(GetVPanel());
  vgui::surface()->RestrictPaintToSinglePanel(GetVPanel());
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VideoPanel::OnKeyCodeTyped(vgui::KeyCode code) {
  if (code == KEY_ESCAPE) {
    OnClose();
  } else {
    BaseClass::OnKeyCodeTyped(code);
  }
}

//-----------------------------------------------------------------------------
// Purpose: Handle keys that should cause us to close
//-----------------------------------------------------------------------------
void VideoPanel::OnKeyCodePressed(vgui::KeyCode code) {
  // These keys cause the panel to shutdown
  if (code == KEY_ESCAPE || code == KEY_BACKQUOTE || code == KEY_SPACE ||
      code == KEY_ENTER || code == KEY_XBUTTON_A || code == KEY_XBUTTON_B ||
      code == KEY_XBUTTON_X || code == KEY_XBUTTON_Y ||
      code == KEY_XBUTTON_START || code == KEY_XBUTTON_BACK) {
    OnClose();
  } else {
    BaseClass::OnKeyCodePressed(code);
  }
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VideoPanel::OnClose(void) {
  enginesound->NotifyEndMoviePlayback();
  BaseClass::OnClose();

  if (vgui::input()->GetAppModalSurface() == GetVPanel()) {
    vgui::input()->ReleaseAppModalSurface();
  }

  vgui::surface()->RestrictPaintToSinglePanel(NULL);

  // Fire an exit command if we're asked to do so
  if (m_szExitCommand[0]) {
    engine->ClientCmd(m_szExitCommand);
  }

  SetVisible(false);
  MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void VideoPanel::GetPanelPos(int &xpos, int &ypos) {
  xpos = ((float)(GetWide() - m_nPlaybackWidth) / 2);
  ypos = ((float)(GetTall() - m_nPlaybackHeight) / 2);
}

//-----------------------------------------------------------------------------
// Purpose: Update and draw the frame
//-----------------------------------------------------------------------------
void VideoPanel::Paint(void) {
  BaseClass::Paint();

  // No video to play, so do nothing
  if (m_BIKHandle == BIKHANDLE_INVALID) return;

  // Update our frame
  if (bik->Update(m_BIKHandle) == false) {
    // Issue a close command
    OnVideoOver();
    OnClose();
  }

  // Sit in the "center"
  int xpos, ypos;
  GetPanelPos(xpos, ypos);

  // Black out the background (we could omit drawing under the video surface,
  // but this is straight-forward)
  if (m_bBlackBackground) {
    vgui::surface()->DrawSetColor(0, 0, 0, 255);
    vgui::surface()->DrawFilledRect(0, 0, GetWide(), GetTall());
  }

  // Draw the polys to draw this out
  CMatRenderContextPtr pRenderContext(materials);

  pRenderContext->MatrixMode(MATERIAL_VIEW);
  pRenderContext->PushMatrix();
  pRenderContext->LoadIdentity();

  pRenderContext->MatrixMode(MATERIAL_PROJECTION);
  pRenderContext->PushMatrix();
  pRenderContext->LoadIdentity();

  pRenderContext->Bind(m_pMaterial, NULL);

  CMeshBuilder meshBuilder;
  IMesh *pMesh = pRenderContext->GetDynamicMesh(true);
  meshBuilder.Begin(pMesh, MATERIAL_QUADS, 1);

  float flLeftX = xpos;
  float flRightX = xpos + (m_nPlaybackWidth - 1);

  float flTopY = ypos;
  float flBottomY = ypos + (m_nPlaybackHeight - 1);

  // Map our UVs to cut out just the portion of the video we're interested in
  float flLeftU = 0.0f;
  float flTopV = 0.0f;

  // We need to subtract off a pixel to make sure we don't bleed
  float flRightU = m_flU - (1.0f / (float)m_nPlaybackWidth);
  float flBottomV = m_flV - (1.0f / (float)m_nPlaybackHeight);

  // Get the current viewport size
  int vx, vy, vw, vh;
  pRenderContext->GetViewport(vx, vy, vw, vh);

  // map from screen pixel coords to -1..1
  flRightX = FLerp(-1, 1, 0, vw, flRightX);
  flLeftX = FLerp(-1, 1, 0, vw, flLeftX);
  flTopY = FLerp(1, -1, 0, vh, flTopY);
  flBottomY = FLerp(1, -1, 0, vh, flBottomY);

  float alpha = ((float)GetFgColor()[3] / 255.0f);

  for (int corner = 0; corner < 4; corner++) {
    bool bLeft = (corner == 0) || (corner == 3);
    meshBuilder.Position3f((bLeft) ? flLeftX : flRightX,
                           (corner & 2) ? flBottomY : flTopY, 0.0f);
    meshBuilder.Normal3f(0.0f, 0.0f, 1.0f);
    meshBuilder.TexCoord2f(0, (bLeft) ? flLeftU : flRightU,
                           (corner & 2) ? flBottomV : flTopV);
    meshBuilder.TangentS3f(0.0f, 1.0f, 0.0f);
    meshBuilder.TangentT3f(1.0f, 0.0f, 0.0f);
    meshBuilder.Color4f(1.0f, 1.0f, 1.0f, alpha);
    meshBuilder.AdvanceVertex();
  }

  meshBuilder.End();
  pMesh->Draw();

  pRenderContext->MatrixMode(MATERIAL_VIEW);
  pRenderContext->PopMatrix();

  pRenderContext->MatrixMode(MATERIAL_PROJECTION);
  pRenderContext->PopMatrix();
}

//-----------------------------------------------------------------------------
// Purpose: Create and playback a video in a panel
// Input  : nWidth - Width of panel (in pixels)
//			nHeight - Height of panel
//			*pVideoFilename - Name of the file to play
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool VideoPanel_Create(unsigned int nXPos, unsigned int nYPos,
                       unsigned int nWidth, unsigned int nHeight,
                       const char *pVideoFilename,
                       const char *pExitCommand /*= NULL*/) {
  // Create the base video panel
  VideoPanel *pVideoPanel = new VideoPanel(nXPos, nYPos, nHeight, nWidth);
  if (pVideoPanel == NULL) return false;

  // Set the command we'll call (if any) when the video is interrupted or
  // completes
  pVideoPanel->SetExitCommand(pExitCommand);

  // Start it going
  if (pVideoPanel->BeginPlayback(pVideoFilename) == false) {
    delete pVideoPanel;
    return false;
  }

  // Take control
  pVideoPanel->DoModal();

  return true;
}

//-----------------------------------------------------------------------------
// Purpose: Used to launch a video playback (Debug)
//-----------------------------------------------------------------------------

CON_COMMAND(playvideo, "Plays a video: <filename> [width height]") {
  if (args.ArgC() < 2) return;

  unsigned int nScreenWidth = Q_atoi(args[2]);
  unsigned int nScreenHeight = Q_atoi(args[3]);

  char strFullpath[SOURCE_MAX_PATH];
  Q_strncpy(strFullpath, "media/",
            SOURCE_MAX_PATH);  // Assume we must play out of the media directory
  char strFilename[SOURCE_MAX_PATH];
  Q_StripExtension(args[1], strFilename, SOURCE_MAX_PATH);
  Q_strncat(strFullpath, args[1], SOURCE_MAX_PATH);
  Q_strncat(strFullpath, ".bik",
            SOURCE_MAX_PATH);  // Assume we're a .bik extension type

  if (nScreenWidth == 0) {
    nScreenWidth = ScreenWidth();
  }

  if (nScreenHeight == 0) {
    nScreenHeight = ScreenHeight();
  }

  // Create the panel and go!
  if (VideoPanel_Create(0, 0, nScreenWidth, nScreenHeight, strFullpath) ==
      false) {
    Warning("Unable to play video: %s\n", strFullpath);
  }
}

//-----------------------------------------------------------------------------
// Purpose: Used to launch a video playback and fire a command on completion
//-----------------------------------------------------------------------------

CON_COMMAND(playvideo_exitcommand,
            "Plays a video and fires and exit command when it is stopped or "
            "finishes: <filename> <exit command>") {
  if (args.ArgC() < 2) return;

  unsigned int nScreenWidth = ScreenWidth();
  unsigned int nScreenHeight = ScreenHeight();

  char strFullpath[SOURCE_MAX_PATH];
  Q_strncpy(strFullpath, "media/",
            SOURCE_MAX_PATH);  // Assume we must play out of the media directory
  char strFilename[SOURCE_MAX_PATH];
  Q_StripExtension(args[1], strFilename, SOURCE_MAX_PATH);
  Q_strncat(strFullpath, args[1], SOURCE_MAX_PATH);
  Q_strncat(strFullpath, ".bik",
            SOURCE_MAX_PATH);  // Assume we're a .bik extension type

  char *pExitCommand = Q_strstr(args.GetCommandString(), args[2]);

  // Create the panel and go!
  if (VideoPanel_Create(0, 0, nScreenWidth, nScreenHeight, strFullpath,
                        pExitCommand) == false) {
    Warning("Unable to play video: %s\n", strFullpath);
    engine->ClientCmd(pExitCommand);
  }
}
