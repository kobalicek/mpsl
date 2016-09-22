// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#if defined(MPSL_API_SCOPE)
# undef MPSL_API_SCOPE
#else
# error "MPSL - api-scope not active, forgot to include mpsl_apibegin.h?"
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
# if _MSC_VER < 1900
#  if defined(MPSL_UNDEF_VSNPRINTF)
#   undef vsnprintf
#   undef MPSL_UNDEF_VSNPRINTF
#  endif // MPSL_UNDEF_VSNPRINTF
#  if defined(MPSL_UNDEF_SNPRINTF)
#   undef snprintf
#   undef MPSL_UNDEF_SNPRINTF
#  endif // MPSL_UNDEF_SNPRINTF
# endif
#endif // _MSC_VER

// ============================================================================
// [Clang]
// ============================================================================

#if defined(__clang__)
# pragma clang diagnostic pop
#endif // __clang__
