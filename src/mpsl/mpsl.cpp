// [MPSL]
// Shader-Like Mathematical Expression JIT Engine for C++.
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
  { kTypeVoid    , 0                 , 0, 4, "void"     },
  { kTypeInt     , kTypeFlagInt      , 4, 3, "int"      },
  { kTypeFloat   , kTypeFlagReal     , 4, 5, "float"    },
  { kTypeDouble  , kTypeFlagReal     , 8, 6, "double"   },
  { kTypeBool32  , kTypeFlagBool     , 4, 6, "__bool32" },
  { kTypeBool64  , kTypeFlagBool     , 4, 6, "__bool64" },
  { kTypeObject  , kTypeFlagObject   , 0, 8, "__object" }
};

// 1. Nothing can be casted to `float` except `__boolXX`. This prevents from
//    losing precision accidentaly. Use explicit cast to cast to the float
//    type.
//
// 2. Only `__boolXX`, `int`, and `float` can be implicitly casted to `double`,
//    otherwise an explicit cast is required.
#define T(id) (1 << kType##id)
const uint32_t mpImplicitCast[kTypeCount] = {
  /* void     */ 0,

  /* int      */ T(Bool32) | T(Bool64) | 0      | 0        | 0,
  /* float    */ T(Bool32) | T(Bool64) | 0      | 0        | 0,
  /* double   */ T(Bool32) | T(Bool64) | T(Int) | T(Float) | 0,
  /* __bool32 */ 0         | T(Bool64) | T(Int) | T(Float) | T(Double),
  /* __bool64 */ 0         | T(Bool64) | T(Int) | T(Float) | T(Double),
  /* __object */ 0         | 0         | 0      | 0        | 0
};
#undef T

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
// [mpsl::mpElementNamnes]
// ============================================================================

// NOTE: Each letter can only be used at a specific index - indexes can repeat,
// in multiple `ElementNames` records, but cannot change once assigned.
const ElementNames mpElementNames[2] = {
  { 'x', 'y', 'z', 'w', 'i', 'j', 'k', 'l' }, // xyzwijkl.
  { 'r', 'g', 'b', 'a', 'i', 'j', 'k', 'l' }  // rgbaijkl.
};

// ============================================================================
// [mpsl::mpOpInfo]
// ============================================================================

