// [MPSL]
// Shader-Like Mathematical Expression JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPASTTOIR_P_H
#define _MPSL_MPASTTOIR_P_H

// [Dependencies - MPSL]
#include "./mpast_p.h"
#include "./mpir_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [mpsl::AstOptimizer]
// ============================================================================

struct AstToIR : public AstVisitor {
  MPSL_NO_COPY(AstToIR)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_NOAPI AstToIR(AstBuilder* ast, IRBuilder* ir) noexcept;
  MPSL_NOAPI virtual ~AstToIR() noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE IRBuilder* getIR() const noexcept { return _ir; }
  MPSL_INLINE Allocator* getAllocator() const noexcept { return _ir->getAllocator(); }

  MPSL_INLINE IRBlock* getCurrentBlock() const noexcept { return _currentBlock; }
  MPSL_INLINE IRObject* getCurrentValue() const noexcept { return _currentValue; }

  MPSL_INLINE Error setCurrentValue(IRObject* obj) noexcept {
    _currentValue = obj;
    return kErrorOk;
  }
  MPSL_INLINE void resetCurrentValue() noexcept { _currentValue = nullptr; }

  // --------------------------------------------------------------------------
  // [OnNode]
  // --------------------------------------------------------------------------

  MPSL_NOAPI virtual Error onProgram(AstProgram* node) noexcept override;
  MPSL_NOAPI virtual Error onFunction(AstFunction* node) noexcept override;
  MPSL_NOAPI virtual Error onBlock(AstBlock* node) noexcept override;
  MPSL_NOAPI virtual Error onBranch(AstBranch* node) noexcept override;
  MPSL_NOAPI virtual Error onCondition(AstCondition* node) noexcept override;
  MPSL_NOAPI virtual Error onLoop(AstLoop* node) noexcept override;
  MPSL_NOAPI virtual Error onBreak(AstBreak* node) noexcept override;
  MPSL_NOAPI virtual Error onContinue(AstContinue* node) noexcept override;
  MPSL_NOAPI virtual Error onReturn(AstReturn* node) noexcept override;
  MPSL_NOAPI virtual Error onVarDecl(AstVarDecl* node) noexcept override;
  MPSL_NOAPI virtual Error onVarMemb(AstVarMemb* node) noexcept override;
  MPSL_NOAPI virtual Error onVar(AstVar* node) noexcept override;
  MPSL_NOAPI virtual Error onImm(AstImm* node) noexcept override;
  MPSL_NOAPI virtual Error onUnaryOp(AstUnaryOp* node) noexcept override;
  MPSL_NOAPI virtual Error onBinaryOp(AstBinaryOp* node) noexcept override;
  MPSL_NOAPI virtual Error onCall(AstCall* node) noexcept override;

  // --------------------------------------------------------------------------
  // [Utilities]
  // --------------------------------------------------------------------------

  MPSL_NOAPI Error mapVarToAst(AstSymbol* sym, IRVar* var) noexcept;
  MPSL_NOAPI Error requireVar(IRVar** varOut, IRObject* obj, uint32_t typeInfo) noexcept;

  // TODO: Rename after API is completed.
  MPSL_NOAPI Error emitFetchX(IRVar* dst, IRMem* src, uint32_t typeInfo) noexcept;
  MPSL_NOAPI Error emitStoreX(IRMem* dst, IRVar* src, uint32_t typeInfo) noexcept;

  MPSL_NOAPI Error fetchIfMem(IRVar** dst, IRObject* src, uint32_t typeInfo) noexcept;
  MPSL_NOAPI Error storeIfMem(IRObject* dst, IRVar* src, uint32_t typeInfo) noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! IR builder.
  IRBuilder* _ir;
  //! Current block.
  IRBlock* _currentBlock;

  //! Current value (result of the last processed node).
  IRObject* _currentValue;
  //! Current block level (0 if root).
  int _blockLevel;

  //! Private return variable internally named `@ret`.
  AstSymbol* _retPriv;

  //! Mapping of `AstVar` to `IRVar`.
  Map<AstSymbol*, IRVar*> _varMap;
  //! Mapping of `AstVarMemb` to `IRMem`.
  Map<AstSymbol*, IRMem*> _memMap;
};

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPASTTOIR_P_H
