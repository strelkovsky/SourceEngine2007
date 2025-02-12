// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_MATHLIB_VECTOR2D_H_
#define SOURCE_MATHLIB_VECTOR2D_H_

#ifdef _WIN32
#pragma once
#endif

#include <cfloat>
#include <cmath>
#include <cstdlib>  // std::rand

#include "base/include/base_types.h"
#include "tier0/include/dbg.h"
#include "tier0/include/floattypes.h"

#include "mathlib/math_pfns.h"

// 2D Vector2D
class Vector2D {
 public:
  // Members
  f32 x, y;

  // Construction/destruction
  Vector2D();
  Vector2D(f32 X, f32 Y);
  Vector2D(const f32* pFloat);

  // Initialization
  void Init(f32 ix = 0.0f, f32 iy = 0.0f);

  // Got any nasty NAN's?
  bool IsValid() const;

  // array access...
  f32 operator[](int i) const;
  f32& operator[](int i);

  // Base address...
  f32* Base();
  f32 const* Base() const;

  // Initialization methods
  void Random(f32 minVal, f32 maxVal);

  // equality
  bool operator==(const Vector2D& v) const;
  bool operator!=(const Vector2D& v) const;

  // arithmetic operations
  Vector2D& operator+=(const Vector2D& v);
  Vector2D& operator-=(const Vector2D& v);
  Vector2D& operator*=(const Vector2D& v);
  Vector2D& operator*=(f32 s);
  Vector2D& operator/=(const Vector2D& v);
  Vector2D& operator/=(f32 s);

  // negate the Vector2D components
  void Negate();

  // Get the Vector2D's magnitude.
  f32 Length() const;

  // Get the Vector2D's magnitude squared.
  f32 LengthSqr(void) const;

  // return true if this vector is (0,0) within tolerance
  bool IsZero(f32 tolerance = 0.01f) const {
    return (x > -tolerance && x < tolerance && y > -tolerance && y < tolerance);
  }

  // Normalize in place and return the old length.
  f32 NormalizeInPlace();

  // Compare length.
  bool IsLengthGreaterThan(f32 val) const;
  bool IsLengthLessThan(f32 val) const;

  // Get the distance from this Vector2D to the other one.
  f32 DistTo(const Vector2D& vOther) const;

  // Get the distance from this Vector2D to the other one squared.
  f32 DistToSqr(const Vector2D& vOther) const;

  // Copy
  void CopyToArray(f32* rgfl) const;

  // Multiply, add, and assign to this (ie: *this = a + b * scalar). This
  // is about 12% faster than the actual Vector2D equation (because it's done
  // per-component rather than per-Vector2D).
  void MulAdd(const Vector2D& a, const Vector2D& b, f32 scalar);

  // Dot product.
  f32 Dot(const Vector2D& vOther) const;

  // assignment
  Vector2D& operator=(const Vector2D& vOther);

#ifndef VECTOR_NO_SLOW_OPERATIONS
  // copy constructors
  Vector2D(const Vector2D& vOther);

  // arithmetic operations
  Vector2D operator-(void) const;

  Vector2D operator+(const Vector2D& v) const;
  Vector2D operator-(const Vector2D& v) const;
  Vector2D operator*(const Vector2D& v) const;
  Vector2D operator/(const Vector2D& v) const;
  Vector2D operator*(f32 fl) const;
  Vector2D operator/(f32 fl) const;

  // Cross product between two vectors.
  Vector2D Cross(const Vector2D& vOther) const;

  // Returns a Vector2D with the min or max in X, Y, and Z.
  Vector2D Min(const Vector2D& vOther) const;
  Vector2D Max(const Vector2D& vOther) const;

#else

 private:
  // No copy constructors allowed if we're in optimal mode
  Vector2D(const Vector2D& vOther);
#endif
};

const Vector2D vec2_origin(0, 0);
const Vector2D vec2_invalid(FLT_MAX, FLT_MAX);

// Vector2D related operations

// Vector2D clear
void Vector2DClear(Vector2D& a);

// Copy
void Vector2DCopy(const Vector2D& src, Vector2D& dst);

