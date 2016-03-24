// [MPSL]
// Shader-Like Mathematical Expression JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MPSL_EXPORTS

// [Dependencies - MPSL]
#include "./mpasttoir_p.h"
#include "./mpmath_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

static uint32_t mpRegSizeFromTypeInfo(uint32_t typeInfo) noexcept {
  uint32_t size = TypeInfo::getSize(typeInfo & kTypeIdMask);
  return size * TypeInfo::getVectorSize(typeInfo);
}

static uint32_t mpGetIRFlags(uint32_t typeId, uint32_t vecLen) noexcept {
  if (vecLen <= 1)
    return 0;

  uint32_t size = TypeInfo::getSize(typeId) * vecLen;
  return size <= 16 ? kIRInstVec128 : kIRInstVec256;
}

// ============================================================================
// [mpsl::AstToIR - Construction / Destruction]
// ============================================================================

AstToIR::AstToIR(AstBuilder* ast, IRBuilder* ir) noexcept
  : AstVisitor(ast),
    _ir(ir),
    _currentBlock(nullptr),
    _currentValue(nullptr),
    _blockLevel(0),
    _varMap(ir->getAllocator()),
    _memMap(ir->getAllocator()) {
  _retPriv = ast->getGlobalScope()->resolveSymbol(StringRef("@ret", 4));
}
AstToIR::~AstToIR() noexcept {}

// ============================================================================
// [mpsl::AstToIR - OnNode]
// ============================================================================

Error AstToIR::onProgram(AstProgram* node) noexcept {
  AstNode** children = node->getChildren();
  uint32_t i, count = node->getLength();

  // Find the "main()" function and call onFunction() with it.
  for (i = 0; i < count; i++) {
    AstNode* child = children[i];
    if (child->getNodeType() == kAstNodeFunction) {
      AstFunction* func = static_cast<AstFunction*>(child);
      if (func->getFunc()->eq("main", 4)) {
        MPSL_PROPAGATE(_ir->initMainBlock());
        _currentBlock = _ir->getMainBlock();

        return onFunction(func);
      }
    }
  }

  return MPSL_TRACE_ERROR(kErrorNoEntryPoint);
}

// NOTE: This is only called once with the "main()" entry point. Other
// functions are simply inlined during the AST to IR translation.
Error AstToIR::onFunction(AstFunction* node) noexcept {
  AstSymbol* funcSumbol = node->getFunc();

  if (node->hasBody())
    MPSL_PROPAGATE(onNode(node->getBody()));

  return kErrorOk;
}

Error AstToIR::onBlock(AstBlock* node) noexcept {
  AstNode** children = node->getChildren();
  uint32_t i, count = node->getLength();

  for (i = 0; i < count; i++) {
    MPSL_PROPAGATE(onNode(children[i]));
    // Destroy the result as there is no use for it.
    resetCurrentValue();
  }

  return kErrorOk;
}

Error AstToIR::onBranch(AstBranch* node) noexcept {
  // TODO:
  MPSL_ASSERT(!"Implemented");
  return kErrorOk;
}

Error AstToIR::onCondition(AstCondition* node) noexcept {
  // TODO:
  MPSL_ASSERT(!"Implemented");
  return kErrorOk;
}

Error AstToIR::onLoop(AstLoop* node) noexcept {
  // TODO:
  MPSL_ASSERT(!"Implemented");
  return kErrorOk;
}

Error AstToIR::onBreak(AstBreak* node) noexcept {
  // TODO:
  MPSL_ASSERT(!"Implemented");
  return kErrorOk;
}

Error AstToIR::onContinue(AstContinue* node) noexcept {
  // TODO:
  MPSL_ASSERT(!"Implemented");
  return kErrorOk;
}

Error AstToIR::onReturn(AstReturn* node) noexcept {
  AstNode* retNode = node->getChild();
  if (retNode != nullptr) {
    MPSL_PROPAGATE(onNode(retNode));

    IRVar* retVar = nullptr;
    uint32_t typeInfo = _retPriv->getTypeInfo();
    MPSL_PROPAGATE(requireVar(&retVar, getCurrentValue(), typeInfo));

    IRMem* retMem = getIR()->newMem(
      getIR()->getDataPtr(_retPriv->getDataSlot()), nullptr, _retPriv->getDataOffset());
    MPSL_NULLCHECK(retMem);

    return emitStoreX(retMem, retVar, typeInfo);
  }

  return kErrorOk;
}

Error AstToIR::onVarDecl(AstVarDecl* node) noexcept {
  AstSymbol* sym = node->getSymbol();
  IRVar* var = nullptr;

  if (node->hasChild()) {
    MPSL_PROPAGATE(onNode(node->getChild()));
    MPSL_PROPAGATE(requireVar(&var, getCurrentValue(), node->getTypeInfo()));
  }
  else {
    var = getIR()->newVarByTypeInfo(sym->getTypeInfo());
    MPSL_NULLCHECK(var);
  }

  return mapVarToAst(sym, var);
}

