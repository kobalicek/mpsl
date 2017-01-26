// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPSL_P_H
#define _MPSL_MPSL_P_H

// [Dependencies - MPSL]
#include "./mpsl.h"

// [Dependencies - C]
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// [Dependencies - C++]
#include <new>

// [Dependencies - AsmJit]
#include <asmjit/asmjit.h>

// [Dependencies - SSE2]
#if MPSL_ARCH_X64 || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(__SSE2__)
# define MPSL_USE_SSE2 1
#else
# define MPSL_USE_SSE2 0
#endif

#if MPSL_USE_SSE2
# include <xmmintrin.h>
# include <emmintrin.h>
#endif

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [MPSL_ASSERT]
// // ============================================================================

//! \internal
//! \def MPSL_ASSERT

#if defined(MPSL_DEBUG)
# define MPSL_ASSERT(exp) \
  do { \
    if (MPSL_UNLIKELY(!(exp))) \
      ::mpsl::mpAssertionFailed(#exp, __FILE__, __LINE__); \
  } while (0)
#else
# define MPSL_ASSERT(exp) ((void)0)
#endif

// ============================================================================
// [MPSL_TRACE_ERROR]
// ============================================================================

#if defined(MPSL_DEBUG)
# define MPSL_TRACE_ERROR(err) ::mpsl::mpTraceError(err)
#else
# define MPSL_TRACE_ERROR(err) static_cast< ::mpsl::Error >(err)
#endif

// ============================================================================
// [MPSL_PROPAGATE]
// ============================================================================

//! \internal
#define MPSL_PROPAGATE(...) \
  do { \
    ::mpsl::Error _errorValue = __VA_ARGS__; \
    if (MPSL_UNLIKELY(_errorValue != ::mpsl::kErrorOk)) \
      return _errorValue; \
  } while (0)

//! \internal
#define MPSL_PROPAGATE_(exp, cleanup) \
  do { \
    ::mpsl::Error _errorCode = (exp); \
    if (MPSL_UNLIKELY(_errorCode != ::mpsl::kErrorOk)) { \
      cleanup \
      return _errorCode; \
    } \
  } while (0)

//! \internal
#define MPSL_NULLCHECK(ptr) \
  do { \
    if (MPSL_UNLIKELY(!(ptr))) \
      return MPSL_TRACE_ERROR(::mpsl::kErrorNoMemory); \
  } while (0)

//! \internal
#define MPSL_NULLCHECK_(ptr, cleanup) \
  do { \
    if (MPSL_UNLIKELY(!(ptr))) { \
      cleanup \
      return MPSL_TRACE_ERROR(::mpsl::kErrorNoMemory); \
    } \
  } while (0)

// ============================================================================
// [Reuse]
// ============================================================================

// Reuse these classes - we depend on asmjit anyway and these are internal.
using asmjit::Lock;
using asmjit::AutoLock;

using asmjit::StringBuilder;
using asmjit::StringBuilderTmp;

using asmjit::Zone;
using asmjit::ZoneHeap;
using asmjit::ZoneVector;

// ============================================================================
// [mpsl::Miscellaneous]
// ============================================================================

enum { kInvalidDataSlot = 0xFF };
enum { kInvalidRegId = asmjit::Globals::kInvalidRegId };
enum { kPointerWidth = static_cast<int>(sizeof(void*)) };

static const uint32_t kB32_0 = 0x00000000U;
static const uint32_t kB32_1 = 0xFFFFFFFFU;

static const uint64_t kB64_0 = ASMJIT_UINT64_C(0x0000000000000000);
static const uint64_t kB64_1 = ASMJIT_UINT64_C(0xFFFFFFFFFFFFFFFF);

// ============================================================================
// [mpsl::InternalOptions]
// ============================================================================

//! \internal
//!
//! Compilation options MPSL uses internally.
enum InternalOptions {
  //! Set if `OutputLog` is present. MPSL then checks only this flag to use it.
  kInternalOptionLog = 0x10000000
};

// ============================================================================
// [mpsl::mpAssertionFailed]
// ============================================================================

//! \internal
MPSL_NOAPI void mpAssertionFailed(const char* exp, const char* file, int line) noexcept;

// ============================================================================
// [mpsl::mpTraceError]
// ============================================================================

//! \internal
//!
//! Error tracking available in debug-mode.
//!
//! The mpTraceError function can be used to track any error reported by MPSL.
//! Put a breakpoint inside mpTraceError and the program execution will be
//! stopped immediately after the error is created.
MPSL_NOAPI Error mpTraceError(Error error) noexcept;

// ============================================================================
// [mpsl::RuntimeData]
// ============================================================================

class RuntimeData {
public:
  MPSL_NONCOPYABLE(RuntimeData)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE RuntimeData() noexcept
    : _refCount(1),
      _runtime() {}
  MPSL_INLINE ~RuntimeData() noexcept {}

  // --------------------------------------------------------------------------
  // [Internal]
  // --------------------------------------------------------------------------

  MPSL_INLINE void destroy() noexcept {
    this->~RuntimeData();
    ::free(this);
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE asmjit::JitRuntime* getRuntime() const noexcept {
    return const_cast<asmjit::JitRuntime*>(&_runtime);
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uintptr_t _refCount;                   //!< Reference count.
  asmjit::JitRuntime _runtime;           //!< JIT runtime.
};

// ============================================================================
// [mpsl::ErrorReporter]
// ============================================================================

//! Error reporter.
struct ErrorReporter {
  MPSL_INLINE ErrorReporter(const char* body, size_t len, uint32_t options, OutputLog* log) noexcept
    : _body(body),
      _len(len),
      _options(options),
      _log(log) {

    // These should be handled by MPSL before the `ErrorReporter` is created.
    MPSL_ASSERT((log == nullptr && (_options & kInternalOptionLog) == 0) ||
                (log != nullptr && (_options & kInternalOptionLog) != 0) );
  }

  // --------------------------------------------------------------------------
  // [Error Handling]
  // --------------------------------------------------------------------------

  MPSL_INLINE bool reportsErrors() const noexcept { return (_options & kInternalOptionLog) != 0; }
  MPSL_INLINE bool reportsWarnings() const noexcept { return (_options & kOptionVerbose) != 0; }

  void getLineAndColumn(uint32_t position, uint32_t& line, uint32_t& column) noexcept;

  void onWarning(uint32_t position, const char* fmt, ...) noexcept;
  void onWarning(uint32_t position, const StringBuilder& msg) noexcept;

  Error onError(Error error, uint32_t position, const char* fmt, ...) noexcept;
  Error onError(Error error, uint32_t position, const StringBuilder& msg) noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  const char* _body;
  size_t _len;

  uint32_t _options;
  OutputLog* _log;
};

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPSL_P_H
