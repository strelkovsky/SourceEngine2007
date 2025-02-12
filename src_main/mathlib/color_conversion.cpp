// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Color conversion routines.

#include "mathlib/mathlib.h"

#include <memory.h>
#include <algorithm>
#include <cfloat>  // Needed for FLT_EPSILON
#include <cmath>
#include "mathlib/vector.h"
#include "tier0/include/basetypes.h"
#include "tier0/include/dbg.h"

#include "tier0/include/memdbgon.h"

// Gamma conversion support

// palette is sent through this to convert to screen gamma
static u8 texgammatable[256];

static f32 texturetolinear[256];  // texture (0..255) to linear (0..1)
static u8 lineartotexture[1024];  // linear (0..1) to texture (0..255)
static u8 lineartoscreen[1024];   // linear (0..1) to gamma corrected vertex
                                  // light (0..255)

// build a lightmap texture to combine with surface texture, adjust for
// src*dst+dst*src, ramp reprogramming, etc
f32 lineartovertex[4096];   // linear (0..4) to screen corrected vertex space
                            // (0..1?)
u8 lineartolightmap[4096];  // linear (0..4) to screen corrected
                            // texture value (0..255)

static f32 g_Mathlib_GammaToLinear[256];  // gamma (0..1) to linear (0..1)
static f32 g_Mathlib_LinearToGamma[256];  // linear (0..1) to gamma (0..1)

