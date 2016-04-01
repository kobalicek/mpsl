// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPUTILS_P_H
#define _MPSL_MPUTILS_P_H

// [Dependencies - MPSL]
#include "./mpsl_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [mpsl::Utils]
// ============================================================================

//! \internal
//!
//! Helpers.
struct Utils {
  //! Append MPSL specific formatted string `fmt` into `sb`.
  static MPSL_NOAPI StringBuilder& sformat(StringBuilder& sb, const char* fmt, ...) noexcept;
  //! Append MPSL specific formatted string `fmt` into `sb` (`va_list` version).
  static MPSL_NOAPI StringBuilder& vformat(StringBuilder& sb, const char* fmt, va_list ap) noexcept;

  //! Append a formatted type name and related information of `type`.
  //!
  //! Provided also by `sformat`'s `"%{Type}"` extension.
  static MPSL_NOAPI StringBuilder& formatType(StringBuilder& sb, uint32_t typeInfo) noexcept;

  //! Append a formatted `value` (scalar or vector) of type `type`.
  //!
  //! Provided also by `sformat`'s `"%{Value}"` extension.
  static MPSL_NOAPI StringBuilder& formatValue(StringBuilder& sb, uint32_t typeInfo, const Value* value) noexcept;

  //! Append a formatted swizzle letters.
  static MPSL_NOAPI void formatSwizzle(char* dst, uint32_t swizzleMask, uint32_t count) noexcept;
};

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPUTILS_P_H