// Vector2D arithmetic
void Vector2DAdd(const Vector2D& a, const Vector2D& b, Vector2D& result);
void Vector2DSubtract(const Vector2D& a, const Vector2D& b, Vector2D& result);
void Vector2DMultiply(const Vector2D& a, f32 b, Vector2D& result);
void Vector2DMultiply(const Vector2D& a, const Vector2D& b, Vector2D& result);
void Vector2DDivide(const Vector2D& a, f32 b, Vector2D& result);
void Vector2DDivide(const Vector2D& a, const Vector2D& b, Vector2D& result);
void Vector2DMA(const Vector2D& start, f32 s, const Vector2D& dir,
                Vector2D& result);

// Store the min or max of each of x, y, and z into the result.
void Vector2DMin(const Vector2D& a, const Vector2D& b, Vector2D& result);
void Vector2DMax(const Vector2D& a, const Vector2D& b, Vector2D& result);

#define Vector2DExpand(v) (v).x, (v).y

// Normalization
f32 Vector2DNormalize(Vector2D& v);

// Length
f32 Vector2DLength(const Vector2D& v);

// Dot Product
f32 DotProduct2D(const Vector2D& a, const Vector2D& b);

// Linearly interpolate between two vectors
void Vector2DLerp(const Vector2D& src1, const Vector2D& src2, f32 t,
                  Vector2D& dest);

//
// Inlined Vector2D methods
//

// constructors

inline Vector2D::Vector2D() {
#ifdef _DEBUG
  // Initialize to NAN to catch errors
  x = y = FLOAT32_NAN;
#endif
}

inline Vector2D::Vector2D(f32 X, f32 Y) {
  x = X;
  y = Y;
  Assert(IsValid());
}

inline Vector2D::Vector2D(const f32* pFloat) {
  Assert(pFloat);
  x = pFloat[0];
  y = pFloat[1];
  Assert(IsValid());
}

// copy constructor

inline Vector2D::Vector2D(const Vector2D& vOther) {
  Assert(vOther.IsValid());
  x = vOther.x;
  y = vOther.y;
}

// initialization

inline void Vector2D::Init(f32 ix, f32 iy) {
  x = ix;
  y = iy;
  Assert(IsValid());
}

inline void Vector2D::Random(f32 minVal, f32 maxVal) {
  x = minVal + ((f32)rand() / RAND_MAX) * (maxVal - minVal);
  y = minVal + ((f32)rand() / RAND_MAX) * (maxVal - minVal);
}

inline void Vector2DClear(Vector2D& a) { a.x = a.y = 0.0f; }

// assignment

inline Vector2D& Vector2D::operator=(const Vector2D& vOther) {
  Assert(vOther.IsValid());
  x = vOther.x;
  y = vOther.y;
  return *this;
}

// Array access

inline f32& Vector2D::operator[](int i) {
  Assert((i >= 0) && (i < 2));
  return ((f32*)this)[i];
}

inline f32 Vector2D::operator[](int i) const {
  Assert((i >= 0) && (i < 2));
  return ((f32*)this)[i];
}

// Base address...

inline f32* Vector2D::Base() { return (f32*)this; }

inline f32 const* Vector2D::Base() const { return (f32 const*)this; }

// IsValid?

inline bool Vector2D::IsValid() const { return IsFinite(x) && IsFinite(y); }

// comparison

inline bool Vector2D::operator==(const Vector2D& src) const {
  Assert(src.IsValid() && IsValid());
  return (src.x == x) && (src.y == y);
}

inline bool Vector2D::operator!=(const Vector2D& src) const {
  Assert(src.IsValid() && IsValid());
  return (src.x != x) || (src.y != y);
}

// Copy

inline void Vector2DCopy(const Vector2D& src, Vector2D& dst) {
  Assert(src.IsValid());
  dst.x = src.x;
  dst.y = src.y;
}

inline void Vector2D::CopyToArray(f32* rgfl) const {
  Assert(IsValid());
  Assert(rgfl);
  rgfl[0] = x;
  rgfl[1] = y;
}

// standard math operations

inline void Vector2D::Negate() {
  Assert(IsValid());
  x = -x;
  y = -y;
}

