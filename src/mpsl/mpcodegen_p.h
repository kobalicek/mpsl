// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPCODEGEN_P_H
#define _MPSL_MPCODEGEN_P_H

// [Dependencies - MPSL]
#include "./mpast_p.h"
#include "./mpir_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [mpsl::CodeGen]
// ============================================================================

class CodeGenResult {
public:
  MPSL_INLINE CodeGenResult(bool dependsOnResult = true) noexcept {
    this->result.reset();
    this->dependsOnResult = dependsOnResult;
  }

  MPSL_INLINE bool hasValue() const noexcept {
    return result.lo != nullptr;
  }

  IRPair<IRObject> result;
  bool dependsOnResult;
};

//! \internal
class CodeGen : public AstVisitorWithArgs<CodeGen, CodeGenResult&> {
public:
  MPSL_NONCOPYABLE(CodeGen)

  typedef CodeGenResult Result;

  typedef Set< AstFunction*              > FunctionSet;
  typedef Map< AstSymbol*, IRPair<IRReg> > VarMap;
  typedef Map< AstSymbol*, IRMem*        > MemMap;

  struct DataSlot {
    MPSL_INLINE DataSlot(uint32_t slot, int32_t offset) noexcept
      : slot(slot),
        offset(offset) {}

    uint32_t slot;
    int32_t offset;
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  CodeGen(AstBuilder* ast, IRBuilder* ir) noexcept;
  ~CodeGen() noexcept;

  // --------------------------------------------------------------------------
  // [OnNode]
  // --------------------------------------------------------------------------

  Error onProgram(AstProgram* node, Result& out) noexcept;
  Error onFunction(AstFunction* node, Result& out) noexcept;
  Error onBlock(AstBlock* node, Result& out) noexcept;
  Error onBranch(AstBranch* node, Result& out) noexcept;
  Error onLoop(AstLoop* node, Result& out) noexcept;
  Error onBreak(AstBreak* node, Result& out) noexcept;
  Error onContinue(AstContinue* node, Result& out) noexcept;
  Error onReturn(AstReturn* node, Result& out) noexcept;
  Error onVarDecl(AstVarDecl* node, Result& out) noexcept;
  Error onVarMemb(AstVarMemb* node, Result& out) noexcept;
  Error onVar(AstVar* node, Result& out) noexcept;
  Error onImm(AstImm* node, Result& out) noexcept;
  Error onUnaryOp(AstUnaryOp* node, Result& out) noexcept;
  Error onBinaryOp(AstBinaryOp* node, Result& out) noexcept;
  Error onCall(AstCall* node, Result& out) noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE IRBuilder* ir() const noexcept { return _ir; }
  MPSL_INLINE IRBlock* block() const noexcept { return _block; }
  MPSL_INLINE ZoneAllocator* allocator() const noexcept { return _ir->allocator(); }

  MPSL_INLINE bool hasV256() const noexcept { return _hasV256; }
  MPSL_INLINE bool needSplit(uint32_t width) const { return width > 16 && !_hasV256; }

  // --------------------------------------------------------------------------
  // [Utilities]
  // --------------------------------------------------------------------------

  Error mapVarToAst(AstSymbol* sym, IRPair<IRReg> var) noexcept;

  Error newVar(IRPair<IRObject>& dst, uint32_t typeInfo) noexcept;
  Error newImm(IRPair<IRObject>& dst, const Value& value, uint32_t typeInfo) noexcept;
  Error addrOfData(IRPair<IRObject>& dst, DataSlot data, uint32_t width) noexcept;

  Error asVar(IRPair<IRObject>& out, IRPair<IRObject> in, uint32_t typeInfo) noexcept;

  Error emitMove(IRPair<IRReg> dst, IRPair<IRReg> src, uint32_t typeInfo) noexcept;
  Error emitStore(IRPair<IRObject> dst, IRPair<IRReg> src, uint32_t typeInfo) noexcept;
  Error emitInst2(uint32_t instCode,
    IRPair<IRObject> o0,
    IRPair<IRObject> o1, uint32_t typeInfo) noexcept;

  Error emitInst3(uint32_t instCode,
    IRPair<IRObject> o0,
    IRPair<IRObject> o1,
    IRPair<IRObject> o2, uint32_t typeInfo) noexcept;

  // TODO: Rename after API is completed.
  Error emitFetchX(IRReg* dst, IRMem* src, uint32_t typeInfo) noexcept;
  Error emitStoreX(IRMem* dst, IRReg* src, uint32_t typeInfo) noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  IRBuilder* _ir;                        //!< IR builder.
  IRBlock* _block;                       //!< Current block.

  int _functionLevel;                    //!< Current function level (0 if main).
  bool _hasV256;                         //!< Use 256-bit SIMD instructions.

  AstSymbol* _hiddenRet;                 //!< A hidden return variable internally named `@ret`.
  IRPair<IRObject> _currentRet;          //!< Current return, required by \ref onReturn().

  FunctionSet _nestedFunctions;          //!< Hash of all nested functions.
  VarMap _varMap;                        //!< Mapping of `AstVar` to `IRPair<IRReg>`.
  MemMap _memMap;                        //!< Mapping of `AstVarMemb` to `IRMem`.
};

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPCODEGEN_P_H
