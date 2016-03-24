// [MPSL]
// Shader-Like Mathematical Expression JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPMATH_P_H
#define _MPSL_MPMATH_P_H

// [Dependencies - MPSL]
#include "./mpsl_p.h"

// [Dependencies - C]
#include <float.h>
#include <math.h>

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [mpsl::Platform Support]
// ============================================================================

#if defined(_MSC_VER)
# define MPSL_IS_NAN_D(X) _isnan(X)
# define MPSL_IS_FINITE_D(X) _finite(X)
#endif // MPSL_CC_MSC

#if MPSL_CC_GCC && !defined(__APPLE__)
# define MPSL_GET_NAN_F() __builtin_nanf("")
# define MPSL_GET_NAN_D() __builtin_nan("")
# define MPSL_GET_INF_F() __builtin_inff()
# define MPSL_GET_INF_D() __builtin_inf()
# define MPSL_IS_NAN_F(X) __builtin_isnanf(X)
# define MPSL_IS_NAN_D(X) __builtin_isnan(X)
# define MPSL_IS_INF_F(X) __builtin_isinff(X)
# define MPSL_IS_INF_D(X) __builtin_isinf(X)
# define MPSL_IS_FINITE_F(X) __builtin_finitef(X)
# define MPSL_IS_FINITE_D(X) __builtin_finite(X)
#endif // MPSL_CC_GCC && !MPSL_OS_MAC

// ============================================================================
// [mpsl::FloatBits]
// ============================================================================

//! SP-FP binary representation and utilities.
union FloatBits {
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  static MPSL_INLINE FloatBits fromFloat(float val) { FloatBits u; u.f = val; return u; }
  static MPSL_INLINE FloatBits fromUInt(uint32_t val) { FloatBits u; u.u = val; return u; }

  // --------------------------------------------------------------------------
  // [Nan]
  // --------------------------------------------------------------------------

  MPSL_INLINE bool isNan() const { return (u & 0x7FFFFFFFU) > 0x7F800000U; }
  MPSL_INLINE void setNan() { u = 0x7FC00000U; }

  // --------------------------------------------------------------------------
  // [Inf]
  // --------------------------------------------------------------------------

  MPSL_INLINE bool isInf() const { return (u & 0x7FFFFFFFU) == 0x7F800000U; }
  MPSL_INLINE void setInf() { u = 0x7F800000; }

  // --------------------------------------------------------------------------
  // [IsFinite]
  // --------------------------------------------------------------------------

  MPSL_INLINE bool isFinite() const { return (u & 0x7F800000U) != 0x7F800000U; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Value as `uint`.
  uint32_t u;
  //! Value as `float`.
  float f;
};

// ============================================================================
// [mpsl::DoubleBits]
// ============================================================================

//! DP-FP binary representation and utilities.
union DoubleBits {
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  static MPSL_INLINE DoubleBits fromDouble(double val) { DoubleBits u; u.d = val; return u; }
  static MPSL_INLINE DoubleBits fromUInt64(uint64_t val) { DoubleBits u; u.u = val; return u; }

  // --------------------------------------------------------------------------
  // [Nan]
  // --------------------------------------------------------------------------

#if MPSL_ARCH_64BIT
  MPSL_INLINE bool isNan() const { return (u & MPSL_UINT64_C(0x7FFFFFFFFFFFFFFF)) > MPSL_UINT64_C(0x7F80000000000000); }
#else
  MPSL_INLINE bool isNan() const { return ((hi & 0x7FF00000U)) == 0x7FF00000U && ((hi & 0x000FFFFFU) | lo) != 0x00000000U; }
#endif

  MPSL_INLINE void setNan() { u = MPSL_UINT64_C(0x7FF8000000000000); }

  // --------------------------------------------------------------------------
  // [Inf]
  // --------------------------------------------------------------------------

#if MPSL_ARCH_64BIT
  MPSL_INLINE bool isInf() const { return (u & MPSL_UINT64_C(0x7FFFFFFFFFFFFFFF)) == MPSL_UINT64_C(0x7F80000000000000); }
#else
  MPSL_INLINE bool isInf() const { return (hi & 0x7FFFFFFFU) == 0x7FF00000U && lo == 0x00000000U; }
#endif

  MPSL_INLINE void setInf() { u = MPSL_UINT64_C(0x7FF0000000000000); }

