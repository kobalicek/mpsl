// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#if defined(MPSL_API_SCOPE)
# undef MPSL_API_SCOPE
#else
# error "MPSL - Api-Scope not active, forgot to include apibegin.h?"
#endif // MPSL_API_SCOPE

// ============================================================================
// [NoExcept]
// ============================================================================

#if defined(MPSL_UNDEF_NOEXCEPT)
# undef noexcept
# undef MPSL_UNDEF_NOEXCEPT
#endif // MPSL_UNDEF_NOEXCEPT

// ============================================================================
// [NullPtr]
// ============================================================================

#if defined(MPSL_UNDEF_NULLPTR)
# undef nullptr
# undef MPSL_UNDEF_NULLPTR
#endif // MPSL_UNDEF_NULLPTR

// ============================================================================
// [Override]
// ============================================================================

#if defined(MPSL_UNDEF_OVERRIDE)
# undef override
# undef MPSL_UNDEF_OVERRIDE
#endif // MPSL_UNDEF_OVERRIDE

// ============================================================================
// [MSC]
// ============================================================================

#if defined(_MSC_VER)
# pragma warning(pop)

# if defined(MPSL_UNDEF_VSNPRINTF)
#  undef vsnprintf
#  undef MPSL_UNDEF_VSNPRINTF
# endif // MPSL_UNDEF_VSNPRINTF

# if defined(MPSL_UNDEF_SNPRINTF)
#  undef snprintf
#  undef MPSL_UNDEF_SNPRINTF
# endif // MPSL_UNDEF_SNPRINTF

#endif // _MSC_VER

// ============================================================================
// [CLang]
// ============================================================================

#if defined(__clang__)
# pragma clang diagnostic pop
#endif // __clang__

// ============================================================================
// [GCC]
// ============================================================================

#if defined(__GNUC__) && !defined(__clang__)
# if __GNUC__ >= 4 && !defined(__MINGW32__)
#  pragma GCC visibility pop
# endif // GCC 4+
#endif // __GNUC__