Error AstToIR::onVarMemb(AstVarMemb* node) noexcept {
  if (!node->hasChild() || !node->getChild()->isVar())
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  AstVar* child = static_cast<AstVar*>(node->getChild());
  IRVar* base = getIR()->getDataPtr(child->getSymbol()->getDataSlot());

  if (MPSL_UNLIKELY(base == nullptr))
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  IRMem* mem = getIR()->newMem(base, nullptr, node->getOffset());
  if (MPSL_UNLIKELY(mem == nullptr))
    return MPSL_TRACE_ERROR(kErrorNoMemory);

  return setCurrentValue(mem);
}

Error AstToIR::onVar(AstVar* node) noexcept {
  AstSymbol* symbol = node->getSymbol();

  if (symbol->getDataSlot() != kInvalidDataSlot) {
    // This is the same as `onVarMemb()`, this variable has been denested.
    IRVar* base = _ir->getDataPtr(symbol->getDataSlot());
    if (MPSL_UNLIKELY(base == nullptr))
      return MPSL_TRACE_ERROR(kErrorInvalidState);

    IRMem* mem = getIR()->newMem(base, nullptr, symbol->getDataOffset());
    if (MPSL_UNLIKELY(mem == nullptr))
      return MPSL_TRACE_ERROR(kErrorNoMemory);

    return setCurrentValue(mem);
  }
  else {
    // The variable has to be already mapped if not declared by `onVarDecl()`.
    IRVar* var = _varMap.get(symbol);
    if (MPSL_UNLIKELY(var == nullptr))
      return MPSL_TRACE_ERROR(kErrorInvalidState);

    return setCurrentValue(var);
  }
}

Error AstToIR::onImm(AstImm* node) noexcept {
  uint32_t typeInfo = node->getTypeInfo();
  IRImm* imm = getIR()->newImmByTypeInfo(node->getValue(), node->getTypeInfo());

  MPSL_NULLCHECK(imm);
  return setCurrentValue(imm);
}

#define COMBINE_OP_TYPE(op, typeId) (((op) << 8) | ((typeId) << 4))
#define COMBINE_OP_CAST(toId, fromId) (((toId) << 8) | ((fromId) << 4))

