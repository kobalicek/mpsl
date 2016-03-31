// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MPSL_EXPORTS

// [Dependencies - MPSL]
#include "./mplang_p.h"

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
  /* +--------------+---------+----------+---------+----------+------------+ */
  /* | Type         | bool    | __qbool  | int     | float    | double     | */
  /* +--------------+---------+----------+---------+----------+------------+ */
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

// Operator information, precedence and association. The table's associativity
// and precedence is mostly based on the C-language standard, but also adjusted
// to support MPSL's specific operators and rules.
#define ROW(opType, name, altType, opCount, precedence, assignment, intrinsic, flags, insti, instf) \
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
    static_cast<uint16_t>(kInstCode##insti), \
    static_cast<uint16_t>(kInstCode##instf), \
    name \
  }
#define LTR 0
#define RTL kOpFlagRightToLeft
#define F(flag) kOpFlag##flag
const OpInfo mpOpInfo[kOpCount] = {
  // +------------+------------+-------+--+--+--+--+-----+---------------------------------------------+-----------+-----------+
  // | OpType     | Name       | AltT. |#N|#P|:=|#I|Assoc| Flags                                       | InstI     | InstF/D   |
  // +------------+------------+-------+--+--+--+--+-----+---------------------------------------------+-----------+-----------+
  ROW(None        , "<none>"   , None  , 0, 0, 0, 0, LTR | 0                                           , None      , None      ),
  ROW(Cast        , "(cast)"   , None  , 1, 3, 0, 0, RTL | 0                                           , None      , None      ),
  ROW(PreInc      , "++(.)"    , None  , 1, 3,-1, 0, RTL | F(Arithmetic)     | F(AnyOp)                , Addi      , Addf      ),
  ROW(PreDec      , "--(.)"    , None  , 1, 3,-1, 0, RTL | F(Arithmetic)     | F(AnyOp)                , Subi      , Subf      ),
  ROW(PostInc     , "(.)++"    , None  , 1, 2, 1, 0, LTR | F(Arithmetic)     | F(AnyOp)                , Addi      , Addf      ),
  ROW(PostDec     , "(.)--"    , None  , 1, 2, 1, 0, LTR | F(Arithmetic)     | F(AnyOp)                , Subi      , Subf      ),
  ROW(Abs         , "abs"      , None  , 1, 0, 0, 1, LTR | 0                 | F(IntFPOp)              , Absi      , Absf      ),
  ROW(BitNeg      , "~"        , None  , 1, 3, 0, 0, RTL | F(Bitwise)        | F(AnyOp)                , Bitnegi   , Bitnegf   ),
  ROW(Neg         , "-"        , None  , 1, 3, 0, 0, RTL | F(Arithmetic)     | F(IntFPOp)              , Negi      , Negf      ),
  ROW(Not         , "!"        , None  , 1, 3, 0, 0, RTL | F(Conditional)    | F(AnyOp)                , Noti      , Notf      ),
  ROW(Lzcnt       , "lzcnt"    , None  , 1, 0, 0, 1, LTR | 0                 | F(IntOp)                , Lzcnti    , None      ),
  ROW(Popcnt      , "popcnt"   , None  , 1, 0, 0, 1, LTR | 0                 | F(IntOp)                , Popcnti   , None      ),
  ROW(SignBit     , "signbit"  , None  , 1, 0, 0, 1, LTR | F(Conditional)    | F(IntFPOp)              , Signbiti  , Signbitf  ),
  ROW(IsNan       , "isnan"    , None  , 1, 0, 0, 1, LTR | F(Conditional)    | F(FloatOp)              , None      , Isnanf    ),
  ROW(IsInf       , "isinf"    , None  , 1, 0, 0, 1, LTR | F(Conditional)    | F(FloatOp)              , None      , Isinff    ),
  ROW(IsFinite    , "isfinite" , None  , 1, 0, 0, 1, LTR | F(Conditional)    | F(FloatOp)              , None      , Isfinitef ),
  ROW(Round       , "round"    , None  , 1, 0, 0, 1, LTR | F(Rounding)       | F(FloatOp)              , None      , Roundf    ),
  ROW(RoundEven   , "roundeven", None  , 1, 0, 0, 1, LTR | F(Rounding)       | F(FloatOp)              , None      , Roundevenf),
  ROW(Trunc       , "trunc"    , None  , 1, 0, 0, 1, LTR | F(Rounding)       | F(FloatOp)              , None      , Truncf    ),
  ROW(Floor       , "floor"    , None  , 1, 0, 0, 1, LTR | F(Rounding)       | F(FloatOp)              , None      , Floorf    ),
  ROW(Ceil        , "ceil"     , None  , 1, 0, 0, 1, LTR | F(Rounding)       | F(FloatOp)              , None      , Ceilf     ),
  ROW(Frac        , "frac"     , None  , 1, 0, 0, 1, LTR | 0                 | F(FloatOp)              , None      , Fracf     ),
  ROW(Sqrt        , "sqrt"     , None  , 1, 0, 0, 1, LTR | 0                 | F(FloatOp)              , None      , Sqrtf     ),
  ROW(Exp         , "exp"      , None  , 1, 0, 0, 1, LTR | 0                 | F(FloatOp)              , None      , Expf      ),
  ROW(Log         , "log"      , None  , 1, 0, 0, 1, LTR | 0                 | F(FloatOp)              , None      , Logf      ),
  ROW(Log2        , "log2"     , None  , 1, 0, 0, 1, LTR | 0                 | F(FloatOp)              , None      , Log2f     ),
  ROW(Log10       , "log10"    , None  , 1, 0, 0, 1, LTR | 0                 | F(FloatOp)              , None      , Log10f    ),
  ROW(Sin         , "sin"      , None  , 1, 0, 0, 1, LTR | F(Trigonometric)  | F(FloatOp)              , None      , Sinf      ),
  ROW(Cos         , "cos"      , None  , 1, 0, 0, 1, LTR | F(Trigonometric)  | F(FloatOp)              , None      , Cosf      ),
  ROW(Tan         , "tan"      , None  , 1, 0, 0, 1, LTR | F(Trigonometric)  | F(FloatOp)              , None      , Tanf      ),
  ROW(Asin        , "asin"     , None  , 1, 0, 0, 1, LTR | F(Trigonometric)  | F(FloatOp)              , None      , Asinf     ),
  ROW(Acos        , "acos"     , None  , 1, 0, 0, 1, LTR | F(Trigonometric)  | F(FloatOp)              , None      , Acosf     ),
  ROW(Atan        , "atan"     , None  , 1, 0, 0, 1, LTR | F(Trigonometric)  | F(FloatOp)              , None      , Atanf     ),
  ROW(Assign      , "="        , Assign, 2,15,-1, 0, RTL | 0                                           , None      , None      ),
  ROW(AssignAdd   , "+="       , Add   , 2,15,-1, 0, RTL | F(Arithmetic)     | F(NopIfR0) | F(NopIfR0) , Addi      , Addf      ),
  ROW(AssignSub   , "-="       , Sub   , 2,15,-1, 0, RTL | F(Arithmetic)     | F(NopIfR0) | F(NopIfR0) , Subi      , Subf      ),
  ROW(AssignMul   , "*="       , Mul   , 2,15,-1, 0, RTL | F(Arithmetic)                  | F(NopIfR1) , Muli      , Mulf      ),
  ROW(AssignDiv   , "/="       , Div   , 2,15,-1, 0, RTL | F(Arithmetic)     | F(NopIfR1) | F(NopIfR1) , Divi      , Divf      ),
  ROW(AssignMod   , "%="       , Mod   , 2,15,-1, 0, RTL | F(Arithmetic)                               , Modi      , Modf      ),
  ROW(AssignAnd   , "&="       , And   , 2,15,-1, 0, RTL | F(Bitwise)        | F(AnyOp)                , Andi      , Andf      ),
  ROW(AssignOr    , "|="       , Or    , 2,15,-1, 0, RTL | F(Bitwise)        | F(AnyOp)   | F(NopIfR0) , Ori       , Orf       ),
  ROW(AssignXor   , "^="       , Xor   , 2,15,-1, 0, RTL | F(Bitwise)        | F(AnyOp)   | F(NopIfR0) , Xori      , Xorf      ),
  ROW(AssignSll   , "<<="      , Sll   , 2,15,-1, 0, RTL | F(Shift)          | F(IntOp)   | F(NopIfR0) , Slli      , None      ),
  ROW(AssignSrl   , ">>>="     , Srl   , 2,15,-1, 0, RTL | F(Shift)          | F(IntOp)   | F(NopIfR0) , Srli      , None      ),
  ROW(AssignSra   , ">>="      , Sra   , 2,15,-1, 0, RTL | F(Shift)          | F(IntOp)   | F(NopIfR0) , Srai      , None      ),
  ROW(Eq          , "=="       , None  , 2, 9, 0, 0, LTR | F(Conditional)    | F(AnyOp)                , Cmpeqi    , Cmpeqf    ),
  ROW(Ne          , "!="       , None  , 2, 9, 0, 0, LTR | F(Conditional)    | F(AnyOp)                , Cmpnei    , Cmpnef    ),
  ROW(Lt          , "<"        , None  , 2, 8, 0, 0, LTR | F(Conditional)    | F(IntFPOp)              , Cmplti    , Cmpltf    ),
  ROW(Le          , "<="       , None  , 2, 8, 0, 0, LTR | F(Conditional)    | F(IntFPOp)              , Cmplei    , Cmplef    ),
  ROW(Gt          , ">"        , None  , 2, 8, 0, 0, LTR | F(Conditional)    | F(IntFPOp)              , Cmpgti    , Cmpgtf    ),
  ROW(Ge          , ">="       , None  , 2, 8, 0, 0, LTR | F(Conditional)    | F(IntFPOp)              , Cmpgei    , Cmpgef    ),
  ROW(LogAnd      , "&&"       , None  , 2,13, 0, 0, LTR | F(Conditional)    | F(BoolOp)  | F(Logical) , None      , None      ),
  ROW(LogOr       , "||"       , None  , 2,14, 0, 0, LTR | F(Conditional)    | F(BoolOp)  | F(Logical) , None      , None      ),
  ROW(Add         , "+"        , None  , 2, 6, 0, 0, LTR | F(Arithmetic)     | F(IntFPOp) | F(NopIf0)  , Addi      , Addf      ),
  ROW(Sub         , "-"        , None  , 2, 6, 0, 0, LTR | F(Arithmetic)     | F(IntFPOp) | F(NopIfR0) , Subi      , Subf      ),
  ROW(Mul         , "*"        , None  , 2, 5, 0, 0, LTR | F(Arithmetic)     | F(IntFPOp) | F(NopIf1)  , Muli      , Mulf      ),
  ROW(Div         , "/"        , None  , 2, 5, 0, 0, LTR | F(Arithmetic)     | F(IntFPOp) | F(NopIfR1) , Divi      , Divf      ),
  ROW(Mod         , "%"        , None  , 2, 5, 0, 0, LTR | F(Arithmetic)     | F(IntFPOp)              , Modi      , Modf      ),
  ROW(And         , "&"        , None  , 2,10, 0, 0, LTR | F(Bitwise)        | F(AnyOp)                , Andi      , Andf      ),
  ROW(Or          , "|"        , None  , 2,12, 0, 0, LTR | F(Bitwise)        | F(AnyOp)   | F(NopIf0)  , Ori       , Orf       ),
  ROW(Xor         , "^"        , None  , 2,11, 0, 0, LTR | F(Bitwise)        | F(AnyOp)   | F(NopIf0)  , Xori      , Xorf      ),
  ROW(Sll         , "<<"       , None  , 2, 7, 0, 0, LTR | F(Shift)          | F(IntOp)   | F(NopIf0)  , Slli      , None      ),
  ROW(Srl         , ">>>"      , None  , 2, 7, 0, 0, LTR | F(Shift)          | F(IntOp)   | F(NopIf0)  , Srli      , None      ),
  ROW(Sra         , ">>"       , None  , 2, 7, 0, 0, LTR | F(Shift)          | F(IntOp)   | F(NopIf0)  , Srai      , None      ),
  ROW(Rol         , "rol"      , None  , 2, 0, 0, 1, LTR | F(Shift)          | F(IntOp)   | F(NopIf0)  , Roli      , None      ),
  ROW(Ror         , "ror"      , None  , 2, 0, 0, 1, LTR | F(Shift)          | F(IntOp)   | F(NopIf0)  , Rori      , None      ),
  ROW(Min         , "min"      , None  , 2, 0, 0, 1, LTR | 0                 | F(AnyOp)                , Mini      , Minf      ),
  ROW(Max         , "max"      , None  , 2, 0, 0, 1, LTR | 0                 | F(AnyOp)                , Maxi      , Maxf      ),
  ROW(Pow         , "pow"      , None  , 2, 0, 0, 1, LTR | 0                 | F(FloatOp) | F(NopIfR1) , None      , Powf      ),
  ROW(Atan2       , "atan2"    , None  , 2, 0, 0, 1, LTR | F(Trigonometric)  | F(FloatOp)              , None      , Atan2f    ),
  ROW(CopySign    , "copysign" , None  , 2, 0, 0, 1, LTR | 0                 | F(FloatOp)              , None      , Copysignf ),
  ROW(Vabsb       , "vabsb"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vabsb     , None      ),
  ROW(Vabsw       , "vabsw"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vabsw     , None      ),
  ROW(Vabsd       , "vabsd"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vabsd     , None      ),
  ROW(Vaddb       , "vaddb"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vaddb     , None      ),
  ROW(Vaddw       , "vaddw"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vaddw     , None      ),
  ROW(Vaddd       , "vaddd"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vaddd     , None      ),
  ROW(Vaddq       , "vaddq"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vaddq     , None      ),
  ROW(Vaddssb     , "vaddssb"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vaddssb   , None      ),
  ROW(Vaddusb     , "vaddusb"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vaddusb   , None      ),
  ROW(Vaddssw     , "vaddssw"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vaddssw   , None      ),
  ROW(Vaddusw     , "vaddusw"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vaddusw   , None      ),
  ROW(Vsubb       , "vsubb"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vsubb     , None      ),
  ROW(Vsubw       , "vsubw"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vsubw     , None      ),
  ROW(Vsubd       , "vsubd"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vsubd     , None      ),
  ROW(Vsubq       , "vsubq"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vsubq     , None      ),
  ROW(Vsubssb     , "vsubssb"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vsubssb   , None      ),
  ROW(Vsubusb     , "vsubusb"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vsubusb   , None      ),
  ROW(Vsubssw     , "vsubssw"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vsubssw   , None      ),
  ROW(Vsubusw     , "vsubusw"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vsubusw   , None      ),
  ROW(Vmulw       , "vmulw"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vmulw     , None      ),
  ROW(Vmulhsw     , "vmulhsw"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vmulhsw   , None      ),
  ROW(Vmulhuw     , "vmulhuw"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vmulhuw   , None      ),
  ROW(Vmuld       , "vmuld"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vmuld     , None      ),
  ROW(Vminsb      , "vminsb"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vminsb    , None      ),
  ROW(Vminub      , "vminub"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vminub    , None      ),
  ROW(Vminsw      , "vminsw"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vminsw    , None      ),
  ROW(Vminuw      , "vminuw"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vminuw    , None      ),
  ROW(Vminsd      , "vminsd"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vminsd    , None      ),
  ROW(Vminud      , "vminud"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vminud    , None      ),
  ROW(Vmaxsb      , "vmaxsb"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vmaxsb    , None      ),
  ROW(Vmaxub      , "vmaxub"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vmaxub    , None      ),
  ROW(Vmaxsw      , "vmaxsw"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vmaxsw    , None      ),
  ROW(Vmaxuw      , "vmaxuw"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vmaxuw    , None      ),
  ROW(Vmaxsd      , "vmaxsd"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vmaxsd    , None      ),
  ROW(Vmaxud      , "vmaxud"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vmaxud    , None      ),
  ROW(Vsllw       , "vsllw"    , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Shift) | F(IntOp)                , Vsllw     , None      ),
  ROW(Vsrlw       , "vsrlw"    , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Shift) | F(IntOp)                , Vsrlw     , None      ),
  ROW(Vsraw       , "vsraw"    , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Shift) | F(IntOp)                , Vsraw     , None      ),
  ROW(Vslld       , "vslld"    , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Shift) | F(IntOp)                , Vslld     , None      ),
  ROW(Vsrld       , "vsrld"    , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Shift) | F(IntOp)                , Vsrld     , None      ),
  ROW(Vsrad       , "vsrad"    , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Shift) | F(IntOp)                , Vsrad     , None      ),
  ROW(Vsllq       , "vsllq"    , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Shift) | F(IntOp)                , Vsllq     , None      ),
  ROW(Vsrlq       , "vsrlq"    , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Shift) | F(IntOp)                , Vsrlq     , None      ),
  ROW(Vcmpeqb     , "vcmpeqb"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vcmpeqb   , None      ),
  ROW(Vcmpeqw     , "vcmpeqw"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vcmpeqw   , None      ),
  ROW(Vcmpeqd     , "vcmpeqd"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vcmpeqd   , None      ),
  ROW(Vcmpgtb     , "vcmpgtb"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vcmpgtb   , None      ),
  ROW(Vcmpgtw     , "vcmpgtw"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vcmpgtw   , None      ),
  ROW(Vcmpgtd     , "vcmpgtd"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Vcmpgtd   , None      )
};
#undef F
#undef RTL
#undef LTR
#undef ROW

