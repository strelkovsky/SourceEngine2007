// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#include "mathlib/quantize.h"

#include <algorithm>

constexpr inline usize N_EXTRAVALUES{1};
constexpr inline usize N_DIMENSIONS{3 + N_EXTRAVALUES};

constexpr inline u8 PIXEL(u8 const *image, int width, int x, int y, int c) {
  return image[4 * (x + width * y) + c];
}

static u8 Weights[] = {5, 7, 4, 8};
static int ExtraValueXForms[3 * N_EXTRAVALUES] = {76, 151, 28};

constexpr inline usize MAX_QUANTIZE_IMAGE_WIDTH{4096};

void ColorQuantize(u8 const *image, int width, int height, int flags,
                   int colors_num, u8 *out_pixels, u8 *out_palette,
                   int first_color) {
  int errors[MAX_QUANTIZE_IMAGE_WIDTH + 1][3][2];
  struct Sample *s = AllocSamples(width * height, N_DIMENSIONS);

  for (int y = 0; y < height; y++)
    for (int x = 0; x < width; x++) {
      for (int c = 0; c < 3; c++)
        NthSample(s, y * width + x, N_DIMENSIONS)->Value[c] =
            PIXEL(image, width, x, y, c);

      // now, let's generate extra values to quantize on
      for (int i = 0; i < N_EXTRAVALUES; i++) {
        int extra_val = 0;

        for (int c = 0; c < 3; c++)
          extra_val +=
              PIXEL(image, width, x, y, c) * ExtraValueXForms[i * 3 + c];

        extra_val >>= 8;

        for (int c = 0; c < 3; c++)
          NthSample(s, y * width + x, N_DIMENSIONS)->Value[c] =
              (u8)(std::min(255, std::max(0, extra_val)));
      }
    }

  struct QuantizedValue *q = Quantize(s, width * height, N_DIMENSIONS,
                                      colors_num, Weights, first_color);
  FreeSamples(s);
  memset(out_palette, 0x55, 768);

  for (int p = 0; p < 256; p++) {
    struct QuantizedValue *v = FindQNode(q, p);
    if (v)
      for (int c = 0; c < 3; c++) out_palette[p * 3 + c] = v->Mean[c];
  }

  memset(errors, 0, sizeof(errors));

  for (int y = 0; y < height; y++) {
    int error_use = y & 1, error_update = error_use ^ 1;

    for (int x = 0; x < width; x++) {
      u8 samp[3];

      for (int c = 0; c < 3; c++) {
        int tryc = PIXEL(image, width, x, y, c);

        if (!(flags & QUANTFLAGS_NODITHER)) {
          tryc += errors[x][c][error_use];
          errors[x][c][error_use] = 0;
        }

        samp[c] = (u8)std::min(255, std::max(0, tryc));
      }

      struct QuantizedValue *f = FindMatch(samp, 3, Weights, q);
      out_pixels[width * y + x] = (u8)(f->value);

      if (!(flags & QUANTFLAGS_NODITHER))
        for (int i = 0; i < 3; i++) {
          int newerr = samp[i] - f->Mean[i];
          int orthog_error = (newerr * 3) / 8;
          errors[x + 1][i][error_use] += orthog_error;
          errors[x][i][error_update] = orthog_error;
          errors[x + 1][i][error_update] = newerr - 2 * orthog_error;
        }
    }
  }

  if (q) FreeQuantization(q);
}