Error AstToIR::onUnaryOp(AstUnaryOp* node) noexcept {
  if (MPSL_UNLIKELY(!node->hasChild()))
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  MPSL_PROPAGATE(onNode(node->getChild()));
  IRObject* value = getCurrentValue();
  IRBlock* block = getCurrentBlock();

  uint32_t op = node->getOp();
  uint32_t typeId = node->getTypeInfo() & kTypeIdMask;
  uint32_t vecLen = TypeInfo::getVectorSize(node->getTypeInfo());

  IRVar* var = nullptr;
  MPSL_PROPAGATE(requireVar(&var, value, node->getTypeInfo()));

  // Special case for `(x)++` and `++(x)` like operators.
  if (op == kOpPreInc || op == kOpPreDec || op == kOpPostInc || op == kOpPostDec) {
    // If the `value` is in memory then:
    //   1. Dereference it.
    //   2. Perform increment or decrement op.
    //   3. Store back in memory.
    bool preOp = false;
    Value immValue;
    uint32_t instCode = kIRInstNone;

    switch (COMBINE_OP_TYPE(op, typeId)) {
      case COMBINE_OP_TYPE(kOpPreInc  , kTypeInt):
        preOp = true;
        MPSL_FALLTHROUGH;
      case COMBINE_OP_TYPE(kOpPostInc , kTypeInt):
        instCode = kIRInstAddI32;
        immValue.i.set(1);
        break;

      case COMBINE_OP_TYPE(kOpPreDec  , kTypeInt):
        preOp = true;
        MPSL_FALLTHROUGH;
      case COMBINE_OP_TYPE(kOpPostDec , kTypeInt):
        instCode = kIRInstAddI32;
        immValue.i.set(-1);
        break;

      case COMBINE_OP_TYPE(kOpPreInc  , kTypeFloat):
        preOp = true;
        MPSL_FALLTHROUGH;
      case COMBINE_OP_TYPE(kOpPostInc , kTypeFloat):
        instCode = kIRInstAddF32;
        immValue.f.set(1.0f);
        break;

      case COMBINE_OP_TYPE(kOpPreDec  , kTypeFloat):
        preOp = true;
        MPSL_FALLTHROUGH;
      case COMBINE_OP_TYPE(kOpPostDec , kTypeFloat):
        instCode = kIRInstAddF32;
        immValue.f.set(-1.0f);
        break;

      case COMBINE_OP_TYPE(kOpPreInc  , kTypeDouble):
        preOp = true;
        MPSL_FALLTHROUGH;
      case COMBINE_OP_TYPE(kOpPostInc , kTypeDouble):
        instCode = kIRInstAddF64;
        immValue.d.set(1.0);
        break;

      case COMBINE_OP_TYPE(kOpPreDec  , kTypeDouble):
        preOp = true;
        MPSL_FALLTHROUGH;
      case COMBINE_OP_TYPE(kOpPostDec , kTypeDouble):
        instCode = kIRInstAddF64;
        immValue.d.set(-1.0);
        break;

      default:
        return MPSL_TRACE_ERROR(kErrorInvalidState);
    }

    IRImm* immOp = getIR()->newImmByTypeInfo(immValue, node->getTypeInfo());
    IRVar* result = getIR()->newVarByTypeInfo(node->getTypeInfo());

    if (preOp) {
      MPSL_PROPAGATE(getIR()->emitInst(block, instCode | mpGetIRFlags(typeId, vecLen), var, var, immOp));
      MPSL_PROPAGATE(getIR()->emitMove(block, result, var));
      MPSL_PROPAGATE(storeIfMem(value, var, node->getTypeInfo()));
    }
    else {
      MPSL_PROPAGATE(getIR()->emitMove(block, result, var));
      MPSL_PROPAGATE(getIR()->emitInst(block, instCode | mpGetIRFlags(typeId, vecLen), var, var, immOp));
      MPSL_PROPAGATE(storeIfMem(value, var, node->getTypeInfo()));
    }

    setCurrentValue(result);
  }
  else {
    IRVar* result = getIR()->newVarByTypeInfo(node->getTypeInfo());
    MPSL_NULLCHECK(result);

    uint32_t instCode = kIRInstNone;
    if (op == kOpCast) {
      uint32_t fromId = node->getChild()->getTypeInfo() & kTypeIdMask;
      switch (COMBINE_OP_CAST(typeId, fromId)) {
        case COMBINE_OP_CAST(kTypeFloat , kTypeDouble): instCode = kIRInstCvtF64ToF32; break;
        case COMBINE_OP_CAST(kTypeFloat , kTypeInt   ): instCode = kIRInstCvtF64ToI32; break;
        case COMBINE_OP_CAST(kTypeDouble, kTypeFloat ): instCode = kIRInstCvtF32ToF64; break;
        case COMBINE_OP_CAST(kTypeDouble, kTypeInt   ): instCode = kIRInstCvtF32ToI32; break;
        case COMBINE_OP_CAST(kTypeInt   , kTypeFloat ): instCode = kIRInstCvtI32ToF32; break;
        case COMBINE_OP_CAST(kTypeInt   , kTypeDouble): instCode = kIRInstCvtI32ToF64; break;

        default:
          return MPSL_TRACE_ERROR(kErrorInvalidState);
      }
    }
    else {
      switch (COMBINE_OP_TYPE(op, typeId)) {
        case COMBINE_OP_TYPE(kOpBitNeg  , kTypeInt   ): instCode = kIRInstBitNegI32; break;
        case COMBINE_OP_TYPE(kOpBitNeg  , kTypeFloat ): instCode = kIRInstBitNegF32; break;
        case COMBINE_OP_TYPE(kOpBitNeg  , kTypeDouble): instCode = kIRInstBitNegF64; break;

        case COMBINE_OP_TYPE(kOpNeg     , kTypeInt   ): instCode = kIRInstNegI32; break;
        case COMBINE_OP_TYPE(kOpNeg     , kTypeFloat ): instCode = kIRInstNegF32; break;
        case COMBINE_OP_TYPE(kOpNeg     , kTypeDouble): instCode = kIRInstNegF64; break;

        case COMBINE_OP_TYPE(kOpNot     , kTypeInt   ): instCode = kIRInstNotI32; break;
        case COMBINE_OP_TYPE(kOpNot     , kTypeFloat ): instCode = kIRInstNotF32; break;
        case COMBINE_OP_TYPE(kOpNot     , kTypeDouble): instCode = kIRInstNotF64; break;

        case COMBINE_OP_TYPE(kOpIsNan   , kTypeFloat ): instCode = kIRInstIsNanF32; break;
        case COMBINE_OP_TYPE(kOpIsNan   , kTypeDouble): instCode = kIRInstIsNanF64; break;

        case COMBINE_OP_TYPE(kOpIsInf   , kTypeFloat ): instCode = kIRInstIsInfF32; break;
        case COMBINE_OP_TYPE(kOpIsInf   , kTypeDouble): instCode = kIRInstIsInfF64; break;

        case COMBINE_OP_TYPE(kOpIsFinite, kTypeFloat ): instCode = kIRInstIsFiniteF32; break;
        case COMBINE_OP_TYPE(kOpIsFinite, kTypeDouble): instCode = kIRInstIsFiniteF64; break;

        case COMBINE_OP_TYPE(kOpSignBit , kTypeFloat ): instCode = kIRInstSignBitF32; break;
        case COMBINE_OP_TYPE(kOpSignBit , kTypeDouble): instCode = kIRInstSignBitF64; break;

        case COMBINE_OP_TYPE(kOpFloor   , kTypeFloat ): instCode = kIRInstFloorF32; break;
        case COMBINE_OP_TYPE(kOpFloor   , kTypeDouble): instCode = kIRInstFloorF64; break;

        case COMBINE_OP_TYPE(kOpRound   , kTypeFloat ): instCode = kIRInstRoundF32; break;
        case COMBINE_OP_TYPE(kOpRound   , kTypeDouble): instCode = kIRInstRoundF64; break;

        case COMBINE_OP_TYPE(kOpCeil    , kTypeFloat ): instCode = kIRInstCeilF32; break;
        case COMBINE_OP_TYPE(kOpCeil    , kTypeDouble): instCode = kIRInstCeilF64; break;

        case COMBINE_OP_TYPE(kOpAbs     , kTypeInt   ): instCode = kIRInstAbsI32; break;
        case COMBINE_OP_TYPE(kOpAbs     , kTypeFloat ): instCode = kIRInstAbsF32; break;
        case COMBINE_OP_TYPE(kOpAbs     , kTypeDouble): instCode = kIRInstAbsF64; break;

        case COMBINE_OP_TYPE(kOpExp     , kTypeFloat ): instCode = kIRInstExpF32; break;
        case COMBINE_OP_TYPE(kOpExp     , kTypeDouble): instCode = kIRInstExpF64; break;

        case COMBINE_OP_TYPE(kOpLog     , kTypeFloat ): instCode = kIRInstLogF32; break;
        case COMBINE_OP_TYPE(kOpLog     , kTypeDouble): instCode = kIRInstLogF64; break;

        case COMBINE_OP_TYPE(kOpLog2    , kTypeFloat ): instCode = kIRInstLog2F32; break;
        case COMBINE_OP_TYPE(kOpLog2    , kTypeDouble): instCode = kIRInstLog2F64; break;

        case COMBINE_OP_TYPE(kOpLog10   , kTypeFloat ): instCode = kIRInstLog10F32; break;
        case COMBINE_OP_TYPE(kOpLog10   , kTypeDouble): instCode = kIRInstLog10F64; break;

        case COMBINE_OP_TYPE(kOpSqrt    , kTypeFloat ): instCode = kIRInstSqrtF32; break;
        case COMBINE_OP_TYPE(kOpSqrt    , kTypeDouble): instCode = kIRInstSqrtF64; break;

        case COMBINE_OP_TYPE(kOpSin     , kTypeFloat ): instCode = kIRInstSinF32; break;
        case COMBINE_OP_TYPE(kOpSin     , kTypeDouble): instCode = kIRInstSinF64; break;

        case COMBINE_OP_TYPE(kOpCos     , kTypeFloat ): instCode = kIRInstCosF32; break;
        case COMBINE_OP_TYPE(kOpCos     , kTypeDouble): instCode = kIRInstCosF64; break;

        case COMBINE_OP_TYPE(kOpTan     , kTypeFloat ): instCode = kIRInstTanF32; break;
        case COMBINE_OP_TYPE(kOpTan     , kTypeDouble): instCode = kIRInstTanF64; break;

        case COMBINE_OP_TYPE(kOpAsin    , kTypeFloat ): instCode = kIRInstAsinF32; break;
        case COMBINE_OP_TYPE(kOpAsin    , kTypeDouble): instCode = kIRInstAsinF64; break;

        case COMBINE_OP_TYPE(kOpAcos    , kTypeFloat ): instCode = kIRInstAcosF32; break;
        case COMBINE_OP_TYPE(kOpAcos    , kTypeDouble): instCode = kIRInstAcosF64; break;

        case COMBINE_OP_TYPE(kOpAtan    , kTypeFloat ): instCode = kIRInstAtanF32; break;
        case COMBINE_OP_TYPE(kOpAtan    , kTypeDouble): instCode = kIRInstAtanF64; break;

        default:
          return MPSL_TRACE_ERROR(kErrorInvalidState);
      }
    }

    MPSL_PROPAGATE(getIR()->emitInst(block, instCode | mpGetIRFlags(typeId, vecLen), result, var));
    setCurrentValue(result);
  }

  return kErrorOk;
}

