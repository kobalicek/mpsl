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
#define MPSL_VECTOR_LETTERS(I0, I1, I2, I3, I4, I5, I6, I7) \
  {                                                         \
    { I0, I1, I2, I3, I4, I5, I6, I7 },                     \
    (1u << (I0 - 'a')) | (1u << (I1 - 'a')) |               \
    (1u << (I2 - 'a')) | (1u << (I3 - 'a')) |               \
    (1u << (I4 - 'a')) | (1u << (I5 - 'a')) |               \
    (1u << (I6 - 'a')) | (1u << (I7 - 'a'))                 \
  }
const VectorIdentifiers mpVectorIdentifiers[2] = {
  MPSL_VECTOR_LETTERS('x', 'y', 'z', 'w', 'i', 'j', 'k', 'l'),
  MPSL_VECTOR_LETTERS('r', 'g', 'b', 'a', 'i', 'j', 'k', 'l')
};
#undef MPSL_VECTOR_LETTERS

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
  ROW(Swizzle     , "(swizzle)", None  , 1, 3, 0, 0, LTR | 0                 | F(AnyOp)                , None      , None      ),
  ROW(PreInc      , "++(.)"    , None  , 1, 3,-1, 0, RTL | F(Arithmetic)     | F(AnyOp)                , Paddd     , Addf      ),
  ROW(PreDec      , "--(.)"    , None  , 1, 3,-1, 0, RTL | F(Arithmetic)     | F(AnyOp)                , Psubd     , Subf      ),
  ROW(PostInc     , "(.)++"    , None  , 1, 2, 1, 0, LTR | F(Arithmetic)     | F(AnyOp)                , Paddd     , Addf      ),
  ROW(PostDec     , "(.)--"    , None  , 1, 2, 1, 0, LTR | F(Arithmetic)     | F(AnyOp)                , Psubd     , Subf      ),
  ROW(Abs         , "abs"      , None  , 1, 0, 0, 1, LTR | 0                 | F(IntFPOp)              , Pabsd     , Absf      ),
  ROW(BitNeg      , "~"        , None  , 1, 3, 0, 0, RTL | F(Bitwise)        | F(AnyOp)                , Bitnegi   , Bitnegf   ),
  ROW(Neg         , "-"        , None  , 1, 3, 0, 0, RTL | F(Arithmetic)     | F(IntFPOp)              , Negi      , Negf      ),
  ROW(Not         , "!"        , None  , 1, 3, 0, 0, RTL | F(Conditional)    | F(AnyOp)                , Noti      , Notf      ),
  ROW(IsNan       , "isnan"    , None  , 1, 0, 0, 1, LTR | F(Conditional)    | F(FloatOp)              , None      , Isnanf    ),
  ROW(IsInf       , "isinf"    , None  , 1, 0, 0, 1, LTR | F(Conditional)    | F(FloatOp)              , None      , Isinff    ),
  ROW(IsFinite    , "isfinite" , None  , 1, 0, 0, 1, LTR | F(Conditional)    | F(FloatOp)              , None      , Isfinitef ),
  ROW(SignMask    , "signmask" , None  , 1, 0, 0, 1, LTR | F(Conditional)    | F(IntFPOp)              , Signmaski , Signmaski ),
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
  ROW(Pabsb       , "pabsb"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pabsb     , None      ),
  ROW(Pabsw       , "pabsw"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pabsw     , None      ),
  ROW(Pabsd       , "pabsd"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pabsd     , None      ),
  ROW(Lzcnt       , "lzcnt"    , None  , 1, 0, 0, 1, LTR | 0                 | F(IntOp)                , Lzcnti    , None      ),
  ROW(Popcnt      , "popcnt"   , None  , 1, 0, 0, 1, LTR | 0                 | F(IntOp)                , Popcnti   , None      ),
  ROW(Assign      , "="        , Assign, 2,15,-1, 0, RTL | 0                                           , None      , None      ),
  ROW(AssignAdd   , "+="       , Add   , 2,15,-1, 0, RTL | F(Arithmetic)     | F(NopIfR0) | F(NopIfR0) , Paddd     , Addf      ),
  ROW(AssignSub   , "-="       , Sub   , 2,15,-1, 0, RTL | F(Arithmetic)     | F(NopIfR0) | F(NopIfR0) , Psubd     , Subf      ),
  ROW(AssignMul   , "*="       , Mul   , 2,15,-1, 0, RTL | F(Arithmetic)                  | F(NopIfR1) , Pmuld     , Mulf      ),
  ROW(AssignDiv   , "/="       , Div   , 2,15,-1, 0, RTL | F(Arithmetic)     | F(NopIfR1) | F(NopIfR1) , Pdivsd    , Divf      ),
  ROW(AssignMod   , "%="       , Mod   , 2,15,-1, 0, RTL | F(Arithmetic)                               , Pmodsd    , Modf      ),
  ROW(AssignAnd   , "&="       , And   , 2,15,-1, 0, RTL | F(Bitwise)        | F(AnyOp)                , Andi      , Andf      ),
  ROW(AssignOr    , "|="       , Or    , 2,15,-1, 0, RTL | F(Bitwise)        | F(AnyOp)   | F(NopIfR0) , Ori       , Orf       ),
  ROW(AssignXor   , "^="       , Xor   , 2,15,-1, 0, RTL | F(Bitwise)        | F(AnyOp)   | F(NopIfR0) , Xori      , Xorf      ),
  ROW(AssignSll   , "<<="      , Sll   , 2,15,-1, 0, RTL | F(Shift)          | F(IntOp)   | F(NopIfR0) , Pslld     , None      ),
  ROW(AssignSrl   , ">>>="     , Srl   , 2,15,-1, 0, RTL | F(Shift)          | F(IntOp)   | F(NopIfR0) , Psrld     , None      ),
  ROW(AssignSra   , ">>="      , Sra   , 2,15,-1, 0, RTL | F(Shift)          | F(IntOp)   | F(NopIfR0) , Psrad     , None      ),
  ROW(Add         , "+"        , None  , 2, 6, 0, 0, LTR | F(Arithmetic)     | F(IntFPOp) | F(NopIf0)  , Paddd     , Addf      ),
  ROW(Sub         , "-"        , None  , 2, 6, 0, 0, LTR | F(Arithmetic)     | F(IntFPOp) | F(NopIfR0) , Psubd     , Subf      ),
  ROW(Mul         , "*"        , None  , 2, 5, 0, 0, LTR | F(Arithmetic)     | F(IntFPOp) | F(NopIf1)  , Pmuld     , Mulf      ),
  ROW(Div         , "/"        , None  , 2, 5, 0, 0, LTR | F(Arithmetic)     | F(IntFPOp) | F(NopIfR1) , Pdivsd    , Divf      ),
  ROW(Mod         , "%"        , None  , 2, 5, 0, 0, LTR | F(Arithmetic)     | F(IntFPOp)              , Pmodsd    , Modf      ),
  ROW(And         , "&"        , None  , 2,10, 0, 0, LTR | F(Bitwise)        | F(AnyOp)                , Andi      , Andf      ),
  ROW(Or          , "|"        , None  , 2,12, 0, 0, LTR | F(Bitwise)        | F(AnyOp)   | F(NopIf0)  , Ori       , Orf       ),
  ROW(Xor         , "^"        , None  , 2,11, 0, 0, LTR | F(Bitwise)        | F(AnyOp)   | F(NopIf0)  , Xori      , Xorf      ),
  ROW(Min         , "min"      , None  , 2, 0, 0, 1, LTR | 0                 | F(AnyOp)                , Pminsd    , Minf      ),
  ROW(Max         , "max"      , None  , 2, 0, 0, 1, LTR | 0                 | F(AnyOp)                , Pmaxsd    , Maxf      ),
  ROW(Sll         , "<<"       , None  , 2, 7, 0, 0, LTR | F(Shift)          | F(IntOp)   | F(NopIf0)  , Pslld     , None      ),
  ROW(Srl         , ">>>"      , None  , 2, 7, 0, 0, LTR | F(Shift)          | F(IntOp)   | F(NopIf0)  , Psrld     , None      ),
  ROW(Sra         , ">>"       , None  , 2, 7, 0, 0, LTR | F(Shift)          | F(IntOp)   | F(NopIf0)  , Psrad     , None      ),
  ROW(Rol         , "rol"      , None  , 2, 0, 0, 1, LTR | F(Shift)          | F(IntOp)   | F(NopIf0)  , Roli      , None      ),
  ROW(Ror         , "ror"      , None  , 2, 0, 0, 1, LTR | F(Shift)          | F(IntOp)   | F(NopIf0)  , Rori      , None      ),
  ROW(CopySign    , "copysign" , None  , 2, 0, 0, 1, LTR | 0                 | F(FloatOp)              , None      , Copysignf ),
  ROW(Pow         , "pow"      , None  , 2, 0, 0, 1, LTR | 0                 | F(FloatOp) | F(NopIfR1) , None      , Powf      ),
  ROW(Atan2       , "atan2"    , None  , 2, 0, 0, 1, LTR | F(Trigonometric)  | F(FloatOp)              , None      , Atan2f    ),
  ROW(LogAnd      , "&&"       , None  , 2,13, 0, 0, LTR | F(Conditional)    | F(BoolOp)  | F(Logical) , Andi      , Andf      ),
  ROW(LogOr       , "||"       , None  , 2,14, 0, 0, LTR | F(Conditional)    | F(BoolOp)  | F(Logical) , Ori       , Orf       ),
  ROW(CmpEq       , "=="       , None  , 2, 9, 0, 0, LTR | F(Conditional)    | F(AnyOp)                , Pcmpeqd   , Cmpeqf    ),
  ROW(CmpNe       , "!="       , None  , 2, 9, 0, 0, LTR | F(Conditional)    | F(AnyOp)                , Pcmpned   , Cmpnef    ),
  ROW(CmpLt       , "<"        , None  , 2, 8, 0, 0, LTR | F(Conditional)    | F(IntFPOp)              , Pcmpltd   , Cmpltf    ),
  ROW(CmpLe       , "<="       , None  , 2, 8, 0, 0, LTR | F(Conditional)    | F(IntFPOp)              , Pcmpled   , Cmplef    ),
  ROW(CmpGt       , ">"        , None  , 2, 8, 0, 0, LTR | F(Conditional)    | F(IntFPOp)              , Pcmpgtd   , Cmpgtf    ),
  ROW(CmpGe       , ">="       , None  , 2, 8, 0, 0, LTR | F(Conditional)    | F(IntFPOp)              , Pcmpged   , Cmpgef    ),
  ROW(Pmovsxbw    , "vmovsxbw" , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Unpack)| F(IntOp)                , Pmovsxbw  , None      ),
  ROW(Pmovzxbw    , "vmovzxbw" , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Unpack)| F(IntOp)                , Pmovzxbw  , None      ),
  ROW(Pmovsxwd    , "vmovsxwd" , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Unpack)| F(IntOp)                , Pmovsxwd  , None      ),
  ROW(Pmovzxwd    , "vmovzxwd" , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Unpack)| F(IntOp)                , Pmovzxwd  , None      ),
  ROW(Packsswb    , "packsswb" , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Pack)  | F(IntOp)                , Packsswb  , None      ),
  ROW(Packuswb    , "packuswb" , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Pack)  | F(IntOp)                , Packuswb  , None      ),
  ROW(Packssdw    , "packssdw" , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Pack)  | F(IntOp)                , Packssdw  , None      ),
  ROW(Packusdw    , "packusdw" , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Pack)  | F(IntOp)                , Packusdw  , None      ),
  ROW(Paddb       , "paddb"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Paddb     , None      ),
  ROW(Paddw       , "paddw"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Paddw     , None      ),
  ROW(Paddd       , "paddd"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Paddd     , None      ),
  ROW(Paddq       , "paddq"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Paddq     , None      ),
  ROW(Paddssb     , "paddssb"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Paddssb   , None      ),
  ROW(Paddusb     , "paddusb"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Paddusb   , None      ),
  ROW(Paddssw     , "paddssw"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Paddssw   , None      ),
  ROW(Paddusw     , "paddusw"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Paddusw   , None      ),
  ROW(Psubb       , "psubb"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Psubb     , None      ),
  ROW(Psubw       , "psubw"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Psubw     , None      ),
  ROW(Psubd       , "psubd"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Psubd     , None      ),
  ROW(Psubq       , "psubq"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Psubq     , None      ),
  ROW(Psubssb     , "psubssb"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Psubssb   , None      ),
  ROW(Psubusb     , "psubusb"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Psubusb   , None      ),
  ROW(Psubssw     , "psubssw"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Psubssw   , None      ),
  ROW(Psubusw     , "psubusw"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Psubusw   , None      ),
  ROW(Pmulw       , "pmulw"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pmulw     , None      ),
  ROW(Pmulhsw     , "pmulhsw"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pmulhsw   , None      ),
  ROW(Pmulhuw     , "pmulhuw"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pmulhuw   , None      ),
  ROW(Pmuld       , "pmuld"    , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pmuld     , None      ),
  ROW(Pminsb      , "pminsb"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pminsb    , None      ),
  ROW(Pminub      , "pminub"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pminub    , None      ),
  ROW(Pminsw      , "pminsw"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pminsw    , None      ),
  ROW(Pminuw      , "pminuw"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pminuw    , None      ),
  ROW(Pminsd      , "pminsd"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pminsd    , None      ),
  ROW(Pminud      , "pminud"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pminud    , None      ),
  ROW(Pmaxsb      , "pmaxsb"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pmaxsb    , None      ),
  ROW(Pmaxub      , "pmaxub"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pmaxub    , None      ),
  ROW(Pmaxsw      , "pmaxsw"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pmaxsw    , None      ),
  ROW(Pmaxuw      , "pmaxuw"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pmaxuw    , None      ),
  ROW(Pmaxsd      , "pmaxsd"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pmaxsd    , None      ),
  ROW(Pmaxud      , "pmaxud"   , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pmaxud    , None      ),
  ROW(Psllw       , "psllw"    , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Shift) | F(IntOp)                , Psllw     , None      ),
  ROW(Psrlw       , "psrlw"    , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Shift) | F(IntOp)                , Psrlw     , None      ),
  ROW(Psraw       , "psraw"    , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Shift) | F(IntOp)                , Psraw     , None      ),
  ROW(Pslld       , "pslld"    , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Shift) | F(IntOp)                , Pslld     , None      ),
  ROW(Psrld       , "psrld"    , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Shift) | F(IntOp)                , Psrld     , None      ),
  ROW(Psrad       , "psrad"    , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Shift) | F(IntOp)                , Psrad     , None      ),
  ROW(Psllq       , "psllq"    , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Shift) | F(IntOp)                , Psllq     , None      ),
  ROW(Psrlq       , "psrlq"    , None  , 2, 0, 0, 1, LTR | F(DSP) | F(Shift) | F(IntOp)                , Psrlq     , None      ),
  ROW(Pmaddwd     , "pmaddwd"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pmaddwd   , None      ),
  ROW(Pcmpeqb     , "pcmpeqb"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pcmpeqb   , None      ),
  ROW(Pcmpeqw     , "pcmpeqw"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pcmpeqw   , None      ),
  ROW(Pcmpeqd     , "pcmpeqd"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pcmpeqd   , None      ),
  ROW(Pcmpneb     , "pcmpneb"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pcmpneb   , None      ),
  ROW(Pcmpnew     , "pcmpnew"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pcmpnew   , None      ),
  ROW(Pcmpned     , "pcmpned"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pcmpned   , None      ),
  ROW(Pcmpltb     , "pcmpltb"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pcmpltb   , None      ),
  ROW(Pcmpltw     , "pcmpltw"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pcmpltw   , None      ),
  ROW(Pcmpltd     , "pcmpltd"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pcmpltd   , None      ),
  ROW(Pcmpleb     , "pcmpleb"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pcmpleb   , None      ),
  ROW(Pcmplew     , "pcmplew"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pcmplew   , None      ),
  ROW(Pcmpled     , "pcmpled"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pcmpled   , None      ),
  ROW(Pcmpgtb     , "pcmpgtb"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pcmpgtb   , None      ),
  ROW(Pcmpgtw     , "pcmpgtw"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pcmpgtw   , None      ),
  ROW(Pcmpgtd     , "pcmpgtd"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pcmpgtd   , None      ),
  ROW(Pcmpgeb     , "pcmpgeb"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pcmpgeb   , None      ),
  ROW(Pcmpgew     , "pcmpgew"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pcmpgew   , None      ),
  ROW(Pcmpged     , "pcmpged"  , None  , 2, 0, 0, 1, LTR | F(DSP)            | F(IntOp)                , Pcmpged   , None      )
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
  ROW(Jnz       , "jnz"         , 2, I(Jxx)                               ),
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
  ROW(Signmaski , "signmaski"   , 2, I(I32)                               ),
  ROW(Signmaskf , "signmaskf"   , 2, I(F32)                               ),
  ROW(Signmaskd , "signmaskd"   , 2, I(F64)                               ),

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

  ROW(Pabsb     , "pabsb"       , 2, I(I32)                               ),
  ROW(Pabsw     , "pabsw"       , 2, I(I32)                               ),
  ROW(Pabsd     , "pabsd"       , 2, I(I32)                               ),
  ROW(Lzcnti    , "lzcnti"      , 2, I(I32)                               ),
  ROW(Popcnti   , "popcnti"     , 2, I(I32)                               ),

  ROW(Addf      , "addf"        , 3, I(F32)                               ),
  ROW(Addd      , "addd"        , 3, I(F64)                               ),
  ROW(Subf      , "subf"        , 3, I(F32)                               ),
  ROW(Subd      , "subd"        , 3, I(F64)                               ),
  ROW(Mulf      , "mulf"        , 3, I(F32)                               ),
  ROW(Muld      , "muld"        , 3, I(F64)                               ),
  ROW(Divf      , "divf"        , 3, I(F32)                               ),
  ROW(Divd      , "divd"        , 3, I(F64)                               ),
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
  ROW(Minf      , "minf"        , 3, I(F32)                               ),
  ROW(Mind      , "mind"        , 3, I(F64)                               ),
  ROW(Maxf      , "maxf"        , 3, I(F32)                               ),
  ROW(Maxd      , "maxd"        , 3, I(F64)                               ),

  ROW(Roli      , "roli"        , 3, I(I32)                       | I(Imm)),
  ROW(Rori      , "rori"        , 3, I(I32)                       | I(Imm)),

  ROW(Cmpeqf    , "cmpeqf"      , 3, I(F32)                               ),
  ROW(Cmpeqd    , "cmpeqd"      , 3, I(F64)                               ),
  ROW(Cmpnef    , "cmpnef"      , 3, I(F32)                               ),
  ROW(Cmpned    , "cmpned"      , 3, I(F64)                               ),
  ROW(Cmpltf    , "cmpltf"      , 3, I(F32)                               ),
  ROW(Cmpltd    , "cmpltd"      , 3, I(F64)                               ),
  ROW(Cmplef    , "cmplef"      , 3, I(F32)                               ),
  ROW(Cmpled    , "cmpled"      , 3, I(F64)                               ),
  ROW(Cmpgtf    , "cmpgtf"      , 3, I(F32)                               ),
  ROW(Cmpgtd    , "cmpgtd"      , 3, I(F64)                               ),
  ROW(Cmpgef    , "cmpgef"      , 3, I(F32)                               ),
  ROW(Cmpged    , "cmpged"      , 3, I(F64)                               ),

  ROW(Copysignf , "copysignf"   , 3, I(F32)                               ),
  ROW(Copysignd , "copysignd"   , 3, I(F64)                               ),
  ROW(Powf      , "powf"        , 3, I(F32) | I(Complex)                  ),
  ROW(Powd      , "powd"        , 3, I(F64) | I(Complex)                  ),
  ROW(Atan2f    , "atan2f"      , 3, I(F32) | I(Complex)                  ),
  ROW(Atan2d    , "atan2d"      , 3, I(F64) | I(Complex)                  ),

  ROW(Pshufd    , "pshufd"      , 3, I(I32) | I(F32) | I(F64)     | I(Imm)),

  ROW(Pmovsxbw  , "pmovsxbw"    , 3, I(I32)                               ),
  ROW(Pmovzxbw  , "pmovzxbw"    , 3, I(I32)                               ),
  ROW(Pmovsxwd  , "pmovsxwd"    , 3, I(I32)                               ),
  ROW(Pmovzxwd  , "pmovzxwd"    , 3, I(I32)                               ),
  ROW(Packsswb  , "ppacksswb"   , 3, I(I32)                               ),
  ROW(Packuswb  , "ppackuswb"   , 3, I(I32)                               ),
  ROW(Packssdw  , "ppackssdw"   , 3, I(I32)                               ),
  ROW(Packusdw  , "ppackusdw"   , 3, I(I32)                               ),
  ROW(Paddb     , "paddb"       , 3, I(I32)                               ),
  ROW(Paddw     , "paddw"       , 3, I(I32)                               ),
  ROW(Paddd     , "paddd"       , 3, I(I32)                               ),
  ROW(Paddq     , "paddq"       , 3, I(I32)                               ),
  ROW(Paddssb   , "paddssb"     , 3, I(I32)                               ),
  ROW(Paddusb   , "paddusb"     , 3, I(I32)                               ),
  ROW(Paddssw   , "paddssw"     , 3, I(I32)                               ),
  ROW(Paddusw   , "paddusw"     , 3, I(I32)                               ),
  ROW(Psubb     , "psubb"       , 3, I(I32)                               ),
  ROW(Psubw     , "psubw"       , 3, I(I32)                               ),
  ROW(Psubd     , "psubd"       , 3, I(I32)                               ),
  ROW(Psubq     , "psubq"       , 3, I(I32)                               ),
  ROW(Psubssb   , "psubssb"     , 3, I(I32)                               ),
  ROW(Psubusb   , "psubusb"     , 3, I(I32)                               ),
  ROW(Psubssw   , "psubssw"     , 3, I(I32)                               ),
  ROW(Psubusw   , "psubusw"     , 3, I(I32)                               ),
  ROW(Pmulw     , "pmuld"       , 3, I(I32)                               ),
  ROW(Pmulhsw   , "pmulhsw"     , 3, I(I32)                               ),
  ROW(Pmulhuw   , "pmulhuw"     , 3, I(I32)                               ),
  ROW(Pmuld     , "pmuld"       , 3, I(I32)                               ),
  ROW(Pdivsd    , "pdivsd"      , 3, I(I32)                               ),
  ROW(Pmodsd    , "pmodsd"      , 3, I(I32)                               ),
  ROW(Pminsb    , "pminsb"      , 3, I(I32)                               ),
  ROW(Pminub    , "pminub"      , 3, I(I32)                               ),
  ROW(Pminsw    , "pminsw"      , 3, I(I32)                               ),
  ROW(Pminuw    , "pminuw"      , 3, I(I32)                               ),
  ROW(Pminsd    , "pminsd"      , 3, I(I32)                               ),
  ROW(Pminud    , "pminud"      , 3, I(I32)                               ),
  ROW(Pmaxsb    , "pmaxsb"      , 3, I(I32)                               ),
  ROW(Pmaxub    , "pmaxub"      , 3, I(I32)                               ),
  ROW(Pmaxsw    , "pmaxsw"      , 3, I(I32)                               ),
  ROW(Pmaxuw    , "pmaxuw"      , 3, I(I32)                               ),
  ROW(Pmaxsd    , "pmaxsd"      , 3, I(I32)                               ),
  ROW(Pmaxud    , "pmaxud"      , 3, I(I32)                               ),
  ROW(Psllw     , "psllw"       , 3, I(I32)                       | I(Imm)),
  ROW(Psrlw     , "psrlw"       , 3, I(I32)                       | I(Imm)),
  ROW(Psraw     , "psraw"       , 3, I(I32)                       | I(Imm)),
  ROW(Pslld     , "pslld"       , 3, I(I32)                       | I(Imm)),
  ROW(Psrld     , "psrld"       , 3, I(I32)                       | I(Imm)),
  ROW(Psrad     , "psrad"       , 3, I(I32)                       | I(Imm)),
  ROW(Psllq     , "psllq"       , 3, I(I32)                       | I(Imm)),
  ROW(Psrlq     , "psrlq"       , 3, I(I32)                       | I(Imm)),
  ROW(Pmaddwd   , "pmaddwd"     , 3, I(I32)                               ),
  ROW(Pcmpeqb   , "pcmpeqb"     , 3, I(I32)                               ),
  ROW(Pcmpeqw   , "pcmpeqw"     , 3, I(I32)                               ),
  ROW(Pcmpeqd   , "pcmpeqd"     , 3, I(I32)                               ),
  ROW(Pcmpgtb   , "pcmpgtb"     , 3, I(I32)                               ),
  ROW(Pcmpgtw   , "pcmpgtw"     , 3, I(I32)                               ),
  ROW(Pcmpgtd   , "pcmpgtd"     , 3, I(I32)                               )
};
#undef I
#undef ROW

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
