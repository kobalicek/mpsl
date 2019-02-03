// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPLANG_P_H
#define _MPSL_MPLANG_P_H

// [Dependencies - MPSL]
#include "./mpsl_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

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

  kOpCast,              // (cast)a
  kOpSwizzle,           // (swizzle)a

  kOpPreInc,            // ++a
  kOpPreDec,            // --a
  kOpPostInc,           // a++
  kOpPostDec,           // a--

  kOpAbs,               // abs(a)
  kOpBitNeg,            // ~a
  kOpNeg,               // -a
  kOpNot,               // !a

  kOpIsNan,             // isnan(a)
  kOpIsInf,             // isinf(a)
  kOpIsFinite,          // isfinite(a)
  kOpSignMask,          // signmask(a)

  kOpRound,             // round(a)
  kOpRoundEven,         // roundeven(a)
  kOpTrunc,             // trunc(a)
  kOpFloor,             // floor(a)
  kOpCeil,              // ceil(a)
  kOpFrac,              // frac(a)

  kOpSqrt,              // sqrt(a)
  kOpExp,               // exp(a)
  kOpLog,               // log(a)
  kOpLog2,              // log2(a)
  kOpLog10,             // log10(a)

  kOpSin,               // sin(a)
  kOpCos,               // cos(a)
  kOpTan,               // tan(a)
  kOpAsin,              // asin(a)
  kOpAcos,              // acos(a)
  kOpAtan,              // atan(a)

  kOpPabsb,             // pabsb(a)
  kOpPabsw,             // pabsw(a)
  kOpPabsd,             // pabsd(a)
  kOpLzcnt,             // lzcnt(a)
  kOpPopcnt,            // popcnt(a)

  kOpAssign,            // a = b
  kOpAssignAdd,         // a += b
  kOpAssignSub,         // a -= b
  kOpAssignMul,         // a *= b
  kOpAssignDiv,         // a /= b
  kOpAssignMod,         // a %= b
  kOpAssignAnd,         // a &= b
  kOpAssignOr,          // a |= b
  kOpAssignXor,         // a ^= b
  kOpAssignSll,         // a <<= b
  kOpAssignSrl,         // a >>>= b
  kOpAssignSra,         // a >>= b

  kOpAdd,               // a + b
  kOpSub,               // a - b
  kOpMul,               // a * b
  kOpDiv,               // a / b
  kOpMod,               // a % b
  kOpAnd,               // a & b
  kOpOr,                // a | b
  kOpXor,               // a ^ b
  kOpMin,               // min(a, b)
  kOpMax,               // max(a, b)

  kOpSll,               // a << b
  kOpSrl,               // a >>> b
  kOpSra,               // a >> b
  kOpRol,               // rol(a, b)
  kOpRor,               // ror(a, b)

  kOpCopySign,          // copysign(a, b)
  kOpPow,               // pow(a, b)
  kOpAtan2,             // atan2(a, b)

  kOpLogAnd,            // a && b
  kOpLogOr,             // a || b

  kOpCmpEq,             // a == b
  kOpCmpNe,             // a != b
  kOpCmpLt,             // a <  b
  kOpCmpLe,             // a <= b
  kOpCmpGt,             // a >  b
  kOpCmpGe,             // a >= b

  // TODO:
  // kOpBlend,          // blend(a, b, mask)

  kOpPmovsxbw,          // pmovsxbw(a, b)
  kOpPmovzxbw,          // pmovzxbw(a, b)
  kOpPmovsxwd,          // pmovsxwd(a, b)
  kOpPmovzxwd,          // pmovzxwd(a, b)
  kOpPacksswb,          // ppacksswb(a, b)
  kOpPackuswb,          // ppackuswb(a, b)
  kOpPackssdw,          // ppackssdw(a, b)
  kOpPackusdw,          // ppackusdw(a, b)
  kOpPaddb,             // paddb(a, b)
  kOpPaddw,             // paddw(a, b)
  kOpPaddd,             // paddd(a, b)
  kOpPaddq,             // paddq(a, b)
  kOpPaddssb,           // paddssb(a, b)
  kOpPaddusb,           // paddusb(a, b)
  kOpPaddssw,           // paddssw(a, b)
  kOpPaddusw,           // paddusw(a, b)
  kOpPsubb,             // psubb(a, b)
  kOpPsubw,             // psubw(a, b)
  kOpPsubd,             // psubd(a, b)
  kOpPsubq,             // psubq(a, b)
  kOpPsubssb,           // psubssb(a, b)
  kOpPsubusb,           // psubusb(a, b)
  kOpPsubssw,           // psubssw(a, b)
  kOpPsubusw,           // psubusw(a, b)
  kOpPmulw,             // pmulw(a, b)
  kOpPmulhsw,           // pmulhsw(a, b)
  kOpPmulhuw,           // pmulhuw(a, b)
  kOpPmuld,             // pmuld(a, b)
  kOpPminsb,            // pminsb(a, b)
  kOpPminub,            // pminub(a, b)
  kOpPminsw,            // pminsw(a, b)
  kOpPminuw,            // pminuw(a, b)
  kOpPminsd,            // pminsd(a, b)
  kOpPminud,            // pminud(a, b)
  kOpPmaxsb,            // pmaxsb(a, b)
  kOpPmaxub,            // pmaxub(a, b)
  kOpPmaxsw,            // pmaxsw(a, b)
  kOpPmaxuw,            // pmaxuw(a, b)
  kOpPmaxsd,            // pmaxsd(a, b)
  kOpPmaxud,            // pmaxud(a, b)
  kOpPsllw,             // psllw(a, b)
  kOpPsrlw,             // psrlw(a, b)
  kOpPsraw,             // psraw(a, b)
  kOpPslld,             // pslld(a, b)
  kOpPsrld,             // psrld(a, b)
  kOpPsrad,             // psrad(a, b)
  kOpPsllq,             // psllq(a, b)
  kOpPsrlq,             // psrlq(a, b)
  kOpPmaddwd,           // pmaddwd(a, b)
  kOpPcmpeqb,           // pcmpeqb(a, b)
  kOpPcmpeqw,           // pcmpeqw(a, b)
  kOpPcmpeqd,           // pcmpeqd(a, b)
  kOpPcmpneb,           // pcmpneb(a, b)
  kOpPcmpnew,           // pcmpnew(a, b)
  kOpPcmpned,           // pcmpned(a, b)
  kOpPcmpltb,           // pcmpltb(a, b)
  kOpPcmpltw,           // pcmpltw(a, b)
  kOpPcmpltd,           // pcmpltd(a, b)
  kOpPcmpleb,           // pcmpleb(a, b)
  kOpPcmplew,           // pcmplew(a, b)
  kOpPcmpled,           // pcmpled(a, b)
  kOpPcmpgtb,           // pcmpgtb(a, b)
  kOpPcmpgtw,           // pcmpgtw(a, b)
  kOpPcmpgtd,           // pcmpgtd(a, b)
  kOpPcmpgeb,           // pcmpgeb(a, b)
  kOpPcmpgew,           // pcmpgew(a, b)
  kOpPcmpged,           // pcmpged(a, b)

  //! \internal
  //!
  //! Count of operators.
  kOpCount
};