Error AstToIR::onBinaryOp(AstBinaryOp* node) noexcept {
  AstNode* lNode = node->getLeft();
  AstNode* rNode = node->getRight();

  if (MPSL_UNLIKELY(lNode == nullptr || rNode == nullptr))
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  MPSL_PROPAGATE(onNode(lNode));
  IRObject* lValue = getCurrentValue();

  MPSL_PROPAGATE(onNode(rNode));
  IRObject* rValue = getCurrentValue();

  uint32_t op = node->getOp();
  uint32_t typeId = node->getTypeInfo() & kTypeIdMask;
  uint32_t vecLen = TypeInfo::getVectorSize(node->getTypeInfo());

  uint32_t instCode = kIRInstNone;
  switch (COMBINE_OP_TYPE(op, typeId)) {
    case COMBINE_OP_TYPE(kOpEq          , kTypeInt   ): instCode = kIRInstCmpEqI32; break;
    case COMBINE_OP_TYPE(kOpEq          , kTypeFloat ): instCode = kIRInstCmpEqF32; break;
    case COMBINE_OP_TYPE(kOpEq          , kTypeDouble): instCode = kIRInstCmpEqF64; break;

    case COMBINE_OP_TYPE(kOpNe          , kTypeInt   ): instCode = kIRInstCmpNeI32; break;
    case COMBINE_OP_TYPE(kOpNe          , kTypeFloat ): instCode = kIRInstCmpNeF32; break;
    case COMBINE_OP_TYPE(kOpNe          , kTypeDouble): instCode = kIRInstCmpNeF64; break;

    case COMBINE_OP_TYPE(kOpLt          , kTypeInt   ): instCode = kIRInstCmpLtI32; break;
    case COMBINE_OP_TYPE(kOpLt          , kTypeFloat ): instCode = kIRInstCmpLtF32; break;
    case COMBINE_OP_TYPE(kOpLt          , kTypeDouble): instCode = kIRInstCmpLtF64; break;

    case COMBINE_OP_TYPE(kOpLe          , kTypeInt   ): instCode = kIRInstCmpLeI32; break;
    case COMBINE_OP_TYPE(kOpLe          , kTypeFloat ): instCode = kIRInstCmpLeF32; break;
    case COMBINE_OP_TYPE(kOpLe          , kTypeDouble): instCode = kIRInstCmpLeF64; break;

    case COMBINE_OP_TYPE(kOpGt          , kTypeInt   ): instCode = kIRInstCmpGtI32; break;
    case COMBINE_OP_TYPE(kOpGt          , kTypeFloat ): instCode = kIRInstCmpGtF32; break;
    case COMBINE_OP_TYPE(kOpGt          , kTypeDouble): instCode = kIRInstCmpGtF64; break;

    case COMBINE_OP_TYPE(kOpGe          , kTypeInt   ): instCode = kIRInstCmpGeI32; break;
    case COMBINE_OP_TYPE(kOpGe          , kTypeFloat ): instCode = kIRInstCmpGeF32; break;
    case COMBINE_OP_TYPE(kOpGe          , kTypeDouble): instCode = kIRInstCmpGeF64; break;

    case COMBINE_OP_TYPE(kOpAdd         , kTypeInt   ): instCode = kIRInstAddI32; break;
    case COMBINE_OP_TYPE(kOpAdd         , kTypeFloat ): instCode = kIRInstAddF32; break;
    case COMBINE_OP_TYPE(kOpAdd         , kTypeDouble): instCode = kIRInstAddF64; break;

    case COMBINE_OP_TYPE(kOpSub         , kTypeInt   ): instCode = kIRInstSubI32; break;
    case COMBINE_OP_TYPE(kOpSub         , kTypeFloat ): instCode = kIRInstSubF32; break;
    case COMBINE_OP_TYPE(kOpSub         , kTypeDouble): instCode = kIRInstSubF64; break;

    case COMBINE_OP_TYPE(kOpMul         , kTypeInt   ): instCode = kIRInstMulI32; break;
    case COMBINE_OP_TYPE(kOpMul         , kTypeFloat ): instCode = kIRInstMulF32; break;
    case COMBINE_OP_TYPE(kOpMul         , kTypeDouble): instCode = kIRInstMulF64; break;

    case COMBINE_OP_TYPE(kOpDiv         , kTypeInt   ): instCode = kIRInstDivI32; break;
    case COMBINE_OP_TYPE(kOpDiv         , kTypeFloat ): instCode = kIRInstDivF32; break;
    case COMBINE_OP_TYPE(kOpDiv         , kTypeDouble): instCode = kIRInstDivF64; break;

    case COMBINE_OP_TYPE(kOpMod         , kTypeInt   ): instCode = kIRInstModI32; break;
    case COMBINE_OP_TYPE(kOpMod         , kTypeFloat ): instCode = kIRInstModF32; break;
    case COMBINE_OP_TYPE(kOpMod         , kTypeDouble): instCode = kIRInstModF64; break;

    case COMBINE_OP_TYPE(kOpBitAnd      , kTypeInt   ): instCode = kIRInstAndI32; break;
    case COMBINE_OP_TYPE(kOpBitAnd      , kTypeFloat ): instCode = kIRInstAndF32; break;
    case COMBINE_OP_TYPE(kOpBitAnd      , kTypeDouble): instCode = kIRInstAndF64; break;

    case COMBINE_OP_TYPE(kOpBitOr       , kTypeInt   ): instCode = kIRInstOrI32; break;
    case COMBINE_OP_TYPE(kOpBitOr       , kTypeFloat ): instCode = kIRInstOrF32; break;
    case COMBINE_OP_TYPE(kOpBitOr       , kTypeDouble): instCode = kIRInstOrF64; break;

    case COMBINE_OP_TYPE(kOpBitXor      , kTypeInt   ): instCode = kIRInstXorI32; break;
    case COMBINE_OP_TYPE(kOpBitXor      , kTypeFloat ): instCode = kIRInstXorF32; break;
    case COMBINE_OP_TYPE(kOpBitXor      , kTypeDouble): instCode = kIRInstXorF64; break;

    case COMBINE_OP_TYPE(kOpBitSar      , kTypeInt   ): instCode = kIRInstSarI32; break;
    case COMBINE_OP_TYPE(kOpBitShr      , kTypeInt   ): instCode = kIRInstShrI32; break;
    case COMBINE_OP_TYPE(kOpBitShl      , kTypeInt   ): instCode = kIRInstShlI32; break;
    case COMBINE_OP_TYPE(kOpBitRor      , kTypeInt   ): instCode = kIRInstRorI32; break;
    case COMBINE_OP_TYPE(kOpBitRol      , kTypeInt   ): instCode = kIRInstRolI32; break;

    case COMBINE_OP_TYPE(kOpMin         , kTypeInt   ): instCode = kIRInstMinI32; break;
    case COMBINE_OP_TYPE(kOpMin         , kTypeFloat ): instCode = kIRInstMinF32; break;
    case COMBINE_OP_TYPE(kOpMin         , kTypeDouble): instCode = kIRInstMinF64; break;

    case COMBINE_OP_TYPE(kOpMax         , kTypeInt   ): instCode = kIRInstMaxI32; break;
    case COMBINE_OP_TYPE(kOpMax         , kTypeFloat ): instCode = kIRInstMaxF32; break;
    case COMBINE_OP_TYPE(kOpMax         , kTypeDouble): instCode = kIRInstMaxF64; break;

    case COMBINE_OP_TYPE(kOpPow         , kTypeFloat ): instCode = kIRInstPowF32; break;
    case COMBINE_OP_TYPE(kOpPow         , kTypeDouble): instCode = kIRInstPowF64; break;

    case COMBINE_OP_TYPE(kOpAtan2       , kTypeFloat ): instCode = kIRInstAtan2F32; break;
    case COMBINE_OP_TYPE(kOpAtan2       , kTypeDouble): instCode = kIRInstAtan2F64; break;

    case COMBINE_OP_TYPE(kOpCopySign    , kTypeFloat ): instCode = kIRInstCopySignF32; break;
    case COMBINE_OP_TYPE(kOpCopySign    , kTypeDouble): instCode = kIRInstCopySignF64; break;

    case COMBINE_OP_TYPE(kOpAssign      , kTypeInt   ): break;
    case COMBINE_OP_TYPE(kOpAssign      , kTypeFloat ): break;
    case COMBINE_OP_TYPE(kOpAssign      , kTypeDouble): break;

    case COMBINE_OP_TYPE(kOpAssignAdd   , kTypeInt   ): instCode = kIRInstAddI32; break;
    case COMBINE_OP_TYPE(kOpAssignAdd   , kTypeFloat ): instCode = kIRInstAddF32; break;
    case COMBINE_OP_TYPE(kOpAssignAdd   , kTypeDouble): instCode = kIRInstAddF64; break;

    case COMBINE_OP_TYPE(kOpAssignSub   , kTypeInt   ): instCode = kIRInstSubI32; break;
    case COMBINE_OP_TYPE(kOpAssignSub   , kTypeFloat ): instCode = kIRInstSubF32; break;
    case COMBINE_OP_TYPE(kOpAssignSub   , kTypeDouble): instCode = kIRInstSubF64; break;

    case COMBINE_OP_TYPE(kOpAssignMul   , kTypeInt   ): instCode = kIRInstMulI32; break;
    case COMBINE_OP_TYPE(kOpAssignMul   , kTypeFloat ): instCode = kIRInstMulF32; break;
    case COMBINE_OP_TYPE(kOpAssignMul   , kTypeDouble): instCode = kIRInstMulF64; break;

    case COMBINE_OP_TYPE(kOpAssignDiv   , kTypeInt   ): instCode = kIRInstDivI32; break;
    case COMBINE_OP_TYPE(kOpAssignDiv   , kTypeFloat ): instCode = kIRInstDivF32; break;
    case COMBINE_OP_TYPE(kOpAssignDiv   , kTypeDouble): instCode = kIRInstDivF64; break;

    case COMBINE_OP_TYPE(kOpAssignMod   , kTypeInt   ): instCode = kIRInstModI32; break;
    case COMBINE_OP_TYPE(kOpAssignMod   , kTypeFloat ): instCode = kIRInstModF32; break;
    case COMBINE_OP_TYPE(kOpAssignMod   , kTypeDouble): instCode = kIRInstModF64; break;

    case COMBINE_OP_TYPE(kOpAssignBitAnd, kTypeInt   ): instCode = kIRInstAndI32; break;
    case COMBINE_OP_TYPE(kOpAssignBitAnd, kTypeFloat ): instCode = kIRInstAndF32; break;
    case COMBINE_OP_TYPE(kOpAssignBitAnd, kTypeDouble): instCode = kIRInstAndF64; break;

    case COMBINE_OP_TYPE(kOpAssignBitOr , kTypeInt   ): instCode = kIRInstOrI32; break;
    case COMBINE_OP_TYPE(kOpAssignBitOr , kTypeFloat ): instCode = kIRInstOrF32; break;
    case COMBINE_OP_TYPE(kOpAssignBitOr , kTypeDouble): instCode = kIRInstOrF64; break;

    case COMBINE_OP_TYPE(kOpAssignBitXor, kTypeInt   ): instCode = kIRInstXorI32; break;
    case COMBINE_OP_TYPE(kOpAssignBitXor, kTypeFloat ): instCode = kIRInstXorF32; break;
    case COMBINE_OP_TYPE(kOpAssignBitXor, kTypeDouble): instCode = kIRInstXorF64; break;

    case COMBINE_OP_TYPE(kOpAssignBitSar, kTypeInt   ): instCode = kIRInstSarI32; break;
    case COMBINE_OP_TYPE(kOpAssignBitShr, kTypeInt   ): instCode = kIRInstShrI32; break;
    case COMBINE_OP_TYPE(kOpAssignBitShl, kTypeInt   ): instCode = kIRInstShlI32; break;

    default:
      return MPSL_TRACE_ERROR(kErrorInvalidState);
  }

  IRVar* result = getIR()->newVarByTypeInfo(node->getTypeInfo());
  MPSL_NULLCHECK(result);

  IRVar* lVar = nullptr;
  IRVar* rVar = nullptr;

  if (OpInfo::get(op).isAssignment()) {
    if (op == kOpAssign) {
      MPSL_PROPAGATE(requireVar(&rVar, rValue, node->getTypeInfo()));

      if (lValue->isMem())
        MPSL_PROPAGATE(emitStoreX(static_cast<IRMem*>(lValue), rVar, node->getTypeInfo()));
      else
        MPSL_PROPAGATE(getIR()->emitMove(getCurrentBlock(), lVar, rVar));

      MPSL_PROPAGATE(getIR()->emitMove(getCurrentBlock(), result, rVar));
      setCurrentValue(result);
    }
    else {
      MPSL_PROPAGATE(requireVar(&lVar, lValue, node->getTypeInfo()));
      MPSL_PROPAGATE(requireVar(&rVar, rValue, node->getTypeInfo()));
      MPSL_PROPAGATE(getIR()->emitInst(getCurrentBlock(), instCode | mpGetIRFlags(typeId, vecLen), lVar, lVar, rVar));

      if (lValue->isMem()) {
        MPSL_PROPAGATE(emitStoreX(static_cast<IRMem*>(lValue), lVar, node->getTypeInfo()));
        setCurrentValue(lVar);
      }
      else {
        MPSL_PROPAGATE(getIR()->emitMove(getCurrentBlock(), result, lVar));
        setCurrentValue(result);
      }
    }
  }
  else {
    MPSL_PROPAGATE(requireVar(&lVar, lValue, node->getTypeInfo()));
    MPSL_PROPAGATE(requireVar(&rVar, rValue, node->getTypeInfo()));
    MPSL_PROPAGATE(getIR()->emitInst(getCurrentBlock(), instCode | mpGetIRFlags(typeId, vecLen), result, lVar, rVar));
    setCurrentValue(result);
  }

  return kErrorOk;
}

