// Copyright © 1996-2018, Valve Corporation, All rights reserved.

// HDRFIXME: reduce the number of include files here.

#include "render_pch.h"

#include "../utils/common/bsplib.h"
#include "Overlay.h"
#include "bitmap/float_bm.h"
#include "bitmap/imageformat.h"
#include "cbenchmark.h"
#include "cdll_engine_int.h"
#include "cdll_int.h"
#include "client.h"
#include "client_class.h"
#include "dmxloader/dmxelement.h"
#include "dmxloader/dmxloader.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "gl_cvars.h"
#include "gl_drawlights.h"
#include "ibsppack.h"
#include "icliententity.h"
#include "icliententitylist.h"
#include "ispatialpartitioninternal.h"
#include "ivideomode.h"
#include "ivtex.h"
#include "lightcache.h"
#include "materialsystem/itexture.h"
#include "r_decal.h"
#include "server.h"
#include "staticpropmgr.h"
#include "tier0/include/dbg.h"
#include "tier0/include/icommandline.h"
#include "tier1/utlbuffer.h"
#include "tier2/fileutils.h"
#include "tier2/tier2.h"
#include "traceinit.h"
#include "view.h"
#include "vmodes.h"
#include "vtf/vtf.h"

#include "tier0/include/memdbgon.h"

// putting this here so that it is replicated to the client.dll and
// materialsystem.dll
ConVar dynamic_tonemap("mat_dynamic_tonemapping", "1", FCVAR_CHEAT);
ConVar building_cubemaps("building_cubemaps", "0");
ConVar reload_materials("reload_materials", "0");
ConVar r_DrawBeams("r_DrawBeams", "1", FCVAR_CHEAT,
                   "0=Off, 1=Normal, 2=Wireframe");

static ConVar mat_force_tonemap_scale("mat_force_tonemap_scale", "0.0",
                                      FCVAR_CHEAT);

static const ch *facingName[6] = {"rt", "lf", "bk", "ft", "up", "dn"};

//-----------------------------------------------------------------------------
// Load, unload vtex
//-----------------------------------------------------------------------------
static IVTex *VTex_Load(CSysModule **pModule) {
  // load the vtex dll
  IVTex *pIVTex = NULL;
  *pModule = FileSystem_LoadModule("vtex_dll");
  if (*pModule) {
    CreateInterfaceFn factory = Sys_GetFactory(*pModule);
    if (factory) {
      pIVTex = (IVTex *)factory(IVTEX_VERSION_STRING, NULL);
    }
  }

  if (!pIVTex) {
    ConMsg("Can't load vtex_dll.dll\n");
  }

  return pIVTex;
}

static void VTex_Unload(CSysModule *pModule) {
  FileSystem_UnloadModule(pModule);
}