// ============================================================================
// [mpsl::OpFlags]
// ============================================================================

//! \internal
//!
//! Operator flags.
enum OpFlags {
  //! The operator has one parameter (unary node).
  kOpFlagUnary         = 0x00000001,
  //! The operator has two parameters (binary node).
  kOpFlagBinary        = 0x00000002,

  //! The operator has right-to-left associativity.
  kOpFlagRightToLeft   = 0x00000008,

  //! The operator is an built-in intrinsic function (like `round`, `min`, `max`).
  kOpFlagIntrinsic     = 0x00000010,
  //! The operator does an assignment to a variable (like `=`, `+=`, etc...).
  kOpFlagAssign        = 0x00000020,
  //! The operator is a postfix increment or decrement (.)++ or (.)--.
  kOpFlagAssignPost    = 0x00000040,
  //! The operator performs an arithmetic operation.
  kOpFlagArithmetic    = 0x00000100,
  //! The operator performs a logical operation - `&&` and `||`.
  kOpFlagLogical       = 0x00000200,
  //! The operator performs a floating-point rounding (float-only)
  kOpFlagRounding      = 0x00000400,
  //! The operator performs a conditional operation (bool result).
  kOpFlagConditional   = 0x00000800,
  //! The operator calculates a trigonometric function (float-only).
  kOpFlagTrigonometric = 0x00001000,

