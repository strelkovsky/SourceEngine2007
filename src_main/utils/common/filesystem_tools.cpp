// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "filesystem_tools.h"

#ifdef OS_WIN
#include <direct.h>
#include <io.h>  // _chmod

#include "base/include/windows/windows_light.h"
#elif OS_POSIX
#include <unistd.h>
#endif

#include <sys/stat.h>
#include <cstdio>
#include "tier0/include/icommandline.h"
#include "tier1/keyvalues.h"
#include "tier1/strtools.h"
#include "tier2/tier2.h"

#ifdef MPI
#include "vmpi.h"
#include "vmpi_filesystem.h"
#include "vmpi_tools_shared.h"
#endif

#include "tier0/include/memdbgon.h"

IBaseFileSystem *g_pFileSystem = nullptr;

// These are only used for tools that need the search paths that the engine's
// file system provides.
CSysModule *g_pFullFileSystemModule = nullptr;

// These are the base paths that everything will be referenced relative to
// (textures especially) All of these directories include the trailing slash

// This is the the path of the initial source file (relative to the cwd)
char qdir[SOURCE_MAX_PATH];

// This is the base engine + mod-specific game dir (e.g. "c:\tf2\mytfmod\")
char gamedir[SOURCE_MAX_PATH];

void FileSystem_SetupStandardDirectories(const char *pFilename,
                                         const char *pGameInfoPath) {
  // Set qdir.
  if (!pFilename) {
    pFilename = ".";
  }

  Q_MakeAbsolutePath(qdir, std::size(qdir), pFilename, nullptr);
  Q_StripFilename(qdir);
  Q_strlower(qdir);
  if (qdir[0] != '\0') {
    Q_AppendSlash(qdir, std::size(qdir));
  }

  // Set gamedir.
  Q_MakeAbsolutePath(gamedir, std::size(gamedir), pGameInfoPath);
  Q_AppendSlash(gamedir, std::size(gamedir));
}

bool FileSystem_Init_Normal(const char *pFilename, FSInitType_t initType,
                            bool bOnlyUseDirectoryName) {
  if (initType == FS_INIT_FULL) {
    // First, get the name of the module
    char fileSystemDLLName[SOURCE_MAX_PATH];
    bool bSteam;
    if (FileSystem_GetFileSystemDLLName(
            fileSystemDLLName, std::size(fileSystemDLLName), bSteam) != FS_OK)
      return false;

    // If we're under Steam we need extra setup to let us find the proper
    // modules
    FileSystem_SetupSteamInstallPath();

    // Next, load the module, call Connect/Init.
    CFSLoadModuleInfo loadModuleInfo;
    loadModuleInfo.m_pFileSystemDLLName = fileSystemDLLName;
    loadModuleInfo.m_pDirectoryName = pFilename;
    loadModuleInfo.m_bOnlyUseDirectoryName = bOnlyUseDirectoryName;
    loadModuleInfo.m_ConnectFactory = Sys_GetFactoryThis();
    loadModuleInfo.m_bSteam = bSteam;
    loadModuleInfo.m_bToolsMode = true;
    if (FileSystem_LoadFileSystemModule(loadModuleInfo) != FS_OK) return false;

    // Next, mount the content
    CFSMountContentInfo mountContentInfo;
    mountContentInfo.m_pDirectoryName = loadModuleInfo.m_GameInfoPath;
    mountContentInfo.m_pFileSystem = loadModuleInfo.m_pFileSystem;
    mountContentInfo.m_bToolsMode = true;
    if (FileSystem_MountContent(mountContentInfo) != FS_OK) return false;

    // Finally, load the search paths.
    CFSSearchPathsInit searchPathsInit;
    searchPathsInit.m_pDirectoryName = loadModuleInfo.m_GameInfoPath;
    searchPathsInit.m_pFileSystem = loadModuleInfo.m_pFileSystem;
    if (FileSystem_LoadSearchPaths(searchPathsInit) != FS_OK) return false;

    // Store the data we got from filesystem_init.
    g_pFileSystem = g_pFullFileSystem = loadModuleInfo.m_pFileSystem;
    g_pFullFileSystemModule = loadModuleInfo.m_pModule;

    FileSystem_AddSearchPath_Platform(g_pFullFileSystem,
                                      loadModuleInfo.m_GameInfoPath);

    FileSystem_SetupStandardDirectories(pFilename,
                                        loadModuleInfo.m_GameInfoPath);
  } else {
    if (!Sys_LoadInterface("filesystem_stdio", FILESYSTEM_INTERFACE_VERSION,
                           &g_pFullFileSystemModule,
                           (void **)&g_pFullFileSystem)) {
      return false;
    }

    if (g_pFullFileSystem->Init() != INIT_OK) return false;

    g_pFullFileSystem->RemoveAllSearchPaths();
    g_pFullFileSystem->AddSearchPath("../platform", "PLATFORM");
    g_pFullFileSystem->AddSearchPath(".", "GAME");

    g_pFileSystem = g_pFullFileSystem;
  }

  return true;
}

bool FileSystem_Init(const char *pBSPFilename, int maxMemoryUsage,
                     FSInitType_t initType, bool bOnlyUseFilename) {
  // Should have called CreateCmdLine by now.
  Assert(!!CommandLine()->GetCmdLine());

  // If this app uses VMPI, then let VMPI intercept all filesystem calls.
#ifdef MPI
  if (g_bUseMPI) {
    if (g_bMPIMaster) {
      if (!FileSystem_Init_Normal(pBSPFilename, initType, bOnlyUseFilename))
        return false;

      g_pFileSystem = g_pFullFileSystem =
          VMPI_FileSystem_Init(maxMemoryUsage, g_pFullFileSystem);
      SendQDirInfo();
    } else {
      g_pFileSystem = g_pFullFileSystem =
          VMPI_FileSystem_Init(maxMemoryUsage, nullptr);
      RecvQDirInfo();
    }
    return true;
  }
#endif

  return FileSystem_Init_Normal(pBSPFilename, initType, bOnlyUseFilename);
}

void FileSystem_Term() {
#ifdef MPI
  if (g_bUseMPI) {
    g_pFileSystem = g_pFullFileSystem = VMPI_FileSystem_Term();
  }
#endif

  if (g_pFullFileSystem) {
    g_pFullFileSystem->Shutdown();
    g_pFullFileSystem = nullptr;
    g_pFileSystem = nullptr;
  }

  if (g_pFullFileSystemModule) {
    Sys_UnloadModule(g_pFullFileSystemModule);
    g_pFullFileSystemModule = nullptr;
  }
}

CreateInterfaceFn FileSystem_GetFactory() {
#ifdef MPI
  if (g_bUseMPI) return VMPI_FileSystem_GetFactory();
#endif
  return Sys_GetFactory(g_pFullFileSystemModule);
}

bool FileSystem_SetGame(const char *szModDir) {
  g_pFullFileSystem->RemoveAllSearchPaths();
  if (FileSystem_SetBasePaths(g_pFullFileSystem) != FS_OK) return false;

  CFSSearchPathsInit fsInit;
  fsInit.m_pDirectoryName = szModDir;
  fsInit.m_pFileSystem = g_pFullFileSystem;

  return FileSystem_LoadSearchPaths(fsInit) == FS_OK;
}