//-----------------------------------------------------------------------------
// Main entry point for taking cubemap snapshots
//-----------------------------------------------------------------------------
static void TakeCubemapSnapshot(const Vector &origin, const ch *pFileNameBase,
                                int screenBufSize, int tgaSize, bool bPFM) {
  if (g_LostVideoMemory) return;

  ITexture *pSaveRenderTarget = NULL;

  CMatRenderContextPtr pRenderContext(materials);

  // HDRFIXME: push/pop
  if (bPFM) {
    pSaveRenderTarget = pRenderContext->GetRenderTarget();
    pRenderContext->SetRenderTarget(NULL);
  }

  // HACK HACK HACK!!!!
  // If this is lower than the size of the render target (I think) we don't get
  // water.
  screenBufSize = 512;

  ch name[1024];
  CViewSetup view;
  memset(&view, 0, sizeof(view));
  view.origin = origin;
  view.m_flAspectRatio = 1.0f;
  view.m_bRenderToSubrectOfLargerScreen = true;

  // garymcthack
  view.zNear = 8.0f;
  view.zFar = 28400.0f;

  view.x = 0;
  view.y = 0;

  view.width = (float)screenBufSize;
  view.height = (float)screenBufSize;

  const ch *pExtension = ".tga";
  if (bPFM) pExtension = ".pfm";

  Shader_BeginRendering();

  if (bPFM) {
    int backbufferWidth, backbufferHeight;
    materials->GetBackBufferDimensions(backbufferWidth, backbufferHeight);
    pRenderContext->Viewport(0, 0, backbufferWidth, backbufferHeight);
    pRenderContext->ClearColor3ub(128, 128, 128);
    pRenderContext->ClearBuffers(true, true);
  }

  int nFlags = VIEW_CLEAR_COLOR | VIEW_CLEAR_DEPTH;

  // NOTE: This is for a workaround on ATI with building cubemaps.
  // Clearing just the viewport doesn't seem to work properly.
  nFlags |= VIEW_CLEAR_FULL_TARGET;

  static float angle0[6] = {0, 0, 0, 0, -90, 90};
  static float angle1[6] = {0, 180, 90, 270, 0, 0};
  static CubeMapFaceIndex_t face_idx[6] = {
      CUBEMAP_FACE_RIGHT, CUBEMAP_FACE_LEFT, CUBEMAP_FACE_BACK,
      CUBEMAP_FACE_FRONT, CUBEMAP_FACE_UP,   CUBEMAP_FACE_DOWN};
  static int engine_cubemap_idx_to_fbm_idx[6] = {4, 3, 0, 2, 5, 1};

  if (bPFM) {
    FloatCubeMap_t Envmap(tgaSize, tgaSize);
    for (int side = 0; side < 6; side++) {
      view.angles[0] = angle0[side];
      view.angles[1] = angle1[side];
      view.angles[2] = 0;
      view.fov = 90;
      view.fovViewmodel = 90;
      view.origin = origin;
      if (g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_INTEGER) {
        FloatBitMap_t &hdr_map =
            Envmap.face_maps[engine_cubemap_idx_to_fbm_idx[side]];
        hdr_map.Clear(0, 0, 0, 1);
        // we are going to need to render multiple exposures
        float exposure = 16.0;
        bool bOverExposedTexels = true;
        while (bOverExposedTexels && (exposure > 0.05)) {
          mat_force_tonemap_scale.SetValue(0.0f);
          pRenderContext->ResetToneMappingScale(exposure);
          g_ClientDLL->RenderView(view, nFlags, 0);
          uint8_t *pImage = new uint8_t[screenBufSize * screenBufSize * 4];
          uint8_t *pImage1 = new uint8_t[tgaSize * tgaSize * 4];

          // Get Bits from the material system
          pRenderContext->ReadPixels(0, 0, screenBufSize, screenBufSize, pImage,
                                     IMAGE_FORMAT_RGBA8888);

          ImageLoader::ResampleInfo_t info;
          info.m_pSrc = pImage;
          info.m_pDest = pImage1;
          info.m_nSrcWidth = screenBufSize;
          info.m_nSrcHeight = screenBufSize;
          info.m_nDestWidth = tgaSize;
          info.m_nDestHeight = tgaSize;
          info.m_flSrcGamma = 1.0f;
          info.m_flDestGamma = 1.0f;

          if (!ImageLoader::ResampleRGBA8888(info)) {
            Sys_Error("Can't resample\n");
          }
          FloatBitMap_t ldr_map(tgaSize, tgaSize);
          for (int x1 = 0; x1 < tgaSize; x1++)
            for (int y1 = 0; y1 < tgaSize; y1++)
              for (int c = 0; c < 3; c++)
                ldr_map.Pixel(x1, y1, c) =
                    pImage1[c + 4 * (x1 + tgaSize * y1)] * (1 / 255.0);
          delete[] pImage;
          delete[] pImage1;
          ldr_map.RaiseToPower(2.2);  // gamma to linear
          float scale = 1.0 / exposure;
          bOverExposedTexels = false;
          for (int x = 0; x < hdr_map.Width; x++)
            for (int y = 0; y < hdr_map.Height; y++)
              for (int c = 0; c < 3; c++) {
                float texel = ldr_map.Pixel(x, y, c);
                if (texel > 0.98) bOverExposedTexels = true;
                texel *= scale;
                hdr_map.Pixel(x, y, c) =
                    std::max(hdr_map.Pixel(x, y, c), texel);
              }
          exposure *= 0.75;
          materials->SwapBuffers();
        }
        Q_snprintf(name, sizeof(name), "%s%s%s", pFileNameBase,
                   facingName[side], pExtension);
        //				hdr_map.WritePFM(name);
      } else {
        g_ClientDLL->RenderView(view, nFlags, 0);
        Q_snprintf(name, sizeof(name), "%s%s%s", pFileNameBase,
                   facingName[side], pExtension);
        Assert(strlen(name) < 1023);
        videomode->TakeSnapshotTGARect(name, 0, 0, screenBufSize, screenBufSize,
                                       tgaSize, tgaSize, bPFM, face_idx[side]);
      }
    }
    if (g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_INTEGER) {
      Q_snprintf(name, sizeof(name), "%s", pFileNameBase);
      Envmap.WritePFMs(name);
    }

  } else {
    for (int side = 0; side < 6; side++) {
      view.angles[0] = angle0[side];
      view.angles[1] = angle1[side];
      view.angles[2] = 0;
      view.fov = 90;
      view.fovViewmodel = 90;
      view.origin = origin;

      g_ClientDLL->RenderView(view, nFlags, 0);
      Q_snprintf(name, sizeof(name), "%s%s%s", pFileNameBase, facingName[side],
                 pExtension);
      Assert(strlen(name) < 1023);
      videomode->TakeSnapshotTGARect(name, 0, 0, screenBufSize, screenBufSize,
                                     tgaSize, tgaSize, bPFM, face_idx[side]);
    }
  }

  if (bPFM) {
    materials->SwapBuffers();
    // HDRFIXME: push/pop
    pRenderContext->SetRenderTarget(pSaveRenderTarget);
  }
}

