// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MPSL_EXPORTS

// [Dependencies - MPSL]
#include "./mpast_p.h"
#include "./mpastanalysis_p.h"
#include "./mpastoptimizer_p.h"
#include "./mpasttoir_p.h"
#include "./mpatomic_p.h"
#include "./mpcompiler_x86_p.h"
#include "./mpparser_p.h"
#include "./mputils_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [mpsl::mpTypeInfo]
// ============================================================================

const TypeInfo mpTypeInfo[kTypeCount] = {
  { kTypeVoid    , 0                 , 0, 0, 4, "void"     },
  { kTypeBool    , kTypeFlagBool     , 4, 8, 4, "bool"     },
  { kTypeQBool   , kTypeFlagBool     , 8, 4, 7, "__qbool"  },
  { kTypeInt     , kTypeFlagInt      , 4, 8, 3, "int"      },
  { kTypeFloat   , kTypeFlagFP       , 4, 8, 5, "float"    },
  { kTypeDouble  , kTypeFlagFP       , 8, 4, 6, "double"   },
  { kTypePtr     , kTypeFlagPtr      , 0, 1, 5, "__ptr"    }
};

// Implicit cast rules. Basically any type can implicitly cast to `double`, but
// nothing can implicitly cast to `float` and to `int`. This rules map is here
// in case more types with more casting options are added to MPSL.
#define T(id) (1 << kType##id)
const uint32_t mpImplicitCast[kTypeCount] = {
  /* +--------------+---------+----------+---------+----------+--------- */
  /* | Type         | bool    | __qbool  | int     | float    | double   */
  /* +--------------+---------+----------+---------+----------+--------- */
  /* | void     <- */ 0       | 0        | 0       | 0        | 0,
  /* | bool     <- */ 0       | T(QBool) | T(Int)  | T(Float) | T(Double),
  /* | __qbool  <- */ T(Bool) | 0        | T(Int)  | T(Float) | T(Double),
  /* | int      <- */ T(Bool) | T(QBool) | 0       | 0        | 0,
  /* | float    <- */ T(Bool) | T(QBool) | 0       | 0        | 0,
  /* | double   <- */ T(Bool) | T(QBool) | T(Int)  | T(Float) | 0,
  /* | __ptr    <- */ 0       | 0        | 0       | 0        | 0
};
#undef T

// ============================================================================
// [mpsl::mpVectorIdentifiers]
// ============================================================================

// NOTE: Each letter can only be used at a specific index. Letters can repeat
// in multiple `VectorIdentifiers` records, but cannot change their index once
// assigned.
const VectorIdentifiers mpVectorIdentifiers[2] = {
  { 'x', 'y', 'z', 'w', 'i', 'j', 'k', 'l' }, // xyzwijkl.
  { 'r', 'g', 'b', 'a', 'i', 'j', 'k', 'l' }  // rgbaijkl.
};

// ============================================================================
// [mpsl::mpConstInfo]
// ============================================================================

const ConstInfo mpConstInfo[13] = {
  { "M_E"       , 2.71828182845904523536  },
  { "M_LOG2E"   , 1.44269504088896340736  },
  { "M_LOG10E"  , 0.434294481903251827651 },
  { "M_LN2"     , 0.693147180559945309417 },
  { "M_LN10"    , 2.30258509299404568402  },
  { "M_PI"      , 3.14159265358979323846  },
  { "M_PI_2"    , 1.57079632679489661923  },
  { "M_PI_4"    , 0.785398163397448309616 },
  { "M_1_PI"    , 0.318309886183790671538 },
  { "M_2_PI"    , 0.636619772367581343076 },
  { "M_2_SQRTPI", 1.1283791670955125739   },
  { "M_SQRT2"   , 1.4142135623730950488   },
  { "M_SQRT1_2" , 0.707106781186547524401 }
};

// ============================================================================
// [mpsl::mpOpInfo]
// ============================================================================

