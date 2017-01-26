// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPMATH_P_H
#define _MPSL_MPMATH_P_H

// [Dependencies - MPSL]
#include "./mpsl_p.h"

// [Dependencies - C]
#include <math.h>

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [mpsl::FloatBits]
// ============================================================================

//! Single-precision floating point and its binary representation.
union FloatBits {
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  static MPSL_INLINE FloatBits fromFloat(float val) noexcept { FloatBits u; u.f = val; return u; }
  static MPSL_INLINE FloatBits fromUInt(uint32_t val) noexcept { FloatBits u; u.u = val; return u; }

  // --------------------------------------------------------------------------
  // [Utilities]
  // --------------------------------------------------------------------------

  MPSL_INLINE bool isNan() const noexcept { return (u & 0x7FFFFFFFU) > 0x7F800000U; }
  MPSL_INLINE bool isInf() const noexcept { return (u & 0x7FFFFFFFU) == 0x7F800000U; }
  MPSL_INLINE bool isFinite() const noexcept { return (u & 0x7F800000U) != 0x7F800000U; }

  MPSL_INLINE FloatBits& setNan() noexcept { u = 0x7FC00000U; return *this; }
  MPSL_INLINE FloatBits& setInf() noexcept { u = 0x7F800000U; return *this; }

  MPSL_INLINE FloatBits& invSign() noexcept { u ^= 0x80000000U; return *this; }
  MPSL_INLINE FloatBits& setSign() noexcept { u |= 0x80000000U; return *this; }
  MPSL_INLINE FloatBits& clearSign() noexcept { u &= ~0x80000000U; return *this; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Value as `uint32_t`.
  uint32_t u;
  //! Value as `float`.
  float f;
};

// ============================================================================
// [mpsl::DoubleBits]
// ============================================================================

//! Double-precision floating point and its binary representation.
union DoubleBits {
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  static MPSL_INLINE DoubleBits fromDouble(double val) noexcept { DoubleBits u; u.d = val; return u; }
  static MPSL_INLINE DoubleBits fromUInt64(uint64_t val) noexcept { DoubleBits u; u.u = val; return u; }

  // --------------------------------------------------------------------------
  // [Utilities]
  // --------------------------------------------------------------------------

  MPSL_INLINE bool isNan() const noexcept {
    if (MPSL_ARCH_64BIT)
      return (u & ASMJIT_UINT64_C(0x7FFFFFFFFFFFFFFF)) > ASMJIT_UINT64_C(0x7F80000000000000);
    else
      return ((hi & 0x7FF00000U)) == 0x7FF00000U && ((hi & 0x000FFFFFU) | lo) != 0x00000000U;
  }

  MPSL_INLINE bool isInf() const noexcept {
    if (MPSL_ARCH_64BIT)
      return (u & ASMJIT_UINT64_C(0x7FFFFFFFFFFFFFFF)) == ASMJIT_UINT64_C(0x7F80000000000000);
    else
      return (hi & 0x7FFFFFFFU) == 0x7FF00000U && lo == 0x00000000U;
  }

  MPSL_INLINE bool isFinite() const noexcept {
    if (MPSL_ARCH_64BIT)
      return (u & ASMJIT_UINT64_C(0x7FF0000000000000)) != ASMJIT_UINT64_C(0x7FF0000000000000);
    else
      return (hi & 0x7FF00000U) != 0x7FF00000U;
  }

  MPSL_INLINE DoubleBits& setNan() noexcept { u = ASMJIT_UINT64_C(0x7FF8000000000000); return *this; }
  MPSL_INLINE DoubleBits& setInf() noexcept { u = ASMJIT_UINT64_C(0x7FF0000000000000); return *this; }