//-----------------------------------------------------------------------------
// Interface factory for VTex
//-----------------------------------------------------------------------------
void *CubemapsFSFactory(const ch *pName, int *pReturnCode) {
  if (Q_stricmp(pName, FILESYSTEM_INTERFACE_VERSION) == 0) return g_pFileSystem;

  return NULL;
}

//-----------------------------------------------------------------------------
// Generates a cubemap .vtf from .TGA snapshots
//-----------------------------------------------------------------------------
static void BuildSingleCubemap(const ch *pVTFName, const Vector &vecOrigin,
                               int nSize, bool bHDR, const ch *pGameDir,
                               IVTex *ivt) {
  int nScreenBufSize = 4 * nSize;
  TakeCubemapSnapshot(vecOrigin, pVTFName, nScreenBufSize, nSize, bHDR);

  ch pTXTName[SOURCE_MAX_PATH];
  Q_strncpy(pTXTName, pVTFName, sizeof(pTXTName));
  Q_SetExtension(pTXTName, ".txt", sizeof(pTXTName));

  // HDRFIXME: Make this go to a buffer instead.
  FileHandle_t fp = g_pFileSystem->Open(pTXTName, "w");
  if (bHDR) {
    g_pFileSystem->FPrintf(fp, "\"pfm\" \"1\"\n");
    // HDRFIXME: Make sure that we can mip and lod and get rid of this.
  }
  // don't let any dest alpha creep into the image
  g_pFileSystem->FPrintf(fp, "\"stripalphachannel\" \"1\"\n");
  g_pFileSystem->Close(fp);

  if (ivt) {
    const ch *argv[64] = {
        "", "-quiet",
        // These are only here for the -currently released- version of vtex.dll.
        "-UseStandardError", "-WarningsAsErrors", pTXTName};
    int iArg = 5;
    // TODO: Terrible hack (UB).
    ivt->VTex(CubemapsFSFactory, pGameDir, iArg, const_cast<ch **>(argv));
  }

  g_pFileSystem->RemoveFile(pTXTName, NULL);

  const ch *pSrcExtension = bHDR ? ".pfm" : ".tga";
  for (int i = 0; i < 6; i++) {
    ch pTempName[SOURCE_MAX_PATH];
    Q_snprintf(pTempName, sizeof(pTempName), "%s%s", pVTFName, facingName[i]);
    Q_SetExtension(pTempName, pSrcExtension, sizeof(pTempName));
    g_pFileSystem->RemoveFile(pTempName, NULL);
  }
}

#ifdef _WIN32
#if !defined(SWDS)

//-----------------------------------------------------------------------------
// Grab six views for environment mapping tests
//-----------------------------------------------------------------------------
CON_COMMAND(envmap, "") {
  ch base[256];
  IClientEntity *world = entitylist->GetClientEntity(0);

  if (world && world->GetModel()) {
    Q_FileBase(modelloader->GetName((model_t *)world->GetModel()), base,
               sizeof(base));
  } else {
    Q_strncpy(base, "Env", sizeof(base));
  }

  usize strLen = strlen(base) + std::size("cubemap_screenshots/");
  ch *str = stack_alloc<ch>(strLen);
  sprintf_s(str, strLen, "cubemap_screenshots/%s", base);
  g_pFileSystem->CreateDirHierarchy("cubemap_screenshots",
                                    "DEFAULT_WRITE_PATH");

  TakeCubemapSnapshot(
      MainViewOrigin(), str, mat_envmapsize.GetInt(),
      mat_envmaptgasize.GetInt(),
      g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE);
}

