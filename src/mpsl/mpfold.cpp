// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MPSL_EXPORTS

// [Dependencies - MPSL]
#include "./mpfold_p.h"
#include "./mplang_p.h"
#include "./mpmath_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {
namespace Fold {

// ============================================================================
// [mpsl::Fold - Internal]
// ============================================================================

template<typename T>
static MPSL_INLINE T irol(T x, T y) noexcept {
  unsigned int bits = sizeof(T) * 8;
  unsigned int mask = bits - 1;

  unsigned int r = static_cast<unsigned int>(y & mask);
  unsigned int l = (bits - r) & mask;

  return (x << l) | (x >> r);
}

template<typename T>
static MPSL_INLINE T iror(T x, T y) noexcept {
  unsigned int bits = sizeof(T) * 8;
  unsigned int mask = bits - 1;

  unsigned int l = static_cast<unsigned int>(y & mask);
  unsigned int r = (bits - l) & mask;

  return (x << l) | (x >> r);
}

template<typename T>
static MPSL_INLINE T idiv(T x, T y) noexcept {
  double xd = static_cast<double>(x);
  double yd = static_cast<double>(y);
  return static_cast<T>(xd / yd);
}

template<typename T>
static MPSL_INLINE T imod(T x, T y) noexcept {
  double xd = static_cast<double>(x);
  double yd = static_cast<double>(y);
  return static_cast<T>(mpModD(xd, yd));
}

static MPSL_INLINE uint32_t lzcnt_kernel(uint32_t x) noexcept {
#if MPSL_CC_MSC_GE(14, 0, 0) && (MPSL_ARCH_X86 || MPSL_ARCH_X64 || MPSL_ARCH_ARM32 || MPSL_ARCH_ARM64)
  DWORD i;
  return _BitScanForward(&i, x) ? uint32_t(i) : uint32_t(0xFFFFFFFFU);
#elif MPSL_CC_GCC_GE(3, 4, 6) || MPSL_CC_CLANG
  return x ? uint32_t(__builtin_ctz(x)) : uint32_t(0xFFFFFFFFU);
#else
  uint32_t i = 1;
  while (x != 0) {
    uint32_t two = x & 0x3;
    if (two != 0x0)
      return i - (two & 0x1);
    i += 2;
    x >>= 2;
  }
  return 0xFFFFFFFFU;
#endif
}

static MPSL_INLINE uint32_t popcnt_kernel(uint32_t x) noexcept {
#if MPSL_CC_GCC || MPSL_CC_CLANG
  return __builtin_popcount(x);
#else
  // From: http://graphics.stanford.edu/~seander/bithacks.html
  x = x - ((x >> 1) & 0x55555555U);
  x = (x & 0x33333333U) + ((x >> 2) & 0x33333333U);
  return (((x + (x >> 4)) & 0x0F0F0F0FU) * 0x01010101U) >> 24;
#endif
}

static MPSL_INLINE uint32_t vmaddwd_kernel(uint32_t x, uint32_t y) noexcept {
  int32_t xLo = static_cast<int32_t>(x & 0xFFFFU);
  int32_t yLo = static_cast<int32_t>(y & 0xFFFFU);

  int32_t xHi = static_cast<int32_t>(x >> 16);
  int32_t yHi = static_cast<int32_t>(y >> 16);

  return static_cast<uint32_t>(xLo * yLo) + static_cast<uint32_t>(xHi * yHi);
}

static MPSL_INLINE void pshufd(void* _pd, const void* _pl, const void* _pr, uint32_t width) noexcept {
  uint32_t i = 0;
  uint32_t count = width / 4;

  uint32_t* pd = static_cast<uint32_t*>(_pd);
  const uint32_t* pl = static_cast<const uint32_t*>(_pl);
  uint32_t sel = static_cast<const uint32_t*>(_pr)[0];

  do {
    pd[i] = pl[sel & 0xF];
    sel >>= 4;
  } while (++i < count);
}

#define FOLD_FN2(fn, dsttype, srctype, worktype, ...)                          \
static MPSL_INLINE void fn(                                                    \
  void* _pd, const void* _ps, uint32_t width) noexcept {                       \
                                                                               \
  uint32_t i = 0;                                                              \
  uint32_t count = width / static_cast<uint32_t>(sizeof(dsttype));             \
                                                                               \
  dsttype* pd = static_cast<dsttype*>(_pd);                                    \
  const srctype* ps = static_cast<const srctype*>(_ps);                        \
                                                                               \
  do {                                                                         \
    worktype s = static_cast<worktype>(ps[i]);                                 \
    pd[i] = static_cast<dsttype>(__VA_ARGS__);                                 \
  } while (++i < count);                                                       \
}

#define FOLD_FN3(fn, dsttype, srctype, worktype, ...)                          \
static MPSL_INLINE void fn(                                                    \
  void* _pd, const void* _pl, const void* _pr, uint32_t width) noexcept {      \
                                                                               \
  uint32_t i = 0;                                                              \
  uint32_t count = width / static_cast<uint32_t>(sizeof(dsttype));             \
                                                                               \
  dsttype* pd = static_cast<dsttype*>(_pd);                                    \
  const srctype* pl = static_cast<const srctype*>(_pl);                        \
  const srctype* pr = static_cast<const srctype*>(_pr);                        \
                                                                               \
  do {                                                                         \
    worktype l = static_cast<worktype>(pl[i]);                                 \
    worktype r = static_cast<worktype>(pr[i]);                                 \
    pd[i] = static_cast<dsttype>(__VA_ARGS__);                                 \
  } while (++i < count);                                                       \
}

#define FOLD_IMM(fn, dsttype, srctype, worktype, ...)                          \
static MPSL_INLINE void fn(                                                    \
  void* _pd, const void* _pl, const void* _pr, uint32_t width) noexcept {      \
                                                                               \
  uint32_t i = 0;                                                              \
  uint32_t count = width / static_cast<uint32_t>(sizeof(dsttype));             \
                                                                               \
  dsttype* pd = static_cast<dsttype*>(_pd);                                    \
  const srctype* pl = static_cast<const srctype*>(_pl);                        \
  uint32_t r = static_cast<const uint32_t*>(_pr)[0];                           \
                                                                               \
  do {                                                                         \
    worktype l = static_cast<worktype>(pl[i]);                                 \
    pd[i] = static_cast<dsttype>(__VA_ARGS__);                                 \
  } while (++i < count);                                                       \
}

FOLD_FN2(pcopy32   , uint32_t, uint32_t, uint32_t, s)
FOLD_FN2(pcopy64   , uint64_t, uint64_t, uint64_t, s)

FOLD_FN2(pinci     , uint32_t, uint32_t, uint32_t, s + 1U  )
FOLD_FN2(fincf     , float   , float   , float   , s + 1.0f)
FOLD_FN2(fincd     , double  , double  , double  , s + 1.0 )

FOLD_FN2(pdeci     , uint32_t, uint32_t, uint32_t, s + 1U  )
FOLD_FN2(fdecf     , float   , float   , float   , s + 1.0f)
FOLD_FN2(fdecd     , double  , double  , double  , s + 1.0 )

FOLD_FN2(pnotd     , uint32_t, uint32_t, uint32_t, ~s)
FOLD_FN2(pnotq     , uint64_t, uint64_t, uint64_t, ~s)
FOLD_FN2(pnegd     , uint32_t, uint32_t, uint32_t, (~s) + static_cast<uint32_t>(1U))

FOLD_FN2(fisnanf   , uint32_t, float   , float   , mpIsNanF(s) ? kB32_0 : kB32_1)
FOLD_FN2(fisnand   , uint64_t, double  , double  , mpIsNanD(s) ? kB64_0 : kB64_1)
FOLD_FN2(fisinff   , uint32_t, float   , float   , mpIsInfF(s) ? kB32_0 : kB32_1)
FOLD_FN2(fisinfd   , uint64_t, double  , double  , mpIsInfD(s) ? kB64_0 : kB64_1)
FOLD_FN2(fisfinitef, uint32_t, float   , float   , mpIsFiniteF(s) ? kB32_0 : kB32_1)
FOLD_FN2(fisfinited, uint64_t, double  , double  , mpIsFiniteD(s) ? kB64_0 : kB64_1)

FOLD_FN2(fsignmaskf, int32_t , int32_t , int32_t , s >> 31)
FOLD_FN2(fsignmaskd, int64_t , int64_t , int64_t , s >> 63)

FOLD_FN2(ftruncf   , float   , float   , float   , mpTruncF(s))
FOLD_FN2(ftruncd   , double  , double  , double  , mpTruncD(s))
FOLD_FN2(ffloorf   , float   , float   , float   , mpFloorF(s))
FOLD_FN2(ffloord   , double  , double  , double  , mpFloorD(s))
FOLD_FN2(froundf   , float   , float   , float   , mpRoundF(s))
FOLD_FN2(froundd   , double  , double  , double  , mpRoundD(s))
FOLD_FN2(froundevenf,float   , float   , float   , mpRoundEvenF(s))
FOLD_FN2(froundevend,double  , double  , double  , mpRoundEvenD(s))
FOLD_FN2(fceilf    , float   , float   , float   , mpCeilF(s))
FOLD_FN2(fceild    , double  , double  , double  , mpCeilD(s))
FOLD_FN2(ffracf    , float   , float   , float   , mpFracF(s))
FOLD_FN2(ffracd    , double  , double  , double  , mpFracD(s))

FOLD_FN2(fabsf     , float   , float   , float   , mpAbsF(s))
FOLD_FN2(fabsd     , double  , double  , double  , mpAbsD(s))
FOLD_FN2(fnegf     , float   , float   , float   , mpNegF(s))
FOLD_FN2(fnegd     , double  , double  , double  , mpNegD(s))
FOLD_FN2(fexpf     , float   , float   , float   , mpExpF(s))
FOLD_FN2(fexpd     , double  , double  , double  , mpExpD(s))
FOLD_FN2(flogf     , float   , float   , float   , mpLogF(s))
FOLD_FN2(flogd     , double  , double  , double  , mpLogD(s))
FOLD_FN2(flog2f    , float   , float   , float   , mpLog2F(s))
FOLD_FN2(flog2d    , double  , double  , double  , mpLog2D(s))
FOLD_FN2(flog10f   , float   , float   , float   , mpLog10F(s))
FOLD_FN2(flog10d   , double  , double  , double  , mpLog10D(s))
FOLD_FN2(fsqrtf    , float   , float   , float   , mpSqrtF(s))
FOLD_FN2(fsqrtd    , double  , double  , double  , mpSqrtD(s))
FOLD_FN2(fsinf     , float   , float   , float   , mpSinF(s))
FOLD_FN2(fsind     , double  , double  , double  , mpSinD(s))
FOLD_FN2(fcosf     , float   , float   , float   , mpCosF(s))
FOLD_FN2(fcosd     , double  , double  , double  , mpCosD(s))
FOLD_FN2(ftanf     , float   , float   , float   , mpTanF(s))
FOLD_FN2(ftand     , double  , double  , double  , mpTanD(s))
FOLD_FN2(fasinf    , float   , float   , float   , mpAsinF(s))
FOLD_FN2(fasind    , double  , double  , double  , mpAsinD(s))
FOLD_FN2(facosf    , float   , float   , float   , mpAcosF(s))
FOLD_FN2(facosd    , double  , double  , double  , mpAcosD(s))
FOLD_FN2(fatanf    , float   , float   , float   , mpAtanF(s))
FOLD_FN2(fatand    , double  , double  , double  , mpAtanD(s))

FOLD_FN2(lzcnt     , uint32_t, uint32_t, uint32_t, lzcnt_kernel(s))
FOLD_FN2(popcnt    , uint32_t, uint32_t, uint32_t, popcnt_kernel(s))

FOLD_FN3(fcopysignf, float   , float   , float   , mpCopySignF(l, r))
FOLD_FN3(fcopysignd, double  , double  , double  , mpCopySignD(l, r))
FOLD_FN3(fpowf     , float   , float   , float   , mpPowF(l, r))
FOLD_FN3(fpowd     , double  , double  , double  , mpPowD(l, r))
FOLD_FN3(fatan2f   , float   , float   , float   , mpAtan2F(l, r))
FOLD_FN3(fatan2d   , double  , double  , double  , mpAtan2D(l, r))

FOLD_FN3(faddf     , float   , float   , float   , l + r)
FOLD_FN3(faddd     , double  , double  , double  , l + r)
FOLD_FN3(fsubf     , float   , float   , float   , l - r)
FOLD_FN3(fsubd     , double  , double  , double  , l - r)
FOLD_FN3(fmulf     , float   , float   , float   , l * r)
FOLD_FN3(fmuld     , double  , double  , double  , l * r)
FOLD_FN3(fdivf     , float   , float   , float   , l / r)
FOLD_FN3(fdivd     , double  , double  , double  , l / r)
FOLD_FN3(fmodf     , float   , float   , float   , mpModF(l, r))
FOLD_FN3(fmodd     , double  , double  , double  , mpModD(l, r))
FOLD_FN3(fandf     , uint32_t, uint32_t, uint32_t, l & r)
FOLD_FN3(fandd     , uint64_t, uint64_t, uint64_t, l & r)
FOLD_FN3(forf      , uint32_t, uint32_t, uint32_t, l | r)
FOLD_FN3(ford      , uint64_t, uint64_t, uint64_t, l | r)
FOLD_FN3(fxorf     , uint32_t, uint32_t, uint32_t, l ^ r)
FOLD_FN3(fxord     , uint64_t, uint64_t, uint64_t, l ^ r)
FOLD_FN3(fminf     , float   , float   , float   , l < r ? l : r)
FOLD_FN3(fmind     , double  , double  , double  , l < r ? l : r)
FOLD_FN3(fmaxf     , float   , float   , float   , l > r ? l : r)
FOLD_FN3(fmaxd     , double  , double  , double  , l > r ? l : r)

FOLD_FN3(fcmpeqf   , uint32_t, float   , float   , l == r ? kB32_0 : kB32_1)
FOLD_FN3(fcmpeqd   , uint64_t, double  , double  , l == r ? kB64_0 : kB64_1)
FOLD_FN3(fcmpnef   , uint32_t, float   , float   , l != r ? kB32_0 : kB32_1)
FOLD_FN3(fcmpned   , uint64_t, double  , double  , l != r ? kB64_0 : kB64_1)
FOLD_FN3(fcmpltf   , uint32_t, float   , float   , l <  r ? kB32_0 : kB32_1)
FOLD_FN3(fcmpltd   , uint64_t, double  , double  , l <  r ? kB64_0 : kB64_1)
FOLD_FN3(fcmplef   , uint32_t, float   , float   , l <= r ? kB32_0 : kB32_1)
FOLD_FN3(fcmpled   , uint64_t, double  , double  , l <= r ? kB64_0 : kB64_1)
FOLD_FN3(fcmpgtf   , uint32_t, float   , float   , l >  r ? kB32_0 : kB32_1)
FOLD_FN3(fcmpgtd   , uint64_t, double  , double  , l >  r ? kB64_0 : kB64_1)
FOLD_FN3(fcmpgef   , uint32_t, float   , float   , l >= r ? kB32_0 : kB32_1)
FOLD_FN3(fcmpged   , uint64_t, double  , double  , l >= r ? kB64_0 : kB64_1)

FOLD_FN2(pabsb     , int8_t  , int8_t  , int32_t , mpAbsI(s))
FOLD_FN2(pabsw     , int16_t , int16_t , int32_t , mpAbsI(s))
FOLD_FN2(pabsd     , int32_t , int32_t , int32_t , mpAbsI(s))

FOLD_FN3(pand      , uint32_t, uint32_t, uint32_t, l & r)
FOLD_FN3(por       , uint32_t, uint32_t, uint32_t, l | r)
FOLD_FN3(pxor      , uint32_t, uint32_t, uint32_t, l ^ r)

FOLD_FN3(paddb     , uint8_t , uint8_t , uint32_t, (l + r) & 0xFFU)
FOLD_FN3(paddw     , uint16_t, uint16_t, uint32_t, (l + r) & 0xFFFFU)
FOLD_FN3(paddd     , uint32_t, uint32_t, uint32_t, l + r)
FOLD_FN3(paddq     , uint64_t, uint64_t, uint64_t, l + r)
FOLD_FN3(paddssb   , int8_t  , int8_t  , int32_t , mpBound<int32_t>(l + r, -128, 127))
FOLD_FN3(paddusb   , uint8_t , uint8_t , uint32_t, mpMin<int32_t>(l + r, 255))
FOLD_FN3(paddssw   , int16_t , int16_t , int32_t , mpBound<int32_t>(l + r, -32768, 32767))
FOLD_FN3(paddusw   , uint16_t, uint16_t, uint32_t, mpMin<int32_t>(l + r, 65535))
FOLD_FN3(psubb     , uint8_t , uint8_t , uint32_t, (l - r) & 0xFFU)
FOLD_FN3(psubw     , uint16_t, uint16_t, uint32_t, (l - r) & 0xFFFFU)
FOLD_FN3(psubd     , uint32_t, uint32_t, uint32_t, l - r)
FOLD_FN3(psubq     , uint64_t, uint64_t, uint64_t, l - r)
FOLD_FN3(psubssb   , int8_t  , int8_t  , int32_t , mpBound<int32_t>(l - r, -128, 127))
FOLD_FN3(psubusb   , uint8_t , uint8_t , uint32_t, (l >= r) ? l - r : static_cast<uint32_t>(0))
FOLD_FN3(psubssw   , int16_t , int16_t , int32_t , mpBound<int32_t>(l - r, -32768, 32767))
FOLD_FN3(psubusw   , uint16_t, uint16_t, uint32_t, (l >= r) ? l - r : static_cast<uint32_t>(0))
FOLD_FN3(pmulw     , uint16_t, uint16_t, uint32_t, (l * r) & 0xFFFFU)
FOLD_FN3(pmulhsw   , int16_t , int16_t , int32_t , (l * r) >> 16)
FOLD_FN3(pmulhuw   , uint16_t, uint16_t, uint32_t, (l * r) >> 16)
FOLD_FN3(pmuld     , uint16_t, uint16_t, uint32_t, l * r)
FOLD_FN3(pdivsd    , int32_t , int32_t , int32_t , idiv(l, r))
FOLD_FN3(pmodsd    , int32_t , int32_t , int32_t , imod(l, r))
FOLD_FN3(pminsb    , int8_t  , int8_t  , int32_t , mpMin<int32_t>(l, r))
FOLD_FN3(pminub    , uint8_t , uint8_t , uint32_t, mpMin<uint32_t>(l, r))
FOLD_FN3(pminsw    , int16_t , int16_t , int32_t , mpMin<int32_t>(l, r))
FOLD_FN3(pminuw    , uint16_t, uint16_t, uint32_t, mpMin<uint32_t>(l, r))
FOLD_FN3(pminsd    , int32_t , int32_t , int32_t , mpMin<int32_t>(l, r))
FOLD_FN3(pminud    , uint32_t, uint32_t, uint32_t, mpMin<uint32_t>(l, r))
FOLD_FN3(pmaxsb    , int8_t  , int8_t  , int32_t , mpMax<int32_t>(l, r))
FOLD_FN3(pmaxub    , uint8_t , uint8_t , uint32_t, mpMax<uint32_t>(l, r))
FOLD_FN3(pmaxsw    , int16_t , int16_t , int32_t , mpMax<int32_t>(l, r))
FOLD_FN3(pmaxuw    , uint16_t, uint16_t, uint32_t, mpMax<uint32_t>(l, r))
FOLD_FN3(pmaxsd    , int32_t , int32_t , int32_t , mpMax<int32_t>(l, r))
FOLD_FN3(pmaxud    , uint32_t, uint32_t, uint32_t, mpMax<uint32_t>(l, r))
FOLD_IMM(psllw     , uint16_t, uint16_t, uint32_t, (l << r) & 0xFFFFU)
FOLD_IMM(psrlw     , uint16_t, uint16_t, uint32_t, (l >> r) & 0xFFFFU)
FOLD_IMM(psraw     , int16_t , int16_t , int32_t , (l >> r) & 0xFFFF)
FOLD_IMM(pslld     , uint32_t, uint32_t, uint32_t, l << r)
FOLD_IMM(psrld     , uint32_t, uint32_t, uint32_t, l >> r)
FOLD_IMM(psrad     , int32_t , int32_t , int32_t , l >> r)
FOLD_IMM(psllq     , uint64_t, uint64_t, uint64_t, l << r)
FOLD_IMM(psrlq     , uint64_t, uint64_t, uint64_t, l >> r)
FOLD_IMM(prold     , uint32_t, uint32_t, uint32_t, irol<uint32_t>(l, r))
FOLD_IMM(prord     , uint32_t, uint32_t, uint32_t, iror<uint32_t>(l, r))
FOLD_FN3(pmaddwd   , uint32_t, uint32_t, uint32_t, vmaddwd_kernel(l, r))
FOLD_FN3(pcmpeqb   , int8_t  , uint8_t , uint32_t, l == r ? -1 : 0)
FOLD_FN3(pcmpeqw   , int16_t , uint16_t, uint32_t, l == r ? -1 : 0)
FOLD_FN3(pcmpeqd   , int32_t , uint32_t, uint32_t, l == r ? -1 : 0)
FOLD_FN3(pcmpneb   , int8_t  , uint8_t , uint32_t, l != r ? -1 : 0)
FOLD_FN3(pcmpnew   , int16_t , uint16_t, uint32_t, l != r ? -1 : 0)
FOLD_FN3(pcmpned   , int32_t , uint32_t, uint32_t, l != r ? -1 : 0)
FOLD_FN3(pcmpltb   , int8_t  , int8_t  , int32_t , l <  r ? -1 : 0)
FOLD_FN3(pcmpltw   , int16_t , int16_t , int32_t , l <  r ? -1 : 0)
FOLD_FN3(pcmpltd   , int32_t , int32_t , int32_t , l <  r ? -1 : 0)
FOLD_FN3(pcmpleb   , int8_t  , int8_t  , int32_t , l <= r ? -1 : 0)
FOLD_FN3(pcmplew   , int16_t , int16_t , int32_t , l <= r ? -1 : 0)
FOLD_FN3(pcmpled   , int32_t , int32_t , int32_t , l <= r ? -1 : 0)
FOLD_FN3(pcmpgtb   , int8_t  , int8_t  , int32_t , l >  r ? -1 : 0)
FOLD_FN3(pcmpgtw   , int16_t , int16_t , int32_t , l >  r ? -1 : 0)
FOLD_FN3(pcmpgtd   , int32_t , int32_t , int32_t , l >  r ? -1 : 0)
FOLD_FN3(pcmpgeb   , int8_t  , int8_t  , int32_t , l >= r ? -1 : 0)
FOLD_FN3(pcmpgew   , int16_t , int16_t , int32_t , l >= r ? -1 : 0)
FOLD_FN3(pcmpged   , int32_t , int32_t , int32_t , l >= r ? -1 : 0)

#undef FOLD_IMM
#undef FOLD_FN3
#undef FOLD_FN2

static Error foldInternal(uint32_t instCode, uint32_t width, Value& dVal, const Value& sVal) noexcept {
  // Instruction code without SIMD flags.
  switch (instCode) {
    case kInstCodeMov32:
    case kInstCodeMov64:
    case kInstCodeMov128:
    case kInstCodeMov256:

    case kInstCodeCvtitof   :
    case kInstCodeCvtitod   :
    case kInstCodeCvtftoi   :
    case kInstCodeCvtftod   :
    case kInstCodeCvtdtoi   :
    case kInstCodeCvtdtof   :

    case kInstCodeAbsf      : fabsf(&dVal, &sVal, width); break;
    case kInstCodeAbsd      : fabsd(&dVal, &sVal, width); break;
    case kInstCodeBitnegi   :
    case kInstCodeBitnegf   : pnotd(&dVal, &sVal, width); break;
    case kInstCodeBitnegd   : pnotq(&dVal, &sVal, width); break;
    case kInstCodeNegi      : pnegd(&dVal, &sVal, width); break;
    case kInstCodeNegf      : fnegf(&dVal, &sVal, width); break;
    case kInstCodeNegd      : fnegd(&dVal, &sVal, width); break;
    case kInstCodeNoti      :
    case kInstCodeNotf      : pnotd(&dVal, &sVal, width); break;
    case kInstCodeNotd      : pnotq(&dVal, &sVal, width); break;

    case kInstCodeIsnanf    : fisnanf(&dVal, &sVal, width); break;
    case kInstCodeIsnand    : fisnand(&dVal, &sVal, width); break;
    case kInstCodeIsinff    : fisinff(&dVal, &sVal, width); break;
    case kInstCodeIsinfd    : fisinfd(&dVal, &sVal, width); break;
    case kInstCodeIsfinitef : fisfinitef(&dVal, &sVal, width); break;
    case kInstCodeIsfinited : fisfinited(&dVal, &sVal, width); break;
    case kInstCodeSignmaski :
    case kInstCodeSignmaskf : fsignmaskf(&dVal, &sVal, width); break;
    case kInstCodeSignmaskd : fsignmaskd(&dVal, &sVal, width); break;

    case kInstCodeTruncf    : ftruncf(&dVal, &sVal, width); break;
    case kInstCodeTruncd    : ftruncd(&dVal, &sVal, width); break;
    case kInstCodeFloorf    : ffloorf(&dVal, &sVal, width); break;
    case kInstCodeFloord    : ffloord(&dVal, &sVal, width); break;
    case kInstCodeRoundf    : froundf(&dVal, &sVal, width); break;
    case kInstCodeRoundd    : froundf(&dVal, &sVal, width); break;
    case kInstCodeRoundevenf: froundevenf(&dVal, &sVal, width); break;
    case kInstCodeRoundevend: froundevend(&dVal, &sVal, width); break;
    case kInstCodeCeilf     : fceilf(&dVal, &sVal, width); break;
    case kInstCodeCeild     : fceild(&dVal, &sVal, width); break;
    case kInstCodeFracf     : ffracf(&dVal, &sVal, width); break;
    case kInstCodeFracd     : ffracd(&dVal, &sVal, width); break;

    case kInstCodeSqrtf     : fsqrtf(&dVal, &sVal, width); break;
    case kInstCodeSqrtd     : fsqrtd(&dVal, &sVal, width); break;
    case kInstCodeExpf      : fexpf(&dVal, &sVal, width); break;
    case kInstCodeExpd      : fexpd(&dVal, &sVal, width); break;
    case kInstCodeLogf      : flogf(&dVal, &sVal, width); break;
    case kInstCodeLogd      : flogd(&dVal, &sVal, width); break;
    case kInstCodeLog2f     : flog2f(&dVal, &sVal, width); break;
    case kInstCodeLog2d     : flog2d(&dVal, &sVal, width); break;
    case kInstCodeLog10f    : flog10f(&dVal, &sVal, width); break;
    case kInstCodeLog10d    : flog10d(&dVal, &sVal, width); break;

    case kInstCodeSinf      : fsinf(&dVal, &sVal, width); break;
    case kInstCodeSind      : fsind(&dVal, &sVal, width); break;
    case kInstCodeCosf      : fcosf(&dVal, &sVal, width); break;
    case kInstCodeCosd      : fcosd(&dVal, &sVal, width); break;
    case kInstCodeTanf      : ftanf(&dVal, &sVal, width); break;
    case kInstCodeTand      : ftand(&dVal, &sVal, width); break;
    case kInstCodeAsinf     : fasinf(&dVal, &sVal, width); break;
    case kInstCodeAsind     : fasind(&dVal, &sVal, width); break;
    case kInstCodeAcosf     : facosf(&dVal, &sVal, width); break;
    case kInstCodeAcosd     : facosd(&dVal, &sVal, width); break;
    case kInstCodeAtanf     : fatanf(&dVal, &sVal, width); break;
    case kInstCodeAtand     : fatand(&dVal, &sVal, width); break;

    case kInstCodePabsb     : pabsb(&dVal, &sVal, width); break;
    case kInstCodePabsw     : pabsw(&dVal, &sVal, width); break;
    case kInstCodePabsd     : pabsd(&dVal, &sVal, width); break;
    case kInstCodeLzcnti    : lzcnt(&dVal, &sVal, width); break;
    case kInstCodePopcnti   : popcnt(&dVal, &sVal, width); break;

    default:
      return MPSL_TRACE_ERROR(kErrorInvalidState);
  }

  return kErrorOk;
}

static Error foldInternal(uint32_t instCode, uint32_t width, Value& dVal, const Value& lVal, const Value& rVal) noexcept {
  // Instruction code without SIMD flags.
  switch (instCode) {
    case kInstCodeAddf      : faddf(&dVal, &lVal, &rVal, width); break;
    case kInstCodeAddd      : faddd(&dVal, &lVal, &rVal, width); break;
    case kInstCodeSubf      : fsubf(&dVal, &lVal, &rVal, width); break;
    case kInstCodeSubd      : fsubd(&dVal, &lVal, &rVal, width); break;
    case kInstCodeMulf      : fmulf(&dVal, &lVal, &rVal, width); break;
    case kInstCodeMuld      : fmuld(&dVal, &lVal, &rVal, width); break;
    case kInstCodeDivf      : fdivf(&dVal, &lVal, &rVal, width); break;
    case kInstCodeDivd      : fdivd(&dVal, &lVal, &rVal, width); break;
    case kInstCodeModf      : fmodf(&dVal, &lVal, &rVal, width); break;
    case kInstCodeModd      : fmodd(&dVal, &lVal, &rVal, width); break;
    case kInstCodeAndi      : pand(&dVal, &lVal, &rVal, width); break;
    case kInstCodeAndf      : fandf(&dVal, &lVal, &rVal, width); break;
    case kInstCodeAndd      : fandd(&dVal, &lVal, &rVal, width); break;
    case kInstCodeOri       : por(&dVal, &lVal, &rVal, width); break;
    case kInstCodeOrf       : forf(&dVal, &lVal, &rVal, width); break;
    case kInstCodeOrd       : ford(&dVal, &lVal, &rVal, width); break;
    case kInstCodeXori      : pxor(&dVal, &lVal, &rVal, width); break;
    case kInstCodeXorf      : fxorf(&dVal, &lVal, &rVal, width); break;
    case kInstCodeXord      : fxord(&dVal, &lVal, &rVal, width); break;
    case kInstCodeMinf      : fminf(&dVal, &lVal, &rVal, width); break;
    case kInstCodeMind      : fmind(&dVal, &lVal, &rVal, width); break;
    case kInstCodeMaxf      : fmaxf(&dVal, &lVal, &rVal, width); break;
    case kInstCodeMaxd      : fmaxd(&dVal, &lVal, &rVal, width); break;
    case kInstCodeRoli      : prold(&dVal, &lVal, &rVal, width); break;
    case kInstCodeRori      : prord(&dVal, &lVal, &rVal, width); break;
    case kInstCodeCmpeqf    : fcmpeqf(&dVal, &lVal, &rVal, width); break;
    case kInstCodeCmpeqd    : fcmpeqd(&dVal, &lVal, &rVal, width); break;
    case kInstCodeCmpnef    : fcmpnef(&dVal, &lVal, &rVal, width); break;
    case kInstCodeCmpned    : fcmpned(&dVal, &lVal, &rVal, width); break;
    case kInstCodeCmpltf    : fcmpltf(&dVal, &lVal, &rVal, width); break;
    case kInstCodeCmpltd    : fcmpltd(&dVal, &lVal, &rVal, width); break;
    case kInstCodeCmplef    : fcmplef(&dVal, &lVal, &rVal, width); break;
    case kInstCodeCmpled    : fcmpled(&dVal, &lVal, &rVal, width); break;
    case kInstCodeCmpgtf    : fcmpgtf(&dVal, &lVal, &rVal, width); break;
    case kInstCodeCmpgtd    : fcmpgtd(&dVal, &lVal, &rVal, width); break;
    case kInstCodeCmpgef    : fcmpgef(&dVal, &lVal, &rVal, width); break;
    case kInstCodeCmpged    : fcmpged(&dVal, &lVal, &rVal, width); break;
    case kInstCodePshufd    : pshufd(&dVal, &lVal, &rVal, width); break;
    case kInstCodeCopysignf : fcopysignf(&dVal, &lVal, &rVal, width); break;
    case kInstCodeCopysignd : fcopysignd(&dVal, &lVal, &rVal, width); break;
    case kInstCodePowf      : fpowf(&dVal, &lVal, &rVal, width); break;
    case kInstCodePowd      : fpowd(&dVal, &lVal, &rVal, width); break;
    case kInstCodeAtan2f    : fatan2f(&dVal, &lVal, &rVal, width); break;
    case kInstCodeAtan2d    : fatan2d(&dVal, &lVal, &rVal, width); break;
    /*
    case kInstCodePmovsxbw  :
    case kInstCodePmovzxbw  :
    case kInstCodePmovsxwd  :
    case kInstCodePmovzxwd  :
    case kInstCodePacksswb  :
    case kInstCodePackuswb  :
    case kInstCodePackssdw  :
    case kInstCodePackusdw  :
    */
    case kInstCodePaddb     : paddb(&dVal, &lVal, &rVal, width); break;
    case kInstCodePaddw     : paddw(&dVal, &lVal, &rVal, width); break;
    case kInstCodePaddd     : paddd(&dVal, &lVal, &rVal, width); break;
    case kInstCodePaddq     : paddq(&dVal, &lVal, &rVal, width); break;
    case kInstCodePaddssb   : paddssb(&dVal, &lVal, &rVal, width); break;
    case kInstCodePaddusb   : paddusb(&dVal, &lVal, &rVal, width); break;
    case kInstCodePaddssw   : paddssw(&dVal, &lVal, &rVal, width); break;
    case kInstCodePaddusw   : paddusw(&dVal, &lVal, &rVal, width); break;
    case kInstCodePsubb     : psubb(&dVal, &lVal, &rVal, width); break;
    case kInstCodePsubw     : psubw(&dVal, &lVal, &rVal, width); break;
    case kInstCodePsubd     : psubd(&dVal, &lVal, &rVal, width); break;
    case kInstCodePsubq     : psubq(&dVal, &lVal, &rVal, width); break;
    case kInstCodePsubssb   : psubssb(&dVal, &lVal, &rVal, width); break;
    case kInstCodePsubusb   : psubusb(&dVal, &lVal, &rVal, width); break;
    case kInstCodePsubssw   : psubssw(&dVal, &lVal, &rVal, width); break;
    case kInstCodePsubusw   : psubusw(&dVal, &lVal, &rVal, width); break;
    case kInstCodePmulw     : pmulw(&dVal, &lVal, &rVal, width); break;
    case kInstCodePmulhsw   : pmulhsw(&dVal, &lVal, &rVal, width); break;
    case kInstCodePmulhuw   : pmulhuw(&dVal, &lVal, &rVal, width); break;
    case kInstCodePmuld     : pmuld(&dVal, &lVal, &rVal, width); break;
    case kInstCodePdivsd    : pdivsd(&dVal, &lVal, &rVal, width); break;
    case kInstCodePmodsd    : pmodsd(&dVal, &lVal, &rVal, width); break;
    case kInstCodePminsb    : pminsb(&dVal, &lVal, &rVal, width); break;
    case kInstCodePminub    : pminub(&dVal, &lVal, &rVal, width); break;
    case kInstCodePminsw    : pminsw(&dVal, &lVal, &rVal, width); break;
    case kInstCodePminuw    : pminuw(&dVal, &lVal, &rVal, width); break;
    case kInstCodePminsd    : pminsd(&dVal, &lVal, &rVal, width); break;
    case kInstCodePminud    : pminud(&dVal, &lVal, &rVal, width); break;
    case kInstCodePmaxsb    : pmaxsb(&dVal, &lVal, &rVal, width); break;
    case kInstCodePmaxub    : pmaxub(&dVal, &lVal, &rVal, width); break;
    case kInstCodePmaxsw    : pmaxsw(&dVal, &lVal, &rVal, width); break;
    case kInstCodePmaxuw    : pmaxuw(&dVal, &lVal, &rVal, width); break;
    case kInstCodePmaxsd    : pmaxsd(&dVal, &lVal, &rVal, width); break;
    case kInstCodePmaxud    : pmaxud(&dVal, &lVal, &rVal, width); break;
    case kInstCodePsllw     : psllw(&dVal, &lVal, &rVal, width); break;
    case kInstCodePsrlw     : psrlw(&dVal, &lVal, &rVal, width); break;
    case kInstCodePsraw     : psraw(&dVal, &lVal, &rVal, width); break;
    case kInstCodePslld     : pslld(&dVal, &lVal, &rVal, width); break;
    case kInstCodePsrld     : psrld(&dVal, &lVal, &rVal, width); break;
    case kInstCodePsrad     : psrad(&dVal, &lVal, &rVal, width); break;
    case kInstCodePsllq     : psllq(&dVal, &lVal, &rVal, width); break;
    case kInstCodePsrlq     : psrlq(&dVal, &lVal, &rVal, width); break;
    case kInstCodePmaddwd   : pmaddwd(&dVal, &lVal, &rVal, width); break;
    case kInstCodePcmpeqb   : pcmpeqb(&dVal, &lVal, &rVal, width); break;
    case kInstCodePcmpeqw   : pcmpeqw(&dVal, &lVal, &rVal, width); break;
    case kInstCodePcmpeqd   : pcmpeqd(&dVal, &lVal, &rVal, width); break;
    case kInstCodePcmpneb   : pcmpneb(&dVal, &lVal, &rVal, width); break;
    case kInstCodePcmpnew   : pcmpnew(&dVal, &lVal, &rVal, width); break;
    case kInstCodePcmpned   : pcmpned(&dVal, &lVal, &rVal, width); break;
    case kInstCodePcmpltb   : pcmpltb(&dVal, &lVal, &rVal, width); break;
    case kInstCodePcmpltw   : pcmpltw(&dVal, &lVal, &rVal, width); break;
    case kInstCodePcmpltd   : pcmpltd(&dVal, &lVal, &rVal, width); break;
    case kInstCodePcmpleb   : pcmpleb(&dVal, &lVal, &rVal, width); break;
    case kInstCodePcmplew   : pcmplew(&dVal, &lVal, &rVal, width); break;
    case kInstCodePcmpled   : pcmpled(&dVal, &lVal, &rVal, width); break;
    case kInstCodePcmpgtb   : pcmpgtb(&dVal, &lVal, &rVal, width); break;
    case kInstCodePcmpgtw   : pcmpgtw(&dVal, &lVal, &rVal, width); break;
    case kInstCodePcmpgtd   : pcmpgtd(&dVal, &lVal, &rVal, width); break;
    case kInstCodePcmpgeb   : pcmpgeb(&dVal, &lVal, &rVal, width); break;
    case kInstCodePcmpgew   : pcmpgew(&dVal, &lVal, &rVal, width); break;
    case kInstCodePcmpged   : pcmpged(&dVal, &lVal, &rVal, width); break;

    default:
      return MPSL_TRACE_ERROR(kErrorInvalidState);
  }

  return kErrorOk;
}

// ============================================================================
// [mpsl::Fold - AST-Based Constant Folding]
// ============================================================================

Error foldCast(
  Value& dVal, uint32_t dTypeInfo,
  const Value& sVal, uint32_t sTypeInfo) noexcept {

  uint32_t dTypeId = dTypeInfo & kTypeIdMask;
  uint32_t sTypeId = sTypeInfo & kTypeIdMask;

  uint32_t i = 0;
  uint32_t count = TypeInfo::elementsOf(dTypeInfo);

  if (count != TypeInfo::elementsOf(sTypeInfo))
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  Value out;
  out.zero();

#define COMB(dstTypeId, srcTypeId) (((dstTypeId) << 4) | (srcTypeId))
  do {
    switch (COMB(dTypeId, sTypeId)) {
      // 32-bit copy.
      case COMB(kTypeBool  , kTypeBool  ):
      case COMB(kTypeInt   , kTypeInt   ):
      case COMB(kTypeFloat , kTypeFloat ): out.u[i] = sVal.u[i]; break;

      // 64-bit copy.
      case COMB(kTypeQBool , kTypeQBool):
      case COMB(kTypeDouble, kTypeDouble): out.q[i] = sVal.q[i]; break;

      // Cast to a boolean.
      case COMB(kTypeBool  , kTypeQBool ): out.u[i] = sVal.q[i] ? kB32_1 : kB32_0; break;
      case COMB(kTypeQBool , kTypeBool  ): out.u[i] = sVal.u[i] ? kB64_1 : kB64_0; break;

      // Cast to an integer.
      case COMB(kTypeInt   , kTypeBool  ): out.i[i] = sVal.u[i] ? 1 : 0; break;
      case COMB(kTypeInt   , kTypeQBool ): out.i[i] = sVal.q[i] ? 1 : 0; break;
      case COMB(kTypeInt   , kTypeFloat ): out.i[i] = static_cast<int>(sVal.f[i]); break;
      case COMB(kTypeInt   , kTypeDouble): out.i[i] = static_cast<int>(sVal.d[i]); break;

      // Cast to float.
      case COMB(kTypeFloat , kTypeBool  ): out.f[i] = sVal.u[i] ? 1.0f : 0.0f; break;
      case COMB(kTypeFloat , kTypeQBool ): out.f[i] = sVal.q[i] ? 1.0f : 0.0f; break;
      case COMB(kTypeFloat , kTypeInt   ): out.f[i] = static_cast<float>(sVal.i[i]); break;
      case COMB(kTypeFloat , kTypeDouble): out.f[i] = static_cast<float>(sVal.d[i]); break;

      // Cast to double.
      case COMB(kTypeDouble, kTypeBool  ): out.d[i] = sVal.u[i] ? 1.0 : 0.0; break;
      case COMB(kTypeDouble, kTypeQBool ): out.d[i] = sVal.q[i] ? 1.0 : 0.0; break;
      case COMB(kTypeDouble, kTypeInt   ): out.d[i] = static_cast<double>(sVal.i[i]); break;
      case COMB(kTypeDouble, kTypeFloat ): out.d[i] = static_cast<double>(sVal.f[i]); break;

      default:
        return MPSL_TRACE_ERROR(kErrorInvalidState);
    }
  } while (++i < count);
#undef COMB

  dVal = out;
  return kErrorOk;
}

Error foldSwizzle(const uint8_t* swizzleArray, Value& dVal, const Value& sVal, uint32_t sTypeInfo) noexcept {
  uint32_t typeId = sTypeInfo & kTypeIdMask;
  uint32_t i = 0;
  uint32_t count = TypeInfo::elementsOf(sTypeInfo);

  Value out;
  out.zero();

  switch (TypeInfo::sizeOf(typeId)) {
    case 4:
      do {
        uint32_t sIdx = swizzleArray[i];
        if (MPSL_UNLIKELY(sIdx >= 8))
          return MPSL_TRACE_ERROR(kErrorInvalidState);
        out.i[i] = sVal.i[sIdx];
      } while (++i < count);
      break;

    case 8:
      do {
        uint32_t sIdx = swizzleArray[i];
        if (MPSL_UNLIKELY(sIdx >= 4))
          return MPSL_TRACE_ERROR(kErrorInvalidState);
        out.q[i] = sVal.q[sIdx];
      } while (++i < count);
      break;

    default:
      return MPSL_TRACE_ERROR(kErrorInvalidState);
  }

  dVal = out;
  return kErrorOk;
}

Error foldUnaryOp(uint32_t op, Value& dVal, const Value& sVal, uint32_t sTypeInfo) noexcept {
  uint32_t typeId = sTypeInfo & kTypeIdMask;
  uint32_t width = TypeInfo::widthOf(sTypeInfo);

  Value out;
  out.zero();

#define COMB(op, typeId) (((op) << 8) | (typeId))
  switch (COMB(op, typeId)) {
    // Operators that look differently at IR level have to be implemented here.
    case COMB(kOpPreInc   , kTypeInt   ):
    case COMB(kOpPostInc  , kTypeInt   ): pinci(&out, &sVal, width); break;
    case COMB(kOpPreInc   , kTypeFloat ):
    case COMB(kOpPostInc  , kTypeFloat ): fincf(&out, &sVal, width); break;
    case COMB(kOpPreInc   , kTypeDouble):
    case COMB(kOpPostInc  , kTypeDouble): fincd(&out, &sVal, width); break;

    case COMB(kOpPreDec   , kTypeInt   ):
    case COMB(kOpPostDec  , kTypeInt   ): pdeci(&out, &sVal, width); break;
    case COMB(kOpPreDec   , kTypeFloat ):
    case COMB(kOpPostDec  , kTypeFloat ): fdecf(&out, &sVal, width); break;
    case COMB(kOpPreDec   , kTypeDouble):
    case COMB(kOpPostDec  , kTypeDouble): fdecd(&out, &sVal, width); break;

    default: {
      uint32_t code = OpInfo::get(op).getInstByTypeId(typeId);
      if (code == kInstCodeNone)
        return MPSL_TRACE_ERROR(kErrorInvalidState);

      MPSL_PROPAGATE(foldInternal(code, width, out, sVal));
      break;
    }
  }
#undef COMB

  dVal = out;
  return kErrorOk;
}

Error foldBinaryOp(uint32_t op, Value& dVal,
  const Value& lVal, uint32_t lTypeInfo,
  const Value& rVal, uint32_t rTypeInfo) noexcept {

  // Keep only the operator type without `Assign` part (except for `=`).
  op = OpInfo::get(op).altType;

  uint32_t typeId = lTypeInfo & kTypeIdMask;
  uint32_t width = TypeInfo::widthOf(lTypeInfo);

  Value out;
  out.zero();

#define COMB(op, typeId) (((op) << 8) | (typeId))
  switch (COMB(op, typeId)) {
    // Operators that look differently at IR level have to be implemented here.
    case COMB(kOpAssign, kTypeBool  ):
    case COMB(kOpAssign, kTypeInt   ):
    case COMB(kOpAssign, kTypeFloat ): pcopy32(&out, &rVal, width); break;
    case COMB(kOpAssign, kTypeQBool ):
    case COMB(kOpAssign, kTypeDouble): pcopy64(&out, &rVal, width); break;

    case COMB(kOpLogAnd, kTypeBool  ): fandf(&out, &lVal, &rVal, width); break;
    case COMB(kOpLogAnd, kTypeQBool ): fandd(&out, &lVal, &rVal, width); break;
    case COMB(kOpLogOr , kTypeBool  ): forf(&out, &lVal, &rVal, width); break;
    case COMB(kOpLogOr , kTypeQBool ): ford(&out, &lVal, &rVal, width); break;

    default: {
      uint32_t code = OpInfo::get(op).getInstByTypeId(typeId);
      if (code == kInstCodeNone)
        return MPSL_TRACE_ERROR(kErrorInvalidState);

      MPSL_PROPAGATE(foldInternal(code, width, out, lVal, rVal));
      break;
    }
  }
#undef COMB

  dVal = out;
  return kErrorOk;
}

// ============================================================================
// [mpsl::Fold - IR-Based Constant Folding]
// ============================================================================

Error foldInst(uint32_t inst, Value& dVal, const Value& sVal) noexcept {
  Value out;
  out.zero();

  MPSL_PROPAGATE(
    foldInternal(inst & kInstCodeMask, InstInfo::widthOf(inst), out, sVal));

  dVal = out;
  return kErrorOk;
}

Error foldInst(uint32_t inst, Value& dVal, const Value& lVal, const Value& rVal) noexcept {
  Value out;
  out.zero();

  MPSL_PROPAGATE(
    foldInternal(inst & kInstCodeMask, InstInfo::widthOf(inst), out, lVal, rVal));

  dVal = out;
  return kErrorOk;
}

} // Fold namespace
} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
