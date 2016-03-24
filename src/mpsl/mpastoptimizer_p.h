// [MPSL]
// Shader-Like Mathematical Expression JIT Engine for C++.
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

struct AstOptimizer : public AstVisitor {
  MPSL_NO_COPY(AstOptimizer)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  AstOptimizer(AstBuilder* ast, ErrorReporter* errorReporter) noexcept;
  virtual ~AstOptimizer() noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstSymbol* getCurrentRet() const noexcept { return _currentRet; }
  MPSL_INLINE bool isUnreachable() const noexcept { return _unreachable; }
  MPSL_INLINE uint8_t isConditional() const noexcept { return _isConditional; }
  MPSL_INLINE uint8_t isLocalScope() const noexcept { return _isLocalScope; }

  // --------------------------------------------------------------------------
  // [OnNode]
  // --------------------------------------------------------------------------

  virtual Error onFunction(AstFunction* node) noexcept override;
  virtual Error onBlock(AstBlock* node) noexcept override;
  virtual Error onBranch(AstBranch* node) noexcept override;
  virtual Error onCondition(AstCondition* node) noexcept override;
  virtual Error onLoop(AstLoop* node) noexcept override;
  virtual Error onBreak(AstBreak* node) noexcept override;
  virtual Error onContinue(AstContinue* node) noexcept override;
  virtual Error onReturn(AstReturn* node) noexcept override;
  virtual Error onVarDecl(AstVarDecl* node) noexcept override;
  virtual Error onVarMemb(AstVarMemb* node) noexcept override;
  virtual Error onVar(AstVar* node) noexcept override;
  virtual Error onImm(AstImm* node) noexcept override;
  virtual Error onUnaryOp(AstUnaryOp* node) noexcept override;
  virtual Error onBinaryOp(AstBinaryOp* node) noexcept override;
  virtual Error onCall(AstCall* node) noexcept override;

  // --------------------------------------------------------------------------
  // [Utilities]
  // --------------------------------------------------------------------------

  Error foldCast(uint32_t position, Value* dVal, uint32_t dTypeInfo,
    const Value* sVal, uint32_t sTypeInfo) noexcept;

  Error foldUnaryOp(uint32_t position, Value* dVal,
    const Value* sVal, uint32_t typeInfo, uint32_t op) noexcept;

  Error foldBinaryOp(uint32_t position, Value* dst,
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
