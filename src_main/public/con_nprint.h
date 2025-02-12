// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Debug overlay / notfication printing

#ifndef CON_NPRINT_H
#define CON_NPRINT_H

// Debug overlay / notfication printing
typedef struct con_nprint_s {
  int index;           // Row #
  float time_to_live;  // # of seconds before it disappears. -1 means to display
                       // for 1 frame then go away.
  float color[3];      // RGB colors ( 0.0 -> 1.0 scale )
  bool fixed_width_font;
} con_nprint_t;

// Print string on line idx
void Con_NPrintf(int idx, const char *fmt, ...);
// Customized printout
void Con_NXPrintf(const con_nprint_t *info, const char *fmt, ...);

#endif  // CON_NPRINT_H
