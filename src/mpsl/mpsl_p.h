// [MPSL]
// Shader-Like Mathematical Expression JIT Engine for C++.
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

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [MPSL_ASSERT]
// ============================================================================

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
# define MPSL_TRACE_ERROR(err) err
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

// ============================================================================
// [mpsl::Miscellaneous]
// ============================================================================

enum { kInvalidDataSlot = 0xFF };
enum { kInvalidValue = asmjit::kInvalidValue };
enum { kPointerWidth = static_cast<int>(sizeof(void*)) };

static const uint32_t kB32_0 = 0x00000000U;
static const uint32_t kB32_1 = 0xFFFFFFFFU;

static const uint64_t kB64_0 = MPSL_UINT64_C(0x0000000000000000);
static const uint64_t kB64_1 = MPSL_UINT64_C(0xFFFFFFFFFFFFFFFF);

// ============================================================================
// [mpsl::TypeFlags]
// ============================================================================

//! \internal
//!
//! Type flags.
enum TypeFlags {
  kTypeFlagBool     = 0x01,
  kTypeFlagInt      = 0x02,
  kTypeFlagReal     = 0x08,
  kTypeFlagObject   = 0x10
};

// ============================================================================
// [mpsl::OpType]
// ============================================================================

//! \internal
//!
//! Operator type.
enum OpType {
  kOpNone = 0,

  kOpCast,              // (a)b

  kOpPreInc,            // ++a
  kOpPreDec,            // --a
  kOpPostInc,           // a++
  kOpPostDec,           // a--

  kOpBitNeg,            // ~a
  kOpNeg,               // -a
  kOpNot,               // !a

  kOpIsNan,             // isnan(a)
  kOpIsInf,             // isinf(a)
  kOpIsFinite,          // isfinite(a)
  kOpSignBit,           // signbit(a)

  kOpRound,             // round(a)
  kOpRoundEven,         // roundeven(a)
  kOpTrunc,             // trunc(a)
  kOpFloor,             // floor(a)
  kOpCeil,              // ceil(a)

  kOpAbs,               // abs(a)
  kOpExp,               // exp(a)
  kOpLog,               // log(a)
  kOpLog2,              // log2(a)
  kOpLog10,             // log10(a)
  kOpSqrt,              // sqrt(a)
  kOpFrac,              // frac(a)

  kOpSin,               // sin(a)
  kOpCos,               // cos(a)
  kOpTan,               // tan(a)
  kOpAsin,              // asin(a)
  kOpAcos,              // acos(a)
  kOpAtan,              // atan(a)

  kOpAssign,            // a = b
  kOpAssignAdd,         // a += b
  kOpAssignSub,         // a -= b
  kOpAssignMul,         // a *= b
  kOpAssignDiv,         // a /= b
  kOpAssignMod,         // a %= b
  kOpAssignBitAnd,      // a &= b
  kOpAssignBitOr,       // a |= b
  kOpAssignBitXor,      // a ^= b
  kOpAssignBitSar,      // a >>= b
  kOpAssignBitShr,      // a >>>= b
  kOpAssignBitShl,      // a <<= b

  kOpEq,                // a == b
  kOpNe,                // a != b
  kOpLt,                // a <  b
  kOpLe,                // a <= b
  kOpGt,                // a >  b
  kOpGe,                // a >= b

  kOpLogAnd,            // a && b
  kOpLogOr,             // a || b

  kOpAdd,               // a + b
  kOpSub,               // a - b
  kOpMul,               // a * b
  kOpDiv,               // a / b
  kOpMod,               // a % b

  kOpBitAnd,            // a & b
  kOpBitOr,             // a | b
  kOpBitXor,            // a ^ b
  kOpBitSar,            // a >> b
  kOpBitShr,            // a >>> b
  kOpBitShl,            // a << b
  kOpBitRor,            // ror(a, b)
  kOpBitRol,            // rol(a, b)

  kOpMin,               // min(a, b)
  kOpMax,               // max(a, b)
  kOpPow,               // pow(a, b)
  kOpAtan2,             // atan2(a, b)
  kOpCopySign,          // copysign(a, b)

  //! \internal
  //!
  //! Count of operators.
  kOpCount
};

// ============================================================================
// [mpsl::OpFlags]
// ============================================================================

//! Operator flags.
enum OpFlags {
  //! The operator has one parameter (unary node).
  kOpFlagUnary         = 0x00000001,
  //! The operator has two parameters (binary node).
  kOpFlagBinary        = 0x00000002,
  //! The operator is an intrinsic function.
  kOpFlagIntrinsic     = 0x00000004,
  //! The operator has right-to-left associativity.
  kOpFlagRightToLeft   = 0x00000008,

