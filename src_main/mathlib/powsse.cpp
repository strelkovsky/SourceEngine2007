// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#include "mathlib/ssemath.h"

fltx4 Pow_FixedPoint_Exponent_SIMD(const fltx4& x, int exponent) {
  fltx4 rslt = Four_Ones;  // x^0=1.0
  int xp = abs(exponent);

  if (xp & 3) {  // fraction present?
    fltx4 sq_rt = SqrtEstSIMD(x);
    if (xp & 1)                   // .25?
      rslt = SqrtEstSIMD(sq_rt);  // x^.25
    if (xp & 2) rslt = MulSIMD(rslt, sq_rt);
  }

  xp >>= 2;            // strip fraction
  fltx4 curpower = x;  // curpower iterates through  x,x^2,x^4,x^8,x^16...

  while (true) {
    if (xp & 1) rslt = MulSIMD(rslt, curpower);
    xp >>= 1;

    if (xp)
      curpower = MulSIMD(curpower, curpower);
    else
      break;
  }

  // pow(x, -b) = 1 / pow(x, b)
  return (exponent < 0) ? ReciprocalEstSIMD(rslt) : rslt;
}