// This is aligned to 16-byte boundaries so that we can load it
// onto SIMD registers easily if needed (used by SSE version of lightmaps)
// TODO: move this into the one DLL that actually uses it, instead of statically
// linking it everywhere via mathlib.
alignas(128) f32 power2_n[256] =  // 2**(index - 128) / 255
    {1.152445441982634800E-041f, 2.304890883965269600E-041f,
     4.609781767930539200E-041f, 9.219563535861078400E-041f,
     1.843912707172215700E-040f, 3.687825414344431300E-040f,
     7.375650828688862700E-040f, 1.475130165737772500E-039f,
     2.950260331475545100E-039f, 5.900520662951090200E-039f,
     1.180104132590218000E-038f, 2.360208265180436100E-038f,
     4.720416530360872100E-038f, 9.440833060721744200E-038f,
     1.888166612144348800E-037f, 3.776333224288697700E-037f,
     7.552666448577395400E-037f, 1.510533289715479100E-036f,
     3.021066579430958200E-036f, 6.042133158861916300E-036f,
     1.208426631772383300E-035f, 2.416853263544766500E-035f,
     4.833706527089533100E-035f, 9.667413054179066100E-035f,
     1.933482610835813200E-034f, 3.866965221671626400E-034f,
     7.733930443343252900E-034f, 1.546786088668650600E-033f,
     3.093572177337301200E-033f, 6.187144354674602300E-033f,
     1.237428870934920500E-032f, 2.474857741869840900E-032f,
     4.949715483739681800E-032f, 9.899430967479363700E-032f,
     1.979886193495872700E-031f, 3.959772386991745500E-031f,
     7.919544773983491000E-031f, 1.583908954796698200E-030f,
     3.167817909593396400E-030f, 6.335635819186792800E-030f,
     1.267127163837358600E-029f, 2.534254327674717100E-029f,
     5.068508655349434200E-029f, 1.013701731069886800E-028f,
     2.027403462139773700E-028f, 4.054806924279547400E-028f,
     8.109613848559094700E-028f, 1.621922769711818900E-027f,
     3.243845539423637900E-027f, 6.487691078847275800E-027f,
     1.297538215769455200E-026f, 2.595076431538910300E-026f,
     5.190152863077820600E-026f, 1.038030572615564100E-025f,
     2.076061145231128300E-025f, 4.152122290462256500E-025f,
     8.304244580924513000E-025f, 1.660848916184902600E-024f,
     3.321697832369805200E-024f, 6.643395664739610400E-024f,
     1.328679132947922100E-023f, 2.657358265895844200E-023f,
     5.314716531791688300E-023f, 1.062943306358337700E-022f,
     2.125886612716675300E-022f, 4.251773225433350700E-022f,
     8.503546450866701300E-022f, 1.700709290173340300E-021f,
     3.401418580346680500E-021f, 6.802837160693361100E-021f,
     1.360567432138672200E-020f, 2.721134864277344400E-020f,
     5.442269728554688800E-020f, 1.088453945710937800E-019f,
     2.176907891421875500E-019f, 4.353815782843751100E-019f,
     8.707631565687502200E-019f, 1.741526313137500400E-018f,
     3.483052626275000900E-018f, 6.966105252550001700E-018f,
     1.393221050510000300E-017f, 2.786442101020000700E-017f,
     5.572884202040001400E-017f, 1.114576840408000300E-016f,
     2.229153680816000600E-016f, 4.458307361632001100E-016f,
     8.916614723264002200E-016f, 1.783322944652800400E-015f,
     3.566645889305600900E-015f, 7.133291778611201800E-015f,
     1.426658355722240400E-014f, 2.853316711444480700E-014f,
     5.706633422888961400E-014f, 1.141326684577792300E-013f,
     2.282653369155584600E-013f, 4.565306738311169100E-013f,
     9.130613476622338300E-013f, 1.826122695324467700E-012f,
     3.652245390648935300E-012f, 7.304490781297870600E-012f,
     1.460898156259574100E-011f, 2.921796312519148200E-011f,
     5.843592625038296500E-011f, 1.168718525007659300E-010f,
     2.337437050015318600E-010f, 4.674874100030637200E-010f,
     9.349748200061274400E-010f, 1.869949640012254900E-009f,
     3.739899280024509800E-009f, 7.479798560049019500E-009f,
     1.495959712009803900E-008f, 2.991919424019607800E-008f,
     5.983838848039215600E-008f, 1.196767769607843100E-007f,
     2.393535539215686200E-007f, 4.787071078431372500E-007f,
     9.574142156862745000E-007f, 1.914828431372549000E-006f,
     3.829656862745098000E-006f, 7.659313725490196000E-006f,
     1.531862745098039200E-005f, 3.063725490196078400E-005f,
     6.127450980392156800E-005f, 1.225490196078431400E-004f,
     2.450980392156862700E-004f, 4.901960784313725400E-004f,
     9.803921568627450800E-004f, 1.960784313725490200E-003f,
     3.921568627450980300E-003f, 7.843137254901960700E-003f,
     1.568627450980392100E-002f, 3.137254901960784300E-002f,
     6.274509803921568500E-002f, 1.254901960784313700E-001f,
     2.509803921568627400E-001f, 5.019607843137254800E-001f,
     1.003921568627451000E+000f, 2.007843137254901900E+000f,
     4.015686274509803900E+000f, 8.031372549019607700E+000f,
     1.606274509803921500E+001f, 3.212549019607843100E+001f,
     6.425098039215686200E+001f, 1.285019607843137200E+002f,
     2.570039215686274500E+002f, 5.140078431372548900E+002f,
     1.028015686274509800E+003f, 2.056031372549019600E+003f,
     4.112062745098039200E+003f, 8.224125490196078300E+003f,
     1.644825098039215700E+004f, 3.289650196078431300E+004f,
     6.579300392156862700E+004f, 1.315860078431372500E+005f,
     2.631720156862745100E+005f, 5.263440313725490100E+005f,
     1.052688062745098000E+006f, 2.105376125490196000E+006f,
     4.210752250980392100E+006f, 8.421504501960784200E+006f,
     1.684300900392156800E+007f, 3.368601800784313700E+007f,
     6.737203601568627400E+007f, 1.347440720313725500E+008f,
     2.694881440627450900E+008f, 5.389762881254901900E+008f,
     1.077952576250980400E+009f, 2.155905152501960800E+009f,
     4.311810305003921500E+009f, 8.623620610007843000E+009f,
     1.724724122001568600E+010f, 3.449448244003137200E+010f,
     6.898896488006274400E+010f, 1.379779297601254900E+011f,
     2.759558595202509800E+011f, 5.519117190405019500E+011f,
     1.103823438081003900E+012f, 2.207646876162007800E+012f,
     4.415293752324015600E+012f, 8.830587504648031200E+012f,
     1.766117500929606200E+013f, 3.532235001859212500E+013f,
     7.064470003718425000E+013f, 1.412894000743685000E+014f,
     2.825788001487370000E+014f, 5.651576002974740000E+014f,
     1.130315200594948000E+015f, 2.260630401189896000E+015f,
     4.521260802379792000E+015f, 9.042521604759584000E+015f,
     1.808504320951916800E+016f, 3.617008641903833600E+016f,
     7.234017283807667200E+016f, 1.446803456761533400E+017f,
     2.893606913523066900E+017f, 5.787213827046133800E+017f,
     1.157442765409226800E+018f, 2.314885530818453500E+018f,
     4.629771061636907000E+018f, 9.259542123273814000E+018f,
     1.851908424654762800E+019f, 3.703816849309525600E+019f,
     7.407633698619051200E+019f, 1.481526739723810200E+020f,
     2.963053479447620500E+020f, 5.926106958895241000E+020f,
     1.185221391779048200E+021f, 2.370442783558096400E+021f,
     4.740885567116192800E+021f, 9.481771134232385600E+021f,
     1.896354226846477100E+022f, 3.792708453692954200E+022f,
     7.585416907385908400E+022f, 1.517083381477181700E+023f,
     3.034166762954363400E+023f, 6.068333525908726800E+023f,
     1.213666705181745400E+024f, 2.427333410363490700E+024f,
     4.854666820726981400E+024f, 9.709333641453962800E+024f,
     1.941866728290792600E+025f, 3.883733456581585100E+025f,
     7.767466913163170200E+025f, 1.553493382632634000E+026f,
     3.106986765265268100E+026f, 6.213973530530536200E+026f,
     1.242794706106107200E+027f, 2.485589412212214500E+027f,
     4.971178824424429000E+027f, 9.942357648848857900E+027f,
     1.988471529769771600E+028f, 3.976943059539543200E+028f,
     7.953886119079086300E+028f, 1.590777223815817300E+029f,
     3.181554447631634500E+029f, 6.363108895263269100E+029f,
     1.272621779052653800E+030f, 2.545243558105307600E+030f,
     5.090487116210615300E+030f, 1.018097423242123100E+031f,
     2.036194846484246100E+031f, 4.072389692968492200E+031f,
     8.144779385936984400E+031f, 1.628955877187396900E+032f,
     3.257911754374793800E+032f, 6.515823508749587500E+032f,
     1.303164701749917500E+033f, 2.606329403499835000E+033f,
     5.212658806999670000E+033f, 1.042531761399934000E+034f,
     2.085063522799868000E+034f, 4.170127045599736000E+034f,
     8.340254091199472000E+034f, 1.668050818239894400E+035f,
     3.336101636479788800E+035f, 6.672203272959577600E+035f};