// Operator information, precedence and association. The table is mostly based
// on the C-language standard, but also adjusted to support MPSL specific
// operators and rules. However, the associativity and precedence should be
// fully compatible with C.
#define ROW(opType, altType, opCount, precedence, assignment, intrinsic, flags, name) \
  { \
    static_cast<uint8_t>(kOp##opType), \
    static_cast<uint8_t>(kOp##altType == kOpNone ? kOp##opType : kOp##altType), \
    static_cast<uint8_t>(opCount), \
    static_cast<uint8_t>(precedence), \
    static_cast<uint32_t>( \
      flags | (assignment ==-1 ? kOpFlagAssign : \
               assignment == 1 ? kOpFlagAssign | kOpFlagAssignPost : 0) \
            | (opCount    == 1 ? kOpFlagUnary : \
               opCount    == 2 ? kOpFlagBinary : 0) \
            | (intrinsic  == 1 ? kOpFlagIntrinsic : 0)), \
    name \
  }
#define LTR 0
#define RTL kOpFlagRightToLeft
#define F(flag) kOpFlag##flag
const OpInfo mpOpInfo[kOpCount] = {
  // +-------------+----------+--+--+--+--+-----+---------------------------------------------+------------+
  // |Operator     | Alt.     |#N|#P|:=|#I|Assoc| Flags                                       | Name       |
  // +-------------+----------+--+--+--+--+-----+---------------------------------------------+------------+
  ROW(None         , None     , 0, 0, 0, 0, LTR | 0                                           , "<none>"   ),
  ROW(Cast         , None     , 1, 3, 0, 0, RTL | 0                                           , "(cast)"   ),
  ROW(PreInc       , None     , 1, 3,-1, 0, RTL | F(Arithmetic)     | F(AnyOp)                , "++(.)"    ),
  ROW(PreDec       , None     , 1, 3,-1, 0, RTL | F(Arithmetic)     | F(AnyOp)                , "--(.)"    ),
  ROW(PostInc      , None     , 1, 2, 1, 0, LTR | F(Arithmetic)     | F(AnyOp)                , "(.)++"    ),
  ROW(PostDec      , None     , 1, 2, 1, 0, LTR | F(Arithmetic)     | F(AnyOp)                , "(.)--"    ),
  ROW(BitNeg       , None     , 1, 3, 0, 0, RTL | F(Bitwise)        | F(AnyOp)                , "~"        ),
  ROW(Neg          , None     , 1, 3, 0, 0, RTL | F(Arithmetic)     | F(IntFPOp)              , "-"        ),
  ROW(Not          , None     , 1, 3, 0, 0, RTL | F(Conditional)    | F(AnyOp)                , "!"        ),
  ROW(Lzcnt        , None     , 1, 0, 0, 1, LTR | 0                 | F(IntOp)                , "lzcnt"    ),
  ROW(Popcnt       , None     , 1, 0, 0, 1, LTR | 0                 | F(IntOp)                , "popcnt"   ),
  ROW(IsNan        , None     , 1, 0, 0, 1, LTR | F(Conditional)    | F(FloatOp)              , "isnan"    ),
  ROW(IsInf        , None     , 1, 0, 0, 1, LTR | F(Conditional)    | F(FloatOp)              , "isinf"    ),
  ROW(IsFinite     , None     , 1, 0, 0, 1, LTR | F(Conditional)    | F(FloatOp)              , "isfinite" ),
  ROW(SignBit      , None     , 1, 0, 0, 1, LTR | F(Conditional)    | F(AnyOp)                , "signbit"  ),
  ROW(Round        , None     , 1, 0, 0, 1, LTR | F(Rounding)       | F(FloatOp)              , "round"    ),
  ROW(RoundEven    , None     , 1, 0, 0, 1, LTR | F(Rounding)       | F(FloatOp)              , "roundeven"),
  ROW(Trunc        , None     , 1, 0, 0, 1, LTR | F(Rounding)       | F(FloatOp)              , "trunc"    ),
  ROW(Floor        , None     , 1, 0, 0, 1, LTR | F(Rounding)       | F(FloatOp)              , "floor"    ),
  ROW(Ceil         , None     , 1, 0, 0, 1, LTR | F(Rounding)       | F(FloatOp)              , "ceil"     ),
  ROW(Abs          , None     , 1, 0, 0, 1, LTR | 0                 | F(IntFPOp)              , "abs"      ),
  ROW(Exp          , None     , 1, 0, 0, 1, LTR | 0                 | F(FloatOp)              , "exp"      ),
  ROW(Log          , None     , 1, 0, 0, 1, LTR | 0                 | F(FloatOp)              , "log"      ),
  ROW(Log2         , None     , 1, 0, 0, 1, LTR | 0                 | F(FloatOp)              , "log2"     ),
  ROW(Log10        , None     , 1, 0, 0, 1, LTR | 0                 | F(FloatOp)              , "log10"    ),
  ROW(Sqrt         , None     , 1, 0, 0, 1, LTR | 0                 | F(FloatOp)              , "sqrt"     ),
  ROW(Frac         , None     , 1, 0, 0, 1, LTR | 0                 | F(FloatOp)              , "frac"     ),
  ROW(Sin          , None     , 1, 0, 0, 1, LTR | F(Trigonometric)  | F(FloatOp)              , "sin"      ),
  ROW(Cos          , None     , 1, 0, 0, 1, LTR | F(Trigonometric)  | F(FloatOp)              , "cos"      ),
  ROW(Tan          , None     , 1, 0, 0, 1, LTR | F(Trigonometric)  | F(FloatOp)              , "tan"      ),
  ROW(Asin         , None     , 1, 0, 0, 1, LTR | F(Trigonometric)  | F(FloatOp)              , "asin"     ),
  ROW(Acos         , None     , 1, 0, 0, 1, LTR | F(Trigonometric)  | F(FloatOp)              , "acos"     ),
  ROW(Atan         , None     , 1, 0, 0, 1, LTR | F(Trigonometric)  | F(FloatOp)              , "atan"     ),
  ROW(Assign       , Assign   , 2,15,-1, 0, RTL | 0                                           , "="        ),
  ROW(AssignAdd    , Add      , 2,15,-1, 0, RTL | F(Arithmetic)     | F(NopIfR0) | F(NopIfR0) , "+="       ),
  ROW(AssignSub    , Sub      , 2,15,-1, 0, RTL | F(Arithmetic)     | F(NopIfR0) | F(NopIfR0) , "-="       ),
  ROW(AssignMul    , Mul      , 2,15,-1, 0, RTL | F(Arithmetic)                  | F(NopIfR1) , "*="       ),
  ROW(AssignDiv    , Div      , 2,15,-1, 0, RTL | F(Arithmetic)     | F(NopIfR1) | F(NopIfR1) , "/="       ),
  ROW(AssignMod    , Mod      , 2,15,-1, 0, RTL | F(Arithmetic)                               , "%="       ),
  ROW(AssignBitAnd , BitAnd   , 2,15,-1, 0, RTL | F(Bitwise)        | F(AnyOp)                , "&="       ),
  ROW(AssignBitOr  , BitOr    , 2,15,-1, 0, RTL | F(Bitwise)        | F(AnyOp)   | F(NopIfR0) , "|="       ),
  ROW(AssignBitXor , BitXor   , 2,15,-1, 0, RTL | F(Bitwise)        | F(AnyOp)   | F(NopIfR0) , "^="       ),
  ROW(AssignBitSar , BitSar   , 2,15,-1, 0, RTL | F(BitShift)       | F(IntOp)   | F(NopIfR0) , ">>="      ),
  ROW(AssignBitShr , BitShr   , 2,15,-1, 0, RTL | F(BitShift)       | F(IntOp)   | F(NopIfR0) , ">>>="     ),
  ROW(AssignBitShl , BitShl   , 2,15,-1, 0, RTL | F(BitShift)       | F(IntOp)   | F(NopIfR0) , "<<="      ),
  ROW(Eq           , None     , 2, 9, 0, 0, LTR | F(Conditional)    | F(AnyOp)                , "=="       ),
  ROW(Ne           , None     , 2, 9, 0, 0, LTR | F(Conditional)    | F(AnyOp)                , "!="       ),
  ROW(Lt           , None     , 2, 8, 0, 0, LTR | F(Conditional)    | F(IntFPOp)              , "<"        ),
  ROW(Le           , None     , 2, 8, 0, 0, LTR | F(Conditional)    | F(IntFPOp)              , "<="       ),
  ROW(Gt           , None     , 2, 8, 0, 0, LTR | F(Conditional)    | F(IntFPOp)              , ">"        ),
  ROW(Ge           , None     , 2, 8, 0, 0, LTR | F(Conditional)    | F(IntFPOp)              , ">="       ),
  ROW(LogAnd       , None     , 2,13, 0, 0, LTR | F(Conditional)    | F(BoolOp)  | F(Logical) , "&&"       ),
  ROW(LogOr        , None     , 2,14, 0, 0, LTR | F(Conditional)    | F(BoolOp)  | F(Logical) , "||"       ),
  ROW(Add          , None     , 2, 6, 0, 0, LTR | F(Arithmetic)     | F(IntFPOp) | F(NopIf0)  , "+"        ),
  ROW(Sub          , None     , 2, 6, 0, 0, LTR | F(Arithmetic)     | F(IntFPOp) | F(NopIfR0) , "-"        ),
  ROW(Mul          , None     , 2, 5, 0, 0, LTR | F(Arithmetic)     | F(IntFPOp) | F(NopIf1)  , "*"        ),
  ROW(Div          , None     , 2, 5, 0, 0, LTR | F(Arithmetic)     | F(IntFPOp) | F(NopIfR1) , "/"        ),
  ROW(Mod          , None     , 2, 5, 0, 0, LTR | F(Arithmetic)     | F(IntFPOp)              , "%"        ),
  ROW(BitAnd       , None     , 2,10, 0, 0, LTR | F(Bitwise)        | F(AnyOp)                , "&"        ),
  ROW(BitOr        , None     , 2,12, 0, 0, LTR | F(Bitwise)        | F(AnyOp)   | F(NopIf0)  , "|"        ),
  ROW(BitXor       , None     , 2,11, 0, 0, LTR | F(Bitwise)        | F(AnyOp)   | F(NopIf0)  , "^"        ),
  ROW(BitSar       , None     , 2, 7, 0, 0, LTR | F(BitShift)       | F(IntOp)   | F(NopIf0)  , ">>"       ),
  ROW(BitShr       , None     , 2, 7, 0, 0, LTR | F(BitShift)       | F(IntOp)   | F(NopIf0)  , ">>>"      ),
  ROW(BitShl       , None     , 2, 7, 0, 0, LTR | F(BitShift)       | F(IntOp)   | F(NopIf0)  , "<<"       ),
  ROW(BitRor       , None     , 2, 0, 0, 1, LTR | F(BitShift)       | F(IntOp)   | F(NopIf0)  , "ror"      ),
  ROW(BitRol       , None     , 2, 0, 0, 1, LTR | F(BitShift)       | F(IntOp)   | F(NopIf0)  , "rol"      ),
  ROW(Min          , None     , 2, 0, 0, 1, LTR | 0                 | F(AnyOp)                , "min"      ),
  ROW(Max          , None     , 2, 0, 0, 1, LTR | 0                 | F(AnyOp)                , "max"      ),
  ROW(Pow          , None     , 2, 0, 0, 1, LTR | 0                 | F(FloatOp) | F(NopIfR1) , "pow"      ),
  ROW(Atan2        , None     , 2, 0, 0, 1, LTR | F(Trigonometric)  | F(FloatOp)              , "atan2"    ),
  ROW(CopySign     , None     , 2, 0, 0, 1, LTR | 0                 | F(FloatOp)              , "copysign" ),
  ROW(Vabsb        , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vabsb"    ),
  ROW(Vabsw        , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vabsw"    ),
  ROW(Vabsd        , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vabsd"    ),
  ROW(Vaddb        , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vaddb"    ),
  ROW(Vaddw        , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vaddw"    ),
  ROW(Vaddd        , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vaddd"    ),
  ROW(Vaddq        , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vaddq"    ),
  ROW(Vaddssb      , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vaddssb"  ),
  ROW(Vaddusb      , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vaddusb"  ),
  ROW(Vaddssw      , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vaddssw"  ),
  ROW(Vaddusw      , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vaddusw"  ),
  ROW(Vsubb        , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vsubb"    ),
  ROW(Vsubw        , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vsubw"    ),
  ROW(Vsubd        , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vsubd"    ),
  ROW(Vsubq        , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vsubq"    ),
  ROW(Vsubssb      , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vsubssb"  ),
  ROW(Vsubusb      , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vsubusb"  ),
  ROW(Vsubssw      , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vsubssw"  ),
  ROW(Vsubusw      , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vsubusw"  ),
  ROW(Vmulw        , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vmulw"    ),
  ROW(Vmulhsw      , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vmulhsw"  ),
  ROW(Vmulhuw      , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vmulhuw"  ),
  ROW(Vmuld        , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vmuld"    ),
  ROW(Vminsb       , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vminsb"   ),
  ROW(Vminub       , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vminub"   ),
  ROW(Vminsw       , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vminsw"   ),
  ROW(Vminuw       , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vminuw"   ),
  ROW(Vminsd       , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vminsd"   ),
  ROW(Vminud       , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vminud"   ),
  ROW(Vmaxsb       , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vmaxsb"   ),
  ROW(Vmaxub       , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vmaxub"   ),
  ROW(Vmaxsw       , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vmaxsw"   ),
  ROW(Vmaxuw       , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vmaxuw"   ),
  ROW(Vmaxsd       , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vmaxsd"   ),
  ROW(Vmaxud       , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vmaxud"   ),
  ROW(Vsllw        , None     , 2, 0, 0, 1, LTR | F(DSP)|F(BitShift)| F(IntOp)                , "vsllw"    ),
  ROW(Vsrlw        , None     , 2, 0, 0, 1, LTR | F(DSP)|F(BitShift)| F(IntOp)                , "vsrlw"    ),
  ROW(Vsraw        , None     , 2, 0, 0, 1, LTR | F(DSP)|F(BitShift)| F(IntOp)                , "vsraw"    ),
  ROW(Vslld        , None     , 2, 0, 0, 1, LTR | F(DSP)|F(BitShift)| F(IntOp)                , "vslld"    ),
  ROW(Vsrld        , None     , 2, 0, 0, 1, LTR | F(DSP)|F(BitShift)| F(IntOp)                , "vsrld"    ),
  ROW(Vsrad        , None     , 2, 0, 0, 1, LTR | F(DSP)|F(BitShift)| F(IntOp)                , "vsrad"    ),
  ROW(Vsllq        , None     , 2, 0, 0, 1, LTR | F(DSP)|F(BitShift)| F(IntOp)                , "vsllq"    ),
  ROW(Vsrlq        , None     , 2, 0, 0, 1, LTR | F(DSP)|F(BitShift)| F(IntOp)                , "vsrlq"    ),
  ROW(Vcmpeqb      , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vcmpeqb"  ),
  ROW(Vcmpeqw      , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vcmpeqw"  ),
  ROW(Vcmpeqd      , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vcmpeqd"  ),
  ROW(Vcmpgtb      , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vcmpgtb"  ),
  ROW(Vcmpgtw      , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vcmpgtw"  ),
  ROW(Vcmpgtd      , None     , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , "vcmpgtd"  )
};
#undef F
#undef RTL
#undef LTR
#undef ROW

// ============================================================================
// [mpsl::mpAssertionFailed]
// ============================================================================

void mpAssertionFailed(const char* exp, const char* file, int line) noexcept {
  ::fprintf(stderr, "Assertion failed: %s\n, file %s, line %d\n", exp, file, line);
  ::abort();
}

// ============================================================================
// [mpsl::mpTraceError]
// ============================================================================

Error mpTraceError(Error error) noexcept {
  return error;
}

// ============================================================================
// [mpsl::Helpers]
// ============================================================================

template<typename T>
MPSL_INLINE T* mpObjectAddRef(T* self) noexcept {
  if (self->_refCount == 0)
    return self;

  mpAtomicInc(&self->_refCount);
  return self;
}

template<typename T>
MPSL_INLINE void mpObjectRelease(T* self) noexcept {
  if (self->_refCount != 0 && mpAtomicDec(&self->_refCount) == 1)
    self->destroy();
}

// Declared in public "mpsl.h" header.
MPSL_INLINE void Isolate::Impl::destroy() noexcept {
  RuntimeData* rt = static_cast<RuntimeData*>(_runtimeData);

  mpObjectRelease(rt);
  ::free(this);
}

MPSL_INLINE void Program::Impl::destroy() noexcept {
  RuntimeData* rt = static_cast<RuntimeData*>(_runtimeData);
  rt->_runtime.release(_main);

  mpObjectRelease(rt);
  ::free(this);
}

// ============================================================================
// [mpsl::Layout - Internal]
// ============================================================================

static MPSL_INLINE bool mpLayoutIsEmbedded(Layout* self, const uint8_t* data) noexcept {
  return self->_embeddedData == data;
}

static MPSL_INLINE uint32_t mpLayoutGetRemainingSize(const Layout* self) noexcept {
  return self->_dataIndex - static_cast<uint32_t>(self->_membersCount * sizeof(Layout::Member));
}

static MPSL_INLINE Layout::Member* mpLayoutFind(
  const Layout* self, const char* name, size_t len) noexcept {

  Layout::Member* m = self->_members;
  uint32_t count = self->_membersCount;

  for (uint32_t i = 0; i < count; i++)
    if (m[i].nameLength == len && (len == 0 || ::memcmp(m[i].name, name, len) == 0))
      return &m[i];

  return nullptr;
}

static MPSL_NOINLINE Error mpLayoutResize(Layout* self, uint32_t dataSize) noexcept {
  uint8_t* oldData = self->_data;
  uint8_t* newData = static_cast<uint8_t*>(::malloc(dataSize));

  if (newData == nullptr)
    return MPSL_TRACE_ERROR(kErrorNoMemory);

  uint32_t dataIndex = dataSize;
  uint32_t count = self->_membersCount;

  Layout::Member* oldMember = reinterpret_cast<Layout::Member*>(oldData);
  Layout::Member* newMember = reinterpret_cast<Layout::Member*>(newData);

  uint32_t len = self->_nameLength;
  if (len != 0) {
    dataIndex -= ++len;

    ::memcpy(newData + dataIndex, self->_name, len);
    self->_name = reinterpret_cast<char*>(newData + dataIndex);
  }

  for (uint32_t i = 0; i < count; i++, oldMember++, newMember++) {
    len = oldMember->nameLength;
    newMember->nameLength = len;
    MPSL_ASSERT(len < dataIndex);

    dataIndex -= ++len;
    MPSL_ASSERT(dataIndex >= (i + 1) * sizeof(Layout::Member));

    ::memcpy(newData + dataIndex, oldMember->name, len);
    newMember->name = reinterpret_cast<char*>(newData + dataIndex);
    newMember->typeInfo = oldMember->typeInfo;
    newMember->offset = oldMember->offset;
  }

  self->_data = newData;
  self->_dataSize = dataSize;
  self->_dataIndex = dataIndex;

  if (oldData != nullptr && !mpLayoutIsEmbedded(self, oldData))
    ::free(oldData);

  return kErrorOk;
}

static MPSL_INLINE Error mpLayoutPrepareAdd(Layout* self, size_t n) noexcept {
  size_t remainingSize = mpLayoutGetRemainingSize(self);
  if (remainingSize < n) {
    uint32_t newSize = self->_dataSize;
    // Make sure we reserve much more than the current size if it wasn't enough.
    if (newSize <= 128)
      newSize = 512;
    else
      newSize *= 2;
    MPSL_PROPAGATE(mpLayoutResize(self, newSize));
  }
  return kErrorOk;
}

// ============================================================================
// [mpsl::Layout - Construction / Destruction]
// ============================================================================

Layout::Layout() noexcept
  : _data(nullptr),
    _name(nullptr),
    _nameLength(0),
    _membersCount(0),
    _dataSize(0),
    _dataIndex(0) {}

Layout::Layout(uint8_t* data, uint32_t dataSize) noexcept
  : _data(data),
    _name(nullptr),
    _nameLength(0),
    _membersCount(0),
    _dataSize(dataSize),
    _dataIndex(dataSize) {}

Layout::~Layout() noexcept {
  uint8_t* data = _data;

  if (data != nullptr && !mpLayoutIsEmbedded(this, data))
    ::free(data);
}

// ============================================================================
// [mpsl::Layout - Interface]
// ============================================================================

Error Layout::_configure(const char* name, size_t len) noexcept {
  if (len == Globals::kInvalidIndex)
    len = name ? ::strlen(name) : (size_t)0;

  if (len > Globals::kMaxIdentifierLength)
    return MPSL_TRACE_ERROR(kErrorInvalidArgument);

  if (_name != nullptr)
    return MPSL_TRACE_ERROR(kErrorAlreadyConfigured);

  len++; // Include NULL terminator.
  MPSL_PROPAGATE(mpLayoutPrepareAdd(this, len));

  uint32_t dataIndex = _dataIndex - static_cast<uint32_t>(len);
  ::memcpy(_data + dataIndex, name, len);

  _name = reinterpret_cast<char*>(_data + dataIndex);
  // Casting to `uint32_t` is safe, see `kMaxIdentifierLength`, store length
  // without a NULL terminator (hence `-1`).
  _nameLength = static_cast<uint32_t>(len - 1);

  _dataIndex = dataIndex;
  return kErrorOk;
}

const Layout::Member* Layout::_get(const char* name, size_t len) const noexcept {
  if (name == nullptr)
    return nullptr;

  if (len == Globals::kInvalidIndex)
    len = ::strlen(name);

  return mpLayoutFind(this, name, len);
}

Error Layout::_add(const char* name, size_t len, uint32_t typeInfo, int32_t offset) noexcept {
  if (name == nullptr)
    return MPSL_TRACE_ERROR(kErrorInvalidArgument);

  if (len == Globals::kInvalidIndex)
    len = ::strlen(name);

  if (len > Globals::kMaxIdentifierLength)
    return MPSL_TRACE_ERROR(kErrorInvalidArgument);

  uint32_t count = _membersCount;
  if (count >= Globals::kMaxMembersCount)
    return MPSL_TRACE_ERROR(kErrorTooManyMembers);

  Member* member = mpLayoutFind(this, name, len);
  if (member != nullptr)
    return MPSL_TRACE_ERROR(kErrorAlreadyExists);

  MPSL_PROPAGATE(mpLayoutPrepareAdd(this, len + 1 + static_cast<uint32_t>(sizeof(Member))));
  member = _members + count;
  uint32_t dataIndex = _dataIndex - (static_cast<uint32_t>(len) + 1);

  ::memcpy(_data + dataIndex, name, len + 1);
  member->name = reinterpret_cast<char*>(_data + dataIndex);
  member->nameLength = static_cast<uint32_t>(len);
  member->typeInfo = typeInfo;
  member->offset = offset;

  _membersCount++;
  _dataIndex = dataIndex;
  return kErrorOk;
}

// ============================================================================
// [mpsl::Isolate - Construction / Destruction]
// ============================================================================

static const Isolate::Impl mpIsolateNull = { 0, nullptr };

Isolate::Isolate() noexcept
  : _d(const_cast<Impl*>(&mpIsolateNull)) {}

Isolate::Isolate(const Isolate& other) noexcept
  : _d(mpObjectAddRef(other._d)) {}

Isolate::~Isolate() noexcept {
  mpObjectRelease(_d);
}

Isolate Isolate::create() noexcept {
  Impl* d = static_cast<Impl*>(::malloc(sizeof(Impl)));

  if (d == nullptr) {
    // Allocation failure.
    d = const_cast<Impl*>(&mpIsolateNull);
  }
  else {
    RuntimeData* rt = static_cast<RuntimeData*>(::malloc(sizeof(RuntimeData)));
    if (rt == nullptr) {
      // Allocation failure.
      ::free(d);
      d = const_cast<Impl*>(&mpIsolateNull);
    }
    else {
      d->_refCount = 1;
      d->_runtimeData = new(rt) RuntimeData();
    }
  }

  return Isolate(d);
}

// ============================================================================
// [mpsl::Isolate - Reset]
// ============================================================================

Error Isolate::reset() noexcept {
  mpObjectRelease(mpAtomicSetXchgT<Impl*>(
    &_d, const_cast<Impl*>(&mpIsolateNull)));

  return kErrorOk;
}

// ============================================================================
// [mpsl::Isolate - Clone / Freeze]
// ============================================================================

Error Isolate::clone() noexcept {

  // TODO:
  return kErrorOk;
}

Error Isolate::freeze() noexcept {

  // TODO:
  return kErrorOk;
}

// ============================================================================
// [mpsl::Isolate - Compile]
// ============================================================================

#define MPSL_PROPAGATE_AND_HANDLE_COLLISION(...) \
  do { \
    AstSymbol* collidedSymbol = nullptr; \
    ::mpsl::Error _errorValue = __VA_ARGS__; \
    \
    if (MPSL_UNLIKELY(_errorValue != ::mpsl::kErrorOk)) { \
      if (_errorValue == ::mpsl::kErrorSymbolCollision && log) { \
        sbTmp.setFormat("Built-in symbol collision: '%s' already defined", collidedSymbol->getName()); \
        log->log(OutputLog::Info(OutputLog::kMessageError, 0, 0, sbTmp.getData(), sbTmp.getLength())); \
      } \
      return _errorValue; \
    } \
  } while (0)

Error Isolate::_compile(Program& program, const CompileArgs& ca, OutputLog* log) noexcept {
  const char* body = ca.body.getData();
  size_t len = ca.body.getLength();

  uint32_t options = ca.options;
  uint32_t numArgs = ca.numArgs;

  if (numArgs == 0 || numArgs > Globals::kMaxArgumentsCount)
    return MPSL_TRACE_ERROR(kErrorInvalidArgument);

  // --------------------------------------------------------------------------
  // [Init]
  // --------------------------------------------------------------------------

  // Init options first.
  options &= _kOptionsMask;

  if (log != nullptr)
    options |= kInternalOptionLog;
  else
    options &= ~(kOptionVerbose | kOptionDebugAST | kOptionDebugIR | kOptionDebugASM);

  if (len == Globals::kInvalidIndex)
    len = ::strlen(body);

  Allocator allocator;
  StringBuilderTmp<512> sbTmp;

  AstBuilder ast(&allocator);
  IRBuilder ir(&allocator, numArgs);

  MPSL_PROPAGATE(ast.addProgramScope());
  MPSL_PROPAGATE(ast.addBuiltInTypes(mpTypeInfo, kTypeCount));
  MPSL_PROPAGATE(ast.addBuiltInConstants(mpConstInfo, MPSL_ARRAY_SIZE(mpConstInfo)));
  MPSL_PROPAGATE(ast.addBuiltInIntrinsics());

  for (uint32_t slot = 0; slot < numArgs; slot++) {
    MPSL_PROPAGATE_AND_HANDLE_COLLISION(ast.addBuiltInObject(slot, ca.layout[slot], &collidedSymbol));
  }

  // Setup basic data structures used during parsing and compilation.
  ErrorReporter errorReporter(body, len, options, log);

  // --------------------------------------------------------------------------
  // [AST]
  // --------------------------------------------------------------------------

  // Parse the source code into AST.
  { MPSL_PROPAGATE(Parser(&ast, &errorReporter, body, len).parseProgram(ast.getProgramNode())); }

  // Do a semantic analysis of the code without doing any optimizations.
  //
  // It can add some nodes required by implicit casts and fail if the code is
  // semantically incorrect, for example invalid implicit cast, explicit-cast,
  // or function call. This pass doesn't do constant folding or optimizations.
  // Perform semantic analysis of the parsed code.
  { MPSL_PROPAGATE(AstAnalysis(&ast, &errorReporter).onProgram(ast.getProgramNode())); }

  if (options & kOptionDebugAST) {
    ast.dump(sbTmp);
    log->log(OutputLog::Info(OutputLog::kMessageAstInitial, 0, 0, sbTmp.getData(), sbTmp.getLength()));
    sbTmp.clear();
  }

  // Perform basic optimizations at AST level (dead code removal and constant
  // folding). This pass shouldn't do any unsafe optimizations and it's a bit
  // limited, but it's faster to do them now than doing these optimizations at
  // IR level.
  { MPSL_PROPAGATE(AstOptimizer(&ast, &errorReporter).onProgram(ast.getProgramNode())); }

  if (options & kOptionDebugAST) {
    ast.dump(sbTmp);
    log->log(OutputLog::Info(OutputLog::kMessageAstFinal, 0, 0, sbTmp.getData(), sbTmp.getLength()));
    sbTmp.clear();
  }

  // --------------------------------------------------------------------------
  // [IR]
  // --------------------------------------------------------------------------

  // Translate AST to IR.
  {
    AstToIR::Args unused(false);
    MPSL_PROPAGATE(AstToIR(&ast, &ir).onProgram(ast.getProgramNode(), unused));
  }

  if (options & kOptionDebugIR) {
    ir.dump(sbTmp);
    log->log(OutputLog::Info(OutputLog::kMessageIRInitial, 0, 0, sbTmp.getData(), sbTmp.getLength()));
    sbTmp.clear();
  }

  // --------------------------------------------------------------------------
  // [ASM]
  // --------------------------------------------------------------------------

  // Compile and store the reference to the `main()` function.
  RuntimeData* rt = static_cast<RuntimeData*>(_d->_runtimeData);
  Program::Impl* programD = program._d;

  void* func = nullptr;
  {
    asmjit::StringLogger asmlog;
    asmjit::X86Assembler a(&rt->_runtime);
    asmjit::X86Compiler c(&a);

    if (options & kOptionDebugASM)
      a.setLogger(&asmlog);

    JitCompiler compiler(&allocator, &c);
    if (options & kOptionDisableSSE4_1)
      compiler._enableSSE4_1 = false;
    MPSL_PROPAGATE(compiler.compileIRAsFunc(&ir));

    asmjit::Error err = c.finalize();
    if (err)
      return MPSL_TRACE_ERROR(kErrorJITFailed);

    func = a.make();
    if (options & kOptionDebugASM)
      log->log(OutputLog::Info(OutputLog::kMessageAsm, 0, 0, asmlog.getString(), asmlog.getLength()));

    if (func == nullptr)
      return MPSL_TRACE_ERROR(kErrorJITFailed);
  }

  if (programD->_refCount == 1 && static_cast<RuntimeData*>(programD->_runtimeData) == rt) {
    rt->_runtime.release(programD->_main);
    programD->_main = func;
  }
  else {
    programD = static_cast<Program::Impl*>(::malloc(sizeof(Program::Impl)));
    if (programD == nullptr) {
      rt->_runtime.release((void*)func);
      return MPSL_TRACE_ERROR(kErrorNoMemory);
    }

    programD->_refCount = 1;
    programD->_runtimeData = mpObjectAddRef(rt);
    programD->_main = func;

    mpObjectRelease(
      mpAtomicSetXchgT<Program::Impl*>(
        &program._d, programD));
  }

  return kErrorOk;
}

// ============================================================================
// [mpsl::Isolate - Operator Overload]
// ============================================================================

Isolate& Isolate::operator=(const Isolate& other) noexcept {
  mpObjectRelease(
    mpAtomicSetXchgT<Impl*>(
      &_d, mpObjectAddRef(other._d)));

  return *this;
}

// ============================================================================
// [mpsl::Program - Construction / Destruction]
// ============================================================================

static const Program::Impl mpProgramNull = { 0, nullptr, nullptr };

Program::Program() noexcept
  : _d(const_cast<Program::Impl*>(&mpProgramNull)) {}

Program::Program(const Program& other) noexcept
  : _d(mpObjectAddRef(other._d)) {}

Program::~Program() noexcept {
  mpObjectRelease(_d);
}

// ============================================================================
// [mpsl::Program - Reset]
// ============================================================================

Error Program::reset() noexcept {
  mpObjectRelease(mpAtomicSetXchgT<Impl*>(
    &_d, const_cast<Impl*>(&mpProgramNull)));
  return kErrorOk;
}

// ============================================================================
// [mpsl::Program - Operator Overload]
// ============================================================================

Program& Program::operator=(const Program& other) noexcept {
  mpObjectRelease(
    mpAtomicSetXchgT<Impl*>(
      &_d, mpObjectAddRef(other._d)));

  return *this;
}

// ============================================================================
// [mpsl::OutputLog - Construction / Destruction]
// ============================================================================

OutputLog::OutputLog() noexcept {}
OutputLog::~OutputLog() noexcept {}

// ============================================================================
// [mpsl::ErrorReporter - Interface]
// ============================================================================

void ErrorReporter::getLineAndColumn(uint32_t position, uint32_t& line, uint32_t& column) noexcept {
  // Should't happen, but be defensive.
  if (static_cast<size_t>(position) >= _len) {
    line = 0;
    column = 0;
    return;
  }

  const char* pStart = _body;
  const char* p = pStart + position;

  uint32_t x = 0;
  uint32_t y = 1;

  // Find column.
  while (p[0] != '\n') {
    x++;
    if (p == pStart)
      break;
    p--;
  }

  // Find line.
  while (p != pStart) {
    y += p[0] == '\n';
    p--;
  }

  line = y;
  column = x;
}

void ErrorReporter::onWarning(uint32_t position, const char* fmt, ...) noexcept {
  if (reportsWarnings()) {
    StringBuilderTmp<256> sb;

    va_list ap;
    va_start(ap, fmt);

    Utils::vformat(sb, fmt, ap);

    va_end(ap);
    onWarning(position, sb);
  }
}

void ErrorReporter::onWarning(uint32_t position, const StringBuilder& msg) noexcept {
  if (reportsWarnings()) {
    uint32_t line, column;
    getLineAndColumn(position, line, column);
    _log->log(OutputLog::Info(OutputLog::kMessageWarning, line, column, msg.getData(), msg.getLength()));
  }
}

Error ErrorReporter::onError(Error error, uint32_t position, const char* fmt, ...) noexcept {
  if (reportsErrors()) {
    StringBuilderTmp<256> sb;

    va_list ap;
    va_start(ap, fmt);

    Utils::vformat(sb, fmt, ap);

    va_end(ap);
    return onError(error, position, sb);
  }
  else {
    return MPSL_TRACE_ERROR(error);
  }
}

Error ErrorReporter::onError(Error error, uint32_t position, const StringBuilder& msg) noexcept {
  if (reportsErrors()) {
    OutputLog::Info info(OutputLog::kMessageError, 0, 0, msg.getData(), msg.getLength());
    getLineAndColumn(position, info._line, info._column);
    _log->log(info);
  }

  return MPSL_TRACE_ERROR(error);
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
