// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "bitmap/float_bm.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "tier0/include/platform.h"
#include "tier1/strtools.h"
#include "vstdlib/random.h"
#include "vstdlib/vstdlib.h"

void FloatBitMap_t::InitializeWithRandomPixelsFromAnotherFloatBM(
    FloatBitMap_t const &other) {
  for (int y = 0; y < Height; y++)
    for (int x = 0; x < Width; x++) {
      f32 x1 = RandomInt(0, other.Width - 1);
      f32 y1 = RandomInt(0, other.Height - 1);
      for (int c = 0; c < 4; c++) {
        Pixel(x, y, c) = other.Pixel(x1, y1, c);
      }
    }
}

FloatBitMap_t *FloatBitMap_t::QuarterSizeWithGaussian() const {
  // generate a new bitmap half on each axis, using a separable gaussian.
  static f32 kernel[] = {.05f, .25f, .4f, .25f, .05f};
  FloatBitMap_t *newbm = new FloatBitMap_t(Width / 2, Height / 2);

  for (int y = 0; y < Height / 2; y++)
    for (int x = 0; x < Width / 2; x++) {
      for (int c = 0; c < 4; c++) {
        f32 sum = 0;
        f32 sumweights = 0;  // for versatility in handling the
                             // offscreen case
        for (int xofs = -2; xofs <= 2; xofs++) {
          int orig_x = std::max(0, std::min(Width - 1, x * 2 + xofs));
          for (int yofs = -2; yofs <= 2; yofs++) {
            int orig_y = std::max(0, std::min(Height - 1, y * 2 + yofs));
            f32 coeff = kernel[xofs + 2] * kernel[yofs + 2];
            sum += Pixel(orig_x, orig_y, c) * coeff;
            sumweights += coeff;
          }
        }
        newbm->Pixel(x, y, c) = sum / sumweights;
      }
    }

  return newbm;
}

FloatImagePyramid_t::FloatImagePyramid_t(FloatBitMap_t const &src,
                                         ImagePyramidMode_t mode) {
  memset(m_pLevels, 0, sizeof(m_pLevels));
  m_nLevels = 1;
  m_pLevels[0] = new FloatBitMap_t(&src);
  ReconstructLowerResolutionLevels(0);
}

void FloatImagePyramid_t::ReconstructLowerResolutionLevels(int start_level) {
  while ((m_pLevels[start_level]->Width > 1) &&
         (m_pLevels[start_level]->Height > 1)) {
    delete m_pLevels[start_level + 1];

    m_pLevels[start_level + 1] =
        m_pLevels[start_level]->QuarterSizeWithGaussian();
    start_level++;
  }
  m_nLevels = start_level + 1;
}

f32 &FloatImagePyramid_t::Pixel(int x, int y, int component, int level) const {
  assert(level < m_nLevels);
  x <<= level;
  y <<= level;
  return m_pLevels[level]->Pixel(x, y, component);
}

void FloatImagePyramid_t::WriteTGAs(ch const *basename) const {
  for (int l = 0; l < m_nLevels; l++) {
    ch bname_out[1024];
    sprintf_s(bname_out, "%s_%02d.tga", basename, l);
    m_pLevels[l]->WriteTGAFile(bname_out);
  }
}

FloatImagePyramid_t::~FloatImagePyramid_t() {
  for (int l = 0; l < m_nLevels; l++) {
    delete m_pLevels[l];
  }
}