// ============================================================================
// [mpsl::InstInfo]
// ============================================================================

#define ROW(inst, name, numOps, flags) \
  { \
    kInstCode##inst, flags, numOps, name \
  }
#define I(x) kInstInfo##x
const InstInfo mpInstInfo[kInstCodeCount] = {
  // +----------+---------------+-----------------------------------------+
  // | InstCode | Name          |#N| Flags                                |
  // +----------+---------------+-----------------------------------------+
  ROW(None      , "<none>"      , 0, 0                                    ),
  ROW(Jmp       , "jmp"         , 1, I(Jxx)                               ),
  ROW(Jcc       , "jcc"         , 2, I(Jxx)                               ),
  ROW(Call      , "call"        , 0, I(Call)                              ),
  ROW(Ret       , "ret"         , 0, I(Ret)                               ),

  ROW(Fetch32   , "fetch32"     , 2, I(Fetch)                             ),
  ROW(Fetch64   , "fetch64"     , 2, I(Fetch)                             ),
  ROW(Fetch96   , "fetch96"     , 2, I(Fetch)                             ),
  ROW(Fetch128  , "fetch128"    , 2, I(Fetch)                             ),
  ROW(Fetch192  , "fetch192"    , 2, I(Fetch)                             ),
  ROW(Fetch256  , "fetch256"    , 2, I(Fetch)                             ),
  ROW(Insert32  , "insert32"    , 3, I(Fetch)                             ),
  ROW(Insert64  , "insert64"    , 3, I(Fetch)                             ),
  ROW(Store32   , "store32"     , 2, I(Store)                             ),
  ROW(Store64   , "store64"     , 2, I(Store)                             ),
  ROW(Store96   , "store96"     , 2, I(Store)                             ),
  ROW(Store128  , "store128"    , 2, I(Store)                             ),
  ROW(Store192  , "store192"    , 2, I(Store)                             ),
  ROW(Store256  , "store256"    , 2, I(Store)                             ),
  ROW(Extract32 , "extract32"   , 3, I(Store)                             ),
  ROW(Extract64 , "extract64"   , 3, I(Store)                             ),
  ROW(Mov32     , "mov32"       , 2, I(Mov)                               ),
  ROW(Mov64     , "mov64"       , 2, I(Mov)                               ),
  ROW(Mov128    , "mov128"      , 2, I(Mov)                               ),
  ROW(Mov256    , "mov256"      , 2, I(Mov)                               ),

  ROW(Cvtitof   , "cvtitof"     , 2, I(I32) | I(F32) | I(Cvt)             ),
  ROW(Cvtitod   , "cvtitod"     , 2, I(I32) | I(F64) | I(Cvt)             ),
  ROW(Cvtftoi   , "cvtftoi"     , 2, I(I32) | I(F32) | I(Cvt)             ),
  ROW(Cvtftod   , "cvtftod"     , 2, I(F32) | I(F64) | I(Cvt)             ),
  ROW(Cvtdtoi   , "cvtdtoi"     , 2, I(I32) | I(F64) | I(Cvt)             ),
  ROW(Cvtdtof   , "cvtdtof"     , 2, I(F32) | I(F64) | I(Cvt)             ),

  ROW(Absi      , "absi"        , 2, I(I32)                               ),
  ROW(Absf      , "absf"        , 2, I(F32)                               ),
  ROW(Absd      , "absd"        , 2, I(F64)                               ),
  ROW(Bitnegi   , "bitnegi"     , 2, I(I32)                               ),
  ROW(Bitnegf   , "bitnegf"     , 2, I(F32)                               ),
  ROW(Bitnegd   , "bitnegd"     , 2, I(F64)                               ),
  ROW(Negi      , "negi"        , 2, I(I32)                               ),
  ROW(Negf      , "negf"        , 2, I(F32)                               ),
  ROW(Negd      , "negd"        , 2, I(F64)                               ),
  ROW(Noti      , "noti"        , 2, I(I32)                               ),
  ROW(Notf      , "notf"        , 2, I(F32)                               ),
  ROW(Notd      , "notd"        , 2, I(F64)                               ),
  ROW(Signbiti  , "signbiti"    , 2, I(I32)                               ),
  ROW(Signbitf  , "signbitf"    , 2, I(F32)                               ),
  ROW(Signbitd  , "signbitd"    , 2, I(F64)                               ),

  ROW(Lzcnti    , "lzcnti"      , 2, I(I32)                               ),
  ROW(Popcnti   , "popcnti"     , 2, I(I32)                               ),

  ROW(Isnanf    , "isnanf"      , 2, I(F32)                               ),
  ROW(Isnand    , "isnand"      , 2, I(F64)                               ),
  ROW(Isinff    , "isinff"      , 2, I(F32)                               ),
  ROW(Isinfd    , "isinfd"      , 2, I(F64)                               ),
  ROW(Isfinitef , "isfinitef"   , 2, I(F32)                               ),
  ROW(Isfinited , "isfinited"   , 2, I(F64)                               ),

  ROW(Truncf    , "truncf"      , 2, I(F32)                               ),
  ROW(Truncd    , "truncd"      , 2, I(F64)                               ),
  ROW(Floorf    , "floorf"      , 2, I(F32)                               ),
  ROW(Floord    , "floord"      , 2, I(F64)                               ),
  ROW(Roundf    , "roundf"      , 2, I(F32)                               ),
  ROW(Roundd    , "roundd"      , 2, I(F64)                               ),
  ROW(Roundevenf, "roundevenf"  , 2, I(F32)                               ),
  ROW(Roundevend, "roundevend"  , 2, I(F64)                               ),
  ROW(Ceilf     , "ceilf"       , 2, I(F32)                               ),
  ROW(Ceild     , "ceild"       , 2, I(F64)                               ),
  ROW(Fracf     , "fracf"       , 2, I(F32)                               ),
  ROW(Fracd     , "fracd"       , 2, I(F64)                               ),

  ROW(Sqrtf     , "sqrtf"       , 2, I(F32)                               ),
  ROW(Sqrtd     , "sqrtd"       , 2, I(F64)                               ),
  ROW(Expf      , "expf"        , 2, I(F32) | I(Complex)                  ),
  ROW(Expd      , "expd"        , 2, I(F64) | I(Complex)                  ),
  ROW(Logf      , "logf"        , 2, I(F32) | I(Complex)                  ),
  ROW(Logd      , "logd"        , 2, I(F64) | I(Complex)                  ),
  ROW(Log2f     , "log2f"       , 2, I(F32) | I(Complex)                  ),
  ROW(Log2d     , "log2d"       , 2, I(F64) | I(Complex)                  ),
  ROW(Log10f    , "log10f"      , 2, I(F32) | I(Complex)                  ),
  ROW(Log10d    , "log10d"      , 2, I(F64) | I(Complex)                  ),

  ROW(Sinf      , "sinf"        , 2, I(F32) | I(Complex)                  ),
  ROW(Sind      , "sind"        , 2, I(F64) | I(Complex)                  ),
  ROW(Cosf      , "cosf"        , 2, I(F32) | I(Complex)                  ),
  ROW(Cosd      , "cosd"        , 2, I(F64) | I(Complex)                  ),
  ROW(Tanf      , "tanf"        , 2, I(F32) | I(Complex)                  ),
  ROW(Tand      , "tand"        , 2, I(F64) | I(Complex)                  ),
  ROW(Asinf     , "asinf"       , 2, I(F32) | I(Complex)                  ),
  ROW(Asind     , "asind"       , 2, I(F64) | I(Complex)                  ),
  ROW(Acosf     , "acosf"       , 2, I(F32) | I(Complex)                  ),
  ROW(Acosd     , "acosd"       , 2, I(F64) | I(Complex)                  ),
  ROW(Atanf     , "atanf"       , 2, I(F32) | I(Complex)                  ),
  ROW(Atand     , "atand"       , 2, I(F64) | I(Complex)                  ),

  ROW(Addi      , "addi"        , 3, I(I32)                               ),
  ROW(Addf      , "addf"        , 3, I(F32)                               ),
  ROW(Addd      , "addd"        , 3, I(F64)                               ),
  ROW(Subi      , "subi"        , 3, I(I32)                               ),
  ROW(Subf      , "subf"        , 3, I(F32)                               ),
  ROW(Subd      , "subd"        , 3, I(F64)                               ),
  ROW(Muli      , "muli"        , 3, I(I32)                               ),
  ROW(Mulf      , "mulf"        , 3, I(F32)                               ),
  ROW(Muld      , "muld"        , 3, I(F64)                               ),
  ROW(Divi      , "divi"        , 3, I(I32)                               ),
  ROW(Divf      , "divf"        , 3, I(F32)                               ),
  ROW(Divd      , "divd"        , 3, I(F64)                               ),
  ROW(Modi      , "modi"        , 3, I(I32) | I(Complex)                  ),
  ROW(Modf      , "modf"        , 3, I(F32) | I(Complex)                  ),
  ROW(Modd      , "modd"        , 3, I(F64) | I(Complex)                  ),
  ROW(Andi      , "andi"        , 3, I(I32)                               ),
  ROW(Andf      , "andf"        , 3, I(F32)                               ),
  ROW(Andd      , "andd"        , 3, I(F64)                               ),
  ROW(Ori       , "ori"         , 3, I(I32)                               ),
  ROW(Orf       , "orf"         , 3, I(F32)                               ),
  ROW(Ord       , "ord"         , 3, I(F64)                               ),
  ROW(Xori      , "xori"        , 3, I(I32)                               ),
  ROW(Xorf      , "xorf"        , 3, I(F32)                               ),
  ROW(Xord      , "xord"        , 3, I(F64)                               ),
  ROW(Mini      , "mini"        , 3, I(I32)                               ),
  ROW(Minf      , "minf"        , 3, I(F32)                               ),
  ROW(Mind      , "mind"        , 3, I(F64)                               ),
  ROW(Maxi      , "maxi"        , 3, I(I32)                               ),
  ROW(Maxf      , "maxf"        , 3, I(F32)                               ),
  ROW(Maxd      , "maxd"        , 3, I(F64)                               ),

  ROW(Slli      , "slli"        , 3, I(I32)                               ),
  ROW(Srli      , "srli"        , 3, I(I32)                               ),
  ROW(Srai      , "srai"        , 3, I(I32)                               ),
  ROW(Roli      , "roli"        , 3, I(I32)                               ),
  ROW(Rori      , "rori"        , 3, I(I32)                               ),

  ROW(Cmpeqi    , "cmpeqi"      , 3, I(I32)                               ),
  ROW(Cmpeqf    , "cmpeqf"      , 3, I(F32)                               ),
  ROW(Cmpeqd    , "cmpeqd"      , 3, I(F64)                               ),
  ROW(Cmpnei    , "cmpnei"      , 3, I(I32)                               ),
  ROW(Cmpnef    , "cmpnef"      , 3, I(F32)                               ),
  ROW(Cmpned    , "cmpned"      , 3, I(F64)                               ),
  ROW(Cmplti    , "cmplti"      , 3, I(I32)                               ),
  ROW(Cmpltf    , "cmpltf"      , 3, I(F32)                               ),
  ROW(Cmpltd    , "cmpltd"      , 3, I(F64)                               ),
  ROW(Cmplei    , "cmplei"      , 3, I(I32)                               ),
  ROW(Cmplef    , "cmplef"      , 3, I(F32)                               ),
  ROW(Cmpled    , "cmpled"      , 3, I(F64)                               ),
  ROW(Cmpgti    , "cmpgti"      , 3, I(I32)                               ),
  ROW(Cmpgtf    , "cmpgtf"      , 3, I(F32)                               ),
  ROW(Cmpgtd    , "cmpgtd"      , 3, I(F64)                               ),
  ROW(Cmpgei    , "cmpgei"      , 3, I(I32)                               ),
  ROW(Cmpgef    , "cmpgef"      , 3, I(F32)                               ),
  ROW(Cmpged    , "cmpged"      , 3, I(F64)                               ),

  ROW(Powf      , "powf"        , 3, I(F32) | I(Complex)                  ),
  ROW(Powd      , "powd"        , 3, I(F64) | I(Complex)                  ),
  ROW(Atan2f    , "atan2f"      , 3, I(F32) | I(Complex)                  ),
  ROW(Atan2d    , "atan2d"      , 3, I(F64) | I(Complex)                  ),
  ROW(Copysignf , "copysignf"   , 3, I(F32)                               ),
  ROW(Copysignd , "copysignd"   , 3, I(F64)                               ),

  ROW(Vabsb     , "vabsb"       , 2, I(I32)                               ),
  ROW(Vabsw     , "vabsw"       , 2, I(I32)                               ),
  ROW(Vabsd     , "vabsd"       , 2, I(I32)                               ),
  ROW(Vaddb     , "vaddb"       , 3, I(I32)                               ),
  ROW(Vaddw     , "vaddw"       , 3, I(I32)                               ),
  ROW(Vaddd     , "vaddd"       , 3, I(I32)                               ),
  ROW(Vaddq     , "vaddq"       , 3, I(I32)                               ),
  ROW(Vaddssb   , "vaddssb"     , 3, I(I32)                               ),
  ROW(Vaddusb   , "vaddusb"     , 3, I(I32)                               ),
  ROW(Vaddssw   , "vaddssw"     , 3, I(I32)                               ),
  ROW(Vaddusw   , "vaddusw"     , 3, I(I32)                               ),
  ROW(Vsubb     , "vsubb"       , 3, I(I32)                               ),
  ROW(Vsubw     , "vsubw"       , 3, I(I32)                               ),
  ROW(Vsubd     , "vsubd"       , 3, I(I32)                               ),
  ROW(Vsubq     , "vsubq"       , 3, I(I32)                               ),
  ROW(Vsubssb   , "vsubssb"     , 3, I(I32)                               ),
  ROW(Vsubusb   , "vsubusb"     , 3, I(I32)                               ),
  ROW(Vsubssw   , "vsubssw"     , 3, I(I32)                               ),
  ROW(Vsubusw   , "vsubusw"     , 3, I(I32)                               ),
  ROW(Vmulw     , "vmuld"       , 3, I(I32)                               ),
  ROW(Vmulhsw   , "vmulhsw"     , 3, I(I32)                               ),
  ROW(Vmulhuw   , "vmulhuw"     , 3, I(I32)                               ),
  ROW(Vmuld     , "vmuld"       , 3, I(I32)                               ),
  ROW(Vminsb    , "vminsb"      , 3, I(I32)                               ),
  ROW(Vminub    , "vminub"      , 3, I(I32)                               ),
  ROW(Vminsw    , "vminsw"      , 3, I(I32)                               ),
  ROW(Vminuw    , "vminuw"      , 3, I(I32)                               ),
  ROW(Vminsd    , "vminsd"      , 3, I(I32)                               ),
  ROW(Vminud    , "vminud"      , 3, I(I32)                               ),
  ROW(Vmaxsb    , "vmaxsb"      , 3, I(I32)                               ),
  ROW(Vmaxub    , "vmaxub"      , 3, I(I32)                               ),
  ROW(Vmaxsw    , "vmaxsw"      , 3, I(I32)                               ),
  ROW(Vmaxuw    , "vmaxuw"      , 3, I(I32)                               ),
  ROW(Vmaxsd    , "vmaxsd"      , 3, I(I32)                               ),
  ROW(Vmaxud    , "vmaxud"      , 3, I(I32)                               ),
  ROW(Vsllw     , "vsllw"       , 3, I(I32)                               ),
  ROW(Vsrlw     , "vsrlw"       , 3, I(I32)                               ),
  ROW(Vsraw     , "vsraw"       , 3, I(I32)                               ),
  ROW(Vslld     , "vslld"       , 3, I(I32)                               ),
  ROW(Vsrld     , "vsrld"       , 3, I(I32)                               ),
  ROW(Vsrad     , "vsrad"       , 3, I(I32)                               ),
  ROW(Vsllq     , "vsllq"       , 3, I(I32)                               ),
  ROW(Vsrlq     , "vsrlq"       , 3, I(I32)                               ),
  ROW(Vcmpeqb   , "vcmpeqb"     , 3, I(I32)                               ),
  ROW(Vcmpeqw   , "vcmpeqw"     , 3, I(I32)                               ),
  ROW(Vcmpeqd   , "vcmpeqd"     , 3, I(I32)                               ),
  ROW(Vcmpgtb   , "vcmpgtb"     , 3, I(I32)                               ),
  ROW(Vcmpgtw   , "vcmpgtw"     , 3, I(I32)                               ),
  ROW(Vcmpgtd   , "vcmpgtd"     , 3, I(I32)                               )
};
#undef I
#undef ROW

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