#undef COMBINE_OP_TYPE
#undef COMBINE_OP_CAST

Error AstToIR::onCall(AstCall* node) noexcept {
  // TODO:
  MPSL_ASSERT(!"Implemented");
  return kErrorOk;
}

// ============================================================================
// [mpsl::AstToIR - Utilities]
// ============================================================================

Error AstToIR::mapVarToAst(AstSymbol* sym, IRVar* var) noexcept {
  MPSL_ASSERT(_varMap.get(sym) == nullptr);
  return _varMap.put(sym, var);
}

Error AstToIR::requireVar(IRVar** varOut, IRObject* any, uint32_t typeInfo) noexcept {
  switch (any->getObjectType()) {
    case kIRObjectVar: {
      *varOut = static_cast<IRVar*>(any);
      return kErrorOk;
    }

    case kIRObjectMem: {
      IRMem* mem = static_cast<IRMem*>(any);
      IRVar* var = getIR()->newVarByTypeInfo(typeInfo);

      MPSL_NULLCHECK(var);
      MPSL_PROPAGATE(emitFetchX(var, mem, typeInfo));

      *varOut = var;
      return kErrorOk;
    }

    case kIRObjectImm: {
      IRImm* imm = static_cast<IRImm*>(any);
      IRVar* var = getIR()->newVar(imm->getReg(), imm->getWidth());

      MPSL_NULLCHECK(var);
      MPSL_PROPAGATE(getIR()->emitFetch(getCurrentBlock(), var, imm));

      *varOut = var;
      return kErrorOk;
    }

    default:
      return MPSL_TRACE_ERROR(kErrorInvalidState);
  }
}