static f32 s_gamma, s_texGamma, s_brightness;
static int s_overbright;

void BuildGammaTable(f32 gamma, f32 texGamma, f32 brightness, int overbright) {
  if (s_gamma == gamma && s_texGamma == texGamma &&
      s_brightness == brightness && s_overbright == overbright)
    return;

  f32 g{1.0f / (gamma <= 3.0f ? gamma : 3.0f)};
  f32 g1{texGamma * g};
  f32 g3;

  if (brightness <= 0.0f) {
    g3 = 0.125f;
  } else if (brightness > 1.0f) {
    g3 = 0.05f;
  } else {
    g3 = 0.125f - (brightness * brightness) * 0.075f;
  }

  for (usize i{0}; i < std::size(texgammatable); ++i) {
    texgammatable[i] =
        std::clamp(implicit_cast<u8>(255 * pow(i / 255.f, g1)), 0ui8, 255ui8);
  }

  for (usize i{0}; i < std::size(lineartoscreen); ++i) {
    f32 f{i / 1023.0f};

    // scale up
    if (brightness > 1.0f) f *= brightness;

    // shift up
    if (f <= g3)
      f = (f / g3) * 0.125f;
    else
      f = 0.125f + ((f - g3) / (1.0f - g3)) * 0.875f;

    // convert linear space to desired gamma space
    lineartoscreen[i] =
        std::clamp(implicit_cast<u8>(255 * pow(f, g)), 0ui8, 255ui8);
  }

  static_assert(std::size(texturetolinear) ==
                std::size(g_Mathlib_LinearToGamma));
  static_assert(std::size(g_Mathlib_LinearToGamma) ==
                std::size(g_Mathlib_GammaToLinear));

  for (usize i{0}; i < std::size(texturetolinear); ++i) {
    // convert from nonlinear texture space (0..255) to linear space (0..1)
    texturetolinear[i] = pow(i / 255.f, texGamma);

    // convert from linear space (0..1) to nonlinear (sRGB) space (0..1)
    g_Mathlib_LinearToGamma[i] = LinearToGammaFullRange(i / 255.f);

    // convert from sRGB gamma space (0..1) to linear space (0..1)
    g_Mathlib_GammaToLinear[i] = GammaToLinearFullRange(i / 255.f);
  }

  for (usize i{0}; i < std::size(lineartotexture); ++i) {
    // convert from linear space (0..1) to nonlinear texture space (0..255)
    lineartotexture[i] = pow(i / 1023.0f, 1.0f / texGamma) * 255;
  }

  // Can't do overbright without texcombine
  // UNDONE: Add GAMMA ramp to rectify this
  const f32 overbrightFactor{
      overbright == 2 ? 0.5f : ((overbright == 4) ? 0.25f : 1.0f)};

  static_assert(std::size(lineartovertex) == std::size(lineartolightmap));

  for (usize i{0}; i < std::size(lineartovertex); ++i) {
    // convert from linear 0..4 (x1024) to screen corrected vertex space
    // (0..1?)
    const f32 f{pow(i / 1024.0f, 1.0f / gamma)};

    lineartovertex[i] = f * overbrightFactor;

    if (lineartovertex[i] > 1) lineartovertex[i] = 1;

    lineartolightmap[i] = std::clamp(
        implicit_cast<u8>(RoundFloatToInt(f * 255 * overbrightFactor)), 0ui8,
        255ui8);
  }

  s_gamma = gamma;
  s_texGamma = texGamma;
  s_brightness = brightness;
  s_overbright = overbright;
}

