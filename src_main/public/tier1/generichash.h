// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Variant Pearson Hash general purpose hashing algorithm described
// by Cargill in C++ Report 1994. Generates a 16-bit result.

#ifndef SOURCE_TIER1_GENERICHASH_H_
#define SOURCE_TIER1_GENERICHASH_H_

#include "base/include/compiler_specific.h"

unsigned SOURCE_FASTCALL HashString(const char *pszKey);
unsigned SOURCE_FASTCALL HashStringCaseless(const char *pszKey);
unsigned SOURCE_FASTCALL HashStringCaselessConventional(const char *pszKey);
unsigned SOURCE_FASTCALL Hash4(const void *pKey);
unsigned SOURCE_FASTCALL Hash8(const void *pKey);
unsigned SOURCE_FASTCALL Hash12(const void *pKey);
unsigned SOURCE_FASTCALL Hash16(const void *pKey);
unsigned SOURCE_FASTCALL HashBlock(const void *pKey, unsigned size);

unsigned SOURCE_FASTCALL HashInt(const int key);

inline unsigned HashIntConventional(const int n)  // faster but less effective
{
  // first byte
  unsigned hash = 0xAAAAAAAA + (n & 0xFF);
  // second byte
  hash = (hash << 5) + hash + ((n >> 8) & 0xFF);
  // third byte
  hash = (hash << 5) + hash + ((n >> 16) & 0xFF);
  // fourth byte
  hash = (hash << 5) + hash + ((n >> 24) & 0xFF);

  return hash;

  /* this is the old version, which would cause a load-hit-store on every
     line on a PowerPC, and therefore took hundreds of clocks to execute!
  byte *p = (byte *)&n;
  unsigned hash = 0xAAAAAAAA + *p++;
  hash = ( ( hash << 5 ) + hash ) + *p++;
  hash = ( ( hash << 5 ) + hash ) + *p++;
  return ( ( hash << 5 ) + hash ) + *p;
  */
}

template <typename T>
inline unsigned HashItem(const T &item) {
  // TODO: Confirm comiler optimizes out unused paths
  MSVC_BEGIN_WARNING_OVERRIDE_SCOPE()
  MSVC_DISABLE_WARNING(4127)
  if (sizeof(item) == 4) return Hash4(&item);

  if (sizeof(item) == 8) return Hash8(&item);

  if (sizeof(item) == 12) return Hash12(&item);

  if (sizeof(item) == 16) return Hash16(&item);
  MSVC_END_WARNING_OVERRIDE_SCOPE()
  return HashBlock(&item, sizeof(item));
}

template <>
inline unsigned HashItem<int>(const int &key) {
  return HashInt(key);
}

template <>
inline unsigned HashItem<unsigned>(const unsigned &key) {
  return HashInt((int)key);
}

template <>
inline unsigned HashItem<const char *>(const char *const &pszKey) {
  return HashString(pszKey);
}

template <>
inline unsigned HashItem<char *>(char *const &pszKey) {
  return HashString(pszKey);
}

#endif  // SOURCE_TIER1_GENERICHASH_H_
