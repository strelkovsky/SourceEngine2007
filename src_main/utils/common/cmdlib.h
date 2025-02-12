// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#ifndef CMDLIB_H
#define CMDLIB_H

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "filesystem.h"
#include "filesystem_tools.h"
#include "tier0/include/basetypes.h"
// This can go away when everything is in bin.
#if defined(CMDLIB_NODBGLIB)
void Error(char const *pMsg, ...);
#else
#include "tier0/include/dbg.h"
#endif

// Tools should use this as the read path ID. It'll look into the paths
// specified by gameinfo.txt
#define TOOLS_READ_PATH_ID "GAME"

// Tools should use this to fprintf data to files.
void CmdLib_FPrintf(FileHandle_t hFile, const char *pFormat, ...);
char *CmdLib_FGets(char *pOut, int outSize, FileHandle_t hFile);

// This can be set so Msg() sends output to hook functions (like the VMPI MySQL
// database), but doesn't actually printf the output.
extern bool g_bSuppressPrintfOutput;

extern IBaseFileSystem *g_pFileSystem;

// These call right into the functions in filesystem_tools.h
void CmdLib_InitFileSystem(const char *pFilename, int maxMemoryUsage = 0);
void CmdLib_TermFileSystem();  // GracefulExit calls this.
CreateInterfaceFn CmdLib_GetFileSystemFactory();

// the dec offsetof macro doesnt work very well...
#define myoffsetof(type, identifier) ((size_t) & ((type *)0)->identifier)

// set these before calling CheckParm
extern int myargc;
extern char **myargv;

int Q_filelength(FileHandle_t f);
int FileTime(char *path);

void Q_mkdir(char *path);

char *ExpandArg(char *path);   // expand relative to CWD
char *ExpandPath(char *path);  // expand relative to gamedir

char *ExpandPathAndArchive(char *path);

// Fills in pOut with "X hours, Y minutes, Z seconds". Leaves out hours or
// minutes if they're zero.
void GetHourMinuteSecondsString(int nInputSeconds, char *pOut, int outLen);

int CheckParm(char *check);

FileHandle_t SafeOpenWrite(const char *filename);
FileHandle_t SafeOpenRead(const char *filename);
void SafeRead(FileHandle_t f, void *buffer, int count);
void SafeWrite(FileHandle_t f, void *buffer, int count);

int LoadFile(const char *filename, void **bufferptr);
void SaveFile(const char *filename, void *buffer, int count);
bool FileExists(const char *filename);

int ParseNum(char *str);

// Do a printf in the specified color.
#define CP_ERROR stderr, 1, 0, 0, 1  // default colors..
#define CP_WARNING stderr, 1, 1, 0, 1
#define CP_STARTUP stdout, 0, 1, 1, 1
#define CP_NOTIFY stdout, 1, 1, 1, 1
void ColorPrintf(FILE *pFile, bool red, bool green, bool blue, bool intensity, char const *pFormat,
                 ...);

// Initialize spew output.
void InstallSpewFunction();

// This registers an extra callback for spew output.
typedef void (*SpewHookFn)(const char *);
void InstallExtraSpewHook(SpewHookFn pFn);

// Install allocation hooks so we error out if an allocation can't happen.
void InstallAllocationFunctions();

// This shuts down mgrs that use threads gracefully. If you just call exit(),
// the threads can get in a state where you can't tell if they are shutdown or
// not, and it can stall forever.
typedef void (*CleanupFn)();
void CmdLib_AtCleanup(CleanupFn pFn);  // register a callback when Cleanup() is called.
void CmdLib_Cleanup();
[[noreturn]] void CmdLib_Exit(int exitCode);  // Use this to cleanup and call exit().

// entrypoint if chaining spew functions
SpewRetval_t CmdLib_SpewOutputFunc(SpewType_t type, char const *pMsg);
unsigned short SetConsoleTextColor(int red, int green, int blue, int intensity);
void RestoreConsoleTextColor(unsigned short color);

// Append all spew output to the specified file.
void SetSpewFunctionLogFile(char const *pFilename);

char *COM_Parse(char *data);

extern char com_token[1024];

char *copystring(const char *s);

void CreatePath(char *path);
void QCopyFile(char *from, char *to);
void SafeCreatePath(char *path);

extern bool archive;
extern char archivedir[1024];

extern bool verbose;

void qprintf(const char *format, ...);

void ExpandWildcards(int *argc, char ***argv);

void CmdLib_AddBasePath(const char *pBasePath);
bool CmdLib_HasBasePath(const char *pFileName, int &pathLength);
int CmdLib_GetNumBasePaths(void);
const char *CmdLib_GetBasePath(int i);

extern bool g_bStopOnExit;

// for compression routines
typedef struct {
  uint8_t *data;
  int count;
} cblock_t;

#endif  // CMDLIB_H