f32 GammaToLinearFullRange(f32 gamma) { return pow(gamma, 2.2f); }

f32 LinearToGammaFullRange(f32 linear) { return pow(linear, 1.0f / 2.2f); }

f32 GammaToLinear(f32 gamma) {
  Assert(s_bMathlibInitialized);

  if (gamma < 0.0f) return 0.0f;
  if (gamma >= 0.95f) return 1.0f;

  const int index{RoundFloatToInt(gamma * 255.0f)};

  Assert(index >= 0 && index < 256);

  return g_Mathlib_GammaToLinear[index];
}

f32 LinearToGamma(f32 linear) {
  Assert(s_bMathlibInitialized);

  if (linear < 0.0f) return 0.0f;
  if (linear > 1.0f) {
    // Use LinearToGammaFullRange maybe if you trip this.
    Assert(0);
    return 1.0f;
  }

  const int index{RoundFloatToInt(linear * 255.0f)};

  Assert(index >= 0 && index < 256);

  return g_Mathlib_LinearToGamma[index];
}

// Helper functions to convert between sRGB and 360 gamma space

f32 SrgbGammaToLinear(f32 flSrgbGammaValue) {
  const f32 x{std::clamp(flSrgbGammaValue, 0.0f, 1.0f)};
  return (x <= 0.04045f) ? (x / 12.92f) : (pow((x + 0.055f) / 1.055f, 2.4f));
}

