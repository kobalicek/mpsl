// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPATOMIC_P_H
#define _MPSL_MPATOMIC_P_H

// [Dependencies - MPSL]
#include "./mpsl_p.h"

// [Dependencies - MSC]
#if defined(_MSC_VER) && _MSC_VER >= 1400
#include <intrin.h>
#endif // _MSC_VER >= 1400

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [mpsl::mpAtomic]
// ============================================================================

//! \internal
static MPSL_INLINE uintptr_t mpAtomicGet(const uintptr_t* atomic) noexcept {
  return *(const uintptr_t volatile *)atomic;
}

//! \internal
static MPSL_INLINE void mpAtomicSet(uintptr_t* atomic, size_t value) noexcept {
  *(uintptr_t volatile *)atomic = value;
}

#if defined(_MSC_VER)
# if defined(__x86_64__) || defined(_WIN64) || defined(_M_IA64) || defined(_M_X64)
#  pragma intrinsic (_InterlockedIncrement64)
#  pragma intrinsic (_InterlockedDecrement64)
#  pragma intrinsic (_InterlockedExchange64)
//! \internal
static MPSL_INLINE uintptr_t mpAtomicSetXchg(uintptr_t* atomic, uintptr_t value) noexcept {
  return _InterlockedExchange64((__int64 volatile *)atomic, static_cast<__int64>(value));
};
//! \internal
static MPSL_INLINE uintptr_t mpAtomicInc(uintptr_t* atomic) noexcept {
  return _InterlockedIncrement64((__int64 volatile *)atomic);
};
//! \internal
static MPSL_INLINE uintptr_t mpAtomicDec(uintptr_t* atomic) noexcept {
  return _InterlockedDecrement64((__int64 volatile *)&atomic);
}
# else
#  pragma intrinsic (_InterlockedIncrement)
#  pragma intrinsic (_InterlockedDecrement)
#  pragma intrinsic (_InterlockedExchange)
//! \internal
static MPSL_INLINE uintptr_t mpAtomicSetXchg(uintptr_t* atomic, uintptr_t value) noexcept {
  return _InterlockedExchange((long volatile *)atomic, static_cast<long>(value));
};
//! \internal
static MPSL_INLINE uintptr_t mpAtomicInc(uintptr_t* atomic) noexcept {
  return _InterlockedIncrement((long volatile *)atomic);
}
//! \internal
static MPSL_INLINE uintptr_t mpAtomicDec(uintptr_t* atomic) noexcept {
  return _InterlockedDecrement((long volatile *)atomic);
}
# endif // _64BIT
#endif // _MSC_VER

#if defined(__GNUC__) || defined(__clang__)
//! \internal
static MPSL_INLINE uintptr_t mpAtomicSetXchg(uintptr_t* atomic, uintptr_t value) noexcept {
  return __sync_lock_test_and_set(atomic, value);
};
//! \internal
static MPSL_INLINE uintptr_t mpAtomicInc(uintptr_t* atomic) noexcept {
  return __sync_fetch_and_add(atomic, 1);
}
//! \internal
static MPSL_INLINE uintptr_t mpAtomicDec(uintptr_t* atomic) noexcept {
  return __sync_fetch_and_sub(atomic, 1);
}
#endif // __GNUC__

template<typename T>
MPSL_INLINE T mpAtomicSetXchgT(T* atomic, T value) noexcept {
  return (T)mpAtomicSetXchg((uintptr_t *)atomic, (uintptr_t)value);
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPATOMIC_P_H
