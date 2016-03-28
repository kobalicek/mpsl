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
// [mpsl::IRPair]
// ============================================================================

//! A pair of two IR objects.
//!
//! Used to split 256-bit wide operations to 128-bit in case the target
//! doesn't support 256-bit wide registers (all pre-AVX architectures, ARM).
template<typename T>
struct IRPair {
  MPSL_INLINE IRPair() noexcept
    : lo(nullptr),
      hi(nullptr) {}

  MPSL_INLINE IRPair(const IRPair<T>& other) noexcept
    : lo(other.lo),
      hi(other.hi) {}

  MPSL_INLINE Error set(T* lo, T* hi = nullptr) noexcept {
    this->lo = lo;
    this->hi = hi;
    return kErrorOk;
  }

  template<typename U>
  MPSL_INLINE Error set(const IRPair<U>& pair) noexcept {
    this->lo = static_cast<T*>(pair.lo);
    this->hi = static_cast<T*>(pair.hi);
    return kErrorOk;
  }

  MPSL_INLINE operator IRPair<IRObject>&() noexcept {
    return *reinterpret_cast<IRPair<IRObject>*>(this);
  }
  MPSL_INLINE operator const IRPair<IRObject>&() const noexcept {
    return *reinterpret_cast<const IRPair<IRObject>*>(this);
  }

  MPSL_INLINE void reset() noexcept {
    this->lo = nullptr;
    this->hi = nullptr;
  }

  union {
    struct {
      T* lo;
      T* hi;
    };

    T* obj[2];
  };
};

// ============================================================================
// [mpsl::AstToIR]
// ============================================================================

struct AstToIRArgs {
  MPSL_INLINE AstToIRArgs(bool dependsOnResult = true) noexcept {
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
struct AstToIR : public AstVisitorWithArgs<AstToIR, AstToIRArgs&> {
  MPSL_NO_COPY(AstToIR)

  typedef AstToIRArgs Args;

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

  MPSL_NOAPI AstToIR(AstBuilder* ast, IRBuilder* ir) noexcept;
  MPSL_NOAPI ~AstToIR() noexcept;

  // --------------------------------------------------------------------------
  // [OnNode]
  // --------------------------------------------------------------------------

  MPSL_NOAPI Error onProgram(AstProgram* node, Args& out) noexcept;
  MPSL_NOAPI Error onFunction(AstFunction* node, Args& out) noexcept;
  MPSL_NOAPI Error onBlock(AstBlock* node, Args& out) noexcept;
  MPSL_NOAPI Error onBranch(AstBranch* node, Args& out) noexcept;
  MPSL_NOAPI Error onCondition(AstCondition* node, Args& out) noexcept;
  MPSL_NOAPI Error onLoop(AstLoop* node, Args& out) noexcept;
  MPSL_NOAPI Error onBreak(AstBreak* node, Args& out) noexcept;
  MPSL_NOAPI Error onContinue(AstContinue* node, Args& out) noexcept;
  MPSL_NOAPI Error onReturn(AstReturn* node, Args& out) noexcept;
  MPSL_NOAPI Error onVarDecl(AstVarDecl* node, Args& out) noexcept;
  MPSL_NOAPI Error onVarMemb(AstVarMemb* node, Args& out) noexcept;
  MPSL_NOAPI Error onVar(AstVar* node, Args& out) noexcept;
  MPSL_NOAPI Error onImm(AstImm* node, Args& out) noexcept;
  MPSL_NOAPI Error onUnaryOp(AstUnaryOp* node, Args& out) noexcept;
  MPSL_NOAPI Error onBinaryOp(AstBinaryOp* node, Args& out) noexcept;
  MPSL_NOAPI Error onCall(AstCall* node, Args& out) noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE IRBuilder* getIR() const noexcept { return _ir; }
  MPSL_INLINE Allocator* getAllocator() const noexcept { return _ir->getAllocator(); }

  MPSL_INLINE IRBlock* getBlock() const noexcept { return _block; }

  MPSL_INLINE bool hasV256() const noexcept { return _hasV256; }
  MPSL_INLINE bool needSplit(uint32_t width) const { return width > 16 && !_hasV256; }

  // --------------------------------------------------------------------------
  // [Utilities]
  // --------------------------------------------------------------------------

  MPSL_NOAPI Error mapVarToAst(AstSymbol* sym, IRPair<IRVar> var) noexcept;

  MPSL_NOAPI Error newVar(IRPair<IRObject>& dst, uint32_t typeInfo) noexcept;
  MPSL_NOAPI Error newImm(IRPair<IRObject>& dst, const Value& value, uint32_t typeInfo) noexcept;
  MPSL_NOAPI Error addrOfData(IRPair<IRObject>& dst, DataSlot data, uint32_t width) noexcept;
  MPSL_NOAPI Error asVar(IRPair<IRVar>& out, IRPair<IRObject> in, uint32_t typeInfo) noexcept;

  MPSL_NOAPI Error emitMove(IRPair<IRVar> dst, IRPair<IRVar> src, uint32_t typeInfo) noexcept;
  MPSL_NOAPI Error emitStore(IRPair<IRObject> dst, IRPair<IRVar> src, uint32_t typeInfo) noexcept
   ;
  MPSL_NOAPI Error emitInst2(uint32_t instCode,
    IRPair<IRObject> o0,
    IRPair<IRObject> o1, uint32_t typeInfo) noexcept;

  MPSL_NOAPI Error emitInst3(uint32_t instCode,
    IRPair<IRObject> o0,
    IRPair<IRObject> o1,
    IRPair<IRObject> o2, uint32_t typeInfo) noexcept;

  // TODO: Rename after API is completed.
  MPSL_NOAPI Error emitFetchX(IRVar* dst, IRMem* src, uint32_t typeInfo) noexcept;
  MPSL_NOAPI Error emitStoreX(IRMem* dst, IRVar* src, uint32_t typeInfo) noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! IR builder.
  IRBuilder* _ir;
  //! A hidden return variable internally named `@ret`.
  AstSymbol* _retPriv;
  //! Current block.
  IRBlock* _block;

  //! Current block level (0 if root).
  int _blockLevel;
  bool _hasV256;

  //! Mapping of `AstVar` to `IRPair<IRVar>`.
  Map<AstSymbol*, IRPair<IRVar> > _varMap;
  //! Mapping of `AstVarMemb` to `IRMem`.
  Map<AstSymbol*, IRMem*> _memMap;
};

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPASTTOIR_P_H
