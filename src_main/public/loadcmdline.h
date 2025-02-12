// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: loads additional command line options from a config file/

#ifndef LOADCMDLINE_H
#define LOADCMDLINE_H

//-----------------------------------------------------------------------------
// Purpose: Loads additional commandline arguments from a config file for an
// app. keyname: Name of the block containing the key/args pairs (ie map or
// model name) appname: Keyname for the commandline arguments to be loaded -
// typically the exe name.
//-----------------------------------------------------------------------------
void LoadCmdLineFromFile(int &argc, char **&argv, const char *keyname,
                         const char *appname);

//-----------------------------------------------------------------------------
// Purpose: Cleans up any memory allocated for the new argv.  Pass in the app's
// argc and argv - this is safe even if no extra arguments were loaded.
//-----------------------------------------------------------------------------
void DeleteCmdLine(int argc, char **argv);

#endif  // LOADCMDLINE_H