  MPSL_INLINE DoubleBits& invSign() noexcept { hi ^= 0x80000000U; return *this; }
  MPSL_INLINE DoubleBits& setSign() noexcept { hi |= 0x80000000U; return *this; }
  MPSL_INLINE DoubleBits& clearSign() noexcept { hi&= ~0x80000000U; return *this; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Value as `uint64_t`.
  uint64_t u;
  //! Value as `double`.
  double d;

#if MPSL_ARCH_LE
  struct { uint32_t lo; uint32_t hi; };
#else
  struct { uint32_t hi; uint32_t lo; };
#endif
};

// ============================================================================
// [mpsl::Math - Min / Max]
// ============================================================================

// The `a != a` condition is used to handle NaN values properly. If one of `a`
// and `b` is NaN the result should be NaN. When `T` isn't a floating point the
// condition should be removed by C++ compiler.
template<typename T> MPSL_INLINE T mpMin(T a, T b) noexcept { return (a != a) | (a < b) ? a : b; }
template<typename T> MPSL_INLINE T mpMax(T a, T b) noexcept { return (a != a) | (a > b) ? a : b; }

template<typename T> MPSL_INLINE T mpBound(T x, T xMin, T xMax) noexcept {
  return x < xMin ? xMin :
         x > xMax ? xMax : x;
}

// ============================================================================
// [mpsl::Math - NaN / Inf]
// ============================================================================

static MPSL_INLINE float mpGetNanF() noexcept { static const FloatBits value = { 0x7FC00000U }; return value.f; }
static MPSL_INLINE float mpGetInfF() noexcept { static const FloatBits value = { 0x7F800000U }; return value.f; }

static MPSL_INLINE double mpGetNanD() noexcept { static const DoubleBits value = { ASMJIT_UINT64_C(0x7FF8000000000000) }; return value.d; }
static MPSL_INLINE double mpGetInfD() noexcept { static const DoubleBits value = { ASMJIT_UINT64_C(0x7FF0000000000000) }; return value.d; }

static MPSL_INLINE bool mpIsNanF(float x) noexcept { return FloatBits::fromFloat(x).isNan(); }
static MPSL_INLINE bool mpIsInfF(float x) noexcept { return FloatBits::fromFloat(x).isInf(); }
static MPSL_INLINE bool mpIsFiniteF(float x) noexcept { return FloatBits::fromFloat(x).isFinite(); }

static MPSL_INLINE bool mpIsNanD(double x) noexcept { return DoubleBits::fromDouble(x).isNan(); }
static MPSL_INLINE bool mpIsInfD(double x) noexcept { return DoubleBits::fromDouble(x).isInf(); }
static MPSL_INLINE bool mpIsFiniteD(double x) noexcept { return DoubleBits::fromDouble(x).isFinite(); }

// ============================================================================
// [mpsl::Math - Normalize]
// ============================================================================

static MPSL_INLINE float mpNormalizeF(float x) noexcept { return x + static_cast<float>(0); }
static MPSL_INLINE double mpNormalizeD(double x) noexcept { return x + static_cast<double>(0); }

// ============================================================================
// [mpsl::Math - Abs]
// ============================================================================

static MPSL_INLINE int32_t mpAbsI(int32_t x) noexcept {
  // To support `mpAbsI(-2147483648)` returning `-2147483648` we have
  // to cast to an unsigned integer to prevent "undefined behavior".
  uint32_t mask = static_cast<uint32_t>(-static_cast<int32_t>(x < 0));
  return static_cast<int32_t>((static_cast<uint32_t>(x) ^ mask) - mask);
}

static MPSL_INLINE float mpAbsF(float x) noexcept { return FloatBits::fromFloat(x).clearSign().f; }
static MPSL_INLINE double mpAbsD(double x) noexcept { return DoubleBits::fromDouble(x).clearSign().d; }

// ============================================================================
// [mpsl::Math - Neg]
// ============================================================================

static MPSL_INLINE float mpNegF(float x) noexcept { return FloatBits::fromFloat(x).invSign().f; }
static MPSL_INLINE double mpNegD(double x) noexcept { return DoubleBits::fromDouble(x).invSign().d; }

// ============================================================================
// [mpsl::Math - Round]
// ============================================================================

static MPSL_INLINE float mpTruncF(float x) noexcept { return ::truncf(x); }
static MPSL_INLINE float mpFloorF(float x) noexcept { return ::floorf(x); }
static MPSL_INLINE float mpCeilF(float x) noexcept { return ::ceilf(x); }

static MPSL_INLINE double mpTruncD(double x) noexcept { return ::trunc(x); }
static MPSL_INLINE double mpFloorD(double x) noexcept { return ::floor(x); }
static MPSL_INLINE double mpCeilD(double x) noexcept { return ::ceil(x); }

static MPSL_INLINE float mpRoundF(float x) noexcept {
  float y = mpFloorF(x);
  return y + (x - y >= 0.5f ? 1.0f : 0.0f);
}

static MPSL_INLINE double mpRoundD(double x) noexcept {
  double y = mpFloorD(x);
  return y + (x - y >= 0.5 ? 1.0 : 0.0);
}

static MPSL_INLINE float mpRoundEvenF(float x) noexcept { return ::rintf(x); }
static MPSL_INLINE double mpRoundEvenD(double x) noexcept { return ::rint(x); }

// ============================================================================
// [mpsl::Math - SignBit]
// ============================================================================

static MPSL_INLINE bool mpSignBitF(float x) noexcept {
  return FloatBits::fromFloat(x).u >= 0x80000000U;
}

static MPSL_INLINE bool mpSignBitD(double x) noexcept {
  return DoubleBits::fromDouble(x).hi >= 0x80000000U;
}

static MPSL_INLINE float mpCopySignF(float x, float y) noexcept {
  FloatBits bits = FloatBits::fromFloat(x);
  bits.u &= 0x7FFFFFFFU;
  bits.u |= FloatBits::fromFloat(y).u & 0x80000000U;
  return bits.f;
}

static MPSL_INLINE double mpCopySignD(double x, double y) noexcept {
  DoubleBits bits = DoubleBits::fromDouble(x);
  bits.hi &= 0x7FFFFFFFU;
  bits.hi |= DoubleBits::fromDouble(y).hi & 0x80000000U;
  return bits.d;
}

// ============================================================================
// [mpsl::Math - Fraction]
// ============================================================================

static MPSL_INLINE float mpFracF(float x) noexcept { return x - mpFloorF(x); }
static MPSL_INLINE double mpFracD(double x) noexcept { return x - mpFloorD(x); }

// ============================================================================
// [mpsl::Math - Modulo / Remainder]
// ============================================================================

static MPSL_INLINE float mpModF(float x, float y) noexcept { return fmodf(x, y); }
static MPSL_INLINE double mpModD(double x, double y) noexcept { return fmod(x, y); }

// ============================================================================
// [mpsl::Math - Sqrt]
// ============================================================================

static MPSL_INLINE float mpSqrtF(float x) noexcept { return ::sqrtf(x); }
static MPSL_INLINE double mpSqrtD(double x) noexcept { return ::sqrt(x); }

// ============================================================================
// [mpsl::Math - Power]
// ============================================================================

static MPSL_INLINE float mpPowF(float x, float y) noexcept { return ::powf(x, y); }
static MPSL_INLINE double mpPowD(double x, double y) noexcept { return ::pow(x, y); }

// ============================================================================
// [mpsl::Math - Exponential]
// ============================================================================

static MPSL_INLINE float mpExpF(float x) noexcept { return ::expf(x); }
static MPSL_INLINE double mpExpD(double x) noexcept { return ::exp(x); }

// ============================================================================
// [mpsl::Math - Logarithm]
// ============================================================================

static MPSL_INLINE float mpLogF(float x) noexcept { return ::logf(x); }
static MPSL_INLINE double mpLogD(double x) noexcept { return ::log(x); }

static MPSL_INLINE float mpLog2F(float x) noexcept { return ::log2f(x); }
static MPSL_INLINE double mpLog2D(double x) noexcept { return ::log2(x); }

static MPSL_INLINE float mpLog10F(float x) noexcept { return ::log10f(x); }
static MPSL_INLINE double mpLog10D(double x) noexcept { return ::log10(x); }

// ============================================================================
// [mpsl::Math - Trigonometric]
// ============================================================================

static MPSL_INLINE float mpSinF(float x) noexcept { return ::sinf(x); }
static MPSL_INLINE double mpSinD(double x) noexcept { return ::sin(x); }

static MPSL_INLINE float mpCosF(float x) noexcept { return ::cosf(x); }
static MPSL_INLINE double mpCosD(double x) noexcept { return ::cos(x); }

static MPSL_INLINE float mpTanF(float x) noexcept { return ::tanf(x); }
static MPSL_INLINE double mpTanD(double x) noexcept { return ::tan(x); }

static MPSL_INLINE float mpAsinF(float x) noexcept { return ::asinf(x); }
static MPSL_INLINE double mpAsinD(double x) noexcept { return ::asin(x); }

static MPSL_INLINE float mpAcosF(float x) noexcept { return ::acosf(x); }
static MPSL_INLINE double mpAcosD(double x) noexcept { return ::acos(x); }

static MPSL_INLINE float mpAtanF(float x) noexcept { return ::atanf(x); }
static MPSL_INLINE double mpAtanD(double x) noexcept { return ::atan(x); }

static MPSL_INLINE float mpAtan2F(float x, float y) noexcept { return ::atan2f(x, y); }
static MPSL_INLINE double mpAtan2D(double x, double y) noexcept { return ::atan2(x, y); }

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPMATH_P_H
