// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_VGUI2_VGUI_SURFACE_LIB_FONTAMALGAM_H_
#define SOURCE_VGUI2_VGUI_SURFACE_LIB_FONTAMALGAM_H_

#include "BitmapFont.h"
#include "tier1/UtlVector.h"
#include "Win32Font.h"

// Purpose: object that holds a set of fonts in specified ranges
class CFontAmalgam {
 public:
  CFontAmalgam();
  ~CFontAmalgam();

  const char *Name();
  void SetName(const char *name);

  // adds a font to the amalgam
  void AddFont(CWin32Font *font, int lowRange, int highRange);

  // returns the font for the given character
  CWin32Font *GetFontForChar(int ch);

  // returns the max height of the font set
  int GetFontHeight();

  // returns the maximum width of a character in a font
  int GetFontMaxWidth();

  // returns the flags used to make the first font
  int GetFlags(int i);

  // returns the windows name for the font
  const char *GetFontName(int i);

  // returns the number of fonts in this amalgam
  int GetCount();

  // returns true if this font is underlined
  bool GetUnderlined();

  // sets the scale of a bitmap font
  void SetFontScale(float sx, float sy);

  // clears the fonts
  void RemoveAll();

 private:
  struct TFontRange {
    int lowRange;
    int highRange;
    CWin32Font *font;
  };

  CUtlVector<TFontRange> m_Fonts;
  char m_szName[32];
  int m_iMaxWidth;
  int m_iMaxHeight;
};

#endif  // SOURCE_VGUI2_VGUI_SURFACE_LIB_FONTAMALGAM_H_