//-----------------------------------------------------------------------------
// Write lighting information to a DMX file
//-----------------------------------------------------------------------------
static void WriteLightProbe(const ch *pBasePath, const LightingState_t &state,
                            bool bHDR) {
  ch pFullPath[SOURCE_MAX_PATH];
  Q_strncpy(pFullPath, pBasePath, sizeof(pFullPath));
  Q_SetExtension(pFullPath, ".prb", sizeof(pFullPath));

  DECLARE_DMX_CONTEXT();
  CDmxElement *pLightProbe = CreateDmxElement("DmeElement");

  const ch *pCubemap = pBasePath + std::size("materials/") - 1;
  CDmxElementModifyScope modify(pLightProbe);
  pLightProbe->SetValue("name", "lightprobe");
  pLightProbe->SetValue("cubemap", pCubemap);

  if (bHDR) {
    ch pTemp[SOURCE_MAX_PATH];
    Q_snprintf(pTemp, sizeof(pTemp), "%s_hdr", pCubemap);
    pLightProbe->SetValue("cubemapHdr", pTemp);
  }

  CDmxAttribute *pAmbientCube = pLightProbe->AddAttribute("ambientCube");
  CUtlVector<Vector> &vec = pAmbientCube->GetArrayForEdit<Vector>();
  for (int i = 0; i < 6; ++i) {
    vec.AddToTail(state.r_boxcolor[i]);
  }

  CDmxAttribute *pLocalLightList = pLightProbe->AddAttribute("localLights");
  CUtlVector<CDmxElement *> &lights =
      pLocalLightList->GetArrayForEdit<CDmxElement *>();

  modify.Release();

  for (int i = 0; i < state.numlights; ++i) {
    CDmxElement *pLight = CreateDmxElement("DmeElement");
    lights.AddToTail(pLight);

    CDmxElementModifyScope modify(pLight);

    const dworldlight_t &wl = *state.locallight[i];

    pLight->SetValue("color", wl.intensity);
    switch (wl.type) {
      case emit_point:
        pLight->SetValue("name", "Point");
        pLight->SetValue("origin", wl.origin);
        pLight->SetValue("attenuation", Vector(wl.constant_attn, wl.linear_attn,
                                               wl.quadratic_attn));
        pLight->SetValue("maxDistance", wl.radius);
        break;

      case emit_spotlight:
        pLight->SetValue("name", "Spot");
        pLight->SetValue("origin", wl.origin);
        pLight->SetValue("direction", wl.normal);
        pLight->SetValue("attenuation", Vector(wl.constant_attn, wl.linear_attn,
                                               wl.quadratic_attn));
        pLight->SetValue("theta", 2.0f * acos(wl.stopdot));
        pLight->SetValue("phi", 2.0f * acos(wl.stopdot2));
        pLight->SetValue("exponent", wl.exponent ? wl.exponent : 1.0f);
        pLight->SetValue("maxDistance", wl.radius);
        break;

      case emit_surface:
        pLight->SetValue("name", "Spot");
        pLight->SetValue("origin", wl.origin);
        pLight->SetValue("direction", wl.normal);
        pLight->SetValue("attenuation", Vector(0.0f, 0.0f, 1.0f));
        pLight->SetValue("theta", 0.0f);
        pLight->SetValue("phi", 0.0f);
        pLight->SetValue("exponent", 1.0f);
        pLight->SetValue("maxDistance", wl.radius);
        break;

      case emit_skylight:
        pLight->SetValue("name", "Directional");
        pLight->SetValue("direction", wl.normal);
        break;
    }
  }

  CUtlBuffer buf(0, 0, CUtlBuffer::TEXT_BUFFER);
  if (SerializeDMX(buf, pLightProbe, pFullPath)) {
    g_pFullFileSystem->WriteFile(pFullPath, "MOD", buf);
  }

  CleanupDMX(pLightProbe);
}

//-----------------------------------------------------------------------------
// Grab an envmap @ the view position + write lighting information
//-----------------------------------------------------------------------------
CON_COMMAND(lightprobe,
            "Samples the lighting environment.\n"
            "Creates a cubemap and a file indicating the local lighting in a "
            "subdirectory called 'materials/lightprobes'\n."
            "The lightprobe command requires you specify a base file name.\n") {
  if (IsX360()) return;

  if (args.ArgC() < 2) {
    ConMsg(
        "sample_lighting usage: lightprobe <base file name> [cubemap "
        "dimension]\n");
    return;
  }

  int nTGASize = mat_envmaptgasize.GetInt();
  if (args.ArgC() >= 3) {
    nTGASize = atoi(args[2]);
  }

  CSysModule *pModule;
  IVTex *pIVTex = VTex_Load(&pModule);
  if (!pIVTex) return;

  ch pBasePath[SOURCE_MAX_PATH];
  Q_snprintf(pBasePath, sizeof(pBasePath), "materials/lightprobes/%s", args[1]);
  Q_StripFilename(pBasePath);
  g_pFileSystem->CreateDirHierarchy(pBasePath, "DEFAULT_WRITE_PATH");

  ch pTemp[SOURCE_MAX_PATH];
  ch pMaterialSrcPath[SOURCE_MAX_PATH];
  Q_snprintf(pTemp, sizeof(pTemp), "materialsrc/lightprobes/%s", args[1]);
  GetModContentSubdirectory(pTemp, pMaterialSrcPath, sizeof(pMaterialSrcPath));
  Q_StripFilename(pMaterialSrcPath);
  g_pFileSystem->CreateDirHierarchy(pMaterialSrcPath, NULL);

  ch pGameDir[MAX_OSPATH];
  COM_GetGameDir(pGameDir, sizeof(pGameDir));

  bool bHDR = g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE;
  if (bHDR) {
    ch pTemp2[SOURCE_MAX_PATH];
    Q_snprintf(pTemp2, sizeof(pTemp2), "materialsrc/lightprobes/%s_hdr",
               args[1]);

    GetModContentSubdirectory(pTemp2, pMaterialSrcPath,
                              sizeof(pMaterialSrcPath));
    BuildSingleCubemap(pMaterialSrcPath, MainViewOrigin(), nTGASize, true,
                       pGameDir, pIVTex);
  }

  GetModContentSubdirectory(pTemp, pMaterialSrcPath, sizeof(pMaterialSrcPath));
  BuildSingleCubemap(pMaterialSrcPath, MainViewOrigin(), nTGASize, false,
                     pGameDir, pIVTex);

  VTex_Unload(pModule);

  // Get the lighting at the point
  LightingState_t lightingState;
  LightcacheGetDynamic_Stats stats;
  LightcacheGetDynamic(MainViewOrigin(), lightingState, stats);

  Q_snprintf(pBasePath, sizeof(pBasePath), "materials/lightprobes/%s", args[1]);
  WriteLightProbe(pBasePath, lightingState, bHDR);
}

