// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPSL_H
#define _MPSL_MPSL_H

// [Dependencies - MPSL]
#include "./mpsl_build.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

// ============================================================================
// [mpsl]
// ============================================================================

namespace mpsl {

// ============================================================================
// [Forward Declarations]
// ============================================================================

struct Context;
struct Program;

struct Layout;
struct OutputLog;

// ============================================================================
// [mpsl::ErrorCode]
// ============================================================================

//! MPSL error.
enum ErrorCode {
  //! No error.
  kErrorOk = 0,

  // --------------------------------------------------------------------------
  // [Commons]
  // --------------------------------------------------------------------------

  //! \internal
  //!
  //! Common errors defined by the Commons framework, reused by MPSL.
  //! \{
  _kErrorCommonsStart = 0x00010000,
  _kErrorCommonsEnd = 0x0001FFFF,
  //! \}

  //! Out of memory.
  kErrorNoMemory = _kErrorCommonsStart,
  //! Invalid argument.
  kErrorInvalidArgument,
  //! Invalid state.
  kErrorInvalidState,

  // --------------------------------------------------------------------------
  // [MPSL Errors]
  // --------------------------------------------------------------------------

  //! Start of error range used by MPSL.
  _kErrorMPSLStart = 0x00020100,
  //! End of namespace used by MPSL.
  _kErrorMPSLEnd   = 0x000201FF,

  //! Invalid syntax.
  kErrorInvalidSyntax = _kErrorMPSLStart,
  //! Invalid program.
  kErrorInvalidProgram,
  //! Recursion not allowed.
  kErrorRecursionNotAllowed,

  //! Return type of "main()" function doesn't match the output object's return
  //! type.
  kErrorReturnMismatch,
  //! The program has no entry point defined - no "main()" function.
  kErrorNoEntryPoint,

  //! Just-in-time compilation failed (asmjit failed).
  kErrorJITFailed,

  //! Integer division by zero.
  //!
  //! This error can happen during a program compilation (constant folding) and
  //! also during execution.
  kErrorIntegerDivisionByZero,

  //! The program execution has been exceeded the maximum number of cycles.
  kErrorCyclesLimitExceeded,

  //! Returned when there is more than one built-in symbol (or identifier) of
  //! the same name. This can only happen in case that a `Layout` object tries
  //! to add a symbol that already exist (was registered by another `Layout`
  //! or is already a built-in). Also, rarely, can happen if one `Layout`
  //! object has a name and contains a member of the same name that is marked
  //! as `kTypeDenest`.
  kErrorSymbolCollision,

  //! Returned when object has been already configured.
  //!
  //! Possibly returned by `Layout::configure()` if called twice.
  kErrorAlreadyConfigured,

  //! Returned when object already exists.
  //!
  //! Possibly returned by `Layout::add()` if the member being added already
  //! exists.
  kErrorAlreadyExists,

  //! Returned by `Layout::add()` in case there is so many members
  //! already.
  //!
  //! The members limit is specified in `Layout::kMaxMembers`.
  kErrorTooManyMembers,

  //! Context is frozen and can't be modified anymore.
  kErrorFrozenContext
};

// ============================================================================
// [mpsl::Type]
// ============================================================================

enum TypeIdFlags {
  //! Type-id mask.
  kTypeIdMask = 0x000000FF,
  //! Type-attributes mask.
  kTypeAttrMask = 0x7FFFFF00,

  // --------------------------------------------------------------------------
  // [Type-ID]
  // --------------------------------------------------------------------------

  //! Void.
  kTypeVoid = 0,
  //! 32-bit boolean value or mask.
  kTypeBool = 1,
  //! 64-bit boolean value or mask (used mostly internally, but provided).
  kTypeQBool = 2,
  //! 32-bit signed integer.
  kTypeInt = 3,
  //! 32-bit single-precision floating point.
  kTypeFloat = 4,
  //! 64-bit double-precision floating point.
  kTypeDouble = 5,
  //! Pointer (internal).
  kTypePtr = 6,
  //! Number of built-in types.
  kTypeCount = 7,

  // --------------------------------------------------------------------------
  // [Type-Vector]
  // --------------------------------------------------------------------------

  //! How many bits to shift typeId to get the number of vector elements.
  kTypeVecShift = 8,
  //! Type-vector mask.
  kTypeVecMask = 0xF << kTypeVecShift,