Error AstToIR::emitFetchX(IRVar* dst, IRMem* src, uint32_t typeInfo) noexcept {
  uint32_t instCode = kIRInstNone;
  bool odd = (typeInfo & kTypeVec3) != 0;

  switch (typeInfo & (kTypeIdMask | kTypeVecMask)) {
    case kTypeInt     : instCode = kIRInstFetch32; break;
    case kTypeInt_1   : instCode = kIRInstFetch32; break;
    case kTypeInt_2   : instCode = kIRInstFetch64; break;
    case kTypeInt_3   : instCode = kIRInstFetch64; break;
    case kTypeInt_4   : instCode = kIRInstFetch128; break;

    case kTypeFloat   : instCode = kIRInstFetch32; break;
    case kTypeFloat_1 : instCode = kIRInstFetch32; break;
    case kTypeFloat_2 : instCode = kIRInstFetch64; break;
    case kTypeFloat_3 : instCode = kIRInstFetch64; break;
    case kTypeFloat_4 : instCode = kIRInstFetch128; break;

    case kTypeDouble  : instCode = kIRInstFetch64; break;
    case kTypeDouble_1: instCode = kIRInstFetch64; break;
    case kTypeDouble_2: instCode = kIRInstFetch128; break;
    case kTypeDouble_3: instCode = kIRInstFetch128; break;
    case kTypeDouble_4: instCode = kIRInstFetch256; break;

    default:
      return MPSL_TRACE_ERROR(kErrorInvalidState);
  }

  MPSL_PROPAGATE(getIR()->emitInst(getCurrentBlock(), instCode, dst, src));

  // TODO: Double3.
  /*
  if (odd) {
    mem = getIR()->newMem(mem->getBase(), mem->getIndex(), mem->getOffset() + 8);
    MPSL_NULLCHECK(mem);
    MPSL_PROPAGATE(getIR()->emitInst(block, kIRInstInsert32, var, var, mem));
  }
  */

  return kErrorOk;
}

