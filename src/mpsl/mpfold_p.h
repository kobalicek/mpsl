// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPFOLD_P_H
#define _MPSL_MPFOLD_P_H

// [Dependencies - MPSL]
#include "./mpsl_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [mpsl::Fold]
// ============================================================================

//! \internal
//!
//! Contains an implementation of all operations (both operations and IR
//! instructions) that are used in MPSL. Functions in this namespace are
//! used by both AST-based and IR-based constant folding.
namespace Fold {

// ============================================================================
// [mpsl::Fold - AST-Based Constant Folding]
// ============================================================================

MPSL_NOAPI Error foldCast(Value& dVal, uint32_t dTypeInfo, const Value& sVal, uint32_t sTypeInfo) noexcept;
MPSL_NOAPI Error foldSwizzle(uint32_t swiz, Value& dVal, const Value& sVal, uint32_t sTypeInfo) noexcept;
MPSL_NOAPI Error foldUnaryOp(uint32_t op, Value& dVal, const Value& sVal, uint32_t sTypeInfo) noexcept;

MPSL_NOAPI Error foldBinaryOp(uint32_t op, Value& dVal,
  const Value& lVal, uint32_t lTypeInfo,
  const Value& rVal, uint32_t rTypeInfo) noexcept;

// ============================================================================
// [mpsl::Fold - IR-Based Constant Folding]
// ============================================================================

MPSL_NOAPI Error foldInst(uint32_t inst, Value& dVal, const Value& sVal) noexcept;
MPSL_NOAPI Error foldInst(uint32_t inst, Value& dVal, const Value& lVal, const Value& rVal) noexcept;

} // Fold namespace
} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPFOLD_P_H