  //! Bit shift or rotation (integer-only).
  kOpFlagShift         = 0x00002000,
  //! Bitwise operation (AND, OR, XOR, NOT).
  kOpFlagBitwise       = 0x00004000,

  //! The operator is a special DSP intrinsic function.
  kOpFlagDSP           = 0x00010000,
  //! DSP intrinsic that requires 64-bit elements (`int2`, `int4`, or `int8`).
  kOpFlagDSP64         = 0x00020000,
  //! Operator does unpacking (i.e. it doubles the output width).
  kOpFlagUnpack        = 0x00040000,
  //! Operator does packing (i.e. it halves the output width).
  kOpFlagPack          = 0x00080000,

  //! The operator is defined for boolean operands.
  kOpFlagBoolOp        = 0x00100000,
  //! The operator is defined for integer operands.
  kOpFlagIntOp         = 0x00200000,
  //! The operator is defined for floating-point operands.
  kOpFlagFloatOp       = 0x00400000,
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
// [mpsl::InstCode]
// ============================================================================

//! \internal
//!
//! Instruction code, width, and meta-data.
enum InstCode {
  //! Not used.
  kInstCodeMask = 0x3FFF,
  kInstCodeNone = 0,

  kInstCodeJmp,
  kInstCodeJnz,
  kInstCodeCall,
  kInstCodeRet,

  kInstCodeFetch32,
  kInstCodeFetch64,
  kInstCodeFetch96,
  kInstCodeFetch128,
  kInstCodeFetch192,
  kInstCodeFetch256,
  kInstCodeInsert32,
  kInstCodeInsert64,

  kInstCodeStore32,
  kInstCodeStore64,
  kInstCodeStore96,
  kInstCodeStore128,
  kInstCodeStore192,
  kInstCodeStore256,
  kInstCodeExtract32,
  kInstCodeExtract64,

  kInstCodeMov32,
  kInstCodeMov64,
  kInstCodeMov128,
  kInstCodeMov256,

  kInstCodeCvtitof,
  kInstCodeCvtitod,
  kInstCodeCvtftoi,
  kInstCodeCvtftod,
  kInstCodeCvtdtoi,
  kInstCodeCvtdtof,

  kInstCodeAbsf,
  kInstCodeAbsd,
  kInstCodeBitnegi,
  kInstCodeBitnegf,
  kInstCodeBitnegd,
  kInstCodeNegi,
  kInstCodeNegf,
  kInstCodeNegd,
  kInstCodeNoti,
  kInstCodeNotf,
  kInstCodeNotd,
  kInstCodeSignmaski,
  kInstCodeSignmaskf,
  kInstCodeSignmaskd,

  kInstCodeIsnanf,
  kInstCodeIsnand,
  kInstCodeIsinff,
  kInstCodeIsinfd,
  kInstCodeIsfinitef,
  kInstCodeIsfinited,

  kInstCodeTruncf,
  kInstCodeTruncd,
  kInstCodeFloorf,
  kInstCodeFloord,
  kInstCodeRoundf,
  kInstCodeRoundd,
  kInstCodeRoundevenf,
  kInstCodeRoundevend,
  kInstCodeCeilf,
  kInstCodeCeild,
  kInstCodeFracf,
  kInstCodeFracd,

  kInstCodeSqrtf,
  kInstCodeSqrtd,
  kInstCodeExpf,
  kInstCodeExpd,
  kInstCodeLogf,
  kInstCodeLogd,
  kInstCodeLog2f,
  kInstCodeLog2d,
  kInstCodeLog10f,
  kInstCodeLog10d,

  kInstCodeSinf,
  kInstCodeSind,
  kInstCodeCosf,
  kInstCodeCosd,
  kInstCodeTanf,
  kInstCodeTand,
  kInstCodeAsinf,
  kInstCodeAsind,
  kInstCodeAcosf,
  kInstCodeAcosd,
  kInstCodeAtanf,
  kInstCodeAtand,

  kInstCodePabsb,
  kInstCodePabsw,
  kInstCodePabsd,
  kInstCodeLzcnti,
  kInstCodePopcnti,