  //! Scalar type.
  //!
  //! \note Embedders do not have to mark type as scalar when passing a type
  //! information to the public MPSL API. However, MPSL always marks all scalar
  //! types to simplify its internals.
  kTypeVec1 = 1 << kTypeVecShift,
  //! Vector of 2 elements (available to all 32-bit and 64-bit types).
  kTypeVec2 = 2 << kTypeVecShift,
  //! Vector of 3 elements (available to all 32-bit and 64-bit types).
  kTypeVec3 = 3 << kTypeVecShift,
  //! Vector of 4 elements (available to all 32-bit and 64-bit types).
  kTypeVec4 = 4 << kTypeVecShift,
  //! Vector of 8 elements (only 32-bit types can form 8-element vectors).
  kTypeVec8 = 8 << kTypeVecShift,

#define MPSL_DEFINE_TYPEID_VEC_4(typeId) \
  typeId##1 = typeId | kTypeVec1, \
  typeId##2 = typeId | kTypeVec2, \
  typeId##3 = typeId | kTypeVec3, \
  typeId##4 = typeId | kTypeVec4
#define MPSL_DEFINE_TYPEID_VEC_8(typeId) \
  typeId##8 = typeId | kTypeVec8


  MPSL_DEFINE_TYPEID_VEC_4(kTypeBool),
  MPSL_DEFINE_TYPEID_VEC_8(kTypeBool),
  MPSL_DEFINE_TYPEID_VEC_4(kTypeInt),
  MPSL_DEFINE_TYPEID_VEC_8(kTypeInt),
  MPSL_DEFINE_TYPEID_VEC_4(kTypeFloat),
  MPSL_DEFINE_TYPEID_VEC_8(kTypeFloat),

  MPSL_DEFINE_TYPEID_VEC_4(kTypeQBool),
  MPSL_DEFINE_TYPEID_VEC_4(kTypeDouble),

#undef MPSL_DEFINE_TYPEID_VEC_8
#undef MPSL_DEFINE_TYPEID_VEC_4

  // --------------------------------------------------------------------------
  // [Type-Flags]
  // --------------------------------------------------------------------------

  //! Variable is a reference (&).
  kTypeRef = 0x00020000,

  //! Variable is denested into a global scope (only used to define a `Layout`,
  //! not used internally). If the `Layout` is anonymous then `kTypeDenest` is
  //! implied for all members, however, if the `Layout` is not anonymous, every
  //! member that should be visible in a global scope has to be marked as
  //! `kTypeDenest`.
  kTypeDenest = 0x00040000,

  // --------------------------------------------------------------------------
  // [Type-RW]
  // --------------------------------------------------------------------------

  //! The variable can be read.
  kTypeRead = 0x00100000,
  //! The variable can be written.
  kTypeWrite = 0x00200000,

  //! Convenience - used in `Layout::add()` to define read-only variable/member.
  kTypeRO = kTypeRead,
  //! Convenience - used in `Layout::add()` to define write-only variable/member.
  kTypeWO = kTypeWrite,
  //! Convenience - used in `Layout::add()` to define read/write variable/member.
  kTypeRW = kTypeRead | kTypeWrite
};

// ============================================================================
// [mpsl::Options]
// ============================================================================

enum Options {
  //! No options.
  kNoOptions = 0x0000,

  //! Show messages and warnings.
  kOptionVerbose = 0x0001,
  //! Debug AST and all AST-based transformations.
  kOptionDebugAst = 0x0002,
  //! Debug IR and all IR-based transformations.
  kOptionDebugIR  = 0x0004,
  //! Debug assembly generated.
  kOptionDebugASM = 0x0008,

  //! Do not use SSE3 (and higher) even if the CPU supports it (X86/X64 only).
  kOptionDisableSSE3 = 0x0100,
  //! Do not use SSSE3 (and higher) even if the CPU supports it (X86/X64 only).
  kOptionDisableSSSE3 = 0x0200,
  //! Do not use SSE4.1 (and higher) even if the CPU supports it (X86/X64 only).
  kOptionDisableSSE4_1 = 0x0400,
  //! Do not use SSE4.2 (and higher) even if the CPU supports it (X86/X64 only).
  kOptionDisableSSE4_2 = 0x0800,
  //! Do not use AVX (and higher) even if the CPU supports it (X86/X64 only).
  kOptionDisableAVX = 0x1000,
  //! Do not use AVX2 (and higher) even if the CPU supports it (X86/X64 only).
  kOptionDisableAVX2 = 0x2000,