  //! The operator does an assignment to a variable.
  kOpFlagAssign        = 0x00000010,
  //! The operator is a postfix operator (.)++ or (.)--.
  kOpFlagAssignPost    = 0x00000020,

  //! The operator performs an arithmetic operation.
  kOpFlagArithmetic    = 0x00000100,
  //! The operator performs a logical operation - `&&` and `||`.
  kOpFlagLogical       = 0x00000200,
  //! The operator performs a conditional operation (the result is a `__bool`).
  kOpFlagCondition     = 0x00000400,

  //! The operator performs bit masking (integer-only).
  kOpFlagBitMask       = 0x00001000,
  //! The operator performs bit shifting/rotation (integer-only).
  kOpFlagBitShift      = 0x00002000,
  //! Combination of `kOpFlagBitMask` and `kOpFlagBitShift`.
  kOpFlagBitwise       = 0x00003000,

  //! The operator works with floating-point only.
  kOpFlagFloatOnly     = 0x00010000,
  //! The operator performs a floating-point rounding (float-only)
  kOpFlagRounding      = 0x00020000,
  //! The operator calculates a trigonometric function (float-only)
  kOpFlagTrigonometric = 0x00040000,

  kOpFlagNopIfLZero    = 0x10000000,
  kOpFlagNopIfRZero    = 0x20000000,
  kOpFlagNopIfLOne     = 0x40000000,
  kOpFlagNopIfROne     = 0x80000000,

  kOpFlagNopIfZero     = kOpFlagNopIfLZero | kOpFlagNopIfRZero,
  kOpFlagNopIfOne      = kOpFlagNopIfLOne  | kOpFlagNopIfROne
};

// ============================================================================
// [mpsl::InternalOptions]
// ============================================================================

//! \internal
//!
//! Compilation options MPSL uses internally.
enum InternalOptions {
  //! Set if `OutputLog` is present. MPSL then checks only this flag to use it.
  kInternalOptionLog = 0x00010000
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
// [mpsl::TypeInfo]
// ============================================================================

struct TypeInfo {
  // --------------------------------------------------------------------------
  // [Statics]
  // --------------------------------------------------------------------------

  static MPSL_INLINE const TypeInfo& get(uint32_t id) noexcept;
  static MPSL_INLINE uint32_t getSize(uint32_t id) noexcept { return get(id).size; }

  static MPSL_INLINE bool isBoolId(uint32_t id) noexcept { return (get(id).flags & kTypeFlagBool) != 0; }
  static MPSL_INLINE bool isBoolType(uint32_t ti) noexcept { return isBoolId(ti & kTypeIdMask); }

  static MPSL_INLINE bool isIntId(uint32_t id) noexcept { return (get(id).flags & kTypeFlagInt) != 0; }
  static MPSL_INLINE bool isIntType(uint32_t ti) noexcept { return isIntId(ti & kTypeIdMask); }

  static MPSL_INLINE bool isRealId(uint32_t id) noexcept { return (get(id).flags & kTypeFlagReal) != 0; }
  static MPSL_INLINE bool isRealType(uint32_t ti) noexcept { return isRealId(ti & kTypeIdMask); }

  static MPSL_INLINE bool isObjectId(uint32_t id) noexcept { return (get(id).flags & (kTypeFlagObject)) != 0; }
  static MPSL_INLINE bool isObjectType(uint32_t ti) noexcept { return isObjectId(ti & kTypeIdMask); }

  static MPSL_INLINE bool isIntOrBoolId(uint32_t id) noexcept { return (get(id).flags & (kTypeFlagInt | kTypeFlagBool)) != 0; }
  static MPSL_INLINE bool isIntOrBoolType(uint32_t ti) noexcept { return isIntOrBoolId(ti & kTypeIdMask); }

  static MPSL_INLINE bool isIntOrRealId(uint32_t id) noexcept { return (get(id).flags & (kTypeFlagInt | kTypeFlagReal)) != 0; }
  static MPSL_INLINE bool isIntOrRealType(uint32_t ti) noexcept { return isIntOrRealId(ti & kTypeIdMask); }

  static MPSL_INLINE bool isVectorType(uint32_t ti) noexcept { return getVectorSize(ti) >= 2; }