inline Vector2D& Vector2D::operator+=(const Vector2D& v) {
  Assert(IsValid() && v.IsValid());
  x += v.x;
  y += v.y;
  return *this;
}

inline Vector2D& Vector2D::operator-=(const Vector2D& v) {
  Assert(IsValid() && v.IsValid());
  x -= v.x;
  y -= v.y;
  return *this;
}

inline Vector2D& Vector2D::operator*=(f32 fl) {
  x *= fl;
  y *= fl;
  Assert(IsValid());
  return *this;
}

inline Vector2D& Vector2D::operator*=(const Vector2D& v) {
  x *= v.x;
  y *= v.y;
  Assert(IsValid());
  return *this;
}

inline Vector2D& Vector2D::operator/=(f32 fl) {
  Assert(fl != 0.0f);
  f32 oofl = 1.0f / fl;
  x *= oofl;
  y *= oofl;
  Assert(IsValid());
  return *this;
}

inline Vector2D& Vector2D::operator/=(const Vector2D& v) {
  Assert(v.x != 0.0f && v.y != 0.0f);
  x /= v.x;
  y /= v.y;
  Assert(IsValid());
  return *this;
}

inline void Vector2DAdd(const Vector2D& a, const Vector2D& b, Vector2D& c) {
  Assert(a.IsValid() && b.IsValid());
  c.x = a.x + b.x;
  c.y = a.y + b.y;
}

inline void Vector2DSubtract(const Vector2D& a, const Vector2D& b,
                             Vector2D& c) {
  Assert(a.IsValid() && b.IsValid());
  c.x = a.x - b.x;
  c.y = a.y - b.y;
}

inline void Vector2DMultiply(const Vector2D& a, f32 b, Vector2D& c) {
  Assert(a.IsValid() && IsFinite(b));
  c.x = a.x * b;
  c.y = a.y * b;
}

inline void Vector2DMultiply(const Vector2D& a, const Vector2D& b,
                             Vector2D& c) {
  Assert(a.IsValid() && b.IsValid());
  c.x = a.x * b.x;
  c.y = a.y * b.y;
}

inline void Vector2DDivide(const Vector2D& a, f32 b, Vector2D& c) {
  Assert(a.IsValid());
  Assert(b != 0.0f);
  f32 oob = 1.0f / b;
  c.x = a.x * oob;
  c.y = a.y * oob;
}

inline void Vector2DDivide(const Vector2D& a, const Vector2D& b, Vector2D& c) {
  Assert(a.IsValid());
  Assert((b.x != 0.0f) && (b.y != 0.0f));
  c.x = a.x / b.x;
  c.y = a.y / b.y;
}

inline void Vector2DMA(const Vector2D& start, f32 s, const Vector2D& dir,
                       Vector2D& result) {
  Assert(start.IsValid() && IsFinite(s) && dir.IsValid());
  result.x = start.x + s * dir.x;
  result.y = start.y + s * dir.y;
}

// TODO(d.rattman): Remove
// For backwards compatability
inline void Vector2D::MulAdd(const Vector2D& a, const Vector2D& b, f32 scalar) {
  x = a.x + b.x * scalar;
  y = a.y + b.y * scalar;
}

inline void Vector2DLerp(const Vector2D& src1, const Vector2D& src2, f32 t,
                         Vector2D& dest) {
  dest[0] = src1[0] + (src2[0] - src1[0]) * t;
  dest[1] = src1[1] + (src2[1] - src1[1]) * t;
}

// dot, cross

inline f32 DotProduct2D(const Vector2D& a, const Vector2D& b) {
  Assert(a.IsValid() && b.IsValid());
  return (a.x * b.x + a.y * b.y);
}

// for backwards compatability
inline f32 Vector2D::Dot(const Vector2D& vOther) const {
  return DotProduct2D(*this, vOther);
}

// length

inline f32 Vector2DLength(const Vector2D& v) {
  Assert(v.IsValid());
  return (f32)FastSqrt(v.x * v.x + v.y * v.y);
}