  //! \internal
  //!
  //! Mask of all accessible options, MPSL uses also \ref InternalOptions that
  //! should not collide with \ref Options.
  _kOptionsMask = 0xFFFF
};

// ============================================================================
// [mpsl::Globals]
// ============================================================================

namespace Globals {

//! Invalid index and/or size.
static const size_t kInvalidIndex = ~static_cast<size_t>(0);

//! MPSL limits.
enum Limits {
  //! Maximum data arguments of a shader program.
  kMaxArgumentsCount = 4,
  //! Maximum size of an identifier.
  kMaxIdentifierLength = 64,
  //! Maximum number of members of one data `Layout`.
  kMaxMembersCount = 512
};

} // Globals namespace

// ============================================================================
// [mpsl::Types]
// ============================================================================

//! MPSL error value, alias to `uint32_t`.
typedef uint32_t Error;

#define MPSL_DEFINE_TYPE_VEC_4(name, type)                                    \
  union name##2 {                                                             \
    MPSL_INLINE void set(type value) noexcept {                               \
      for (size_t i = 0; i < MPSL_ARRAY_SIZE(data); i++)                      \
        data[i] = value;                                                      \
    }                                                                         \
                                                                              \
    MPSL_INLINE void set(type v0, type v1) noexcept {                         \
      data[0] = v0;                                                           \
      data[1] = v1;                                                           \
    }                                                                         \
                                                                              \
    MPSL_INLINE type& operator[](size_t i) noexcept {                         \
      return data[i];                                                         \
    }                                                                         \
    MPSL_INLINE const type& operator[](size_t i) const noexcept {             \
      return data[i];                                                         \
    }                                                                         \
                                                                              \
    struct { type x, y; };                                                    \
    struct { type data[2]; };                                                 \
  };                                                                          \
                                                                              \
  union name##3 {                                                             \
    MPSL_INLINE void set(type value) noexcept {                               \
      for (size_t i = 0; i < MPSL_ARRAY_SIZE(data); i++)                      \
        data[i] = value;                                                      \
    }                                                                         \
                                                                              \
    MPSL_INLINE void set(type v0, type v1, type v2) noexcept {                \
      data[0] = v0;                                                           \
      data[1] = v1;                                                           \
      data[2] = v2;                                                           \
    }                                                                         \
                                                                              \
    MPSL_INLINE type& operator[](size_t i) noexcept {                         \
      return data[i];                                                         \
    }                                                                         \
    MPSL_INLINE const type& operator[](size_t i) const noexcept {             \
      return data[i];                                                         \
    }                                                                         \
                                                                              \
    struct { type x, y, z; };                                                 \
    struct { type r, g, b; };                                                 \
    struct { type data[3]; };                                                 \
  };                                                                          \
                                                                              \
  union name##4 {                                                             \
    MPSL_INLINE void set(type value) noexcept {                               \
      for (size_t i = 0; i < MPSL_ARRAY_SIZE(data); i++)                      \
        data[i] = value;                                                      \
    }                                                                         \
                                                                              \
    MPSL_INLINE void set(type v0, type v1, type v2, type v3) noexcept {       \
      data[0] = v0;                                                           \
      data[1] = v1;                                                           \
      data[2] = v2;                                                           \
      data[3] = v3;                                                           \
    }                                                                         \
                                                                              \
    MPSL_INLINE type& operator[](size_t i) noexcept {                         \
      return data[i];                                                         \
    }                                                                         \
    MPSL_INLINE const type& operator[](size_t i) const noexcept {             \
      return data[i];                                                         \
    }                                                                         \
                                                                              \
    struct { type x, y, z, w; };                                              \
    struct { type r, g, b, a; };                                              \
    struct { type data[4]; };                                                 \
  };

#define MPSL_DEFINE_TYPE_VEC_8(name, type)                                    \
  union name##8 {                                                             \
    MPSL_INLINE void set(type value) noexcept {                               \
      for (size_t i = 0; i < MPSL_ARRAY_SIZE(data); i++)                      \
        data[i] = value;                                                      \
    }                                                                         \
                                                                              \
    MPSL_INLINE void set(type v0, type v1, type v2, type v3,                  \
                         type v4, type v5, type v6, type v7) noexcept {       \
      data[0] = v0;                                                           \
      data[1] = v1;                                                           \
      data[2] = v2;                                                           \
      data[3] = v3;                                                           \
      data[4] = v4;                                                           \
      data[5] = v5;                                                           \
      data[6] = v6;                                                           \
      data[7] = v7;                                                           \
    }                                                                         \
                                                                              \
    MPSL_INLINE type& operator[](size_t i) noexcept {                         \
      return data[i];                                                         \
    }                                                                         \
    MPSL_INLINE const type& operator[](size_t i) const noexcept {             \
      return data[i];                                                         \
    }                                                                         \
                                                                              \
    struct { type x, y, z, w, i, j, k, l; };                                  \
    struct { type r, g, b, a; };                                              \
    struct { type data[8]; };                                                 \
  };

