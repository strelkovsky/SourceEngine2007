// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Shared code for parsing / searching for characters in a string
// using lookup tables.

#ifndef SOURCE_TIER1_CHARACTERSET_H_
#define SOURCE_TIER1_CHARACTERSET_H_

struct characterset_t {
  char set[256];
};

// This is essentially a strpbrk() using a precalculated lookup table

// Purpose: builds a simple lookup table of a group of important characters
// Input  : *pSetBuffer - pointer to the buffer for the group
// *pSetString - list of characters to flag

extern void CharacterSetBuild(characterset_t *pSetBuffer,
                              const char *pSetString);


// Purpose:
// Input  : *pSetBuffer - pre-build group buffer
// character - character to lookup
// Output : int - 1 if the character was in the set

#define IN_CHARACTERSET(SetBuffer, character) ((SetBuffer).set[(character)])

#endif  // SOURCE_TIER1_CHARACTERSET_H_
