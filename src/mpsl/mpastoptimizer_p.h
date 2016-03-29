// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPASTOPTIMIZER_P_H
#define _MPSL_MPASTOPTIMIZER_P_H

// [Dependencies - MPSL]
#include "./mpast_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [mpsl::AstOptimizer]
// ============================================================================

//! \internal
struct AstOptimizer : public AstVisitor<AstOptimizer> {
  MPSL_NO_COPY(AstOptimizer)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_NOAPI AstOptimizer(AstBuilder* ast, ErrorReporter* errorReporter) noexcept;
  MPSL_NOAPI ~AstOptimizer() noexcept;

  // --------------------------------------------------------------------------
  // [OnNode]
  // --------------------------------------------------------------------------

  MPSL_NOAPI Error onProgram(AstProgram* node) noexcept;
  MPSL_NOAPI Error onFunction(AstFunction* node) noexcept;
  MPSL_NOAPI Error onBlock(AstBlock* node) noexcept;
  MPSL_NOAPI Error onBranch(AstBranch* node) noexcept;
  MPSL_NOAPI Error onCondition(AstCondition* node) noexcept;
  MPSL_NOAPI Error onLoop(AstLoop* node) noexcept;
  MPSL_NOAPI Error onBreak(AstBreak* node) noexcept;
  MPSL_NOAPI Error onContinue(AstContinue* node) noexcept;
  MPSL_NOAPI Error onReturn(AstReturn* node) noexcept;
  MPSL_NOAPI Error onVarDecl(AstVarDecl* node) noexcept;
  MPSL_NOAPI Error onVarMemb(AstVarMemb* node) noexcept;
  MPSL_NOAPI Error onVar(AstVar* node) noexcept;
  MPSL_NOAPI Error onImm(AstImm* node) noexcept;
  MPSL_NOAPI Error onUnaryOp(AstUnaryOp* node) noexcept;
  MPSL_NOAPI Error onBinaryOp(AstBinaryOp* node) noexcept;
  MPSL_NOAPI Error onCall(AstCall* node) noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstSymbol* getCurrentRet() const noexcept { return _currentRet; }
  MPSL_INLINE bool isUnreachable() const noexcept { return _unreachable; }
  MPSL_INLINE uint8_t isConditional() const noexcept { return _isConditional; }
  MPSL_INLINE uint8_t isLocalScope() const noexcept { return _isLocalScope; }

  // --------------------------------------------------------------------------
  // [Utilities]
  // --------------------------------------------------------------------------

  MPSL_NOAPI Error foldCast(uint32_t position,
    Value* dVal, uint32_t dTypeInfo,
    const Value* sVal, uint32_t sTypeInfo) noexcept;

  MPSL_NOAPI Error foldUnaryOp(uint32_t position, Value* dVal,
    const Value* sVal, uint32_t typeInfo, uint32_t op) noexcept;

  MPSL_NOAPI Error foldBinaryOp(uint32_t position, Value* dst,
    const Value* lVal, uint32_t lTypeInfo,
    const Value* rVal, uint32_t rTypeInfo, uint32_t op) noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  ErrorReporter* _errorReporter;
  AstSymbol* _currentRet;

  bool _unreachable;
  uint8_t _isConditional;
  uint8_t _isLocalScope;
};

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPASTOPTIMIZER_P_H
