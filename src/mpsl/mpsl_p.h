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
  //! Type is a boolean.
  kTypeFlagBool = 0x01,
  //! Type is an integer.
  kTypeFlagInt = 0x02,
  //! Type is a floating point (either `float` or `double`).
  kTypeFlagFP = 0x04,
  //! Type is a pointer.
  kTypeFlagPtr = 0x08
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

  kOpLzcnt,             // lzcnt(a)
  kOpPopcnt,            // popcnt(a)

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

  // TODO:
  // kOpBlend,          // blend(a, b, mask)

  kOpVabsb,             // vabsb(a)
  kOpVabsw,             // vabsw(a)
  kOpVabsd,             // vabsd(a)
  kOpVaddb,             // vaddb(a, b)
  kOpVaddw,             // vaddw(a, b)
  kOpVaddd,             // vaddd(a, b)
  kOpVaddq,             // vaddq(a, b)
  kOpVaddssb,           // vaddssb(a, b)
  kOpVaddusb,           // vaddusb(a, b)
  kOpVaddssw,           // vaddssw(a, b)
  kOpVaddusw,           // vaddusw(a, b)
  kOpVsubb,             // vsubb(a, b)
  kOpVsubw,             // vsubw(a, b)
  kOpVsubd,             // vsubd(a, b)
  kOpVsubq,             // vsubq(a, b)
  kOpVsubssb,           // vsubssb(a, b)
  kOpVsubusb,           // vsubusb(a, b)
  kOpVsubssw,           // vsubssw(a, b)
  kOpVsubusw,           // vsubusw(a, b)
  kOpVmulw,             // vmulw(a, b)
  kOpVmulhsw,           // vmulhsw(a, b)
  kOpVmulhuw,           // vmulhuw(a, b)
  kOpVmuld,             // vmuld(a, b)
  kOpVminsb,            // vminsb(a, b)
  kOpVminub,            // vminub(a, b)
  kOpVminsw,            // vminsw(a, b)
  kOpVminuw,            // vminuw(a, b)
  kOpVminsd,            // vminsd(a, b)
  kOpVminud,            // vminud(a, b)
  kOpVmaxsb,            // vmaxsb(a, b)
  kOpVmaxub,            // vmaxub(a, b)
  kOpVmaxsw,            // vmaxsw(a, b)
  kOpVmaxuw,            // vmaxuw(a, b)
  kOpVmaxsd,            // vmaxsd(a, b)
  kOpVmaxud,            // vmaxud(a, b)
  kOpVsllw,             // vsllw(a, b)
  kOpVsrlw,             // vsrlw(a, b)
  kOpVsraw,             // vsraw(a, b)
  kOpVslld,             // vslld(a, b)
  kOpVsrld,             // vsrld(a, b)
  kOpVsrad,             // vsrad(a, b)
  kOpVsllq,             // vsllq(a, b)
  kOpVsrlq,             // vsrlq(a, b)
  kOpVcmpeqb,           // vcmpeqb(a, b)
  kOpVcmpeqw,           // vcmpeqw(a, b)
  kOpVcmpeqd,           // vcmpeqd(a, b)
  kOpVcmpgtb,           // vcmpgtb(a, b)
  kOpVcmpgtw,           // vcmpgtw(a, b)
  kOpVcmpgtd,           // vcmpgtd(a, b)

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
  //! The operator is an built-in intrinsic function (like `round`, `min`, `max`).
  kOpFlagIntrinsic     = 0x00000004,
  //! The operator is a special DSP intrinsic function.
  kOpFlagDSP           = 0x00000008,

  //! The operator has right-to-left associativity.
  kOpFlagRightToLeft   = 0x00000010,

  //! The operator does an assignment to a variable.
  kOpFlagAssign        = 0x00000020,
  //! The operator is a postfix operator (.)++ or (.)--.
  kOpFlagAssignPost    = 0x00000040,

  //! The operator performs an arithmetic operation.
  kOpFlagArithmetic    = 0x00000100,
  //! The operator performs a logical operation - `&&` and `||`.
  kOpFlagLogical       = 0x00000200,
  //! The operator performs a floating-point rounding (float-only)
  kOpFlagRounding      = 0x00000400,
  //! The operator performs a conditional operation.
  kOpFlagConditional   = 0x00000800,
  //! The operator calculates a trigonometric function (float-only).
  kOpFlagTrigonometric = 0x00001000,

  //! Bitwise operation (AND, OR, XOR).
  kOpFlagBitwise       = 0x00002000,
  //! Bit shifting or rotation (integer-only).
  kOpFlagBitShift      = 0x00004000,

  //! The operator is defined for boolean operands.
  kOpFlagBoolOp        = 0x00010000,
  //! The operator is defined for ineger operands.
  kOpFlagIntOp         = 0x00020000,
  //! The operator is defined for floating-point operands.
  kOpFlagFloatOp       = 0x00040000,

  //! The operator is defined for `int`, `float` types.
  kOpFlagIntFPOp       = kOpFlagIntOp | kOpFlagFloatOp,
  //! The operator is defined for `int`, `float`, and `bool` types.
  kOpFlagAnyOp         = kOpFlagIntOp | kOpFlagFloatOp | kOpFlagBoolOp,

  kOpFlagNopIfL0       = 0x10000000,
  kOpFlagNopIfR0       = 0x20000000,
  kOpFlagNopIfL1       = 0x40000000,
  kOpFlagNopIfR1       = 0x80000000,

  kOpFlagNopIf0        = kOpFlagNopIfL0 | kOpFlagNopIfR0,
  kOpFlagNopIf1        = kOpFlagNopIfL1 | kOpFlagNopIfR1
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

  static MPSL_INLINE const TypeInfo& get(uint32_t typeId) noexcept;

  static MPSL_INLINE uint32_t sizeOf(uint32_t typeId) noexcept { return get(typeId).size; }

  static MPSL_INLINE uint32_t elementsOf(uint32_t typeInfo) noexcept {
    uint32_t size = (typeInfo & kTypeVecMask) >> kTypeVecShift;
    return size < 1 ? 1 : size;
  }
  static MPSL_INLINE uint32_t widthOf(uint32_t typeInfo) noexcept {
    return sizeOf(typeInfo & kTypeIdMask) * elementsOf(typeInfo);
  }

  static MPSL_INLINE bool isBoolId(uint32_t typeId) noexcept { return (get(typeId).flags & kTypeFlagBool) != 0; }
  static MPSL_INLINE bool isBoolType(uint32_t ti) noexcept { return isBoolId(ti & kTypeIdMask); }

  static MPSL_INLINE bool isIntId(uint32_t typeId) noexcept { return (get(typeId).flags & kTypeFlagInt) != 0; }
  static MPSL_INLINE bool isIntType(uint32_t ti) noexcept { return isIntId(ti & kTypeIdMask); }

  static MPSL_INLINE bool isFloatId(uint32_t typeId) noexcept { return (get(typeId).flags & kTypeFlagFP) != 0; }
  static MPSL_INLINE bool isFloatType(uint32_t ti) noexcept { return isFloatId(ti & kTypeIdMask); }

  static MPSL_INLINE bool isPtrId(uint32_t typeId) noexcept { return (get(typeId).flags & (kTypeFlagPtr)) != 0; }
  static MPSL_INLINE bool isPtrType(uint32_t ti) noexcept { return isPtrId(ti & kTypeIdMask); }

  static MPSL_INLINE bool isIntOrBoolId(uint32_t typeId) noexcept { return (get(typeId).flags & (kTypeFlagInt | kTypeFlagBool)) != 0; }
  static MPSL_INLINE bool isIntOrBoolType(uint32_t ti) noexcept { return isIntOrBoolId(ti & kTypeIdMask); }

  static MPSL_INLINE bool isIntOrFPId(uint32_t typeId) noexcept { return (get(typeId).flags & (kTypeFlagInt | kTypeFlagFP)) != 0; }
  static MPSL_INLINE bool isIntOrFPType(uint32_t ti) noexcept { return isIntOrFPId(ti & kTypeIdMask); }

  static MPSL_INLINE bool isVectorType(uint32_t ti) noexcept { return elementsOf(ti) >= 2; }

  static MPSL_INLINE uint32_t boolIdBySize(uint32_t size) noexcept {
    return size <= 4 ? kTypeBool : kTypeQBool;
  }
  static MPSL_INLINE uint32_t boolIdByTypeId(uint32_t typeId) noexcept {
    return boolIdBySize(sizeOf(typeId));
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Type id.
  uint8_t typeId;
  //! Type flags.
  uint8_t flags;
  //! Type size (0 if object / array).
  uint8_t size;
  //! Maximum number of vector elements the type supports (1, 4, or 8).
  uint8_t maxElements;
  //! Type name length (without a NULL terminator).
  uint8_t nameLength;
  //! Type name.
  char name[11];
};
extern const TypeInfo mpTypeInfo[kTypeCount];
extern const uint32_t mpImplicitCast[kTypeCount];