// Operator information, precedence and association. The table is mostly based
// on the C-language standard, but also adjusted to support MPSL specific
// operators and rules. However, the associativity and precedence should be
// fully compatible with C.
#define ROW(opType, altType, params, precedence, assignment, intrinsic, flags, name) \
  { \
    static_cast<uint8_t>(kOp##opType), \
    static_cast<uint8_t>(kOp##altType), \
    static_cast<uint8_t>(precedence), \
    0, \
    static_cast<uint32_t>( \
      flags | (assignment ==-1 ? kOpFlagAssign : \
               assignment == 1 ? kOpFlagAssign | kOpFlagAssignPost : 0) \
            | (params     == 1 ? kOpFlagUnary : \
               params     == 2 ? kOpFlagBinary : 0) \
            | (intrinsic  == 1 ? kOpFlagIntrinsic : 0)), \
    name \
  }
#define LTR 0
#define RTL kOpFlagRightToLeft
#define F(flag) kOpFlag##flag
const OpInfo mpOpInfo[kOpCount] = {
  // +-------------+----------+--+--+--+--+-----+------------------------------------+------------+
  // |Operator     | Alt.     |#N|#P|:=|#I|Assoc| Flags                              | Name       |
  // +-------------+----------+--+--+--+--+-----+------------------------------------+------------+
  ROW(None         , None     , 0, 0, 0, 0, LTR | 0                                  , "<none>"   ),
  ROW(Cast         , Cast     , 1, 3, 0, 0, RTL | 0                                  , "(cast)"   ),
  ROW(PreInc       , PreInc   , 1, 3,-1, 0, RTL | F(Arithmetic)                      , "++(.)"    ),
  ROW(PreDec       , PreDec   , 1, 3,-1, 0, RTL | F(Arithmetic)                      , "--(.)"    ),
  ROW(PostInc      , PostInc  , 1, 2, 1, 0, LTR | F(Arithmetic)                      , "(.)++"    ),
  ROW(PostDec      , PostDec  , 1, 2, 1, 0, LTR | F(Arithmetic)                      , "(.)--"    ),
  ROW(BitNeg       , BitNeg   , 1, 3, 0, 0, RTL | F(BitMask)                         , "~"        ),
  ROW(Neg          , Neg      , 1, 3, 0, 0, RTL | F(Arithmetic)                      , "-"        ),
  ROW(Not          , Not      , 1, 3, 0, 0, RTL | F(Condition)                       , "!"        ),
  ROW(IsNan        , IsNan    , 1, 0, 0, 1, LTR | F(Condition)     | F(FloatOnly)    , "isnan"    ),
  ROW(IsInf        , IsInf    , 1, 0, 0, 1, LTR | F(Condition)     | F(FloatOnly)    , "isinf"    ),
  ROW(IsFinite     , IsFinite , 1, 0, 0, 1, LTR | F(Condition)     | F(FloatOnly)    , "isfinite" ),
  ROW(SignBit      , SignBit  , 1, 0, 0, 1, LTR | F(Condition)                       , "signbit"  ),
  ROW(Round        , Round    , 1, 0, 0, 1, LTR | F(Rounding)      | F(FloatOnly)    , "round"    ),
  ROW(RoundEven    , RoundEven, 1, 0, 0, 1, LTR | F(Rounding)      | F(FloatOnly)    , "roundeven"),
  ROW(Trunc        , Trunc    , 1, 0, 0, 1, LTR | F(Rounding)      | F(FloatOnly)    , "trunc"    ),
  ROW(Floor        , Floor    , 1, 0, 0, 1, LTR | F(Rounding)      | F(FloatOnly)    , "floor"    ),
  ROW(Ceil         , Ceil     , 1, 0, 0, 1, LTR | F(Rounding)      | F(FloatOnly)    , "ceil"     ),
  ROW(Abs          , Abs      , 1, 0, 0, 1, LTR | 0                                  , "abs"      ),
  ROW(Exp          , Exp      , 1, 0, 0, 1, LTR | 0                | F(FloatOnly)    , "exp"      ),
  ROW(Log          , Log      , 1, 0, 0, 1, LTR | 0                | F(FloatOnly)    , "log"      ),
  ROW(Log2         , Log2     , 1, 0, 0, 1, LTR | 0                | F(FloatOnly)    , "log2"     ),
  ROW(Log10        , Log10    , 1, 0, 0, 1, LTR | 0                | F(FloatOnly)    , "log10"    ),
  ROW(Sqrt         , Sqrt     , 1, 0, 0, 1, LTR | 0                | F(FloatOnly)    , "sqrt"     ),
  ROW(Frac         , Frac     , 1, 0, 0, 1, LTR | 0                | F(FloatOnly)    , "frac"     ),
  ROW(Sin          , Sin      , 1, 0, 0, 1, LTR | F(Trigonometric) | F(FloatOnly)    , "sin"      ),
  ROW(Cos          , Cos      , 1, 0, 0, 1, LTR | F(Trigonometric) | F(FloatOnly)    , "cos"      ),
  ROW(Tan          , Tan      , 1, 0, 0, 1, LTR | F(Trigonometric) | F(FloatOnly)    , "tan"      ),
  ROW(Asin         , Asin     , 1, 0, 0, 1, LTR | F(Trigonometric) | F(FloatOnly)    , "asin"     ),
  ROW(Acos         , Acos     , 1, 0, 0, 1, LTR | F(Trigonometric) | F(FloatOnly)    , "acos"     ),
  ROW(Atan         , Atan     , 1, 0, 0, 1, LTR | F(Trigonometric) | F(FloatOnly)    , "atan"     ),
  ROW(Assign       , Assign   , 2,15,-1, 0, RTL | 0                                  , "="        ),
  ROW(AssignAdd    , Add      , 2,15,-1, 0, RTL | F(Arithmetic)    | F(NopIfRZero)   , "+="       ),
  ROW(AssignSub    , Sub      , 2,15,-1, 0, RTL | F(Arithmetic)    | F(NopIfRZero)   , "-="       ),
  ROW(AssignMul    , Mul      , 2,15,-1, 0, RTL | F(Arithmetic)                      , "&="       ),
  ROW(AssignDiv    , Div      , 2,15,-1, 0, RTL | F(Arithmetic)    | F(NopIfROne)    , "/="       ),
  ROW(AssignMod    , Mod      , 2,15,-1, 0, RTL | F(Arithmetic)                      , "%="       ),
  ROW(AssignBitAnd , BitAnd   , 2,15,-1, 0, RTL | F(BitMask)                         , "&="       ),
  ROW(AssignBitOr  , BitOr    , 2,15,-1, 0, RTL | F(BitMask)       | F(NopIfRZero)   , "|="       ),
  ROW(AssignBitXor , BitXor   , 2,15,-1, 0, RTL | F(BitMask)       | F(NopIfRZero)   , "^="       ),
  ROW(AssignBitSar , BitSar   , 2,15,-1, 0, RTL | F(BitShift)      | F(NopIfRZero)   , ">>="      ),
  ROW(AssignBitShr , BitShr   , 2,15,-1, 0, RTL | F(BitShift)      | F(NopIfRZero)   , ">>>="     ),
  ROW(AssignBitShl , BitShl   , 2,15,-1, 0, RTL | F(BitShift)      | F(NopIfRZero)   , "<<="      ),
  ROW(Eq           , Eq       , 2, 9, 0, 0, LTR | F(Condition)                       , "=="       ),
  ROW(Ne           , Ne       , 2, 9, 0, 0, LTR | F(Condition)                       , "!="       ),
  ROW(Lt           , Lt       , 2, 8, 0, 0, LTR | F(Condition)                       , "<"        ),
  ROW(Le           , Le       , 2, 8, 0, 0, LTR | F(Condition)                       , "<="       ),
  ROW(Gt           , Gt       , 2, 8, 0, 0, LTR | F(Condition)                       , ">"        ),
  ROW(Ge           , Ge       , 2, 8, 0, 0, LTR | F(Condition)                       , ">="       ),
  ROW(LogAnd       , LogAnd   , 2,13, 0, 0, LTR | F(Condition)     | F(Logical)      , "&&"       ),
  ROW(LogOr        , LogOr    , 2,14, 0, 0, LTR | F(Condition)     | F(Logical)      , "||"       ),
  ROW(Add          , Add      , 2, 6, 0, 0, LTR | F(Arithmetic)    | F(NopIfZero)    , "+"        ),
  ROW(Sub          , Sub      , 2, 6, 0, 0, LTR | F(Arithmetic)    | F(NopIfZero)    , "-"        ),
  ROW(Mul          , Mul      , 2, 5, 0, 0, LTR | F(Arithmetic)    | F(NopIfOne)     , "*"        ),
  ROW(Div          , Div      , 2, 5, 0, 0, LTR | F(Arithmetic)    | F(NopIfROne)    , "/"        ),
  ROW(Mod          , Mod      , 2, 5, 0, 0, LTR | F(Arithmetic)                      , "%"        ),
  ROW(BitAnd       , BitAnd   , 2,10, 0, 0, LTR | F(BitMask)                         , "&"        ),
  ROW(BitOr        , BitOr    , 2,12, 0, 0, LTR | F(BitMask)       | F(NopIfZero)    , "|"        ),
  ROW(BitXor       , BitXor   , 2,11, 0, 0, LTR | F(BitMask)       | F(NopIfZero)    , "^"        ),
  ROW(BitSar       , BitSar   , 2, 7, 0, 0, LTR | F(BitShift)      | F(NopIfRZero)   , ">>"       ),
  ROW(BitShr       , BitShr   , 2, 7, 0, 0, LTR | F(BitShift)      | F(NopIfRZero)   , ">>>"      ),
  ROW(BitShl       , BitShl   , 2, 7, 0, 0, LTR | F(BitShift)      | F(NopIfRZero)   , "<<"       ),
  ROW(BitRor       , BitRor   , 2, 0, 0, 1, LTR | F(BitShift)      | F(NopIfRZero)   , "ror"      ),
  ROW(BitRol       , BitRol   , 2, 0, 0, 1, LTR | F(BitShift)      | F(NopIfRZero)   , "rol"      ),
  ROW(Min          , Min      , 2, 0, 0, 1, LTR | 0                                  , "min"      ),
  ROW(Max          , Max      , 2, 0, 0, 1, LTR | 0                                  , "max"      ),
  ROW(Pow          , Pow      , 2, 0, 0, 1, LTR | F(FloatOnly)     | F(NopIfROne)    , "pow"      ),
  ROW(Atan2        , Atan2    , 2, 0, 0, 1, LTR | F(Trigonometric) | F(FloatOnly)    , "atan2"    ),
  ROW(CopySign     , CopySign , 2, 0, 0, 1, LTR | 0                | F(FloatOnly)    , "copysign" )
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
    // Prevent another reallocation if old size wasn't enough.
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
    AstSymbol* collidedSymbol = NULL; \
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
  { MPSL_PROPAGATE(AstToIR(&ast, &ir).onProgram(ast.getProgramNode())); }

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