MPSL_DEFINE_TYPE_VEC_4(Bool, uint32_t)
MPSL_DEFINE_TYPE_VEC_8(Bool, uint32_t)
MPSL_DEFINE_TYPE_VEC_4(Int, int32_t)
MPSL_DEFINE_TYPE_VEC_8(Int, int32_t)
MPSL_DEFINE_TYPE_VEC_4(Float, float)
MPSL_DEFINE_TYPE_VEC_8(Float, float)

MPSL_DEFINE_TYPE_VEC_4(QBool, uint64_t)
MPSL_DEFINE_TYPE_VEC_4(Double, double)

#undef MPSL_DEFINE_TYPE_VEC_8
#undef MPSL_DEFINE_TYPE_VEC_4

//! Packed value of 256-bits that can hold a MPSL variable of any type.
union Value {
  // --------------------------------------------------------------------------
  // [Interface]
  // --------------------------------------------------------------------------

  MPSL_INLINE void zero() noexcept {
    q[0] = 0;
    q[1] = 0;
    q[2] = 0;
    q[3] = 0;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  Bool8 b;
  Int8 i;
  Float8 f;

  QBool4 q;
  Double4 d;

  uint32_t u[8];

  //! Binary buffer representing the data.
  char blob[32];
};

// ============================================================================
// [mpsl::StringRef]
// ============================================================================

//! String reference (pointer to string data data and size).
//!
//! \note MPSL always provides NULL terminated string with size known. On the
//! other hand MPSL doesn't require NULL terminated strings when passed to MPSL
//! APIs.
struct StringRef {
  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE StringRef() noexcept
    : _data(nullptr),
      _size(0) {}

  explicit MPSL_INLINE StringRef(const char* data) noexcept
    : _data(data),
      _size(::strlen(data)) {}

  MPSL_INLINE StringRef(const char* data, size_t size) noexcept
    : _data(data),
      _size(size) {}

  // --------------------------------------------------------------------------
  // [Reset / Setup]
  // --------------------------------------------------------------------------

  MPSL_INLINE void reset() noexcept {
    set(nullptr, 0);
  }

  MPSL_INLINE void set(const char* data) noexcept {
    set(data, ::strlen(data));
  }

