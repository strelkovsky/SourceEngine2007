// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef DEBUGCONSOLE_INTERFACE_H
#define DEBUGCONSOLE_INTERFACE_H

#include "interface.h"

// Purpose: Debugging console interface
the_interface IDebugConsole : public IBaseInterface {
 public:
  virtual void Initialize(const char *consoleName, int logLevel) = 0;
  virtual void Print(int severity, const char *msgDescriptor, ...) = 0;

  // gets a line of command input
  // returns true if anything returned, false otherwise
  virtual bool GetInput(char *buffer, int bufferSize) = 0;
};

#define DEBUGCONSOLE_INTERFACE_VERSION "DebugConsole001"

#endif  // DEBUGCONSOLE_INTERFACE_H