  kInstCodeAddf,
  kInstCodeAddd,
  kInstCodeSubf,
  kInstCodeSubd,
  kInstCodeMulf,
  kInstCodeMuld,
  kInstCodeDivf,
  kInstCodeDivd,
  kInstCodeModf,
  kInstCodeModd,
  kInstCodeAndi,
  kInstCodeAndf,
  kInstCodeAndd,
  kInstCodeOri,
  kInstCodeOrf,
  kInstCodeOrd,
  kInstCodeXori,
  kInstCodeXorf,
  kInstCodeXord,
  kInstCodeMinf,
  kInstCodeMind,
  kInstCodeMaxf,
  kInstCodeMaxd,

  kInstCodeRoli,
  kInstCodeRori,

  kInstCodeCmpeqf,
  kInstCodeCmpeqd,
  kInstCodeCmpnef,
  kInstCodeCmpned,
  kInstCodeCmpltf,
  kInstCodeCmpltd,
  kInstCodeCmplef,
  kInstCodeCmpled,
  kInstCodeCmpgtf,
  kInstCodeCmpgtd,
  kInstCodeCmpgef,
  kInstCodeCmpged,

  kInstCodeCopysignf,
  kInstCodeCopysignd,
  kInstCodePowf,
  kInstCodePowd,
  kInstCodeAtan2f,
  kInstCodeAtan2d,

  kInstCodePshufd,

  kInstCodePmovsxbw,
  kInstCodePmovzxbw,
  kInstCodePmovsxwd,
  kInstCodePmovzxwd,
  kInstCodePacksswb,
  kInstCodePackuswb,
  kInstCodePackssdw,
  kInstCodePackusdw,
  kInstCodePaddb,
  kInstCodePaddw,
  kInstCodePaddd,
  kInstCodePaddq,
  kInstCodePaddssb,
  kInstCodePaddusb,
  kInstCodePaddssw,
  kInstCodePaddusw,
  kInstCodePsubb,
  kInstCodePsubw,
  kInstCodePsubd,
  kInstCodePsubq,
  kInstCodePsubssb,
  kInstCodePsubusb,
  kInstCodePsubssw,
  kInstCodePsubusw,
  kInstCodePmulw,
  kInstCodePmulhsw,
  kInstCodePmulhuw,
  kInstCodePmuld,
  kInstCodePdivsd,
  kInstCodePmodsd,
  kInstCodePminsb,
  kInstCodePminub,
  kInstCodePminsw,
  kInstCodePminuw,
  kInstCodePminsd,
  kInstCodePminud,
  kInstCodePmaxsb,
  kInstCodePmaxub,
  kInstCodePmaxsw,
  kInstCodePmaxuw,
  kInstCodePmaxsd,
  kInstCodePmaxud,
  kInstCodePsllw,
  kInstCodePsrlw,
  kInstCodePsraw,
  kInstCodePslld,
  kInstCodePsrld,
  kInstCodePsrad,
  kInstCodePsllq,
  kInstCodePsrlq,
  kInstCodePmaddwd,
  kInstCodePcmpeqb,
  kInstCodePcmpeqw,
  kInstCodePcmpeqd,
  kInstCodePcmpneb,
  kInstCodePcmpnew,
  kInstCodePcmpned,
  kInstCodePcmpltb,
  kInstCodePcmpltw,
  kInstCodePcmpltd,
  kInstCodePcmpleb,
  kInstCodePcmplew,
  kInstCodePcmpled,
  kInstCodePcmpgtb,
  kInstCodePcmpgtw,
  kInstCodePcmpgtd,
  kInstCodePcmpgeb,
  kInstCodePcmpgew,
  kInstCodePcmpged,

  kInstCodeCount,

