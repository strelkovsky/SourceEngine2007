// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#ifndef UTIL_REGISTRY_H
#define UTIL_REGISTRY_H

#include "tier0/include/platform.h"

// Purpose: Interface to registry
the_interface IRegistry {
 public:
  virtual ~IRegistry();

  // Init/shutdown
  virtual bool Init(const char *platformName) = 0;
  virtual void Shutdown(void) = 0;

  // Read/write integers
  virtual int ReadInt(const char *key, int defaultValue = 0) = 0;
  virtual void WriteInt(const char *key, int value) = 0;

  // Read/write strings
  virtual const char *ReadString(const char *key,
                                 const char *defaultValue = 0) = 0;
  virtual void WriteString(const char *key, const char *value) = 0;

  // Read/write helper methods
  virtual int ReadInt(const char *pKeyBase, const char *pKey,
                      int defaultValue = 0) = 0;
  virtual void WriteInt(const char *pKeyBase, const char *key, int value) = 0;
  virtual const char *ReadString(const char *pKeyBase, const char *key,
                                 const char *defaultValue) = 0;
  virtual void WriteString(const char *pKeyBase, const char *key,
                           const char *value) = 0;
};

extern IRegistry *registry;

// Creates it and calls Init
IRegistry *InstanceRegistry(char const *subDirectoryUnderValve);
// Calls Shutdown and deletes it
void ReleaseInstancedRegistry(IRegistry *reg);

#endif  // UTIL_REGISTRY_H
