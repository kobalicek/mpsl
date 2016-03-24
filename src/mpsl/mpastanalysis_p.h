// [MPSL]
// Shader-Like Mathematical Expression JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPASTANALYSIS_P_H
#define _MPSL_MPASTANALYSIS_P_H

// [Dependencies - MPSL]
#include "./mpast_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [mpsl::AstAnalysis]
// ============================================================================

struct AstAnalysis : public AstVisitor {
  MPSL_NO_COPY(AstAnalysis)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  AstAnalysis(AstBuilder* ast, ErrorReporter* errorReporter) noexcept;
  virtual ~AstAnalysis() noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstSymbol* getCurrentRet() const noexcept { return _currentRet; }
  MPSL_INLINE bool isUnreachable() const noexcept { return _unreachable; }

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
  // [CheckAssignment]
  // --------------------------------------------------------------------------

  Error checkAssignment(AstNode* node, uint32_t op) noexcept;

  // --------------------------------------------------------------------------
  // [Cast]
  // --------------------------------------------------------------------------

  //! Perform an implicit cast.
  uint32_t implicitCast(AstNode* node, AstNode* child, uint32_t type) noexcept;

  //! Perform an internal cast to `__bool32` or `__bool64`.
  uint32_t boolCast(AstNode* node, AstNode* child) noexcept;

  // TODO: Move to `AstBuilder::onInvalidCast()`.
  //! Report an invalid implicit or explicit cast.
  Error invalidCast(uint32_t position, const char* msg, uint32_t fromTypeInfo, uint32_t toTypeInfo) noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  ErrorReporter* _errorReporter;
  AstSymbol* _currentRet;

  bool _unreachable;
};

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPASTANALYSIS_P_H