  //! Specifies how wide the SIMD operation is (if not specified it's scalar).
  kInstVecMask = 0xC000,
  //! Shift to get the instruction width:
  //!   `first bit of kInstVecMask` - `number of shifts to get 16`).
  kInstVecShift = 14 - 4,
  //! The operation is scalar.
  kInstVec0    = 0x0000,
  //! The operation is 128-bit wide.
  kInstVec128  = 0x4000,
  //! The operation is 256-bit wide.
  kInstVec256  = 0x8000
};

// ============================================================================
// [mpsl::InstFlags]
// ============================================================================

//! \internal
//!
//! Instruction flags.
enum InstFlags {
  kInstInfoI32     = 0x0001, //!< Works with I32 operand(s).
  kInstInfoF32     = 0x0002, //!< Works with F32 operand(s).
  kInstInfoF64     = 0x0004, //!< Works with F64 operand(s).
  kInstInfoSIMD    = 0x0008, //!< Requires SIMD registers, doesn't work on GP.
  kInstInfoFetch   = 0x0010,
  kInstInfoStore   = 0x0020,
  kInstInfoMov     = 0x0040,
  kInstInfoCvt     = 0x0080,
  kInstInfoJxx     = 0x0100,
  kInstInfoRet     = 0x0200,
  kInstInfoCall    = 0x0400,
  kInstInfoImm     = 0x0800,
  kInstInfoComplex = 0x8000
};

// ============================================================================
// [mpsl::TypeInfo]
// ============================================================================

//! \internal
//!
//! Type information.
struct TypeInfo {
  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  constexpr uint32_t typeId() const noexcept { return _typeId; }
  constexpr uint32_t flags() const noexcept { return _flags; }
  constexpr uint32_t size() const noexcept { return _size; }
  constexpr uint32_t maxElements() const noexcept { return _maxElements; }
  constexpr uint32_t nameSize() const noexcept { return _nameSize; }
  constexpr const char* name() const noexcept { return _name; }

  // --------------------------------------------------------------------------
  // [Statics]
  // --------------------------------------------------------------------------

  static MPSL_INLINE const TypeInfo& get(uint32_t typeId) noexcept;

  static MPSL_INLINE uint32_t sizeOf(uint32_t typeId) noexcept { return get(typeId)._size; }

  static MPSL_INLINE uint32_t elementsOf(uint32_t typeInfo) noexcept {
    uint32_t size = (typeInfo & kTypeVecMask) >> kTypeVecShift;
    return size < 1 ? 1 : size;
  }
  static MPSL_INLINE uint32_t widthOf(uint32_t typeInfo) noexcept {
    return sizeOf(typeInfo & kTypeIdMask) * elementsOf(typeInfo);
  }

  static MPSL_INLINE bool isBoolId(uint32_t typeId) noexcept { return (get(typeId)._flags & kTypeFlagBool) != 0; }
  static MPSL_INLINE bool isBoolType(uint32_t ti) noexcept { return isBoolId(ti & kTypeIdMask); }

  static MPSL_INLINE bool isIntId(uint32_t typeId) noexcept { return (get(typeId)._flags & kTypeFlagInt) != 0; }
  static MPSL_INLINE bool isIntType(uint32_t ti) noexcept { return isIntId(ti & kTypeIdMask); }

  static MPSL_INLINE bool isFloatId(uint32_t typeId) noexcept { return (get(typeId)._flags & kTypeFlagFP) != 0; }
  static MPSL_INLINE bool isFloatType(uint32_t ti) noexcept { return isFloatId(ti & kTypeIdMask); }

  static MPSL_INLINE bool isPtrId(uint32_t typeId) noexcept { return (get(typeId)._flags & (kTypeFlagPtr)) != 0; }
  static MPSL_INLINE bool isPtrType(uint32_t ti) noexcept { return isPtrId(ti & kTypeIdMask); }

  static MPSL_INLINE bool isIntOrBoolId(uint32_t typeId) noexcept { return (get(typeId)._flags & (kTypeFlagInt | kTypeFlagBool)) != 0; }
  static MPSL_INLINE bool isIntOrBoolType(uint32_t ti) noexcept { return isIntOrBoolId(ti & kTypeIdMask); }

  static MPSL_INLINE bool isIntOrFPId(uint32_t typeId) noexcept { return (get(typeId)._flags & (kTypeFlagInt | kTypeFlagFP)) != 0; }
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

  uint8_t _typeId;                       //!< Type id.
  uint8_t _flags;                        //!< Type flags.
  uint8_t _size;                         //!< Type size (0 if object / array).
  uint8_t _maxElements;                  //!< Maximum number of vector elements the type supports (1, 4, or 8).
  uint8_t _nameSize;                     //!< Type-name size (without a NULL terminator).
  char _name[11];                        //!< Type-name.
};
extern const TypeInfo mpTypeInfo[kTypeCount];
extern const uint32_t mpImplicitCast[kTypeCount];

// ============================================================================
// [mpsl::VectorIdentifiers]
// ============================================================================

//! \internal
//!
//! Identifiers of vector elements.
struct VectorIdentifiers {
  constexpr const char* letters() const noexcept { return _letters; }
  constexpr uint32_t mask() const noexcept { return _mask; }

