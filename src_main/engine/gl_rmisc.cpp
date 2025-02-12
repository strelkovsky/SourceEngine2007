// Copyright © 1996-2018, Valve Corporation, All rights reserved.

// HDRFIXME: reduce the number of include files here.
#include "render_pch.h"

#include "../utils/common/bsplib.h"
#include "Overlay.h"
#include "bitmap/imageformat.h"
#include "cbenchmark.h"
#include "cdll_engine_int.h"
#include "cdll_int.h"
#include "client_class.h"
#include "gl_drawlights.h"
#include "ibsppack.h"
#include "icliententity.h"
#include "icliententitylist.h"
#include "ispatialpartitioninternal.h"
#include "ivideomode.h"
#include "ivtex.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/itexture.h"
#include "r_areaportal.h"
#include "r_decal.h"
#include "server.h"
#include "staticpropmgr.h"
#include "tier0/include/dbg.h"
#include "tier1/utlbuffer.h"
#include "traceinit.h"
#include "view.h"
#include "vmodes.h"
#include "vtf/vtf.h"
#include "initmathlib.h"

#include "tier0/include/memdbgon.h"

void Linefile_Read_f();

/*
====================
//	if( r_drawtranslucentworld )
//	{
//		bSaveDrawTranslucentWorld = r_drawtranslucentworld->GetBool();
                // NOTE! : We use to set this to 0 for HDR.
                //		r_drawtranslucentworld->SetValue( 0 );
//	}
//	if( r_drawtranslucentrenderables )
R_TimeRefresh_f

For program optimization
====================
*/
void R_TimeRefresh_f() {
  CViewSetup view;

  materials->Flush(true);

  memset(&view, 0, sizeof(view));
  view.origin = MainViewOrigin();
  view.angles[0] = 0;
  view.angles[1] = 0;
  view.angles[2] = 0;
  view.x = 0;
  view.y = 0;
  view.width = videomode->GetModeWidth();
  view.height = videomode->GetModeHeight();
  view.fov = 75;
  view.fovViewmodel = 75;
  view.m_flAspectRatio = 1.0f;
  view.zNear = 4;
  view.zFar = MAX_COORD_FLOAT;
  view.zNearViewmodel = 4;
  view.zFarViewmodel = MAX_COORD_FLOAT;

  int savedeveloper = developer.GetInt();
  developer.SetValue(0);

  float start = Plat_FloatTime();
  for (int i = 0; i < 128; i++) {
    view.angles[1] = i / 128.0 * 360.0;
    g_ClientDLL->RenderView(view, VIEW_CLEAR_COLOR,
                            RENDERVIEW_DRAWVIEWMODEL | RENDERVIEW_DRAWHUD);
    Shader_SwapBuffers();
  }

  materials->Flush(true);
  Shader_SwapBuffers();
  float stop = Plat_FloatTime();
  float time = stop - start;

  developer.SetValue(savedeveloper);

  ConMsg("%f seconds (%f fps)\n", time, 128 / time);
}

ConCommand timerefresh("timerefresh", R_TimeRefresh_f, "Profile the renderer.",
                       FCVAR_CHEAT);
ConCommand linefile("linefile", Linefile_Read_f,
                    "Parses map leak data from .lin file", FCVAR_CHEAT);

void R_Init() {
  extern uint8_t *hunk_base;

  InitMathlib();

  UpdateMaterialSystemConfig();
}

void R_Shutdown() {}

void R_ResetLightStyles() {
  for (int i = 0; i < 256; i++) {
    // normal light value
    if (d_lightstylevalue[i] != 264) {
      d_lightstylevalue[i] = 264;
      d_lightstyleframe[i] = r_framecount;
    }
  }
}

void R_RemoveAllDecalsFromAllModels();

CON_COMMAND_F(r_cleardecals, "Usage r_cleardecals <permanent>.",
              FCVAR_CLIENTCMD_CAN_EXECUTE) {
  if (host_state.worldmodel) {
    bool bPermanent = false;
    if (args.ArgC() == 2) {
      if (!Q_stricmp(args[1], "permanent")) {
        bPermanent = true;
      }
    }

    R_DecalTerm(host_state.worldmodel->brush.pShared, bPermanent);
  }

  R_RemoveAllDecalsFromAllModels();
}