Error AstToIR::emitStoreX(IRMem* dst, IRVar* src, uint32_t typeInfo) noexcept {
  uint32_t instCode = kIRInstNone;
  bool odd = (typeInfo & kTypeVec3) != 0;

  switch (typeInfo & (kTypeIdMask | kTypeVecMask)) {
    case kTypeInt     : instCode = kIRInstStore32; break;
    case kTypeInt_1   : instCode = kIRInstStore32; break;
    case kTypeInt_2   : instCode = kIRInstStore64; break;
    case kTypeInt_3   : instCode = kIRInstStore64; break;
    case kTypeInt_4   : instCode = kIRInstStore128; break;

    case kTypeFloat   : instCode = kIRInstStore32; break;
    case kTypeFloat_1 : instCode = kIRInstStore32; break;
    case kTypeFloat_2 : instCode = kIRInstStore64; break;
    case kTypeFloat_3 : instCode = kIRInstStore64; break;
    case kTypeFloat_4 : instCode = kIRInstStore128; break;

    case kTypeDouble  : instCode = kIRInstStore64; break;
    case kTypeDouble_1: instCode = kIRInstStore64; break;
    case kTypeDouble_2: instCode = kIRInstStore128; break;
    case kTypeDouble_3: instCode = kIRInstStore128; break;
    case kTypeDouble_4: instCode = kIRInstStore256; break;

    default:
      return MPSL_TRACE_ERROR(kErrorInvalidState);
  }

  MPSL_PROPAGATE(getIR()->emitInst(getCurrentBlock(), instCode, dst, src));

  // TODO: Double3.
  /*
  if (odd) {
    mem = getIR()->newMem(mem->getBase(), mem->getIndex(), mem->getOffset() + 8);
    MPSL_NULLCHECK(mem);
    MPSL_PROPAGATE(getIR()->emitInst(block, kIRInstInsert32, var, var, mem));
  }
  */

  return kErrorOk;
}

Error AstToIR::fetchIfMem(IRVar** dst, IRObject* src, uint32_t typeInfo) noexcept {
  if (src->isVar()) {
    *dst = static_cast<IRVar*>(src);
    return kErrorOk;
  }

  if (src->isMem()) {
    IRMem* mem = static_cast<IRMem*>(src);
    IRVar* var = getIR()->newVarByTypeInfo(typeInfo);

    MPSL_NULLCHECK(var);
    MPSL_PROPAGATE(emitFetchX(var, mem, typeInfo));

    *dst = var;
    return kErrorOk;
  }

  return MPSL_TRACE_ERROR(kErrorInvalidState);
}

Error AstToIR::storeIfMem(IRObject* dst, IRVar* src, uint32_t typeInfo) noexcept {
  if (dst->isVar())
    return kErrorOk;

  if (dst->isMem())
    return emitStoreX(static_cast<IRMem*>(dst), src, typeInfo);

  return MPSL_TRACE_ERROR(kErrorInvalidState);
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
