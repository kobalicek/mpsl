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
class AstOptimizer : public AstVisitor<AstOptimizer> {
public:
  MPSL_NONCOPYABLE(AstOptimizer)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  AstOptimizer(AstBuilder* ast, ErrorReporter* errorReporter) noexcept;
  ~AstOptimizer() noexcept;

  // --------------------------------------------------------------------------
  // [OnNode]
  // --------------------------------------------------------------------------

  Error onProgram(AstProgram* node) noexcept;
  Error onFunction(AstFunction* node) noexcept;
  Error onBlock(AstBlock* node) noexcept;
  Error onBranch(AstBranch* node) noexcept;
  Error onLoop(AstLoop* node) noexcept;
  Error onBreak(AstBreak* node) noexcept;
  Error onContinue(AstContinue* node) noexcept;
  Error onReturn(AstReturn* node) noexcept;
  Error onVarDecl(AstVarDecl* node) noexcept;
  Error onVarMemb(AstVarMemb* node) noexcept;
  Error onVar(AstVar* node) noexcept;
  Error onImm(AstImm* node) noexcept;
  Error onUnaryOp(AstUnaryOp* node) noexcept;
  Error onBinaryOp(AstBinaryOp* node) noexcept;
  Error onCall(AstCall* node) noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstSymbol* currentRet() const noexcept { return _currentRet; }
  MPSL_INLINE bool isUnreachable() const noexcept { return _unreachable; }
  MPSL_INLINE bool isConditional() const noexcept { return _isConditional; }
  MPSL_INLINE bool isLocalScope() const noexcept { return _isLocalScope; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  ErrorReporter* _errorReporter;
  AstSymbol* _currentRet;

  bool _unreachable;
  bool _isConditional;
  bool _isLocalScope;
};

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPASTOPTIMIZER_P_H