  char _letters[8];                      //!< Letters specifying vector index (0..7).
  uint32_t _mask;                        //!< Mask of all letters for fast matching.
};
extern const VectorIdentifiers mpVectorIdentifiers[2];

// ============================================================================
// [mpsl::ConstInfo]
// ============================================================================

//! \internal
//!
//! Constant information.
struct ConstInfo {
  constexpr const char* name() const noexcept { return _name; }
  constexpr double value() const noexcept { return _value; }

  char _name[16];                        //!< Constant name - built-in constants don't need more than 15 chars.
  double _value;                         //!< Constant value as `double`.
};
extern const ConstInfo mpConstInfo[13];

// ============================================================================
// [mpsl::OpInfo]
// ============================================================================

//! \internal
//!
//! Operator information.
struct OpInfo {
  // --------------------------------------------------------------------------
  // [Statics]
  // --------------------------------------------------------------------------

  static MPSL_INLINE const OpInfo& get(uint32_t opType) noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  constexpr uint32_t type() const noexcept { return _type; }
  constexpr uint32_t altType() const noexcept { return _altType; }
  constexpr uint32_t opCount() const noexcept { return _opCount; }
  constexpr uint32_t precedence() const noexcept { return _precedence; }
  constexpr uint32_t flags() const noexcept { return _flags; }
  constexpr const char* name() const noexcept { return _name; }

  constexpr bool isUnary() const noexcept { return (_flags & kOpFlagUnary) != 0; }
  constexpr bool isBinary() const noexcept { return (_flags & kOpFlagBinary) != 0; }

  constexpr bool isLeftToRight() const noexcept { return (_flags & kOpFlagRightToLeft) == 0; }
  constexpr bool isRightToLeft() const noexcept { return (_flags & kOpFlagRightToLeft) != 0; }

  constexpr bool isCast() const noexcept { return _type == kOpCast; }
  constexpr bool isSwizzle() const noexcept { return _type == kOpSwizzle; }
  constexpr bool isIntrinsic() const noexcept { return (_flags & kOpFlagIntrinsic) != 0; }

  constexpr bool isAssignment() const noexcept { return (_flags & kOpFlagAssign) != 0; }
  constexpr bool isPostAssignment() const noexcept { return (_flags & kOpFlagAssignPost) != 0; }
  constexpr bool isArithmetic() const noexcept { return (_flags & kOpFlagArithmetic) != 0; }
  constexpr bool isLogical() const noexcept { return (_flags & kOpFlagLogical) != 0; }
  constexpr bool isRounding() const noexcept { return (_flags & kOpFlagRounding) != 0; }
  constexpr bool isConditional() const noexcept { return (_flags & kOpFlagConditional) != 0; }
  constexpr bool isTrigonometric() const noexcept { return (_flags & kOpFlagTrigonometric) != 0; }
  constexpr bool isShift() const noexcept { return (_flags & kOpFlagShift) != 0; }
  constexpr bool isBitwise() const noexcept { return (_flags & kOpFlagBitwise) != 0; }

  constexpr bool isDSP() const noexcept { return (_flags & kOpFlagDSP) != 0; }
  constexpr bool isDSP64() const noexcept { return (_flags & kOpFlagDSP64) != 0; }

  constexpr bool isIntOp() const noexcept { return (_flags & kOpFlagIntOp) != 0; }
  constexpr bool isBoolOp() const noexcept { return (_flags & kOpFlagBoolOp) != 0; }
  constexpr bool isFloatOp() const noexcept { return (_flags & kOpFlagFloatOp) != 0; }
  constexpr bool isFloatOnly() const noexcept { return (_flags & kOpFlagAnyOp) == kOpFlagFloatOp; }

  constexpr bool rightAssociate(uint32_t rPrec) const noexcept {
    return _precedence > rPrec || (_precedence == rPrec && isRightToLeft());
  }