  // --------------------------------------------------------------------------
  // [IsFinite]
  // --------------------------------------------------------------------------

#if MPSL_ARCH_64BIT
  MPSL_INLINE bool isFinite() const { return (u & MPSL_UINT64_C(0x7FF0000000000000)) != MPSL_UINT64_C(0x7FF0000000000000); }
#else
  MPSL_INLINE bool isFinite() const { return (hi & 0x7FF00000U) != 0x7FF00000U; }
#endif

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Value as `ulong` (MPSL 64-bit type).
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
// [mpsl::mpBitRor / mpBitRol]
// ============================================================================

template<typename T>
static MPSL_INLINE T mpBitRor(T x, T y) {
  unsigned int bits = sizeof(T) * 8;
  unsigned int mask = bits - 1;

  unsigned int l = static_cast<unsigned int>(y & mask);
  unsigned int r = (bits - l) & mask;

  return (x << l) | (x >> r);
}

template<typename T>
static MPSL_INLINE T mpBitRol(T x, T y) {
  unsigned int bits = sizeof(T) * 8;
  unsigned int mask = bits - 1;

  unsigned int r = static_cast<unsigned int>(y & mask);
  unsigned int l = (bits - r) & mask;

  return (x << l) | (x >> r);
}

// ============================================================================
// [mpsl::mpMin / mpMax]
// ============================================================================

// The `a != a` condition is used to handle NaN values properly. If one of `a`
// and `b` is NaN the result should be NaN. When `T` isn't a floating point the
// condition should be removed by C++ compiler.
template<typename T> MPSL_INLINE T mpMin(T a, T b) { return (a != a) | (a < b) ? a : b; }
template<typename T> MPSL_INLINE T mpMax(T a, T b) { return (a != a) | (a > b) ? a : b; }

// ============================================================================
// [mpsl::mpGetNan]
// ============================================================================

#if defined(MPSL_GET_NAN_F)
static MPSL_INLINE float mpGetNanF() { return MPSL_GET_NAN_F(); }
#undef MPSL_GET_NAN_F
#else
static MPSL_INLINE float mpGetNanF() { static const FloatBits value = { 0x7FC00000U }; return value.f; }
#endif // MPSL_GET_NAN_F

#if defined(MPSL_GET_NAN_D)
static MPSL_INLINE double mpGetNanD() { return MPSL_GET_NAN_D(); }
#undef MPSL_GET_NAN_D
#else
static MPSL_INLINE double mpGetNanD() { static const DoubleBits value = { MPSL_UINT64_C(0x7FF8000000000000) }; return value.d; }
#endif // MPSL_GET_NAN_D

// ============================================================================
// [mpsl::mpGetInf]
// ============================================================================

#if defined(MPSL_GET_INF_F)
static MPSL_INLINE float mpGetInfF() { return MPSL_GET_INF_F(); }
#undef MPSL_GET_INF_F
#else
static MPSL_INLINE float mpGetInfF() { static const FloatBits value = { 0x7F800000U }; return value.f; }
#endif // MPSL_GET_INF_F

#if defined(MPSL_GET_INF_D)
static MPSL_INLINE double mpGetInfD() { return MPSL_GET_INF_D(); }
#undef MPSL_GET_INF_D
#else
static MPSL_INLINE double mpGetInfD() { static const DoubleBits value = { MPSL_UINT64_C(0x7FF0000000000000) }; return value.d; }
#endif // MPSL_GET_INF_D

// ============================================================================
// [mpsl::mpIsNan]
// ============================================================================

#if defined(MPSL_IS_NAN_F)
static MPSL_INLINE bool mpIsNanF(float x) { return MPSL_IS_NAN_F(x); }
#undef MPSL_IS_NAN_F
#else
static MPSL_INLINE bool mpIsNanF(float x) { return FloatBits::fromFloat(x).isNan(); }
#endif // MPSL_IS_NAN_F

#if defined(MPSL_IS_NAN_D)
static MPSL_INLINE bool mpIsNanD(double x) { return MPSL_IS_NAN_D(x); }
#undef MPSL_IS_NAN_D
#else
static MPSL_INLINE bool mpIsNanD(double x) { return DoubleBits::fromDouble(x).isNan(); }
#endif // MPSL_IS_NAN_D

// ============================================================================
// [mpsl::mpIsInf]
// ============================================================================

#if defined(MPSL_IS_INF_F)
static MPSL_INLINE bool mpIsInfF(float x) { return MPSL_IS_INF_F(x); }
#undef MPSL_IS_INF_F
#else
static MPSL_INLINE bool mpIsInfF(float x) { return FloatBits::fromFloat(x).isInf(); }
#endif // MPSL_IS_INF_F

#if defined(MPSL_IS_INF_D)
static MPSL_INLINE bool mpIsInfD(double x) { return MPSL_IS_INF_D(x); }
#undef MPSL_IS_INF_D
#else
static MPSL_INLINE bool mpIsInfD(double x) { return DoubleBits::fromDouble(x).isInf(); }
#endif // MPSL_IS_INF_D

// ============================================================================
// [mpsl::mpIsFinite]
// ============================================================================

#if defined(MPSL_IS_FINITE_F)
static MPSL_INLINE bool mpIsFiniteF(float x) { return MPSL_IS_FINITE_F(x); }
#undef MPSL_IS_FINITE_F
#else
static MPSL_INLINE bool mpIsFiniteF(float x) { return FloatBits::fromFloat(x).isFinite(); }
#endif // MPSL_IS_FINITE_F

#if defined(MPSL_IS_FINITE_D)
static MPSL_INLINE bool mpIsFiniteD(double x) { return MPSL_IS_FINITE_D(x); }
#undef MPSL_IS_FINITE_D
#else
static MPSL_INLINE bool mpIsFiniteD(double x) { return DoubleBits::fromDouble(x).isFinite(); }
#endif // MPSL_IS_FINITE_D

// ============================================================================
// [mpsl::mpFloor / mpCeil / mpRound]
// ============================================================================

// ROUNDING
// ========
//
// +--------------------------+-------+-------+-------+-------+-------+-------+
// | Input                    | -0.9  | -0.5  | -0.1  |  0.1  |  0.5  |  0.9  |
// +--------------------------+-------+-------+-------+-------+-------+-------+
// | Floor                    | -1.0  | -1.0  | -1.0  |  0.0  |  0.0  |  0.0  |
// | Ceil                     |  0.0  |  0.0  |  0.0  |  1.0  |  1.0  |  1.0  |
// | Truncate                 |  0.0  |  0.0  |  0.0  |  0.0  |  0.0  |  0.0  |
// | Round to even            | -1.0  |  0.0  |  0.0  |  0.0  |  0.0  |  1.0  |
// | Round away from zero     | -1.0  | -1.0  |  0.0  |  0.0  |  1.0  |  1.0  |
// +--------------------------+-------+-------+-------+-------+-------+-------+

static MPSL_INLINE float mpTruncF(float x) { return ::truncf(x); }
static MPSL_INLINE double mpTruncD(double x) { return ::trunc(x); }

static MPSL_INLINE float mpFloorF(float x) { return ::floorf(x); }
static MPSL_INLINE double mpFloorD(double x) { return ::floor(x); }

static MPSL_INLINE float mpCeilF(float x) { return ::ceilf(x); }
static MPSL_INLINE double mpCeilD(double x) { return ::ceil(x); }

#if defined(_MSC_VER)
static MPSL_INLINE float mpRoundF(float x) { return x >= 0.0f ? mpFloorF(x + 0.5f) : mpCeilF(x - 0.5f); }
static MPSL_INLINE double mpRoundD(double x) { return x >= 0.0  ? mpFloorD(x + 0.5) : mpCeilD(x - 0.5); }
#else
static MPSL_INLINE float mpRoundF(float x) { return ::roundf(x); }
static MPSL_INLINE double mpRoundD(double x) { return ::round(x); }
#endif

static MPSL_INLINE float mpRoundEvenF(float x) { return ::rintf(x); }
static MPSL_INLINE double mpRoundEvenD(double x) { return ::rint(x); }

// ============================================================================
// [mpsl::mpSignBit]
// ============================================================================

static MPSL_INLINE bool mpSignBitF(float x) { return FloatBits::fromFloat(x).u >= 0x80000000U; }
static MPSL_INLINE bool mpSignBitD(double x) { return DoubleBits::fromDouble(x).hi >= 0x80000000U; }

// ============================================================================
// [mpsl::mpCopySign]
// ============================================================================

static MPSL_INLINE float mpCopySignF(float x, float y) {
  FloatBits bits = FloatBits::fromFloat(x);
  bits.u &= 0x7FFFFFFFU;
  bits.u |= FloatBits::fromFloat(y).u & 0x80000000U;
  return bits.f;
}

static MPSL_INLINE double mpCopySignD(double x, double y) {
  DoubleBits bits = DoubleBits::fromDouble(x);
  bits.hi &= 0x7FFFFFFFU;
  bits.hi |= DoubleBits::fromDouble(y).hi & 0x80000000U;
  return bits.d;
}

// ============================================================================
// [mpsl::mpFrac]
// ============================================================================

static MPSL_INLINE float mpFracF(float x) { return x - mpFloorF(x); }
static MPSL_INLINE double mpFracD(double x) { return x - mpFloorD(x); }

// ============================================================================
// [mpsl::mpNorm]
// ============================================================================

static MPSL_INLINE float mpNormF(float x) { return x + static_cast<float>(0); }
static MPSL_INLINE double mpNormD(double x) { return x + static_cast<double>(0); }

// ============================================================================
// [mpsl::mpMod]
// ============================================================================

static MPSL_INLINE float mpModF(float x, float y) { return fmodf(x, y); }
static MPSL_INLINE double mpModD(double x, double y) { return fmod(x, y); }

// ============================================================================
// [mpsl::mpAbs]
// ============================================================================

static MPSL_INLINE int32_t mpAbsI(int32_t x) { return static_cast<uint32_t>(x >= 0 ? x : -x); }
static MPSL_INLINE float mpAbsF(float x) { return ::fabsf(x); }
static MPSL_INLINE double mpAbsD(double x) { return ::fabs(x); }

// ============================================================================
// [mpsl::mpExp]
// ============================================================================

static MPSL_INLINE float mpExpF(float x) { return ::expf(x); }
static MPSL_INLINE double mpExpD(double x) { return ::exp(x); }

// ============================================================================
// [mpsl::mpPow]
// ============================================================================

static MPSL_INLINE float mpPowF(float x, float y) { return ::powf(x, y); }
static MPSL_INLINE double mpPowD(double x, double y) { return ::pow(x, y); }

// ============================================================================
// [mpsl::mpLog / mpLog2 / mpLog10]
// ============================================================================

static MPSL_INLINE float mpLogF(float x) { return ::logf(x); }
static MPSL_INLINE double mpLogD(double x) { return ::log(x); }

static MPSL_INLINE float mpLog2F(float x) { return ::log2f(x); }
static MPSL_INLINE double mpLog2D(double x) { return ::log2(x); }

static MPSL_INLINE float mpLog10F(float x) { return ::log10f(x); }
static MPSL_INLINE double mpLog10D(double x) { return ::log10(x); }

// ============================================================================
// [mpsl::mpSqrt]
// ============================================================================

static MPSL_INLINE float mpSqrtF(float x) { return ::sqrtf(x); }
static MPSL_INLINE double mpSqrtD(double x) { return ::sqrt(x); }

// ============================================================================
// [mpsl::mpSin / mpCos / mpTan]
// ============================================================================

static MPSL_INLINE float mpSinF(float x) { return ::sinf(x); }
static MPSL_INLINE double mpSinD(double x) { return ::sin(x); }

static MPSL_INLINE float mpCosF(float x) { return ::cosf(x); }
static MPSL_INLINE double mpCosD(double x) { return ::cos(x); }

static MPSL_INLINE float mpTanF(float x) { return ::tanf(x); }
static MPSL_INLINE double mpTanD(double x) { return ::tan(x); }

// ============================================================================
// [mpsl::mpAsin / mpAcos / mpAtan / mpAtan2]
// ============================================================================

static MPSL_INLINE float mpAsinF(float x) { return ::asinf(x); }
static MPSL_INLINE double mpAsinD(double x) { return ::asin(x); }

static MPSL_INLINE float mpAcosF(float x) { return ::acosf(x); }
static MPSL_INLINE double mpAcosD(double x) { return ::acos(x); }

static MPSL_INLINE float mpAtanF(float x) { return ::atanf(x); }
static MPSL_INLINE double mpAtanD(double x) { return ::atan(x); }

static MPSL_INLINE float mpAtan2F(float x, float y) { return ::atan2f(x, y); }
static MPSL_INLINE double mpAtan2D(double x, double y) { return ::atan2(x, y); }

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPMATH_P_H
