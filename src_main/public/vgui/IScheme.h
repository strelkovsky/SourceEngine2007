// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_VGUI_ISCHEME_H_
#define SOURCE_VGUI_ISCHEME_H_

#ifdef _WIN32
#pragma once
#endif

#include "tier1/interface.h"
#include "base/include/compiler_specific.h"
#include "vgui/VGUI.h"

class Color;

namespace vgui {
using HScheme = unsigned long;
using HTexture = unsigned long;

class IBorder;
class IImage;

// Purpose: Holds all panel rendering data
// This functionality is all wrapped in the Panel::GetScheme*() functions
the_interface IScheme : public IBaseInterface {
 public:
  // gets a string from the default settings section
  virtual const char *GetResourceString(const char *stringName) = 0;

  // returns a pointer to an existing border
  virtual IBorder *GetBorder(const char *borderName) = 0;

  // returns a pointer to an existing font
  virtual HFont GetFont(const char *fontName, bool proportional = false) = 0;

  // inverse font lookup
  virtual char const *GetFontName(const HFont &font) = 0;

  // colors
  virtual Color GetColor(const char *colorName, Color defaultColor) = 0;
};

the_interface ISchemeManager : public IBaseInterface {
 public:
  // loads a scheme from a file
  // first scheme loaded becomes the default scheme, and all subsequent loaded
  // scheme are derivitives of that
  virtual HScheme LoadSchemeFromFile(const char *fileName, const char *tag) = 0;

  // reloads the scheme from the file - should only be used during development
  virtual void ReloadSchemes() = 0;

  // reloads scheme fonts
  virtual void ReloadFonts() = 0;

  // returns a handle to the default (first loaded) scheme
  virtual HScheme GetDefaultScheme() = 0;

  // returns a handle to the scheme identified by "tag"
  virtual HScheme GetScheme(const char *tag) = 0;

  // returns a pointer to an image
  virtual IImage *GetImage(const char *imageName, bool hardwareFiltered) = 0;
  virtual HTexture GetImageID(const char *imageName, bool hardwareFiltered) = 0;

  // This can only be called at certain times, like during paint()
  // It will assert-fail if you call it at the wrong time...

  // TODO(d.rattman): This interface should go away!!! It's an icky back-door
  // If you're using this interface, try instead to cache off the information
  // in ApplySchemeSettings
  virtual IScheme *GetIScheme(HScheme scheme) = 0;

  // unload all schemes
  virtual void Shutdown(bool full = true) = 0;

  // gets the proportional coordinates for doing screen-size independant panel
  // layouts use these for font, image and panel size scaling (they all use the
  // pixel height of the display for scaling)
  virtual int GetProportionalScaledValue(int normalizedValue) = 0;
  virtual int GetProportionalNormalizedValue(int scaledValue) = 0;

  // loads a scheme from a file
  // first scheme loaded becomes the default scheme, and all subsequent loaded
  // scheme are derivitives of that
  virtual HScheme LoadSchemeFromFileEx(VPANEL sizingPanel, const char *fileName,
                                       const char *tag) = 0;
  // gets the proportional coordinates for doing screen-size independant panel
  // layouts use these for font, image and panel size scaling (they all use the
  // pixel height of the display for scaling)
  virtual int GetProportionalScaledValueEx(HScheme scheme,
                                           int normalizedValue) = 0;
  virtual int GetProportionalNormalizedValueEx(HScheme scheme,
                                               int scaledValue) = 0;
};
}  // namespace vgui

#define VGUI_SCHEME_INTERFACE_VERSION "VGUI_Scheme010"

#endif  // SOURCE_VGUI_ISCHEME_H_
