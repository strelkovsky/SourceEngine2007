// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#include <conio.h>
#include <direct.h>
#include <stdio.h>
#include "base/include/windows/windows_light.h"

#include "ilaunchabledll.h"
#include "tier0/include/icommandline.h"
#include "tier1/interface.h"
#include "tier1/strtools.h"

char *GetLastErrorString() {
  static char err[2048];

  LPVOID lpMsgBuf;
  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, GetLastError(),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // Default language
                (LPTSTR)&lpMsgBuf, 0, NULL);

  strncpy(err, (char *)lpMsgBuf, sizeof(err));
  LocalFree(lpMsgBuf);

  err[sizeof(err) - 1] = 0;

  return err;
}

void MakeFullPath(const char *pIn, char *pOut, int outLen) {
  if (pIn[0] == '/' || pIn[0] == '\\' || pIn[1] == ':') {
    // It's already a full path.
    Q_strncpy(pOut, pIn, outLen);
  } else {
    _getcwd(pOut, outLen);
    Q_strncat(pOut, "\\", outLen, COPY_ALL_CHARACTERS);
    Q_strncat(pOut, pIn, outLen, COPY_ALL_CHARACTERS);
  }
}

void Pause() {
  //	printf( "Pause\n" );
  //	fflush( stdout );
  //	getch();
}

int main(int argc, char *argv[]) {
  char dllName[512];

  Pause();
  CommandLine()->CreateCmdLine(argc, argv);

  char fullPath[512], redirectFilename[512];
  MakeFullPath(argv[0], fullPath, sizeof(fullPath));
  Q_StripFilename(fullPath);
  Q_snprintf(redirectFilename, sizeof(redirectFilename), "%s\\%s", fullPath,
             "texturecompile.redirect");

  Pause();
  // First, look for vrad.redirect and load the dll specified in there if
  // possible.
  CSysModule *pModule = NULL;
  FILE *fp = fopen(redirectFilename, "rt");
  if (fp) {
    if (fgets(dllName, sizeof(dllName), fp)) {
      char *pEnd = strstr(dllName, "\n");
      if (pEnd) *pEnd = 0;

      pModule = Sys_LoadModule(dllName);
      if (pModule)
        printf(
            "Loaded alternate texturecompile DLL (%s) specified in "
            "texturecompile.redirect.\n",
            dllName);
      else
        printf("Can't find '%s' specified in vrad.redirect.\n", dllName);
    }

    fclose(fp);
  }
  Pause();

  // If it didn't load the module above, then use the
  if (!pModule) {
    strcpy(dllName, "texturecompile_dll.dll");
    pModule = Sys_LoadModule(dllName);
  }

  Pause();
  if (!pModule) {
    printf("texturecompile_launcher error: can't load %s\n%s", dllName,
           GetLastErrorString());
    Pause();
    return 1;
  }

  Pause();
  CreateInterfaceFn fn = Sys_GetFactory(pModule);
  if (!fn) {
    printf(
        "vrad_launcher error: can't get factory from texturecompile_dll.dll\n");
    Sys_UnloadModule(pModule);
    return 2;
  }

  int retCode = 0;
  ILaunchableDLL *pDLL =
      (ILaunchableDLL *)fn(LAUNCHABLE_DLL_INTERFACE_VERSION, &retCode);
  if (!pDLL) {
    printf(
        "texturecompile_launcher error: can't get ITextureCompileDLL interface "
        "from vrad_dll.dll\n");
    Sys_UnloadModule(pModule);
    return 3;
  }

  int returnValue = pDLL->main(argc, argv);
  Sys_UnloadModule(pModule);

  return returnValue;
}