#endif
#endif

#ifdef _WIN32
static bool LoadSrcVTFFiles(IVTFTexture *pSrcVTFTextures[6],
                            const ch *pSkyboxBaseName) {
  int i;
  for (i = 0; i < 6; i++) {
    // !!! TODO(d.rattman): This needs to open the vmt (or some other method) to
    // find the correct LDR or HDR set of skybox textures! Look in
    // vbsp\cubemap.cpp!
    ch srcVTFFileName[1024];
    Q_snprintf(srcVTFFileName, sizeof(srcVTFFileName),
               "materials/skybox/%s%s.vtf", pSkyboxBaseName, facingName[i]);

    CUtlBuffer buf;
    if (!g_pFileSystem->ReadFile(srcVTFFileName, NULL, buf)) return false;

    pSrcVTFTextures[i] = CreateVTFTexture();
    if (!pSrcVTFTextures[i]->Unserialize(buf)) {
      Warning("*** Error unserializing skybox texture: %s\n", pSkyboxBaseName);
      return false;
    }

    // NOTE: texture[0] is a side texture that could be 1/2 height, so allow
    // this and also allow 4x4 faces
    if (((pSrcVTFTextures[i]->Width() != pSrcVTFTextures[0]->Width()) &&
         (pSrcVTFTextures[i]->Width() != 4)) ||
        ((pSrcVTFTextures[i]->Height() != pSrcVTFTextures[0]->Height()) &&
         (pSrcVTFTextures[i]->Height() != pSrcVTFTextures[0]->Height() * 2) &&
         (pSrcVTFTextures[i]->Height() != 4)) ||
        (pSrcVTFTextures[i]->Flags() != pSrcVTFTextures[0]->Flags())) {
      Warning(
          "*** Error: Skybox vtf files for %s weren't compiled with the same "
          "size texture and/or same flags!\n",
          pSkyboxBaseName);
      return false;
    }
  }

  return true;
}

#define DEFAULT_CUBEMAP_SIZE 32