f32 SrgbLinearToGamma(f32 flLinearValue) {
  const f32 x{std::clamp(flLinearValue, 0.0f, 1.0f)};
  return (x <= 0.0031308f) ? (x * 12.92f)
                           : (1.055f * pow(x, (1.0f / 2.4f))) - 0.055f;
}

f32 X360GammaToLinear(f32 fl360GammaValue) {
  f32 flLinearValue;

  fl360GammaValue = std::clamp(fl360GammaValue, 0.0f, 1.0f);
  if (fl360GammaValue < (96.0f / 255.0f)) {
    if (fl360GammaValue < (64.0f / 255.0f)) {
      flLinearValue = fl360GammaValue * 255.0f;
    } else {
      flLinearValue = fl360GammaValue * (255.0f * 2.0f) - 64.0f;
      flLinearValue += floor(flLinearValue * (1.0f / 512.0f));
    }
  } else {
    if (fl360GammaValue < (192.0f / 255.0f)) {
      flLinearValue = fl360GammaValue * (255.0f * 4.0f) - 256.0f;
      flLinearValue += floor(flLinearValue * (1.0f / 256.0f));
    } else {
      flLinearValue = fl360GammaValue * (255.0f * 8.0f) - 1024.0f;
      flLinearValue += floor(flLinearValue * (1.0f / 128.0f));
    }
  }

  flLinearValue *= 1.0f / 1023.0f;
  flLinearValue = std::clamp(flLinearValue, 0.0f, 1.0f);

  return flLinearValue;
}

f32 X360LinearToGamma(f32 flLinearValue) {
  f32 fl360GammaValue;

  flLinearValue = std::clamp(flLinearValue, 0.0f, 1.0f);
  if (flLinearValue < (128.0f / 1023.0f)) {
    if (flLinearValue < (64.0f / 1023.0f)) {
      fl360GammaValue = flLinearValue * (1023.0f * (1.0f / 255.0f));
    } else {
      fl360GammaValue = flLinearValue * ((1023.0f / 2.0f) * (1.0f / 255.0f)) +
                        (32.0f / 255.0f);
    }
  } else {
    if (flLinearValue < (512.0f / 1023.0f)) {
      fl360GammaValue = flLinearValue * ((1023.0f / 4.0f) * (1.0f / 255.0f)) +
                        (64.0f / 255.0f);
    } else {
      fl360GammaValue =
          flLinearValue * ((1023.0f / 8.0f) * (1.0f / 255.0f)) +
          (128.0f / 255.0f);  // 1.0 -> 1.0034313725490196078431372549016
      if (fl360GammaValue > 1.0f) {
        fl360GammaValue = 1.0f;
      }
    }
  }

  fl360GammaValue = std::clamp(fl360GammaValue, 0.0f, 1.0f);

  return fl360GammaValue;
}

f32 SrgbGammaTo360Gamma(f32 flSrgbGammaValue) {
  const f32 flLinearValue{SrgbGammaToLinear(flSrgbGammaValue)};
  const f32 fl360GammaValue{X360LinearToGamma(flLinearValue)};
  return fl360GammaValue;
}

// convert texture to linear 0..1 value
f32 TextureToLinear(int c) {
  Assert(s_bMathlibInitialized);

  if (c < 0) return 0;
  if ((usize)c >= std::size(texturetolinear)) return 1.0;

  return texturetolinear[c];
}

