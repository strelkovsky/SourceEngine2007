// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#include "bitmap/float_bm.h"
#include "mathlib/mathlib.h"
#include "mathlib/ssemath.h"
#include "tier0/include/memdbgon.h"
#include "tier0/include/platform.h"
#include "tier0/include/progressbar.h"
#include "tier2/tier2.h"

#define PROBLEM_SIZE 1000
#define N_ITERS 100000
//#define RECORD_OUTPUT

static FourVectors g_XYZ[PROBLEM_SIZE];
static fltx4 g_CreationTime[PROBLEM_SIZE];

bool SIMDTest() {
  const Vector StartPnt(0, 0, 0);
  const Vector MidP(0, 0, 100);
  const Vector EndPnt(100, 0, 50);

  TestVPUFlags();

  // Initialize g_XYZ[] and g_CreationTime[]
  SeedRandSIMD(1987301);
  for (int i = 0; i < PROBLEM_SIZE; i++) {
    float fourStartTimes[4];
    Vector fourPoints[4];
    Vector offset;
    for (int j = 0; j < 4; j++) {
      float t = (j + 4 * i) / (4.0f * (PROBLEM_SIZE - 1));
      fourStartTimes[j] = t;
      fourPoints[j] = StartPnt + t * (EndPnt - StartPnt);
      offset.Random(-10.0f, +10.0f);
      fourPoints[j] += offset;
    }
    g_XYZ[i].LoadAndSwizzle(fourPoints[0], fourPoints[1], fourPoints[2],
                            fourPoints[3]);
    g_CreationTime[i] = LoadUnalignedSIMD(fourStartTimes);
  }

#ifdef RECORD_OUTPUT
  char outputBuffer[1024];
  Q_snprintf(outputBuffer, sizeof(outputBuffer),
             "float testOutput[%d][4][3] = {\n", N_ITERS);
  Warning(outputBuffer);
#endif  // RECORD_OUTPUT

  double STime = Plat_FloatTime();
  bool bChangedSomething = false;
  for (int i = 0; i < N_ITERS; i++) {
    float t = i * (1.0 / N_ITERS);
    FourVectors *__restrict pXYZ = g_XYZ;

    fltx4 *__restrict pCreationTime = g_CreationTime;

    fltx4 CurTime = ReplicateX4(t);
    fltx4 TimeScale = ReplicateX4(1.0 / (std::max(0.001, 1.0)));

    // calculate radius spline
    bool bConstantRadius = true;
    fltx4 Rad0 = ReplicateX4(2.0);
    fltx4 Radm = Rad0;
    fltx4 Rad1 = Rad0;

    fltx4 RadmMinusRad0 = SubSIMD(Radm, Rad0);
    fltx4 Rad1MinusRadm = SubSIMD(Rad1, Radm);

    fltx4 SIMDMinDist = ReplicateX4(2.0);
    fltx4 SIMDMinDist2 = ReplicateX4(2.0 * 2.0);

    fltx4 SIMDMaxDist = MaxSIMD(Rad0, MaxSIMD(Radm, Rad1));
    fltx4 SIMDMaxDist2 = MulSIMD(SIMDMaxDist, SIMDMaxDist);

    FourVectors StartP;
    StartP.DuplicateVector(StartPnt);

    FourVectors MiddleP;
    MiddleP.DuplicateVector(MidP);

    // form delta terms needed for quadratic bezier
    FourVectors Delta0;
    Delta0.DuplicateVector(MidP - StartPnt);

    FourVectors Delta1;
    Delta1.DuplicateVector(EndPnt - MidP);
    int nLoopCtr = PROBLEM_SIZE;
    do {
      fltx4 TScale = MinSIMD(
          Four_Ones, MulSIMD(TimeScale, SubSIMD(CurTime, *pCreationTime)));

      // bezier(a,b,c,t)=lerp( lerp(a,b,t),lerp(b,c,t),t)
      FourVectors L0 = Delta0;
      L0 *= TScale;
      L0 += StartP;

      FourVectors L1 = Delta1;
      L1 *= TScale;
      L1 += MiddleP;

      FourVectors Center = L1;
      Center -= L0;
      Center *= TScale;
      Center += L0;

      FourVectors pts_original = *(pXYZ);
      FourVectors pts = pts_original;
      pts -= Center;

      // calculate radius at the point. !!speed!! - use special case for
      // constant radius

      fltx4 dist_squared = pts * pts;
      fltx4 TooFarMask = CmpGtSIMD(dist_squared, SIMDMaxDist2);
      if ((!bConstantRadius) && (!IsAnyNegative(TooFarMask))) {
        // need to calculate and adjust for true radius =- we've only trivially
        // rejected note voodoo here - we update simdmaxdist for true radius,
        // but not max dist^2, since that's used only for the trivial reject
        // case, which we've already done
        fltx4 R0 = AddSIMD(Rad0, MulSIMD(RadmMinusRad0, TScale));
        fltx4 R1 = AddSIMD(Radm, MulSIMD(Rad1MinusRadm, TScale));
        SIMDMaxDist = AddSIMD(R0, MulSIMD(SubSIMD(R1, R0), TScale));

        // now that we know the true radius, update our mask
        TooFarMask = CmpGtSIMD(dist_squared, MulSIMD(SIMDMaxDist, SIMDMaxDist));
      }

      fltx4 TooCloseMask = CmpLtSIMD(dist_squared, SIMDMinDist2);
      fltx4 NeedAdjust = OrSIMD(TooFarMask, TooCloseMask);
      if (IsAnyNegative(NeedAdjust))  // any out of bounds?
      {
        // change squared distance into approximate rsqr root
        fltx4 guess = ReciprocalSqrtEstSIMD(dist_squared);
        // newton iteration for 1/sqrt(x) : y(n+1)=1/2 (y(n)*(3-x*y(n)^2));
        guess = MulSIMD(
            guess,
            SubSIMD(Four_Threes, MulSIMD(dist_squared, MulSIMD(guess, guess))));
        guess = MulSIMD(Four_PointFives, guess);
        pts *= guess;

        FourVectors clamp_far = pts;
        clamp_far *= SIMDMaxDist;
        clamp_far += Center;
        FourVectors clamp_near = pts;
        clamp_near *= SIMDMinDist;
        clamp_near += Center;
        pts.x =
            MaskedAssign(TooCloseMask, clamp_near.x,
                         MaskedAssign(TooFarMask, clamp_far.x, pts_original.x));
        pts.y =
            MaskedAssign(TooCloseMask, clamp_near.y,
                         MaskedAssign(TooFarMask, clamp_far.y, pts_original.y));
        pts.z =
            MaskedAssign(TooCloseMask, clamp_near.z,
                         MaskedAssign(TooFarMask, clamp_far.z, pts_original.z));
        *(pXYZ) = pts;
        bChangedSomething = true;
      }

#ifdef RECORD_OUTPUT
      if (nLoopCtr == 257) {
        Q_snprintf(outputBuffer, sizeof(outputBuffer),
                   "/*%04d:*/ { {%+14e,%+14e,%+14e}, {%+14e,%+14e,%+14e}, "
                   "{%+14e,%+14e,%+14e}, {%+14e,%+14e,%+14e} },\n",
                   i, pXYZ->X(0), pXYZ->Y(0), pXYZ->Z(0), pXYZ->X(1),
                   pXYZ->Y(1), pXYZ->Z(1), pXYZ->X(2), pXYZ->Y(2), pXYZ->Z(2),
                   pXYZ->X(3), pXYZ->Y(3), pXYZ->Z(3));
        Warning(outputBuffer);
      }
#endif  // RECORD_OUTPUT

      ++pXYZ;
      ++pCreationTime;
    } while (--nLoopCtr);
  }
  double ETime = Plat_FloatTime() - STime;

#ifdef RECORD_OUTPUT
  Q_snprintf(outputBuffer, sizeof(outputBuffer), "         };\n");
  Warning(outputBuffer);
#endif  // RECORD_OUTPUT

  printf("elapsed time=%f p/s=%f\n", ETime,
         (4.0 * PROBLEM_SIZE * N_ITERS) / ETime);
  return bChangedSomething;
}

void SSEClassTest(const fltx4 &val, fltx4 &out) {
  fltx4 result = Four_Zeros;
  for (int i = 0; i < N_ITERS; i++) {
    result = SubSIMD(val, result);
    result = MulSIMD(val, result);
    result = AddSIMD(val, result);
    result = MinSIMD(val, result);
  }

  FourVectors result4;
  result4.x = result;
  result4.y = result;
  result4.z = result;

  for (int i = 0; i < N_ITERS; i++) {
    result4 *= result4;
    result4 += result4;
    result4 *= result4;
    result4 += result4;
  }

  result = result4 * result4;
  out = result;
}

int main(int argc, char **argv) {
  InitCommandLineProgram(argc, argv);

  // This function is useful for inspecting compiler output.
  fltx4 result;
  SSEClassTest(Four_PointFives, result);
  printf("(%f,%f,%f,%f)\n", SubFloat(result, 0), SubFloat(result, 1),
         SubFloat(result, 2), SubFloat(result, 3));

  // Run the perf. test.
  SIMDTest();

  return 0;
}