void Cubemap_CreateDefaultCubemap(const ch *pMapName, IBSPPack *iBSPPack) {
  // NOTE: This implementation depends on the fact that all VTF files contain
  // all mipmap levels
  ConVarRef skyboxBaseNameConVar("sv_skyname");

  IVTFTexture *pSrcVTFTextures[6];

  if (!skyboxBaseNameConVar.IsValid() || !skyboxBaseNameConVar.GetString()) {
    Warning("Couldn't create default cubemap\n");
    return;
  }

  const ch *pSkyboxBaseName = skyboxBaseNameConVar.GetString();

  if (!LoadSrcVTFFiles(pSrcVTFTextures, pSkyboxBaseName)) {
    Warning("Can't load skybox file %s to build the default cubemap!\n",
            pSkyboxBaseName);
    return;
  }
  Msg("Creating default cubemaps for env_cubemap using skybox %s...\n",
      pSkyboxBaseName);

  // Figure out the mip differences between the two textures
  int iMipLevelOffset = 0;
  int tmp = pSrcVTFTextures[0]->Width();
  while (tmp > DEFAULT_CUBEMAP_SIZE) {
    iMipLevelOffset++;
    tmp >>= 1;
  }

  // Create the destination cubemap
  IVTFTexture *pDstCubemap = CreateVTFTexture();
  pDstCubemap->Init(DEFAULT_CUBEMAP_SIZE, DEFAULT_CUBEMAP_SIZE, 1,
                    pSrcVTFTextures[0]->Format(),
                    pSrcVTFTextures[0]->Flags() | TEXTUREFLAGS_ENVMAP,
                    pSrcVTFTextures[0]->FrameCount());

  // First iterate over all frames
  for (int iFrame = 0; iFrame < pDstCubemap->FrameCount(); ++iFrame) {
    // Next iterate over all normal cube faces (we know there's 6 cause it's an
    // envmap)
    for (int iFace = 0; iFace < 6; ++iFace) {
      // Finally, iterate over all mip levels in the *destination*
      for (int iMip = 0; iMip < pDstCubemap->MipCount(); ++iMip) {
        unsigned char *pDstBits = pDstCubemap->ImageData(iFrame, iFace, iMip);
        int iSize = pDstCubemap->ComputeMipSize(iMip);

        // !!! TODO(d.rattman): Set this to black until the LDR/HDR issues are
        // fixed on line ~563 in this file
        memset(pDstBits, 0, iSize);
      }
    }
  }

  int flagUnion = 0;
  for (int i = 0; i < 6; i++) {
    flagUnion |= pSrcVTFTextures[i]->Flags();
  }
  bool bHasAlpha =
      ((flagUnion & (TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA)) !=
       0);

  // Convert the cube to format that we can apply tools to it...
  //	ImageFormat originalFormat = pDstCubemap->Format();
  pDstCubemap->ConvertImageFormat(IMAGE_FORMAT_DEFAULT, false);

  if (!bHasAlpha) {
    // set alpha to zero since the source doesn't have any alpha in it
    unsigned char *pImageData = pDstCubemap->ImageData();
    int size = pDstCubemap->ComputeTotalSize();  // in bytes!
    unsigned char *pEnd = pImageData + size;
    for (; pImageData < pEnd; pImageData += 4) {
      pImageData[3] = (unsigned char)0;
    }
  }

  // Fixup the cubemap facing
  pDstCubemap->FixCubemapFaceOrientation();

  // Now that the bits are in place, compute the spheremaps...
  pDstCubemap->GenerateSpheremap();

  // Convert the cubemap to the final format
  pDstCubemap->ConvertImageFormat(IMAGE_FORMAT_DXT5, false);

  // Write the puppy out!
  ch dstVTFFileName[1024];
  Q_snprintf(dstVTFFileName, sizeof(dstVTFFileName),
             "materials/maps/%s/cubemapdefault.vtf", pMapName);

  CUtlBuffer outputBuf;
  if (!pDstCubemap->Serialize(outputBuf)) {
    Warning("Error serializing default cubemap %s\n", dstVTFFileName);
    return;
  }

  // spit out the default one.
  iBSPPack->AddBufferToPack(dstVTFFileName, outputBuf.Base(),
                            outputBuf.TellPut(), false);

  // Clean up the textures
  for (int i = 0; i < 6; i++) {
    DestroyVTFTexture(pSrcVTFTextures[i]);
  }
  DestroyVTFTexture(pDstCubemap);
}

static void AddSampleToBSPFile(bool bHDR, mcubemapsample_t *pSample,
                               const ch *matDir, IBSPPack *iBSPPack) {
  ch textureName[512];
  const ch *pHDRExtension = "";
  if (bHDR) {
    pHDRExtension = ".hdr";
  }
  Q_snprintf(textureName, sizeof(textureName), "%s/c%d_%d_%d%s.vtf", matDir,
             (int)pSample->origin[0], (int)pSample->origin[1],
             (int)pSample->origin[2], pHDRExtension);
  ch localPath[1024];
  if (!g_pFileSystem->GetLocalPath(textureName, localPath, sizeof(localPath))) {
    Warning("vtex failed to compile cubemap!\n");
  } else {
    Q_FixSlashes(localPath);
    iBSPPack->AddFileToPack(textureName, localPath);
  }
  g_pFileSystem->RemoveFile(textureName, "DEFAULT_WRITE_PATH");
}