  MPSL_INLINE void set(const char* data, size_t size) noexcept {
    _data = data;
    _size = size;
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get the string data.
  MPSL_INLINE const char* data() const noexcept { return _data; }
  //! Get the string size.
  MPSL_INLINE size_t size() const noexcept { return _size; }

  // --------------------------------------------------------------------------
  // [Eq]
  // --------------------------------------------------------------------------

  MPSL_INLINE bool eq(const char* s) const noexcept {
    const char* a = _data;
    const char* aEnd = a + _size;

    while (a != aEnd) {
      if (*a++ != *s++)
        return false;
    }

    return *s == '\0';
  }

  MPSL_INLINE bool eq(const char* data, size_t size) const noexcept {
    return size == _size && ::memcmp(_data, data, size) == 0;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  const char* _data;
  size_t _size;
};

// ============================================================================
// [mpsl::Layout]
// ============================================================================

//! Data layout used to describe program arguments passed by MPSL embedders.
//!
//! Allows to speficy a layout of data (member name, type, and offset) that is
//! passed as a parameter to a compiled MPSL program. Layout is used to define
//! what the program can use and after the program is compiled it's no longer
//! needed.
struct Layout {
  MPSL_NONCOPYABLE(Layout)

  //! \internal
  struct Member {
    const char* name;                    //!< Member name, it's located somewhere at `Layout::_data`.
    uint32_t nameSize;                   //!< Member name size.
    uint32_t typeInfo;                   //!< Member type information.
    int32_t offset;                      //!< Member offset in the passed data (negative offset is allowed).
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_API Layout() noexcept;
  MPSL_API ~Layout() noexcept;

protected:
  //! \internal
  //!
  //! Used only by `LayoutTmp`.
  MPSL_API Layout(uint8_t* data, uint32_t dataSize) noexcept;

  // --------------------------------------------------------------------------
  // [Interface]
  // --------------------------------------------------------------------------

public:
  MPSL_API Error _configure(const char* name, size_t nameSize) noexcept;

  //! Set the name of this `Layout`.
  //!
  //! Setting a layout name means that the layout object will be accessible as
  //! through `name` in the shader program. All members that are not explicitly
  //! marked as `kTypeDenest` will be only accessible through `name.member`.
  MPSL_INLINE Error configure(const char* name) noexcept {
    return _configure(name, Globals::kInvalidIndex);
  }
  //! \overload
  MPSL_INLINE Error configure(const StringRef& name) noexcept {
    return _configure(name.data(), name.size());
  }

  //! \internal
  MPSL_API const Member* _get(const char* name, size_t nameSize) const noexcept;
  //! \internal
  MPSL_API Error _add(const char* name, size_t nameSize, uint32_t typeInfo, int32_t offset) noexcept;

  //! Get whether the layout has a name. Name is an identifier that is used
  //! in a shader program to access the data and its members. Name has to be
  //! assigned by `Layout::configure()` and it's initially NULL.
  MPSL_INLINE bool hasName() const noexcept { return _name != nullptr; }

  //! Get the layout name.
  MPSL_INLINE const char* name() const noexcept { return _name; }
  MPSL_INLINE uint32_t nameSize() const noexcept { return _nameSize; }

  MPSL_INLINE const Member* membersArray() const noexcept { return _members; }
  MPSL_INLINE uint32_t membersCount() const noexcept { return _membersCount; }

  //! Get whether the layout is empty (has no members).
  MPSL_INLINE bool empty() const noexcept { return _membersCount == 0; }

  //! Get whether the member of name `name` exists.
  MPSL_INLINE bool hasMember(const char* name) const noexcept {
    return _get(name, Globals::kInvalidIndex) != nullptr;
  }

  //! \overload
  MPSL_INLINE bool hasMember(const StringRef& name) const noexcept {
    return _get(name.data(), name.size()) != nullptr;
  }

  //! Get the member of name `name`, returns null if such member doesn't exist.
  MPSL_INLINE const Member* member(const char* name) const noexcept {
    return _get(name, Globals::kInvalidIndex);
  }

  //! \overload
  MPSL_INLINE const Member* member(const StringRef& name) const noexcept {
    return _get(name.data(), name.size());
  }

  //! Add a new member to the arguments object.
  MPSL_INLINE Error addMember(const char* name, uint32_t typeInfo, int32_t offset) noexcept {
    return _add(name, Globals::kInvalidIndex, typeInfo, offset);
  }

  //! \overload
  MPSL_INLINE Error addMember(const StringRef& name, uint32_t typeInfo, int32_t offset) noexcept {
    return _add(name.data(), name.size(), typeInfo, offset);
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  union {
    //! Data used to build the whole structure.
    uint8_t* _data;
    //! Members list.
    Member* _members;
  };

  //! Object name.
  const char* _name;
  //! Object name size;
  uint32_t _nameSize;

  //! Count of members.
  uint32_t _membersCount;

  //! Size of `_data` (in bytes).
  uint32_t _dataSize;
  //! The current data index, initially set to `_dataSize`, decreasing.
  uint32_t _dataIndex;

  //! Used only by `LayoutTmp`,
  uint8_t _embeddedData[8];
};

//! Function arguments builder using a stack for up to `N` bytes.
template<unsigned int N = 256>
struct LayoutTmp : public Layout {
  MPSL_INLINE LayoutTmp() noexcept
    : Layout(_embeddedData, N) {};
  uint8_t _embeddedDataTmp[N - 8];
};

// ============================================================================
// [mpsl::Context]
// ============================================================================

//! Context.
//!
//! Contains a basic environment that is setup by the embedder. Contexts can be
//! extended by embedder by custom constants, functions, and asmjit extensions.
struct Context {
  // --------------------------------------------------------------------------
  // [Impl]
  // --------------------------------------------------------------------------

  //! \internal
  struct Impl {
    //! Implemented in `mpsl.cpp`.
    MPSL_INLINE void destroy() noexcept;

    //! Reference count.
    uintptr_t _refCount;
    //! Runtime data.
    void* _runtimeData;
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! Create a weak-copy of null context (can't be used to create programs).
  MPSL_API Context() noexcept;
  //! Create a weak-copy of `other` context.
  MPSL_API Context(const Context& other) noexcept;
  //! Destroy the MPSL context.
  MPSL_API ~Context() noexcept;

  //! Create a new context.
  static MPSL_API Context create() noexcept;

#if defined(MPSL_EXPORTS)
  explicit MPSL_INLINE Context(Impl* d) noexcept : _d(d) {}
#endif // MPSL_EXPORTS

  // --------------------------------------------------------------------------
  // [Reset]
  // --------------------------------------------------------------------------

  //! Reset the context.
  MPSL_API Error reset() noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE bool isValid() const noexcept {
    return _d->_runtimeData != nullptr;
  }

  // --------------------------------------------------------------------------
  // [Clone / Freeze]
  // --------------------------------------------------------------------------

  //! Clone the context.
  //!
  //! Clone will create a deep copy of the context and unfreeze it; the context
  //! can be modified after it has been cloned regardless of being frozen or
  //! not before.
  MPSL_API Error clone() noexcept;

  //! Freeze the context.
  //!
  //! No modifications are allowed after the context is frozen. This is useful
  //! when building a fixed environment for your programs that can't be modified
  //! after it has been created (it becomes immutable).
  MPSL_API Error freeze() noexcept;

  // --------------------------------------------------------------------------
  // [Compile]
  // --------------------------------------------------------------------------

  //! \internal
  struct CompileArgs {
    MPSL_INLINE CompileArgs(const char* s, size_t size, uint32_t options, uint32_t numArgs) noexcept :
      body(s, size),
      options(options),
      numArgs(numArgs) {}

    //! Body of the function to compile.
    StringRef body;

    //! Program options.
    uint32_t options;
    //! Number of arguments passed to the program (maximum 4).
    uint32_t numArgs;
    //! Layout of arguments passed to the program.
    const Layout* layout[Globals::kMaxArgumentsCount];
  };

  //! \internal
  MPSL_API Error _compile(Program& program, const CompileArgs& ca, OutputLog* log) noexcept;

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

  //! Assign a weak-copy of `other` context.
  MPSL_API Context& operator=(const Context& other) noexcept;

  //! Equality, only true if `other` is the same weak-copy of the context.
  MPSL_INLINE bool operator==(const Context& other) const noexcept { return _d == other._d; }
  //! Inequality.
  MPSL_INLINE bool operator!=(const Context& other) const noexcept { return _d != other._d; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Context data (private).
  Impl* _d;
};

// ============================================================================
// [mpsl::Program]
// ============================================================================

//! A base class for a shader program.
struct Program {
  // --------------------------------------------------------------------------
  // [Impl]
  // --------------------------------------------------------------------------

  //! \internal
  struct Impl {
    //! Prototype of `main()` that accepts a single argument.
    typedef Error (MPSL_CDECL *MainFunc1)(void* arg1);
    //! Prototype of `main()` that accepts two arguments.
    typedef Error (MPSL_CDECL *MainFunc2)(void* arg1, void* arg2);
    //! Prototype of `main()` that accepts three arguments.
    typedef Error (MPSL_CDECL *MainFunc3)(void* arg1, void* arg2, void* arg3);
    //! Prototype of `main()` that accepts four arguments.
    typedef Error (MPSL_CDECL *MainFunc4)(void* arg1, void* arg2, void* arg3, void* arg4);

    // Implemented in `mpsl.cpp`.
    MPSL_INLINE void destroy() noexcept;

    //! Reference count.
    uintptr_t _refCount;
    //! Runtime data (set by \ref Context).
    void* _runtimeData;

    //! Compiled `main()` entry-point.
    union {
      void* _main;
      MainFunc1 _main1;
      MainFunc2 _main2;
      MainFunc3 _main3;
      MainFunc4 _main4;
    };

    //! Number of arguments that is passed to the entry-point.
    uint32_t _argsCount;
    //! Size of the compiled function (in bytes).
    uint32_t _programSize;
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  //! Create a new compiled program.
  MPSL_API Program() noexcept;
  //! Create a weak-copy of `other` program.
  MPSL_API Program(const Program& other) noexcept;
  //! Destroy the compiled program.
  MPSL_API ~Program() noexcept;

  // --------------------------------------------------------------------------
  // [Reset]
  // --------------------------------------------------------------------------

  MPSL_API Error reset() noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get whether the program has been compiled and is valid.
  MPSL_INLINE bool isValid() const noexcept { return _d->_main != nullptr; }

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

protected:
  MPSL_API Program& operator=(const Program& other) noexcept;

public:
  //! Equality, only true if `other` is the same weak-copy of the program.
  MPSL_INLINE bool operator==(const Program& other) const noexcept { return _d == other._d; }
  //! Inequality.
  MPSL_INLINE bool operator!=(const Program& other) const noexcept { return _d != other._d; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Program data (private).
  Impl* _d;
};

// ============================================================================
// [mpsl::Program1<T1>]
// ============================================================================

template<typename T0 = void>
struct Program1 : public Program {
  enum { kNumArgs = 1 };

  MPSL_INLINE Program1() noexcept : Program() {}
  MPSL_INLINE Program1(const Program1& other) noexcept : Program(other) {}
  MPSL_INLINE ~Program1() noexcept {}

  MPSL_INLINE Error compile(Context& context, const char* body, uint32_t options,
    const Layout& layout0,
    OutputLog* log = nullptr) noexcept {

    Context::CompileArgs args(body, Globals::kInvalidIndex, options, kNumArgs);
    args.layout[0] = &layout0;
    return context._compile(*this, args, log);
  }

  MPSL_INLINE Error compile(Context& context, const StringRef& body, uint32_t options,
    const Layout& layout0,
    OutputLog* log = nullptr) noexcept {

    Context::CompileArgs args(body.data(), body.size(), options, kNumArgs);
    args.layout[0] = &layout0;
    return context._compile(*this, args, log);
  }

  MPSL_INLINE Error run(T0* a0) const noexcept {
    return _d->_main1((void*)a0);
  }

  MPSL_INLINE Program1& operator=(const Program1& other) noexcept {
    Program::operator=(other);
    return *this;
  }
};

// ============================================================================
// [mpsl::Program2<T1, T2>]
// ============================================================================

template<typename T0 = void, typename T1 = void>
struct Program2 : public Program {
  enum { kNumArgs = 2 };

  MPSL_INLINE Program2() noexcept : Program() {}
  MPSL_INLINE Program2(const Program2& other) noexcept : Program(other) {}
  MPSL_INLINE ~Program2() noexcept {}

  MPSL_INLINE Error compile(Context& context, const char* body, uint32_t options,
    const Layout& layout0,
    const Layout& layout1,
    OutputLog* log = nullptr) noexcept {

    Context::CompileArgs args(body, Globals::kInvalidIndex, options, kNumArgs);
    args.layout[0] = &layout0;
    args.layout[1] = &layout1;
    return context._compile(*this, args, log);
  }

  MPSL_INLINE Error compile(Context& context, const StringRef& body, uint32_t options,
    const Layout& layout0,
    const Layout& layout1,
    OutputLog* log = nullptr) noexcept {

    Context::CompileArgs args(body.data(), body.size(), options, kNumArgs);
    args.layout[0] = &layout0;
    args.layout[1] = &layout1;
    return context._compile(*this, args, log);
  }

  MPSL_INLINE Error run(T0* a0, T1* a1) const noexcept {
    return _d->_main2((void*)a0, (void*)a1);
  }

  MPSL_INLINE Program2& operator=(const Program2& other) noexcept {
    Program::operator=(other);
    return *this;
  }
};

// ============================================================================
// [mpsl::Program3<T1, T2, T3>]
// ============================================================================

template<typename T1 = void, typename T2 = void, typename T3 = void>
struct Program3 : public Program {
  enum { kNumArgs = 3 };

  MPSL_INLINE Program3() noexcept : Program() {}
  MPSL_INLINE Program3(const Program3& other) noexcept : Program(other) {}
  MPSL_INLINE ~Program3() noexcept {}

  MPSL_INLINE Error compile(Context& context, const char* body, uint32_t options,
    const Layout& layout0,
    const Layout& layout1,
    const Layout& layout2,
    OutputLog* log = nullptr) noexcept {

    Context::CompileArgs args(body, Globals::kInvalidIndex, options, kNumArgs);
    args.layout[0] = &layout0;
    args.layout[1] = &layout1;
    args.layout[2] = &layout2;
    return context._compile(*this, args, log);
  }

  MPSL_INLINE Error compile(Context& context, const StringRef& body, uint32_t options,
    const Layout& layout0,
    const Layout& layout1,
    const Layout& layout2,
    OutputLog* log = nullptr) noexcept {

    Context::CompileArgs args(body.data(), body.size(), options, kNumArgs);
    args.layout[0] = &layout0;
    args.layout[1] = &layout1;
    args.layout[2] = &layout2;
    return context._compile(*this, args, log);
  }

  MPSL_INLINE Error run(T1* a1, T2* a2, T3* a3) const noexcept {
    return _d->_main3((void*)a1, (void*)a2, (void*)a3);
  }

  MPSL_INLINE Program3& operator=(const Program3& other) noexcept {
    Program::operator=(other);
    return *this;
  }
};

// ============================================================================
// [mpsl::Program4<T1, T2, T3, T4>]
// ============================================================================

template<typename T1 = void, typename T2 = void, typename T3 = void, typename T4 = void>
struct Program4 : public Program {
  enum { kNumArgs = 4 };

  MPSL_INLINE Program4() noexcept : Program() {}
  MPSL_INLINE Program4(const Program4& other) noexcept : Program(other) {}
  MPSL_INLINE ~Program4() noexcept {}

  MPSL_INLINE Error compile(Context& context, const char* body, uint32_t options,
    const Layout& layout0,
    const Layout& layout1,
    const Layout& layout2,
    const Layout& layout3,
    OutputLog* log = nullptr) noexcept {

    Context::CompileArgs args(body, Globals::kInvalidIndex, options, kNumArgs);
    args.layout[0] = &layout0;
    args.layout[1] = &layout1;
    args.layout[2] = &layout2;
    args.layout[3] = &layout3;
    return context._compile(*this, args, log);
  }

  MPSL_INLINE Error compile(Context& context, const StringRef& body, uint32_t options,
    const Layout& layout0,
    const Layout& layout1,
    const Layout& layout2,
    const Layout& layout3,
    OutputLog* log = nullptr) noexcept {

    Context::CompileArgs args(body.data(), body.size(), options, kNumArgs);
    args.layout[0] = &layout0;
    args.layout[1] = &layout1;
    args.layout[2] = &layout2;
    args.layout[3] = &layout3;
    return context._compile(*this, args, log);
  }

  MPSL_INLINE Error run(T1* a1, T2* a2, T3* a3, T4* a4) const noexcept {
    return _d->_main4((void*)a1, (void*)a2, (void*)a3, (void*)a4);
  }

  MPSL_INLINE Program4& operator=(const Program4& other) noexcept {
    Program::operator=(other);
    return *this;
  }
};

// ============================================================================
// [mpsl::OutputLog]
// ============================================================================

//! Interface that can be used to catch compiler warnings and errors.
struct MPSL_VIRTAPI OutputLog {
  //! Specifies the type of the message passed to \ref OutputLog::log().
  enum MessageType {
    kMessageError   = 0,                 //!< Error message.
    kMessageWarning = 1,                 //!< Warning message.
    kMessageDebug   = 2,                 //!< Debug message.
    kMessageDump    = 3                  //!< MPSL dump (AST, IR, or ASM).
  };

  //! Output message data.
  struct Message {
    MPSL_INLINE Message(
      uint32_t type,
      uint32_t line,
      uint32_t column,
      const StringRef& header,
      const StringRef& content) noexcept
      : _type(type),
        _line(line),
        _column(column),
        _reserved(0),
        _header(header),
        _content(content) {}

    MPSL_INLINE bool isError() const noexcept { return _type == kMessageError; }
    MPSL_INLINE bool isWarning() const noexcept { return _type == kMessageWarning; }
    MPSL_INLINE bool isDebug() const noexcept { return _type == kMessageDebug; }
    MPSL_INLINE bool isDump() const noexcept { return _type == kMessageDump; }

    //! Get if the message contains a source code position (line and column).
    MPSL_INLINE bool hasPosition() const noexcept { return _line != 0; }

    //! Get message type, see \ref MessageType.
    MPSL_INLINE uint32_t type() const noexcept { return _type; }
    //! Get line number, indexed from 1.
    MPSL_INLINE uint32_t line() const noexcept { return _line; }
    //! Get column, indexed from 1.
    MPSL_INLINE uint32_t column() const noexcept { return _column; }

    MPSL_INLINE const StringRef& header() const noexcept { return _header; }
    MPSL_INLINE const StringRef& content() const noexcept { return _content; }

    uint32_t _type;
    uint32_t _line;
    uint32_t _column;
    uint32_t _reserved;

    StringRef _header;
    StringRef _content;
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_API OutputLog() noexcept;
  MPSL_API virtual ~OutputLog() noexcept;

  // --------------------------------------------------------------------------
  // [Interface]
  // --------------------------------------------------------------------------

  virtual void log(const Message& msg) noexcept = 0;
};

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPSL_H