// Loads world geometry. Called when map changes or dx level changes
void R_LoadWorldGeometry(bool bDXChange) {
  // Recreate the sortinfo arrays ( ack, uses new/delete right now ) because
  // doing it with Hunk_AllocName will leak through every connect that doesn't
  // wipe the hunk ( "reconnect" )
  MaterialSystem_DestroySortinfo();

  MaterialSystem_RegisterLightmapSurfaces();

  MaterialSystem_CreateSortinfo();

  // UNDONE: This is a really crappy place to do this - shouldn't this stuff be
  // in the modelloader?

  // If this is the first time we've tried to render this map, create a few
  // one-time data structures These all get cleared out if Map_UnloadModel is
  // ever called by the modelloader interface ( and that happens
  //  any time we free the Hunk down to the low mark since these things all use
  //  the Hunk for their data ).
  if (!bDXChange) {
    if (!modelloader->Map_GetRenderInfoAllocated()) {
      // create the displacement surfaces for the map
      modelloader->Map_LoadDisplacements(host_state.worldmodel, false);
      // if( !DispInfo_CreateStaticBuffers( host_state.worldmodel,
      // materialSortInfoArray, false ) ) 	Sys_Error( "Can't create static
      // meshes
      // for displacements" );

      modelloader->Map_SetRenderInfoAllocated(true);
    }
  } else {
    // create the displacement surfaces for the map
    modelloader->Map_LoadDisplacements(host_state.worldmodel, true);
  }

  if (bDXChange) {
    // Must be done before MarkWaterSurfaces
    modelloader->RecomputeSurfaceFlags(host_state.worldmodel);
  }

  Mod_MarkWaterSurfaces(host_state.worldmodel);

  // make sure and rebuild lightmaps when the level gets started.
  GL_RebuildLightmaps();

  if (bDXChange) {
    R_BrushBatchInit();
    R_DecalReSortMaterials();
    OverlayMgr()->ReSortMaterials();
  }
}

void R_LevelInit() {
  ConDMsg("Initializing renderer...\n");

  Plat_TimestampedLog("Engine::R_LevelInit start.");

  Assert(g_ClientDLL);

  r_framecount = 1;
  R_ResetLightStyles();
  R_DecalInit();
  R_LoadSkys();
  R_InitStudio();

  // TODO(d.rattman): Is this the best place to initialize the kd tree when
  // we're client-only?
  if (!sv.IsActive()) {
    g_pShadowMgr->LevelShutdown();
    StaticPropMgr()->LevelShutdown();
    SpatialPartition()->Init(host_state.worldmodel->mins,
                             host_state.worldmodel->maxs);
    StaticPropMgr()->LevelInit();
    g_pShadowMgr->LevelInit(host_state.worldbrush->numsurfaces);
  }

  // We've fully loaded the new level, unload any models that we don't care
  // about any more
  modelloader->UnloadUnreferencedModels();

  if (host_state.worldmodel->brush.pShared->numworldlights == 0) {
    ConDMsg("Level unlit, setting 'mat_fullbright 1'\n");
    mat_fullbright.SetValue(1);
  }

  UpdateMaterialSystemConfig();

  // TODO(d.rattman): E3 2003 HACK
  if (mat_levelflush.GetBool()) {
    materials->ResetTempHWMemory(false);
  }

  // precache any textures that are used in this map.
  // this is a no-op for textures that are already cached from the previous map.
  materials->CacheUsedMaterials();

  // Loads the world geometry
  R_LoadWorldGeometry();

  R_Surface_LevelInit();
  R_Areaportal_LevelInit();

  // Build the overlay fragments.
  OverlayMgr()->CreateFragments();

  Plat_TimestampedLog("Engine::R_LevelInit end.");
}

void R_LevelShutdown() {
  R_Surface_LevelShutdown();
  R_Areaportal_LevelShutdown();
  g_DispLightmapSamplePositions.Purge();
}