// Take a cubemap at each "cubemap" entity in the current map.
// HOLY CRAP THIS NEEDS TO BE CLEANED UP
void R_BuildCubemapSamples(int iterations_num) {
  // Make sure that the file is writable before building cubemaps.
  Assert(g_pFileSystem->FileExists(cl.m_szLevelName, "GAME"));
  if (!g_pFileSystem->IsFileWritable(cl.m_szLevelName, "GAME")) {
    Warning(
        "%s is not writable!!!  Check it out before running buildcubemaps.\n",
        cl.m_szLevelName);
    return;
  }

  // disable the mouse so that it won't be recentered all the bloody time.
  ConVarRef cl_mouseenable("cl_mouseenable");
  if (cl_mouseenable.IsValid()) cl_mouseenable.SetValue(0);

  ConVarRef r_shadows("r_shadows");
  bool saveShadows = true;
  if (r_shadows.IsValid()) {
    saveShadows = r_shadows.GetBool();
    r_shadows.SetValue(0);
  }

  // Clear the water surface.
  ConVarRef mat_drawwater("mat_drawwater");
  bool bDrawWater = true;
  if (mat_drawwater.IsValid()) {
    bDrawWater = mat_drawwater.GetBool();
    mat_drawwater.SetValue(0);
  }

  int nSaveLightStyle = -1;
  ConVarRef r_lightstyle("r_lightstyle");
  if (r_lightstyle.IsValid()) {
    nSaveLightStyle = r_lightstyle.GetInt();
    r_lightstyle.SetValue(0);
    R_RedownloadAllLightmaps();
  }

  int bSaveDrawBeams = r_DrawBeams.GetInt();
  r_DrawBeams.SetValue(0);

  bool bSaveMatSpecular = mat_fastspecular.GetBool();
  building_cubemaps.SetValue(1);

  ConVarRef r_portalsopenall("r_portalsopenall");
  if (r_portalsopenall.IsValid()) {
    r_portalsopenall.SetValue(1);
  }

  int nOldOcclusionVal = 1;
  ConVarRef r_occlusion("r_occlusion");
  if (r_occlusion.IsValid()) {
    nOldOcclusionVal = r_occlusion.GetInt();
    r_occlusion.SetValue(0);
  }

  ConVarRef mat_disable_bloom("mat_disable_bloom");
  int nOldBloomDisable = 0;
  if (mat_disable_bloom.IsValid()) {
    nOldBloomDisable = mat_disable_bloom.GetInt();
    mat_disable_bloom.SetValue(1);
  }

  int bounce;
  for (bounce = 0; bounce < iterations_num; bounce++) {
    if (bounce == 0) {
      mat_fastspecular.SetValue("0");
    } else {
      mat_fastspecular.SetValue("1");
    }
    UpdateMaterialSystemConfig();

    ch mapName[256];
    IClientEntity *world = entitylist->GetClientEntity(0);

    if (world && world->GetModel()) {
      const model_t *pModel = world->GetModel();
      const ch *pModelName = modelloader->GetName(pModel);

      // This handles the case where you have a map in a directory under maps.
      // We need to keep everything after "maps/" so it looks for the BSP file
      // in the right place.
      if (Q_stristr(pModelName, "maps/") == pModelName ||
          Q_stristr(pModelName, "maps\\") == pModelName) {
        Q_strncpy(mapName, &pModelName[5], sizeof(mapName));
        Q_StripExtension(mapName, mapName, sizeof(mapName));
      } else {
        Q_FileBase(pModelName, mapName, sizeof(mapName));
      }
    } else {
      ConDMsg("R_BuildCubemapSamples: No map loaded!\n");
      return;
    }

    int oldDrawMRMModelsVal = 1;
    ConVarRef drawMRMModelsCVar("r_drawothermodels");
    if (drawMRMModelsCVar.IsValid()) {
      oldDrawMRMModelsVal = drawMRMModelsCVar.GetInt();
      drawMRMModelsCVar.SetValue(0);
    }

    bool bOldLightSpritesActive = ActivateLightSprites(true);

    // load the vtex dll
    CSysModule *pModule;
    IVTex *ivt = VTex_Load(&pModule);
    if (!ivt) return;

    ch matDir[SOURCE_MAX_PATH];
    Q_snprintf(matDir, sizeof(matDir), "materials/maps/%s", mapName);
    g_pFileSystem->CreateDirHierarchy(matDir, "DEFAULT_WRITE_PATH");

    ch pTemp[SOURCE_MAX_PATH];
    Q_snprintf(pTemp, sizeof(pTemp), "materialsrc/maps/%s", mapName);

    ch pMaterialSrcDir[SOURCE_MAX_PATH];
    GetModContentSubdirectory(pTemp, pMaterialSrcDir, sizeof(pMaterialSrcDir));

    g_pFileSystem->CreateDirHierarchy(pMaterialSrcDir, NULL);

    ch gameDir[MAX_OSPATH];
    COM_GetGameDir(gameDir, sizeof(gameDir));

    model_t *pWorldModel = (model_t *)world->GetModel();
    int i;
    for (i = 0; i < pWorldModel->brush.pShared->m_nCubemapSamples; i++) {
      mcubemapsample_t *pCubemapSample =
          &pWorldModel->brush.pShared->m_pCubemapSamples[i];

      int tgaSize = (pCubemapSample->size == 0)
                        ? mat_envmaptgasize.GetInt()
                        : 1 << (pCubemapSample->size - 1);
      int screenBufSize = 4 * tgaSize;
      if ((screenBufSize > videomode->GetModeWidth()) ||
          (screenBufSize > videomode->GetModeHeight())) {
        Warning(
            "Cube map buffer size %d x %d is bigger than screen!\nRun at a "
            "higher resolution! or reduce your cubemap resolution (needs 4X)\n",
            screenBufSize, screenBufSize);
        // BUGBUG: We'll leak DLLs/handles if we break out here, but this should
        // be infrequent.
        return;
      }
    }

    bool bSupportsHDR =
        g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE;

    for (i = 0; i < pWorldModel->brush.pShared->m_nCubemapSamples; i++) {
      Warning("bounce: %d/%d sample: %d/%d\n", bounce + 1, iterations_num,
              i + 1, pWorldModel->brush.pShared->m_nCubemapSamples);
      mcubemapsample_t *pCubemapSample =
          &pWorldModel->brush.pShared->m_pCubemapSamples[i];

      ch pVTFName[SOURCE_MAX_PATH];
      Q_snprintf(pVTFName, sizeof(pVTFName), "%s/c%d_%d_%d", pMaterialSrcDir,
                 (int)pCubemapSample->origin[0], (int)pCubemapSample->origin[1],
                 (int)pCubemapSample->origin[2]);

      int nTgaSize = (pCubemapSample->size == 0)
                         ? mat_envmaptgasize.GetInt()
                         : 1 << (pCubemapSample->size - 1);
      BuildSingleCubemap(pVTFName, pCubemapSample->origin, nTgaSize,
                         bSupportsHDR, gameDir, ivt);
    }

    if (drawMRMModelsCVar.IsValid()) {
      drawMRMModelsCVar.SetValue(oldDrawMRMModelsVal);
    }

    ActivateLightSprites(bOldLightSpritesActive);

    VTex_Unload(pModule);

    // load the bsppack dll
    IBSPPack *iBSPPack = NULL;
    pModule = FileSystem_LoadModule("bsppack");
    if (pModule) {
      CreateInterfaceFn factory = Sys_GetFactory(pModule);
      if (factory) {
        iBSPPack = (IBSPPack *)factory(IBSPPACK_VERSION_STRING, NULL);
      }
    }

    if (!iBSPPack) {
      ConMsg("Can't load bsppack.dll\n");
      return;
    }

    iBSPPack->SetHDRMode(g_pMaterialSystemHardwareConfig->GetHDRType() !=
                         HDR_TYPE_NONE);

    ch mapPath[1024];
    sprintf_s(mapPath, "maps/%s.bsp", mapName);
    iBSPPack->LoadBSPFile(g_pFileSystem, mapPath);

    // Cram the textures into the bsp.
    sprintf_s(matDir, "materials/maps/%s", mapName);
    for (i = 0; i < pWorldModel->brush.pShared->m_nCubemapSamples; i++) {
      mcubemapsample_t *pSample =
          &pWorldModel->brush.pShared->m_pCubemapSamples[i];
      AddSampleToBSPFile(bSupportsHDR, pSample, matDir, iBSPPack);
    }
    Cubemap_CreateDefaultCubemap(mapName, iBSPPack);

    iBSPPack->WriteBSPFile(mapPath);
    iBSPPack->ClearPackFile();
    FileSystem_UnloadModule(pModule);

    Cbuf_AddText("restart setpos\n");
  }

  // re-enable the mouse.
  if (cl_mouseenable.IsValid()) {
    cl_mouseenable.SetValue(1);
  }
  if (r_shadows.IsValid()) {
    r_shadows.SetValue(saveShadows);
  }
  if (mat_drawwater.IsValid()) {
    mat_drawwater.SetValue(bDrawWater);
  }
  if (bSaveMatSpecular) {
    mat_fastspecular.SetValue("1");
  } else {
    mat_fastspecular.SetValue("0");
  }

  if (r_lightstyle.IsValid()) {
    r_lightstyle.SetValue(nSaveLightStyle);
    R_RedownloadAllLightmaps();
  }

  if (r_portalsopenall.IsValid()) r_portalsopenall.SetValue(0);

  if (r_occlusion.IsValid()) {
    r_occlusion.SetValue(nOldOcclusionVal);
  }

  if (mat_disable_bloom.IsValid()) {
    mat_disable_bloom.SetValue(nOldBloomDisable);
  }

  r_DrawBeams.SetValue(bSaveDrawBeams);

  building_cubemaps.SetValue(0);
  UpdateMaterialSystemConfig();

  // after map reloads, run any state that had to wait for map to reload
  reload_materials.SetValue(1);
}

#if !defined(SWDS) && !defined(_X360)
CON_COMMAND(buildcubemaps, "Rebuild cubemaps.") {
  if (args.ArgC() == 1) {
    R_BuildCubemapSamples(1);
  } else if (args.ArgC() == 2) {
    R_BuildCubemapSamples(atoi(args[1]));
  } else {
    ConMsg("Usage: buildcubemaps [numBounces]\n");
  }
}
#endif  // SWDS

#endif
