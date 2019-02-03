// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPFORMATUTILS_P_H
#define _MPSL_MPFORMATUTILS_P_H

// [Dependencies - MPSL]
#include "./mpsl_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [mpsl::FormatUtils]
// ============================================================================

//! \internal
//!
//! String format helpers.
namespace FormatUtils {

//! Append MPSL specific formatted string `fmt` into `sb`.
String& sformat(String& sb, const char* fmt, ...) noexcept;
//! Append MPSL specific formatted string `fmt` into `sb` (`va_list` version).
String& vformat(String& sb, const char* fmt, va_list ap) noexcept;

//! Append a formatted type name and related information of `type`.
//!
//! Provided also by `sformat`'s `"%{Type}"` extension.
String& formatType(String& sb, uint32_t typeInfo) noexcept;

//! Append a formatted `value` (scalar or vector) of type `type`.
//!
//! Provided also by `sformat`'s `"%{Value}"` extension.
String& formatValue(String& sb, uint32_t typeInfo, const Value* value) noexcept;

//! Append a formatted swizzle letters.
void formatSwizzleArray(char* dst, const uint8_t* swizzleArray, uint32_t count) noexcept;

} // FormatUtils namespace
} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPFORMATUTILS_P_H