MPSL_INLINE const TypeInfo& TypeInfo::get(uint32_t typeId) noexcept {
  MPSL_ASSERT(typeId < kTypeCount);
  return mpTypeInfo[typeId];
}

static MPSL_INLINE bool mpCanImplicitCast(uint32_t dTypeId, uint32_t sTypeId) noexcept {
  MPSL_ASSERT(dTypeId < kTypeCount);
  MPSL_ASSERT(sTypeId < kTypeCount);

  return (mpImplicitCast[dTypeId] & (1 << sTypeId)) != 0;
}

// ============================================================================
// [mpsl::VectorIdentifiers]
// ============================================================================

//! Identifiers of vector elements.
struct VectorIdentifiers {
  //! Letters used to name up to 8 vector elements. Starts from the most
  //! significant element.
  char letters[8];
};
extern const VectorIdentifiers mpVectorIdentifiers[2];

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
  MPSL_INLINE uint32_t getOpCount() const noexcept { return opCount; }

  MPSL_INLINE bool isCast() const noexcept { return type == kOpCast; }
  MPSL_INLINE bool isIntrinsic() const noexcept { return (flags & kOpFlagIntrinsic) != 0; }

  MPSL_INLINE bool isLeftToRight() const noexcept { return (flags & kOpFlagRightToLeft) == 0; }
  MPSL_INLINE bool isRightToLeft() const noexcept { return (flags & kOpFlagRightToLeft) != 0; }

  MPSL_INLINE bool isAssignment() const noexcept { return (flags & kOpFlagAssign) != 0; }
  MPSL_INLINE bool isArithmetic() const noexcept { return (flags & kOpFlagArithmetic) != 0; }
  MPSL_INLINE bool isLogical() const noexcept { return (flags & kOpFlagLogical) != 0; }
  MPSL_INLINE bool isRounding() const noexcept { return (flags & kOpFlagRounding) != 0; }
  MPSL_INLINE bool isConditional() const noexcept { return (flags & kOpFlagConditional) != 0; }
  MPSL_INLINE bool isTrigonometric() const noexcept { return (flags & kOpFlagTrigonometric) != 0; }

  MPSL_INLINE bool isBitwise() const noexcept { return (flags & kOpFlagBitwise) != 0; }
  MPSL_INLINE bool isBitShift() const noexcept { return (flags & kOpFlagBitShift) != 0; }

  MPSL_INLINE bool isIntOp() const noexcept { return (flags & kOpFlagIntOp) != 0; }
  MPSL_INLINE bool isBoolOp() const noexcept { return (flags & kOpFlagBoolOp) != 0; }
  MPSL_INLINE bool isFloatOp() const noexcept { return (flags & kOpFlagFloatOp) != 0; }
  MPSL_INLINE bool isFloatOnly() const noexcept { return (flags & kOpFlagAnyOp) == kOpFlagFloatOp; }

  MPSL_INLINE bool rightAssociate(uint32_t rPrec) const noexcept {
    return precedence > rPrec || (precedence == rPrec && isRightToLeft());
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint8_t type;
  uint8_t altType;
  uint8_t opCount;
  uint8_t precedence;
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