  static MPSL_INLINE uint32_t getVectorSize(uint32_t ti) noexcept {
    uint32_t size = (ti & kTypeVecMask) >> kTypeVecShift;
    return size < 1 ? 1 : size;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Type id.
  uint8_t id;
  //! Type flags.
  uint8_t flags;
  //! Type size (0 if object / array).
  uint8_t size;
  //! Type name length (without a NULL terminator).
  uint8_t nameLength;
  //! Type name.
  char name[12];
};
extern const TypeInfo mpTypeInfo[kTypeCount];
extern const uint32_t mpImplicitCast[kTypeCount];

MPSL_INLINE const TypeInfo& TypeInfo::get(uint32_t typeId) noexcept {
  MPSL_ASSERT(typeId < kTypeCount);
  return mpTypeInfo[typeId];
}

static MPSL_INLINE bool mpCanImplicitCast(uint32_t toId, uint32_t fromId) noexcept {
  MPSL_ASSERT(toId < kTypeCount);
  MPSL_ASSERT(fromId < kTypeCount);

  return (mpImplicitCast[toId] & (1 << fromId)) != 0;
}

// ============================================================================
// [mpsl::ConstInfo]
// ============================================================================

//! Constant information.
struct ConstInfo {
  //! Constant name - built-in constants don't need more than 15 chars.
  char name[16];
  //! Constant value as `double`.
  double value;
};
extern const ConstInfo mpConstInfo[13];

// ============================================================================
// [mpsl::ElementNames]
// ============================================================================

//! Names of vector elements (letters).
struct ElementNames {
  //! 8 letters used to name 8 vector elements, starts from the most significant
  //! element.
  char names[8];
};
extern const ElementNames mpElementNames[2];

// ============================================================================
// [mpsl::OpInfo]
// ============================================================================

//! Operator information.
struct OpInfo {
  // --------------------------------------------------------------------------
  // [Statics]
  // --------------------------------------------------------------------------

  static MPSL_INLINE const OpInfo& get(uint32_t opType) noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE bool isUnary() const noexcept { return (flags & kOpFlagUnary) != 0; }
  MPSL_INLINE bool isBinary() const noexcept { return (flags & kOpFlagBinary) != 0; }
  MPSL_INLINE uint32_t getOpCount() const noexcept { return 1 + ((flags & kOpFlagBinary) != 0); }

  MPSL_INLINE bool isCast() const noexcept { return type == kOpCast; }
  MPSL_INLINE bool isIntrinsic() const noexcept { return (flags & kOpFlagIntrinsic) != 0; }

  MPSL_INLINE bool isLeftToRight() const noexcept { return (flags & kOpFlagRightToLeft) == 0; }
  MPSL_INLINE bool isRightToLeft() const noexcept { return (flags & kOpFlagRightToLeft) != 0; }

  MPSL_INLINE bool isAssignment() const noexcept { return (flags & kOpFlagAssign) != 0; }
  MPSL_INLINE bool isArithmetic() const noexcept { return (flags & kOpFlagArithmetic) != 0; }
  MPSL_INLINE bool isLogical() const noexcept { return (flags & kOpFlagLogical) != 0; }
  MPSL_INLINE bool isCondition() const noexcept { return (flags & kOpFlagCondition) != 0; }

  MPSL_INLINE bool isBitMask() const noexcept { return (flags & kOpFlagBitMask) != 0; }
  MPSL_INLINE bool isBitShift() const noexcept { return (flags & kOpFlagBitShift) != 0; }
  MPSL_INLINE bool isBitwise() const noexcept { return (flags & kOpFlagBitwise) != 0; }

  MPSL_INLINE bool isFloatOnly() const noexcept { return (flags & kOpFlagFloatOnly) != 0; }
  MPSL_INLINE bool isRounding() const noexcept { return (flags & kOpFlagRounding) != 0; }
  MPSL_INLINE bool isTrigonometric() const noexcept { return (flags & kOpFlagTrigonometric) != 0; }

  MPSL_INLINE bool rightAssociate(uint32_t rPrec) const noexcept {
    return precedence > rPrec || (precedence == rPrec && isRightToLeft());
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint8_t type;
  uint8_t altType;
  uint8_t precedence;
  uint8_t reserved;
  uint32_t flags;
  char name[12];
};
extern const OpInfo mpOpInfo[kOpCount];

MPSL_INLINE const OpInfo& OpInfo::get(uint32_t op) noexcept {
  MPSL_ASSERT(op < kOpCount);
  return mpOpInfo[op];
}

// ============================================================================
// [mpsl::RuntimeData]
// ============================================================================

struct RuntimeData {
  MPSL_NO_COPY(RuntimeData)

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

  //! Reference count.
  uintptr_t _refCount;
  //! JIT runtime (AsmJit).
  asmjit::JitRuntime _runtime;
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
