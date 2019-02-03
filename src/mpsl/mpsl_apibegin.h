// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#if !defined(_MPSL_MPSL_H)
# error "MPSL - Invalid use of 'apibegin.h' - Include 'mpsl.h' first."
#endif // !_MPSL_MPSL_H

#if !defined(MPSL_API_SCOPE)
# define MPSL_API_SCOPE
#else
# error "MPSL - api-scope is already active, previous scope not closed by mpsl_apiend.h?"
#endif // MPSL_API_SCOPE

// ============================================================================
// [Compiler Support]
// ============================================================================

// [MSC]
#if defined(_MSC_VER)

# pragma warning(push)
# pragma warning(disable: 4127) // conditional expression is constant
# pragma warning(disable: 4201) // nameless struct/union
# pragma warning(disable: 4244) // '+=' : conversion from 'int' to 'x', possible
                                // loss of data
# pragma warning(disable: 4251) // struct needs to have dll-interface to be used
                                // by clients of struct ...
# pragma warning(disable: 4275) // non dll-interface struct ... used as base for
                                // dll-interface struct
# pragma warning(disable: 4355) // this used in base member initializer list
# pragma warning(disable: 4480) // specifying underlying type for enum
# pragma warning(disable: 4800) // forcing value to bool 'true' or 'false'

# if _MSC_VER < 1900
#  if !defined(vsnprintf)
#   define MPSP_UNDEF_VSNPRINTF
#   define vsnprintf _vsnprintf
#  endif // !vsnprintf
#  if !defined(snprintf)
#   define MPSP_UNDEF_SNPRINTF
#   define snprintf _snprintf
#  endif // !snprintf
# endif

#endif // _MSC_VER

// [Clang]
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wc++11-extensions"
# pragma clang diagnostic ignored "-Wunnamed-type-template-args"
#endif // __clang__

// ============================================================================
// [Custom Macros]
// ============================================================================

// [NONCOPYABLE]
#define MPSL_NONCOPYABLE(...) \
private: \
  MPSL_INLINE __VA_ARGS__(const __VA_ARGS__& other) noexcept = delete; \
  MPSL_INLINE __VA_ARGS__& operator=(const __VA_ARGS__& other) noexcept = delete; \
public:
