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

  typedef Set< AstFunction* > FunctionSet;
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

  MPSL_NOAPI CodeGen(AstBuilder* ast, IRBuilder* ir) noexcept;
  MPSL_NOAPI ~CodeGen() noexcept;

  // --------------------------------------------------------------------------
  // [OnNode]
  // --------------------------------------------------------------------------

  MPSL_NOAPI Error onProgram(AstProgram* node, Result& out) noexcept;
  MPSL_NOAPI Error onFunction(AstFunction* node, Result& out) noexcept;
  MPSL_NOAPI Error onBlock(AstBlock* node, Result& out) noexcept;
  MPSL_NOAPI Error onBranch(AstBranch* node, Result& out) noexcept;
  MPSL_NOAPI Error onCondition(AstCondition* node, Result& out) noexcept;
  MPSL_NOAPI Error onLoop(AstLoop* node, Result& out) noexcept;
  MPSL_NOAPI Error onBreak(AstBreak* node, Result& out) noexcept;
  MPSL_NOAPI Error onContinue(AstContinue* node, Result& out) noexcept;
  MPSL_NOAPI Error onReturn(AstReturn* node, Result& out) noexcept;
  MPSL_NOAPI Error onVarDecl(AstVarDecl* node, Result& out) noexcept;
  MPSL_NOAPI Error onVarMemb(AstVarMemb* node, Result& out) noexcept;
  MPSL_NOAPI Error onVar(AstVar* node, Result& out) noexcept;
  MPSL_NOAPI Error onImm(AstImm* node, Result& out) noexcept;
  MPSL_NOAPI Error onUnaryOp(AstUnaryOp* node, Result& out) noexcept;
  MPSL_NOAPI Error onBinaryOp(AstBinaryOp* node, Result& out) noexcept;
  MPSL_NOAPI Error onCall(AstCall* node, Result& out) noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE IRBuilder* getIR() const noexcept { return _ir; }
  MPSL_INLINE ZoneHeap* getHeap() const noexcept { return _ir->getHeap(); }
  MPSL_INLINE IRBlock* getBlock() const noexcept { return _block; }

  MPSL_INLINE bool hasV256() const noexcept { return _hasV256; }
  MPSL_INLINE bool needSplit(uint32_t width) const { return width > 16 && !_hasV256; }

  // --------------------------------------------------------------------------
  // [Utilities]
  // --------------------------------------------------------------------------

  MPSL_NOAPI Error mapVarToAst(AstSymbol* sym, IRPair<IRReg> var) noexcept;

  MPSL_NOAPI Error newVar(IRPair<IRObject>& dst, uint32_t typeInfo) noexcept;
  MPSL_NOAPI Error newImm(IRPair<IRObject>& dst, const Value& value, uint32_t typeInfo) noexcept;
  MPSL_NOAPI Error addrOfData(IRPair<IRObject>& dst, DataSlot data, uint32_t width) noexcept;

  MPSL_NOAPI Error asVar(IRPair<IRObject>& out, IRPair<IRObject> in, uint32_t typeInfo) noexcept;

  MPSL_NOAPI Error emitMove(IRPair<IRReg> dst, IRPair<IRReg> src, uint32_t typeInfo) noexcept;
  MPSL_NOAPI Error emitStore(IRPair<IRObject> dst, IRPair<IRReg> src, uint32_t typeInfo) noexcept;
  MPSL_NOAPI Error emitInst2(uint32_t instCode,
    IRPair<IRObject> o0,
    IRPair<IRObject> o1, uint32_t typeInfo) noexcept;

  MPSL_NOAPI Error emitInst3(uint32_t instCode,
    IRPair<IRObject> o0,
    IRPair<IRObject> o1,
    IRPair<IRObject> o2, uint32_t typeInfo) noexcept;

  // TODO: Rename after API is completed.
  MPSL_NOAPI Error emitFetchX(IRReg* dst, IRMem* src, uint32_t typeInfo) noexcept;
  MPSL_NOAPI Error emitStoreX(IRMem* dst, IRReg* src, uint32_t typeInfo) noexcept;

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