// convert texture to linear 0..1 value
u8 LinearToTexture(f32 f) {
  Assert(s_bMathlibInitialized);
  Assert(f >= 0.0f && f <= 1.0f);

  const usize i{
      std::clamp(implicit_cast<usize>(f * (std::size(lineartotexture) - 1)),
                 implicit_cast<usize>(0), std::size(lineartotexture) - 1)};

  return lineartotexture[i];
}

// converts 0..1 linear value to screen gamma (0..255)
u8 LinearToScreenGamma(f32 f) {
  Assert(s_bMathlibInitialized);
  Assert(f >= 0.0f && f <= 1.0f);

  const usize i{
      std::clamp(implicit_cast<usize>(f * (std::size(lineartoscreen) - 1)),
                 implicit_cast<usize>(0), std::size(lineartoscreen) - 1)};

  return lineartoscreen[i];
}

void ColorRGBExp32ToVector(const ColorRGBExp32 &in, Vector &out) {
  Assert(s_bMathlibInitialized);

  // TODO(d.rattman): Why is there a factor of 255 built into this?
  out.x = linearToVectorFactor * TexLightToLinear(in.r, in.exponent);
  out.y = linearToVectorFactor * TexLightToLinear(in.g, in.exponent);
  out.z = linearToVectorFactor * TexLightToLinear(in.b, in.exponent);
}

// given a floating point number  f, return an exponent e such that
// for f' = f * 2^e,  f is on [128..255].
// Uses IEEE 754 representation to directly extract this information
// from the f32.
inline static int VectorToColorRGBExp32_CalcExponent(const f32 &in) {
  // The thing we will take advantage of here is that the exponent component
  // is stored in the f32 itself, and because we want to map to 128..255, we
  // want an "ideal" exponent of 2^7. So, we compute the difference between
  // the input exponent and 7 to work out the normalizing exponent. Thus if
  // you pass in 32 (represented in IEEE 754 as 2^5), this function will
  // return 2 (because 32 * 2^2 = 128)
  if (in == 0.0f) return 0;

  u32 fbits;
  // the exponent component is bits 23..30, and biased by +127
  const u32 biasedSeven{7 + 127};

  // now the difference from seven (positive if was less than, etc)
  return ((bitwise_copy(fbits, in) & 0x7F800000) >> 23) - biasedSeven;
}

// Slightly faster version of the function to turn a f32-vector color into
// a compressed-exponent notation 32bit color. However, still not SIMD
// optimized. PS3 developer: note there is a movement of a f32 onto an int
// here, which is bad on the base registers -- consider doing this as Altivec
// code, or better yet moving it onto the cell. \warning: Assumes an IEEE 754
// single-precision f32 representation! Those of you porting to an 8080 are
// out of luck.
void VectorToColorRGBExp32(const Vector &vin, ColorRGBExp32 &c) {
  Assert(s_bMathlibInitialized);
  Assert(vin.x >= 0.0f && vin.y >= 0.0f && vin.z >= 0.0f);

  // work out which of the channels is the largest (we will use that to map the
  // exponent).
  const f32 &the_max{VectorMaximum(vin)};

  // now work out the exponent for this luxel.
  const i32 exponent{VectorToColorRGBExp32_CalcExponent(the_max)};

  // make sure the exponent fits into a signed byte.
  // (in single precision format this is assured because it was a signed byte to
  // begin with)
  Assert(exponent > -128 && exponent <= 127);

  // promote the exponent back onto a scalar that we'll use to normalize all the
  // numbers
  u32 fbits{implicit_cast<u32>(127 - exponent) << 23u};
  f32 scalar;

  bitwise_copy(scalar, fbits);

  // we should never need to clamp:
  Assert(vin.x * scalar <= 255.0f && vin.y * scalar <= 255.0f &&
         vin.z * scalar <= 255.0f);

  c.r = vin.x * scalar;
  c.g = vin.y * scalar;
  c.b = vin.z * scalar;
  c.exponent = (signed char)exponent;
}
