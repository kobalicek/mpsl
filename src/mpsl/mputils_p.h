// [MPSL]
// Shader-Like Mathematical Expression JIT Engine for C++.
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

struct Utils {
  //! Append MPSL specific formatted string `fmt` into `sb`.
  static StringBuilder& sformat(StringBuilder& sb, const char* fmt, ...) noexcept;
  //! Append MPSL specific formatted string `fmt` into `sb` (`va_list` version).
  static StringBuilder& vformat(StringBuilder& sb, const char* fmt, va_list ap) noexcept;

  //! Append a formatted type name and related information of `type`.
  //!
  //! Provided also by `sformat`'s `"%{Type}"` extension.
  static StringBuilder& formatType(StringBuilder& sb, uint32_t type) noexcept;

  //! Append a formatted `value` (scalar or vector) of type `type`.
  //!
  //! Provided also by `sformat`'s `"%{Value}"` extension.
  static StringBuilder& formatValue(StringBuilder& sb, uint32_t type, const Value* value) noexcept;
};

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPUTILS_P_H