  MPSL_INLINE uint32_t instByTypeId(uint32_t typeId) const noexcept {
    switch (typeId) {
      case kTypeBool  :
      case kTypeInt   : return static_cast<uint32_t>(_insti);
      case kTypeFloat : return static_cast<uint32_t>(_instf);
      case kTypeDouble: return static_cast<uint32_t>(_instf) + (_instf != 0);
      default:
        return kInstCodeNone;
    }
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint8_t _type;                         //!< Operator type, see \k OpType.
  uint8_t _altType;                      //!< Alternative type, it maps to the base type if operator is an assignment.
  uint8_t _opCount;                      //!< Count of operands (1, 2, 3).
  uint8_t _precedence;                   //!< Operator precedence.
  uint32_t _flags;                       //!< Operator flags, see \k OpFlags.
  uint16_t _insti;                       //!< IR instruction mapping if the operator has `int` or `bool` operands.
  uint16_t _instf;                       //!< IR instruction mapping if the operator has `float` or `double` (+1) operands.
  char _name[12];                        //!< Operator name.
};
extern const OpInfo mpOpInfo[kOpCount];

// ============================================================================
// [mpsl::InstInfo]
// ============================================================================

//! \internal
//!
//! Instruction information.
struct InstInfo {
  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE uint32_t code() const noexcept { return _code; }
  MPSL_INLINE uint32_t flags() const noexcept { return _flags; }
  MPSL_INLINE bool numOps() const noexcept { return _numOps; }
  MPSL_INLINE const char* name() const noexcept { return _name; }

  MPSL_INLINE bool isI32() const noexcept { return (_flags & kInstInfoI32) != 0; }
  MPSL_INLINE bool isF32() const noexcept { return (_flags & kInstInfoF32) != 0; }
  MPSL_INLINE bool isF64() const noexcept { return (_flags & kInstInfoF64) != 0; }
  MPSL_INLINE bool isCvt() const noexcept { return (_flags & kInstInfoCvt) != 0; }

  MPSL_INLINE bool isFetch() const noexcept { return (_flags & kInstInfoFetch) != 0; }
  MPSL_INLINE bool isStore() const noexcept { return (_flags & kInstInfoStore) != 0; }

  MPSL_INLINE bool isJxx() const noexcept { return (_flags & kInstInfoJxx) != 0; }
  MPSL_INLINE bool isRet() const noexcept { return (_flags & kInstInfoRet) != 0; }
  MPSL_INLINE bool isCall() const noexcept { return (_flags & kInstInfoCall) != 0; }
  MPSL_INLINE bool isComplex() const noexcept { return (_flags & kInstInfoComplex) != 0; }

  MPSL_INLINE bool hasImm() const noexcept { return (_flags & kInstInfoImm) != 0; }

  // --------------------------------------------------------------------------
  // [WidthOf]
  // --------------------------------------------------------------------------

  static MPSL_INLINE const InstInfo& get(uint32_t instId) noexcept;

  static MPSL_INLINE uint32_t widthOf(uint32_t inst) noexcept {
    return (inst & kInstVecMask) >> kInstVecShift;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint16_t _code;
  uint16_t _flags;
  uint8_t _numOps;
  char _name[19];
};
extern const InstInfo mpInstInfo[kInstCodeCount];

// ============================================================================
// [Implemented-Later]
// ============================================================================

MPSL_INLINE const TypeInfo& TypeInfo::get(uint32_t typeId) noexcept {
  MPSL_ASSERT(typeId < kTypeCount);
  return mpTypeInfo[typeId];
}

MPSL_INLINE const OpInfo& OpInfo::get(uint32_t op) noexcept {
  MPSL_ASSERT(op < kOpCount);
  return mpOpInfo[op];
}

MPSL_INLINE const InstInfo& InstInfo::get(uint32_t instId) noexcept {
  MPSL_ASSERT(instId < kInstCodeCount);
  return mpInstInfo[instId];
}

static MPSL_INLINE bool mpCanImplicitCast(uint32_t dTypeId, uint32_t sTypeId) noexcept {
  MPSL_ASSERT(dTypeId < kTypeCount);
  MPSL_ASSERT(sTypeId < kTypeCount);

  return (mpImplicitCast[dTypeId] & (1 << sTypeId)) != 0;
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPLANG_P_H