inline f32 Vector2D::LengthSqr() const {
  Assert(IsValid());
  return (x * x + y * y);
}

inline f32 Vector2D::NormalizeInPlace() { return Vector2DNormalize(*this); }

inline bool Vector2D::IsLengthGreaterThan(f32 val) const {
  return LengthSqr() > val * val;
}

inline bool Vector2D::IsLengthLessThan(f32 val) const {
  return LengthSqr() < val * val;
}

inline f32 Vector2D::Length() const { return Vector2DLength(*this); }

inline void Vector2DMin(const Vector2D& a, const Vector2D& b,
                        Vector2D& result) {
  result.x = (a.x < b.x) ? a.x : b.x;
  result.y = (a.y < b.y) ? a.y : b.y;
}

inline void Vector2DMax(const Vector2D& a, const Vector2D& b,
                        Vector2D& result) {
  result.x = (a.x > b.x) ? a.x : b.x;
  result.y = (a.y > b.y) ? a.y : b.y;
}

// Normalization

inline f32 Vector2DNormalize(Vector2D& v) {
  Assert(v.IsValid());
  f32 l = v.Length();
  if (l != 0.0f) {
    v /= l;
  } else {
    v.x = v.y = 0.0f;
  }
  return l;
}

// Get the distance from this Vector2D to the other one

inline f32 Vector2D::DistTo(const Vector2D& vOther) const {
  Vector2D delta;
  Vector2DSubtract(*this, vOther, delta);
  return delta.Length();
}

inline f32 Vector2D::DistToSqr(const Vector2D& vOther) const {
  Vector2D delta;
  Vector2DSubtract(*this, vOther, delta);
  return delta.LengthSqr();
}

// Computes the closest point to vecTarget no farther than flMaxDist from
// vecStart

inline void ComputeClosestPoint2D(const Vector2D& vecStart, f32 flMaxDist,
                                  const Vector2D& vecTarget,
                                  Vector2D* pResult) {
  Vector2D vecDelta;
  Vector2DSubtract(vecTarget, vecStart, vecDelta);
  f32 flDistSqr = vecDelta.LengthSqr();
  if (flDistSqr <= flMaxDist * flMaxDist) {
    *pResult = vecTarget;
  } else {
    vecDelta /= FastSqrt(flDistSqr);
    Vector2DMA(vecStart, flMaxDist, vecDelta, *pResult);
  }
}

  //
  // Slow methods
  //

#ifndef VECTOR_NO_SLOW_OPERATIONS

// Returns a Vector2D with the min or max in X, Y, and Z.

inline Vector2D Vector2D::Min(const Vector2D& vOther) const {
  return Vector2D(x < vOther.x ? x : vOther.x, y < vOther.y ? y : vOther.y);
}

inline Vector2D Vector2D::Max(const Vector2D& vOther) const {
  return Vector2D(x > vOther.x ? x : vOther.x, y > vOther.y ? y : vOther.y);
}

// arithmetic operations

inline Vector2D Vector2D::operator-() const { return Vector2D(-x, -y); }

inline Vector2D Vector2D::operator+(const Vector2D& v) const {
  Vector2D res;
  Vector2DAdd(*this, v, res);
  return res;
}

inline Vector2D Vector2D::operator-(const Vector2D& v) const {
  Vector2D res;
  Vector2DSubtract(*this, v, res);
  return res;
}

inline Vector2D Vector2D::operator*(f32 fl) const {
  Vector2D res;
  Vector2DMultiply(*this, fl, res);
  return res;
}

inline Vector2D Vector2D::operator*(const Vector2D& v) const {
  Vector2D res;
  Vector2DMultiply(*this, v, res);
  return res;
}

inline Vector2D Vector2D::operator/(f32 fl) const {
  Vector2D res;
  Vector2DDivide(*this, fl, res);
  return res;
}

inline Vector2D Vector2D::operator/(const Vector2D& v) const {
  Vector2D res;
  Vector2DDivide(*this, v, res);
  return res;
}

inline Vector2D operator*(f32 fl, const Vector2D& v) { return v * fl; }

#endif  // slow

#endif  // SOURCE_MATHLIB_VECTOR2D_H_
