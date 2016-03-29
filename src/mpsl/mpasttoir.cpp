// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
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
  uint32_t size = TypeInfo::sizeOf(typeInfo & kTypeIdMask);
  return size * TypeInfo::elementsOf(typeInfo);
}

static MPSL_INLINE uint32_t mpGetSIMDFlags(uint32_t typeInfo) noexcept {
  if ((typeInfo & kTypeVecMask) < kTypeVec2)
    return 0;
  else
    return TypeInfo::widthOf(typeInfo) <= 16 ? kIRInstV128 : kIRInstV256;
}

static bool mpSplitTypeInfo(uint32_t& loOut, uint32_t& hiOut, uint32_t typeInfo) noexcept {
  uint32_t typeId = typeInfo & kTypeIdMask;
  uint32_t vecCnt = typeInfo & kTypeVecMask;

  switch (typeId) {
    case kTypeBool:
    case kTypeInt:
    case kTypeFloat:
      if (vecCnt > kTypeVec4) {
        // MPSL only allows 8 element vectors of 32-bit types, which will always
        // split into the same LO/HI parts of 4 elements. This code is a bit more
        // generic than the assumption.
        uint32_t base = typeInfo & ~kTypeVecMask;
        loOut = base | (kTypeVec4);
        hiOut = base | (vecCnt - kTypeVec4);
        return true;
      }
      break;

    case kTypeQBool:
    case kTypeDouble:
      if (vecCnt > kTypeVec2) {
        // MPSL only allows up to 4 element vectors of 64-bit types. It means
        // that there are basically two possible cases:
        //   1. Splitting a vector of 4 elements as [2, 2] pair.
        //   2. Splitting a vector of 4 elements as [2, 1] pair.
        uint32_t base = typeInfo & ~kTypeVecMask;
        loOut = base | (kTypeVec2);
        hiOut = base | (vecCnt - kTypeVec2);
        return true;
      }
      break;
  }

  // Split not needed.
  loOut = typeInfo;
  hiOut = kTypeVoid;
  return false;
}

static MPSL_INLINE bool mpIsValueLoHiEqual(const Value& val) noexcept {
  return (val.q[0] == val.q[2]) && (val.q[1] == val.q[3]);
}

// ============================================================================
// [mpsl::AstToIR - Construction / Destruction]
// ============================================================================

AstToIR::AstToIR(AstBuilder* ast, IRBuilder* ir) noexcept
  : AstVisitorWithArgs<AstToIR, Args&>(ast),
    _ir(ir),
    _retPriv(nullptr),
    _block(nullptr),
    _blockLevel(0),
    _hasV256(false),
    _varMap(ir->getAllocator()),
    _memMap(ir->getAllocator()) {
  _retPriv = ast->getGlobalScope()->resolveSymbol(StringRef("@ret", 4));
}
AstToIR::~AstToIR() noexcept {}

// ============================================================================
// [mpsl::AstToIR - OnNode]
// ============================================================================

Error AstToIR::onProgram(AstProgram* node, Args& out) noexcept {
  AstNode** children = node->getChildren();
  uint32_t i, count = node->getLength();

  // Find the "main()" function and call onFunction() with it.
  for (i = 0; i < count; i++) {
    AstNode* child = children[i];
    if (child->getNodeType() == kAstNodeFunction) {
      AstFunction* func = static_cast<AstFunction*>(child);
      if (func->getFunc()->eq("main", 4)) {
        MPSL_PROPAGATE(_ir->initMainBlock());
        _block = _ir->getMainBlock();
        return onFunction(func, out);
      }
    }
  }

  return MPSL_TRACE_ERROR(kErrorNoEntryPoint);
}

// NOTE: This is only called once with "main()" function `node`. Other
// functions are simply inlined during the AST to IR translation.
Error AstToIR::onFunction(AstFunction* node, Args& out) noexcept {
  if (node->hasBody())
    MPSL_PROPAGATE(onNode(node->getBody(), out));

  return kErrorOk;
}

Error AstToIR::onBlock(AstBlock* node, Args& out) noexcept {
  AstNode** children = node->getChildren();
  uint32_t i, count = node->getLength();

  for (i = 0; i < count; i++) {
    Args noArgs(false);
    MPSL_PROPAGATE(onNode(children[i], noArgs));
  }

  return kErrorOk;
}

Error AstToIR::onBranch(AstBranch* node, Args& out) noexcept {
  // TODO:
  MPSL_ASSERT(!"Implemented");
  return kErrorOk;
}

Error AstToIR::onCondition(AstCondition* node, Args& out) noexcept {
  // TODO:
  MPSL_ASSERT(!"Implemented");
  return kErrorOk;
}

Error AstToIR::onLoop(AstLoop* node, Args& out) noexcept {
  // TODO:
  MPSL_ASSERT(!"Implemented");
  return kErrorOk;
}

Error AstToIR::onBreak(AstBreak* node, Args& out) noexcept {
  // TODO:
  MPSL_ASSERT(!"Implemented");
  return kErrorOk;
}

Error AstToIR::onContinue(AstContinue* node, Args& out) noexcept {
  // TODO:
  MPSL_ASSERT(!"Implemented");
  return kErrorOk;
}

Error AstToIR::onReturn(AstReturn* node, Args& out) noexcept {
  if (node->hasChild()) {
    Args val(true);
    MPSL_PROPAGATE(onNode(node->getChild(), val));

    uint32_t typeInfo = _retPriv->getTypeInfo();
    uint32_t width = TypeInfo::widthOf(typeInfo);

    IRPair<IRVar> var;
    MPSL_PROPAGATE(asVar(var, val.result, typeInfo));

    IRPair<IRMem> mem;
    MPSL_PROPAGATE(addrOfData(mem, DataSlot(_retPriv->getDataSlot(), _retPriv->getDataOffset()), width));
    MPSL_PROPAGATE(emitStore(mem, var, typeInfo));
  }

  return kErrorOk;
}

Error AstToIR::onVarDecl(AstVarDecl* node, Args& out) noexcept {
  uint32_t typeInfo = node->getTypeInfo();
  IRPair<IRVar> var;

  if (node->hasChild()) {
    Args exp(true);
    MPSL_PROPAGATE(onNode(node->getChild(), exp));
    MPSL_PROPAGATE(asVar(var, exp.result, typeInfo));
  }
  else {
    MPSL_PROPAGATE(newVar(var, typeInfo));
  }

  out.result.set(var);
  return mapVarToAst(node->getSymbol(), var);
}

Error AstToIR::onVarMemb(AstVarMemb* node, Args& out) noexcept {
  AstNode* childNode = node->getChild();
  if (!childNode || !childNode->isVar())
    return MPSL_TRACE_ERROR(kErrorInvalidState);
  AstVar* child = static_cast<AstVar*>(childNode);

  uint32_t typeInfo = node->getTypeInfo();
  uint32_t width = TypeInfo::widthOf(typeInfo);
  return addrOfData(out.result, DataSlot(child->getSymbol()->getDataSlot(), node->getOffset()), width);
}

Error AstToIR::onVar(AstVar* node, Args& out) noexcept {
  AstSymbol* symbol = node->getSymbol();

  if (symbol->getDataSlot() != kInvalidDataSlot) {
    uint32_t typeInfo = node->getTypeInfo();
    uint32_t width = TypeInfo::widthOf(typeInfo);
    return addrOfData(out.result, DataSlot(symbol->getDataSlot(), symbol->getDataOffset()), width);
  }
  else {
    out.result.set(_varMap.get(symbol));
    // Unmapped variable at this stage is a bug in MPSL.
    return out.hasValue() ? kErrorOk : MPSL_TRACE_ERROR(kErrorInvalidState);
  }
}

Error AstToIR::onImm(AstImm* node, Args& out) noexcept {
  return newImm(out.result, node->getValue(), node->getTypeInfo());
}

#define COMBINE_OP_TYPE(op, typeId) (((op) << 8) | ((typeId) << 4))
#define COMBINE_OP_CAST(toId, fromId) (((toId) << 8) | ((fromId) << 4))

Error AstToIR::onUnaryOp(AstUnaryOp* node, Args& out) noexcept {
  if (MPSL_UNLIKELY(!node->hasChild()))
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  Args tmp(true);
  MPSL_PROPAGATE(onNode(node->getChild(), tmp));

  IRBlock* block = getBlock();
  uint32_t op = node->getOp();
  uint32_t typeInfo = node->getTypeInfo();

  IRPair<IRVar> var;
  MPSL_PROPAGATE(asVar(var, tmp.result, typeInfo));

  // Special case for `(x)++` and `++(x)` like operators.
  if (op == kOpPreInc || op == kOpPreDec || op == kOpPostInc || op == kOpPostDec) {
    // If the `value` is in memory then:
    //   1. Dereference it (already done).
    //   2. Perform increment or decrement op.
    //   3. Store back in memory.
    bool preOp = false;
    Value immContent;
    uint32_t instCode = kIRInstIdNone;

    switch (COMBINE_OP_TYPE(op, typeInfo & kTypeIdMask)) {
      case COMBINE_OP_TYPE(kOpPreInc  , kTypeInt):
        preOp = true;
        MPSL_FALLTHROUGH;
      case COMBINE_OP_TYPE(kOpPostInc , kTypeInt):
        instCode = kIRInstIdAddI32;
        immContent.i.set(1);
        break;

      case COMBINE_OP_TYPE(kOpPreDec  , kTypeInt):
        preOp = true;
        MPSL_FALLTHROUGH;
      case COMBINE_OP_TYPE(kOpPostDec , kTypeInt):
        instCode = kIRInstIdAddI32;
        immContent.i.set(-1);
        break;

      case COMBINE_OP_TYPE(kOpPreInc  , kTypeFloat):
        preOp = true;
        MPSL_FALLTHROUGH;
      case COMBINE_OP_TYPE(kOpPostInc , kTypeFloat):
        instCode = kIRInstIdAddF32;
        immContent.f.set(1.0f);
        break;

      case COMBINE_OP_TYPE(kOpPreDec  , kTypeFloat):
        preOp = true;
        MPSL_FALLTHROUGH;
      case COMBINE_OP_TYPE(kOpPostDec , kTypeFloat):
        instCode = kIRInstIdAddF32;
        immContent.f.set(-1.0f);
        break;

      case COMBINE_OP_TYPE(kOpPreInc  , kTypeDouble):
        preOp = true;
        MPSL_FALLTHROUGH;
      case COMBINE_OP_TYPE(kOpPostInc , kTypeDouble):
        instCode = kIRInstIdAddF64;
        immContent.d.set(1.0);
        break;

      case COMBINE_OP_TYPE(kOpPreDec  , kTypeDouble):
        preOp = true;
        MPSL_FALLTHROUGH;
      case COMBINE_OP_TYPE(kOpPostDec , kTypeDouble):
        instCode = kIRInstIdAddF64;
        immContent.d.set(-1.0);
        break;

      default:
        return MPSL_TRACE_ERROR(kErrorInvalidState);
    }

    IRPair<IRObject> imm;
    MPSL_PROPAGATE(newImm(imm, immContent, typeInfo));

    IRPair<IRVar> result;
    if (out.dependsOnResult)
      MPSL_PROPAGATE(newVar(result, typeInfo));

    if (preOp) {
      MPSL_PROPAGATE(emitInst3(instCode, var, var, imm, typeInfo));
      MPSL_PROPAGATE(emitMove(result, var, typeInfo));
      if (tmp.result.lo->isMem())
        MPSL_PROPAGATE(emitStore(reinterpret_cast<IRPair<IRMem>&>(tmp.result), var, typeInfo));
    }
    else {
      MPSL_PROPAGATE(emitMove(result, var, typeInfo));
      MPSL_PROPAGATE(emitInst3(instCode, var, var, imm, typeInfo));
      if (tmp.result.lo->isMem())
        MPSL_PROPAGATE(emitStore(reinterpret_cast<IRPair<IRMem>&>(tmp.result), var, typeInfo));
    }

    return out.result.set(result);
  }
  else {
    uint32_t instCode = kIRInstIdNone;
    if (op == kOpCast) {
      // TODO: Vector casting doesn't work right now.
      uint32_t fromId = node->getChild()->getTypeInfo() & kTypeIdMask;
      switch (COMBINE_OP_CAST(typeInfo & kTypeIdMask, fromId)) {
        case COMBINE_OP_CAST(kTypeFloat , kTypeDouble): instCode = kIRInstIdCvtF64ToF32; break;
        case COMBINE_OP_CAST(kTypeFloat , kTypeInt   ): instCode = kIRInstIdCvtF64ToI32; break;
        case COMBINE_OP_CAST(kTypeDouble, kTypeFloat ): instCode = kIRInstIdCvtF32ToF64; break;
        case COMBINE_OP_CAST(kTypeDouble, kTypeInt   ): instCode = kIRInstIdCvtF32ToI32; break;
        case COMBINE_OP_CAST(kTypeInt   , kTypeFloat ): instCode = kIRInstIdCvtI32ToF32; break;
        case COMBINE_OP_CAST(kTypeInt   , kTypeDouble): instCode = kIRInstIdCvtI32ToF64; break;

        default:
          return MPSL_TRACE_ERROR(kErrorInvalidState);
      }
    }
    else {
      switch (COMBINE_OP_TYPE(op, typeInfo & kTypeIdMask)) {
        case COMBINE_OP_TYPE(kOpBitNeg  , kTypeInt   ): instCode = kIRInstIdBitNegI32; break;
        case COMBINE_OP_TYPE(kOpBitNeg  , kTypeFloat ): instCode = kIRInstIdBitNegF32; break;
        case COMBINE_OP_TYPE(kOpBitNeg  , kTypeDouble): instCode = kIRInstIdBitNegF64; break;

        case COMBINE_OP_TYPE(kOpNeg     , kTypeInt   ): instCode = kIRInstIdNegI32; break;
        case COMBINE_OP_TYPE(kOpNeg     , kTypeFloat ): instCode = kIRInstIdNegF32; break;
        case COMBINE_OP_TYPE(kOpNeg     , kTypeDouble): instCode = kIRInstIdNegF64; break;

        case COMBINE_OP_TYPE(kOpNot     , kTypeInt   ): instCode = kIRInstIdNotI32; break;
        case COMBINE_OP_TYPE(kOpNot     , kTypeFloat ): instCode = kIRInstIdNotF32; break;
        case COMBINE_OP_TYPE(kOpNot     , kTypeDouble): instCode = kIRInstIdNotF64; break;

        case COMBINE_OP_TYPE(kOpPopcnt  , kTypeInt   ): instCode = kIRInstIdPopcntI32; break;
        case COMBINE_OP_TYPE(kOpLzcnt   , kTypeInt   ): instCode = kIRInstIdLzcntI32; break;

        case COMBINE_OP_TYPE(kOpIsNan   , kTypeFloat ): instCode = kIRInstIdIsNanF32; break;
        case COMBINE_OP_TYPE(kOpIsNan   , kTypeDouble): instCode = kIRInstIdIsNanF64; break;

        case COMBINE_OP_TYPE(kOpIsInf   , kTypeFloat ): instCode = kIRInstIdIsInfF32; break;
        case COMBINE_OP_TYPE(kOpIsInf   , kTypeDouble): instCode = kIRInstIdIsInfF64; break;

        case COMBINE_OP_TYPE(kOpIsFinite, kTypeFloat ): instCode = kIRInstIdIsFiniteF32; break;
        case COMBINE_OP_TYPE(kOpIsFinite, kTypeDouble): instCode = kIRInstIdIsFiniteF64; break;

        case COMBINE_OP_TYPE(kOpSignBit , kTypeFloat ): instCode = kIRInstIdSignBitF32; break;
        case COMBINE_OP_TYPE(kOpSignBit , kTypeDouble): instCode = kIRInstIdSignBitF64; break;

        case COMBINE_OP_TYPE(kOpFloor   , kTypeFloat ): instCode = kIRInstIdFloorF32; break;
        case COMBINE_OP_TYPE(kOpFloor   , kTypeDouble): instCode = kIRInstIdFloorF64; break;

        case COMBINE_OP_TYPE(kOpRound   , kTypeFloat ): instCode = kIRInstIdRoundF32; break;
        case COMBINE_OP_TYPE(kOpRound   , kTypeDouble): instCode = kIRInstIdRoundF64; break;

        case COMBINE_OP_TYPE(kOpCeil    , kTypeFloat ): instCode = kIRInstIdCeilF32; break;
        case COMBINE_OP_TYPE(kOpCeil    , kTypeDouble): instCode = kIRInstIdCeilF64; break;

        case COMBINE_OP_TYPE(kOpAbs     , kTypeInt   ): instCode = kIRInstIdAbsI32; break;
        case COMBINE_OP_TYPE(kOpAbs     , kTypeFloat ): instCode = kIRInstIdAbsF32; break;
        case COMBINE_OP_TYPE(kOpAbs     , kTypeDouble): instCode = kIRInstIdAbsF64; break;

        case COMBINE_OP_TYPE(kOpExp     , kTypeFloat ): instCode = kIRInstIdExpF32; break;
        case COMBINE_OP_TYPE(kOpExp     , kTypeDouble): instCode = kIRInstIdExpF64; break;

        case COMBINE_OP_TYPE(kOpLog     , kTypeFloat ): instCode = kIRInstIdLogF32; break;
        case COMBINE_OP_TYPE(kOpLog     , kTypeDouble): instCode = kIRInstIdLogF64; break;

        case COMBINE_OP_TYPE(kOpLog2    , kTypeFloat ): instCode = kIRInstIdLog2F32; break;
        case COMBINE_OP_TYPE(kOpLog2    , kTypeDouble): instCode = kIRInstIdLog2F64; break;

        case COMBINE_OP_TYPE(kOpLog10   , kTypeFloat ): instCode = kIRInstIdLog10F32; break;
        case COMBINE_OP_TYPE(kOpLog10   , kTypeDouble): instCode = kIRInstIdLog10F64; break;

        case COMBINE_OP_TYPE(kOpSqrt    , kTypeFloat ): instCode = kIRInstIdSqrtF32; break;
        case COMBINE_OP_TYPE(kOpSqrt    , kTypeDouble): instCode = kIRInstIdSqrtF64; break;

        case COMBINE_OP_TYPE(kOpSin     , kTypeFloat ): instCode = kIRInstIdSinF32; break;
        case COMBINE_OP_TYPE(kOpSin     , kTypeDouble): instCode = kIRInstIdSinF64; break;

        case COMBINE_OP_TYPE(kOpCos     , kTypeFloat ): instCode = kIRInstIdCosF32; break;
        case COMBINE_OP_TYPE(kOpCos     , kTypeDouble): instCode = kIRInstIdCosF64; break;

        case COMBINE_OP_TYPE(kOpTan     , kTypeFloat ): instCode = kIRInstIdTanF32; break;
        case COMBINE_OP_TYPE(kOpTan     , kTypeDouble): instCode = kIRInstIdTanF64; break;

        case COMBINE_OP_TYPE(kOpAsin    , kTypeFloat ): instCode = kIRInstIdAsinF32; break;
        case COMBINE_OP_TYPE(kOpAsin    , kTypeDouble): instCode = kIRInstIdAsinF64; break;

        case COMBINE_OP_TYPE(kOpAcos    , kTypeFloat ): instCode = kIRInstIdAcosF32; break;
        case COMBINE_OP_TYPE(kOpAcos    , kTypeDouble): instCode = kIRInstIdAcosF64; break;

        case COMBINE_OP_TYPE(kOpAtan    , kTypeFloat ): instCode = kIRInstIdAtanF32; break;
        case COMBINE_OP_TYPE(kOpAtan    , kTypeDouble): instCode = kIRInstIdAtanF64; break;

        default:
          return MPSL_TRACE_ERROR(kErrorInvalidState);
      }
    }

    MPSL_PROPAGATE(newVar(out.result, typeInfo));
    MPSL_PROPAGATE(emitInst2(instCode, out.result, tmp.result, typeInfo));
  }

  return kErrorOk;
}

Error AstToIR::onBinaryOp(AstBinaryOp* node, Args& out) noexcept {
  if (MPSL_UNLIKELY(!node->hasLeft() || !node->hasRight()))
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  Args lValue(true);
  Args rValue(true);

  MPSL_PROPAGATE(onNode(node->getLeft(), lValue));
  MPSL_PROPAGATE(onNode(node->getRight(), rValue));

  uint32_t op = node->getOp();
  uint32_t typeInfo = node->getTypeInfo();
  uint32_t instCode = kIRInstIdNone;

  switch (COMBINE_OP_TYPE(op, typeInfo & kTypeIdMask)) {
    case COMBINE_OP_TYPE(kOpEq          , kTypeInt   ): instCode = kIRInstIdCmpEqI32; break;
    case COMBINE_OP_TYPE(kOpEq          , kTypeFloat ): instCode = kIRInstIdCmpEqF32; break;
    case COMBINE_OP_TYPE(kOpEq          , kTypeDouble): instCode = kIRInstIdCmpEqF64; break;
    case COMBINE_OP_TYPE(kOpNe          , kTypeInt   ): instCode = kIRInstIdCmpNeI32; break;
    case COMBINE_OP_TYPE(kOpNe          , kTypeFloat ): instCode = kIRInstIdCmpNeF32; break;
    case COMBINE_OP_TYPE(kOpNe          , kTypeDouble): instCode = kIRInstIdCmpNeF64; break;
    case COMBINE_OP_TYPE(kOpLt          , kTypeInt   ): instCode = kIRInstIdCmpLtI32; break;
    case COMBINE_OP_TYPE(kOpLt          , kTypeFloat ): instCode = kIRInstIdCmpLtF32; break;
    case COMBINE_OP_TYPE(kOpLt          , kTypeDouble): instCode = kIRInstIdCmpLtF64; break;
    case COMBINE_OP_TYPE(kOpLe          , kTypeInt   ): instCode = kIRInstIdCmpLeI32; break;
    case COMBINE_OP_TYPE(kOpLe          , kTypeFloat ): instCode = kIRInstIdCmpLeF32; break;
    case COMBINE_OP_TYPE(kOpLe          , kTypeDouble): instCode = kIRInstIdCmpLeF64; break;
    case COMBINE_OP_TYPE(kOpGt          , kTypeInt   ): instCode = kIRInstIdCmpGtI32; break;
    case COMBINE_OP_TYPE(kOpGt          , kTypeFloat ): instCode = kIRInstIdCmpGtF32; break;
    case COMBINE_OP_TYPE(kOpGt          , kTypeDouble): instCode = kIRInstIdCmpGtF64; break;
    case COMBINE_OP_TYPE(kOpGe          , kTypeInt   ): instCode = kIRInstIdCmpGeI32; break;
    case COMBINE_OP_TYPE(kOpGe          , kTypeFloat ): instCode = kIRInstIdCmpGeF32; break;
    case COMBINE_OP_TYPE(kOpGe          , kTypeDouble): instCode = kIRInstIdCmpGeF64; break;

    case COMBINE_OP_TYPE(kOpAdd         , kTypeInt   ): instCode = kIRInstIdAddI32; break;
    case COMBINE_OP_TYPE(kOpAdd         , kTypeFloat ): instCode = kIRInstIdAddF32; break;
    case COMBINE_OP_TYPE(kOpAdd         , kTypeDouble): instCode = kIRInstIdAddF64; break;
    case COMBINE_OP_TYPE(kOpSub         , kTypeInt   ): instCode = kIRInstIdSubI32; break;
    case COMBINE_OP_TYPE(kOpSub         , kTypeFloat ): instCode = kIRInstIdSubF32; break;
    case COMBINE_OP_TYPE(kOpSub         , kTypeDouble): instCode = kIRInstIdSubF64; break;
    case COMBINE_OP_TYPE(kOpMul         , kTypeInt   ): instCode = kIRInstIdMulI32; break;
    case COMBINE_OP_TYPE(kOpMul         , kTypeFloat ): instCode = kIRInstIdMulF32; break;
    case COMBINE_OP_TYPE(kOpMul         , kTypeDouble): instCode = kIRInstIdMulF64; break;
    case COMBINE_OP_TYPE(kOpDiv         , kTypeInt   ): instCode = kIRInstIdDivI32; break;
    case COMBINE_OP_TYPE(kOpDiv         , kTypeFloat ): instCode = kIRInstIdDivF32; break;
    case COMBINE_OP_TYPE(kOpDiv         , kTypeDouble): instCode = kIRInstIdDivF64; break;
    case COMBINE_OP_TYPE(kOpMod         , kTypeInt   ): instCode = kIRInstIdModI32; break;
    case COMBINE_OP_TYPE(kOpMod         , kTypeFloat ): instCode = kIRInstIdModF32; break;
    case COMBINE_OP_TYPE(kOpMod         , kTypeDouble): instCode = kIRInstIdModF64; break;

    case COMBINE_OP_TYPE(kOpBitAnd      , kTypeInt   ): instCode = kIRInstIdAndI32; break;
    case COMBINE_OP_TYPE(kOpBitAnd      , kTypeFloat ): instCode = kIRInstIdAndF32; break;
    case COMBINE_OP_TYPE(kOpBitAnd      , kTypeDouble): instCode = kIRInstIdAndF64; break;
    case COMBINE_OP_TYPE(kOpBitOr       , kTypeInt   ): instCode = kIRInstIdOrI32; break;
    case COMBINE_OP_TYPE(kOpBitOr       , kTypeFloat ): instCode = kIRInstIdOrF32; break;
    case COMBINE_OP_TYPE(kOpBitOr       , kTypeDouble): instCode = kIRInstIdOrF64; break;
    case COMBINE_OP_TYPE(kOpBitXor      , kTypeInt   ): instCode = kIRInstIdXorI32; break;
    case COMBINE_OP_TYPE(kOpBitXor      , kTypeFloat ): instCode = kIRInstIdXorF32; break;
    case COMBINE_OP_TYPE(kOpBitXor      , kTypeDouble): instCode = kIRInstIdXorF64; break;

    case COMBINE_OP_TYPE(kOpBitSar      , kTypeInt   ): instCode = kIRInstIdSarI32; break;
    case COMBINE_OP_TYPE(kOpBitShr      , kTypeInt   ): instCode = kIRInstIdShrI32; break;
    case COMBINE_OP_TYPE(kOpBitShl      , kTypeInt   ): instCode = kIRInstIdShlI32; break;
    case COMBINE_OP_TYPE(kOpBitRor      , kTypeInt   ): instCode = kIRInstIdRorI32; break;
    case COMBINE_OP_TYPE(kOpBitRol      , kTypeInt   ): instCode = kIRInstIdRolI32; break;

    case COMBINE_OP_TYPE(kOpMin         , kTypeInt   ): instCode = kIRInstIdMinI32; break;
    case COMBINE_OP_TYPE(kOpMin         , kTypeFloat ): instCode = kIRInstIdMinF32; break;
    case COMBINE_OP_TYPE(kOpMin         , kTypeDouble): instCode = kIRInstIdMinF64; break;

    case COMBINE_OP_TYPE(kOpMax         , kTypeInt   ): instCode = kIRInstIdMaxI32; break;
    case COMBINE_OP_TYPE(kOpMax         , kTypeFloat ): instCode = kIRInstIdMaxF32; break;
    case COMBINE_OP_TYPE(kOpMax         , kTypeDouble): instCode = kIRInstIdMaxF64; break;

    case COMBINE_OP_TYPE(kOpPow         , kTypeFloat ): instCode = kIRInstIdPowF32; break;
    case COMBINE_OP_TYPE(kOpPow         , kTypeDouble): instCode = kIRInstIdPowF64; break;
    case COMBINE_OP_TYPE(kOpAtan2       , kTypeFloat ): instCode = kIRInstIdAtan2F32; break;
    case COMBINE_OP_TYPE(kOpAtan2       , kTypeDouble): instCode = kIRInstIdAtan2F64; break;
    case COMBINE_OP_TYPE(kOpCopySign    , kTypeFloat ): instCode = kIRInstIdCopySignF32; break;
    case COMBINE_OP_TYPE(kOpCopySign    , kTypeDouble): instCode = kIRInstIdCopySignF64; break;

    case COMBINE_OP_TYPE(kOpAssign      , kTypeInt   ): break;
    case COMBINE_OP_TYPE(kOpAssign      , kTypeFloat ): break;
    case COMBINE_OP_TYPE(kOpAssign      , kTypeDouble): break;

    case COMBINE_OP_TYPE(kOpAssignAdd   , kTypeInt   ): instCode = kIRInstIdAddI32; break;
    case COMBINE_OP_TYPE(kOpAssignAdd   , kTypeFloat ): instCode = kIRInstIdAddF32; break;
    case COMBINE_OP_TYPE(kOpAssignAdd   , kTypeDouble): instCode = kIRInstIdAddF64; break;
    case COMBINE_OP_TYPE(kOpAssignSub   , kTypeInt   ): instCode = kIRInstIdSubI32; break;
    case COMBINE_OP_TYPE(kOpAssignSub   , kTypeFloat ): instCode = kIRInstIdSubF32; break;
    case COMBINE_OP_TYPE(kOpAssignSub   , kTypeDouble): instCode = kIRInstIdSubF64; break;
    case COMBINE_OP_TYPE(kOpAssignMul   , kTypeInt   ): instCode = kIRInstIdMulI32; break;
    case COMBINE_OP_TYPE(kOpAssignMul   , kTypeFloat ): instCode = kIRInstIdMulF32; break;
    case COMBINE_OP_TYPE(kOpAssignMul   , kTypeDouble): instCode = kIRInstIdMulF64; break;
    case COMBINE_OP_TYPE(kOpAssignDiv   , kTypeInt   ): instCode = kIRInstIdDivI32; break;
    case COMBINE_OP_TYPE(kOpAssignDiv   , kTypeFloat ): instCode = kIRInstIdDivF32; break;
    case COMBINE_OP_TYPE(kOpAssignDiv   , kTypeDouble): instCode = kIRInstIdDivF64; break;
    case COMBINE_OP_TYPE(kOpAssignMod   , kTypeInt   ): instCode = kIRInstIdModI32; break;
    case COMBINE_OP_TYPE(kOpAssignMod   , kTypeFloat ): instCode = kIRInstIdModF32; break;
    case COMBINE_OP_TYPE(kOpAssignMod   , kTypeDouble): instCode = kIRInstIdModF64; break;

    case COMBINE_OP_TYPE(kOpAssignBitAnd, kTypeInt   ): instCode = kIRInstIdAndI32; break;
    case COMBINE_OP_TYPE(kOpAssignBitAnd, kTypeFloat ): instCode = kIRInstIdAndF32; break;
    case COMBINE_OP_TYPE(kOpAssignBitAnd, kTypeDouble): instCode = kIRInstIdAndF64; break;
    case COMBINE_OP_TYPE(kOpAssignBitOr , kTypeInt   ): instCode = kIRInstIdOrI32; break;
    case COMBINE_OP_TYPE(kOpAssignBitOr , kTypeFloat ): instCode = kIRInstIdOrF32; break;
    case COMBINE_OP_TYPE(kOpAssignBitOr , kTypeDouble): instCode = kIRInstIdOrF64; break;
    case COMBINE_OP_TYPE(kOpAssignBitXor, kTypeInt   ): instCode = kIRInstIdXorI32; break;
    case COMBINE_OP_TYPE(kOpAssignBitXor, kTypeFloat ): instCode = kIRInstIdXorF32; break;
    case COMBINE_OP_TYPE(kOpAssignBitXor, kTypeDouble): instCode = kIRInstIdXorF64; break;

    case COMBINE_OP_TYPE(kOpAssignBitSar, kTypeInt   ): instCode = kIRInstIdSarI32; break;
    case COMBINE_OP_TYPE(kOpAssignBitShr, kTypeInt   ): instCode = kIRInstIdShrI32; break;
    case COMBINE_OP_TYPE(kOpAssignBitShl, kTypeInt   ): instCode = kIRInstIdShlI32; break;

    case COMBINE_OP_TYPE(kOpVaddb       , kTypeInt   ): instCode = kIRInstIdVaddb; break;
    case COMBINE_OP_TYPE(kOpVaddw       , kTypeInt   ): instCode = kIRInstIdVaddw; break;
    case COMBINE_OP_TYPE(kOpVaddd       , kTypeInt   ): instCode = kIRInstIdVaddd; break;
    case COMBINE_OP_TYPE(kOpVaddq       , kTypeInt   ): instCode = kIRInstIdVaddq; break;
    case COMBINE_OP_TYPE(kOpVaddssb     , kTypeInt   ): instCode = kIRInstIdVaddssb; break;
    case COMBINE_OP_TYPE(kOpVaddusb     , kTypeInt   ): instCode = kIRInstIdVaddusb; break;
    case COMBINE_OP_TYPE(kOpVaddssw     , kTypeInt   ): instCode = kIRInstIdVaddssw; break;
    case COMBINE_OP_TYPE(kOpVaddusw     , kTypeInt   ): instCode = kIRInstIdVaddusw; break;
    case COMBINE_OP_TYPE(kOpVsubb       , kTypeInt   ): instCode = kIRInstIdVsubb; break;
    case COMBINE_OP_TYPE(kOpVsubw       , kTypeInt   ): instCode = kIRInstIdVsubw; break;
    case COMBINE_OP_TYPE(kOpVsubd       , kTypeInt   ): instCode = kIRInstIdVsubd; break;
    case COMBINE_OP_TYPE(kOpVsubq       , kTypeInt   ): instCode = kIRInstIdVsubq; break;
    case COMBINE_OP_TYPE(kOpVsubssb     , kTypeInt   ): instCode = kIRInstIdVsubssb; break;
    case COMBINE_OP_TYPE(kOpVsubusb     , kTypeInt   ): instCode = kIRInstIdVsubusb; break;
    case COMBINE_OP_TYPE(kOpVsubssw     , kTypeInt   ): instCode = kIRInstIdVsubssw; break;
    case COMBINE_OP_TYPE(kOpVsubusw     , kTypeInt   ): instCode = kIRInstIdVsubusw; break;
    case COMBINE_OP_TYPE(kOpVmulw       , kTypeInt   ): instCode = kIRInstIdVmulw; break;
    case COMBINE_OP_TYPE(kOpVmulhsw     , kTypeInt   ): instCode = kIRInstIdVmulhsw; break;
    case COMBINE_OP_TYPE(kOpVmulhuw     , kTypeInt   ): instCode = kIRInstIdVmulhuw; break;
    case COMBINE_OP_TYPE(kOpVmuld       , kTypeInt   ): instCode = kIRInstIdVmuld; break;
    case COMBINE_OP_TYPE(kOpVminsb      , kTypeInt   ): instCode = kIRInstIdVminsb; break;
    case COMBINE_OP_TYPE(kOpVminub      , kTypeInt   ): instCode = kIRInstIdVminub; break;
    case COMBINE_OP_TYPE(kOpVminsw      , kTypeInt   ): instCode = kIRInstIdVminsw; break;
    case COMBINE_OP_TYPE(kOpVminuw      , kTypeInt   ): instCode = kIRInstIdVminuw; break;
    case COMBINE_OP_TYPE(kOpVminsd      , kTypeInt   ): instCode = kIRInstIdVminsd; break;
    case COMBINE_OP_TYPE(kOpVminud      , kTypeInt   ): instCode = kIRInstIdVminud; break;
    case COMBINE_OP_TYPE(kOpVmaxsb      , kTypeInt   ): instCode = kIRInstIdVmaxsb; break;
    case COMBINE_OP_TYPE(kOpVmaxub      , kTypeInt   ): instCode = kIRInstIdVmaxub; break;
    case COMBINE_OP_TYPE(kOpVmaxsw      , kTypeInt   ): instCode = kIRInstIdVmaxsw; break;
    case COMBINE_OP_TYPE(kOpVmaxuw      , kTypeInt   ): instCode = kIRInstIdVmaxuw; break;
    case COMBINE_OP_TYPE(kOpVmaxsd      , kTypeInt   ): instCode = kIRInstIdVmaxsd; break;
    case COMBINE_OP_TYPE(kOpVmaxud      , kTypeInt   ): instCode = kIRInstIdVmaxud; break;
    case COMBINE_OP_TYPE(kOpVsllw       , kTypeInt   ): instCode = kIRInstIdVsllw; break;
    case COMBINE_OP_TYPE(kOpVsrlw       , kTypeInt   ): instCode = kIRInstIdVsrlw; break;
    case COMBINE_OP_TYPE(kOpVsraw       , kTypeInt   ): instCode = kIRInstIdVsraw; break;
    case COMBINE_OP_TYPE(kOpVslld       , kTypeInt   ): instCode = kIRInstIdVslld; break;
    case COMBINE_OP_TYPE(kOpVsrld       , kTypeInt   ): instCode = kIRInstIdVsrld; break;
    case COMBINE_OP_TYPE(kOpVsrad       , kTypeInt   ): instCode = kIRInstIdVsrad; break;
    case COMBINE_OP_TYPE(kOpVsllq       , kTypeInt   ): instCode = kIRInstIdVsllq; break;
    case COMBINE_OP_TYPE(kOpVsrlq       , kTypeInt   ): instCode = kIRInstIdVsrlq; break;
    case COMBINE_OP_TYPE(kOpVcmpeqb     , kTypeInt   ): instCode = kIRInstIdVcmpeqb; break;
    case COMBINE_OP_TYPE(kOpVcmpeqw     , kTypeInt   ): instCode = kIRInstIdVcmpeqw; break;
    case COMBINE_OP_TYPE(kOpVcmpeqd     , kTypeInt   ): instCode = kIRInstIdCmpEqI32; break;
    case COMBINE_OP_TYPE(kOpVcmpgtb     , kTypeInt   ): instCode = kIRInstIdVcmpgtb; break;
    case COMBINE_OP_TYPE(kOpVcmpgtw     , kTypeInt   ): instCode = kIRInstIdVcmpgtw; break;
    case COMBINE_OP_TYPE(kOpVcmpgtd     , kTypeInt   ): instCode = kIRInstIdCmpGtI32; break;
    default:
      return MPSL_TRACE_ERROR(kErrorInvalidState);
  }

  IRPair<IRVar> result, lVar, rVar;
  MPSL_PROPAGATE(newVar(result, typeInfo));

  if (OpInfo::get(op).isAssignment()) {
    if (op == kOpAssign) {
      MPSL_PROPAGATE(asVar(rVar, rValue.result, typeInfo));

      if (lValue.result.lo->isMem())
        MPSL_PROPAGATE(emitStore(lValue.result, rVar, typeInfo));
      else
        MPSL_PROPAGATE(emitMove(lVar, rVar, typeInfo));

      if (out.dependsOnResult) {
        MPSL_PROPAGATE(emitMove(result, rVar, typeInfo));
        out.result.set(result);
      }
    }
    else {
      MPSL_PROPAGATE(asVar(lVar, lValue.result, typeInfo));
      MPSL_PROPAGATE(asVar(rVar, rValue.result, typeInfo));
      MPSL_PROPAGATE(emitInst3(instCode, lVar, lVar, rVar, typeInfo));

      if (lValue.result.lo->isMem()) {
        MPSL_PROPAGATE(emitStore(lValue.result, lVar, typeInfo));
        out.result.set(lVar);
      }
      else {
        MPSL_PROPAGATE(emitMove(result, lVar, typeInfo));
        out.result.set(result);
      }
    }
  }
  else {
    MPSL_PROPAGATE(asVar(lVar, lValue.result, typeInfo));
    MPSL_PROPAGATE(asVar(rVar, rValue.result, typeInfo));
    MPSL_PROPAGATE(emitInst3(instCode, result, lVar, rVar, typeInfo));
    out.result.set(result);
  }

  return kErrorOk;
}

#undef COMBINE_OP_TYPE
#undef COMBINE_OP_CAST

Error AstToIR::onCall(AstCall* node, Args& out) noexcept {
  // TODO:
  MPSL_ASSERT(!"Implemented");
  return kErrorOk;
}

// ============================================================================
// [mpsl::AstToIR - Utilities]
// ============================================================================

Error AstToIR::mapVarToAst(AstSymbol* sym, IRPair<IRVar> var) noexcept {
  MPSL_ASSERT(_varMap.has(sym));
  return _varMap.put(sym, var);
}

Error AstToIR::newVar(IRPair<IRObject>& dst, uint32_t typeInfo) noexcept {
  uint32_t width = TypeInfo::widthOf(typeInfo);

  IRVar* lo = nullptr;
  IRVar* hi = nullptr;

  if (needSplit(width)) {
    uint32_t loTI, hiTI;
    mpSplitTypeInfo(loTI, hiTI, typeInfo);

    lo = getIR()->newVarByTypeInfo(loTI);
    MPSL_NULLCHECK(lo);
    hi = getIR()->newVarByTypeInfo(hiTI);
    MPSL_NULLCHECK(hi);
  }
  else {
    lo = getIR()->newVarByTypeInfo(typeInfo);
    MPSL_NULLCHECK(lo);
  }

  return dst.set(lo, hi);
}

Error AstToIR::newImm(IRPair<IRObject>& dst, const Value& value, uint32_t typeInfo) noexcept {
  uint32_t width = TypeInfo::widthOf(typeInfo);
  if (width > 16 && !hasV256()) {
    uint32_t loTI, hiTI;
    mpSplitTypeInfo(loTI, hiTI, typeInfo);

    if (mpIsValueLoHiEqual(value)) {
      IRImm* imm = getIR()->newImmByTypeInfo(value, loTI);
      MPSL_NULLCHECK(imm);
      return dst.set(imm, imm);
    }
    else {
      Value loVal, hiVal;
      loVal.q.set(value.q[0], value.q[1], 0, 0);
      hiVal.q.set(value.q[2], value.q[3], 0, 0);

      IRImm* loImm = getIR()->newImmByTypeInfo(loVal, loTI);
      MPSL_NULLCHECK(loImm);

      IRImm* hiImm = getIR()->newImmByTypeInfo(hiVal, hiTI);
      MPSL_NULLCHECK(hiImm);

      return dst.set(loImm, hiImm);
    }
  }
  else {
    IRImm* imm = getIR()->newImmByTypeInfo(value, typeInfo);
    MPSL_NULLCHECK(imm);
    return dst.set(imm, nullptr);
  }
}

Error AstToIR::addrOfData(IRPair<IRObject>& dst, DataSlot data, uint32_t width) noexcept {
  IRVar* base = getIR()->getDataPtr(data.slot);

  IRMem* lo = nullptr;
  IRMem* hi = nullptr;

  if (width > 16 && !hasV256()) {
    lo = getIR()->newMem(base, nullptr, data.offset);
    hi = getIR()->newMem(base, nullptr, data.offset + 16);

    MPSL_NULLCHECK(lo);
    MPSL_NULLCHECK(hi);
  }
  else {
    lo = getIR()->newMem(base, nullptr, data.offset);
    MPSL_NULLCHECK(lo);
  }

  dst.set(lo, hi);
  return kErrorOk;
}

Error AstToIR::asVar(IRPair<IRVar>& out, IRPair<IRObject> in, uint32_t typeInfo) noexcept {
  if (in.lo == nullptr && in.hi == nullptr)
    return out.set(nullptr, nullptr);

  uint32_t ti[2] = { typeInfo, kTypeVoid };
  uint32_t width = TypeInfo::widthOf(typeInfo);

  if (width > 16 && !hasV256())
    mpSplitTypeInfo(ti[0], ti[1], typeInfo);

  for (uint32_t i = 0; i < 2; i++) {
    IRObject* inObj = in.obj[i];

    if (!inObj) {
      out.obj[i] = nullptr;
      continue;
    }

    switch (inObj->getObjectType()) {
      case kIRObjectVar: {
        out.obj[i] = static_cast<IRVar*>(inObj);
        break;
      }

      case kIRObjectMem: {
        typeInfo = ti[i];

        IRMem* mem = static_cast<IRMem*>(inObj);
        IRVar* var = getIR()->newVarByTypeInfo(typeInfo);

        MPSL_NULLCHECK(var);
        MPSL_PROPAGATE(emitFetchX(var, mem, typeInfo));

        out.obj[i] = var;
        break;
      }

      case kIRObjectImm: {
        IRImm* imm = static_cast<IRImm*>(inObj);
        IRVar* var = getIR()->newVarByTypeInfo(typeInfo);

        MPSL_NULLCHECK(var);
        MPSL_PROPAGATE(getIR()->emitFetch(getBlock(), var, imm));

        out.obj[i] = var;
        break;
      }

    }
  }

  return kErrorOk;
}

Error AstToIR::emitMove(IRPair<IRVar> dst, IRPair<IRVar> src, uint32_t typeInfo) noexcept {
  uint32_t width = TypeInfo::widthOf(typeInfo);
  IRBlock* block = getBlock();

  if (dst.lo) MPSL_PROPAGATE(getIR()->emitMove(block, dst.lo, src.lo));
  if (dst.hi) MPSL_PROPAGATE(getIR()->emitMove(block, dst.hi, src.hi));

  return kErrorOk;
}

Error AstToIR::emitStore(IRPair<IRObject> dst, IRPair<IRVar> src, uint32_t typeInfo) noexcept {
  uint32_t width = TypeInfo::widthOf(typeInfo);

  if (needSplit(width)) {
    uint32_t loTI, hiTI;
    mpSplitTypeInfo(loTI, hiTI, typeInfo);

    if (src.lo) MPSL_PROPAGATE(emitStoreX(static_cast<IRMem*>(dst.lo), src.lo, loTI));
    if (src.hi) MPSL_PROPAGATE(emitStoreX(static_cast<IRMem*>(dst.hi), src.hi, hiTI));

    return kErrorOk;
  }
  else {
    return emitStoreX(static_cast<IRMem*>(dst.lo), src.lo, typeInfo);
  }
}

Error AstToIR::emitInst2(uint32_t instCode,
  IRPair<IRObject> o0,
  IRPair<IRObject> o1, uint32_t typeInfo) noexcept {

  uint32_t width = TypeInfo::widthOf(typeInfo);
  IRBlock* block = getBlock();

  if (needSplit(width)) {
    uint32_t loTI, hiTI;
    mpSplitTypeInfo(loTI, hiTI, typeInfo);

    MPSL_PROPAGATE(getIR()->emitInst(block, instCode | mpGetSIMDFlags(loTI), o0.lo, o1.lo));
    MPSL_PROPAGATE(getIR()->emitInst(block, instCode | mpGetSIMDFlags(hiTI), o0.hi, o1.hi));
    return kErrorOk;
  }
  else {
    return getIR()->emitInst(block, instCode | mpGetSIMDFlags(typeInfo), o0.lo, o1.lo);
  }
}

Error AstToIR::emitInst3(uint32_t instCode,
  IRPair<IRObject> o0,
  IRPair<IRObject> o1,
  IRPair<IRObject> o2, uint32_t typeInfo) noexcept {

  uint32_t width = TypeInfo::widthOf(typeInfo);
  IRBlock* block = getBlock();

  if (needSplit(width)) {
    uint32_t loTI, hiTI;
    mpSplitTypeInfo(loTI, hiTI, typeInfo);

    MPSL_PROPAGATE(getIR()->emitInst(block, instCode | mpGetSIMDFlags(loTI), o0.lo, o1.lo, o2.lo));
    MPSL_PROPAGATE(getIR()->emitInst(block, instCode | mpGetSIMDFlags(hiTI), o0.hi, o1.hi, o2.hi));
    return kErrorOk;
  }
  else {
    return getIR()->emitInst(block, instCode | mpGetSIMDFlags(typeInfo), o0.lo, o1.lo, o2.lo);
  }
}

Error AstToIR::emitFetchX(IRVar* dst, IRMem* src, uint32_t typeInfo) noexcept {
  uint32_t instCode = kIRInstIdNone;
  switch (typeInfo & (kTypeIdMask | kTypeVecMask)) {
    case kTypeBool   : instCode = kIRInstIdFetch32; break;
    case kTypeBool1  : instCode = kIRInstIdFetch32; break;
    case kTypeBool2  : instCode = kIRInstIdFetch64; break;
    case kTypeBool3  : instCode = kIRInstIdFetch96; break;
    case kTypeBool4  : instCode = kIRInstIdFetch128; break;
    case kTypeBool8  : instCode = kIRInstIdFetch256; break;

    case kTypeInt    : instCode = kIRInstIdFetch32; break;
    case kTypeInt1   : instCode = kIRInstIdFetch32; break;
    case kTypeInt2   : instCode = kIRInstIdFetch64; break;
    case kTypeInt3   : instCode = kIRInstIdFetch96; break;
    case kTypeInt4   : instCode = kIRInstIdFetch128; break;
    case kTypeInt8   : instCode = kIRInstIdFetch256; break;

    case kTypeFloat  : instCode = kIRInstIdFetch32; break;
    case kTypeFloat1 : instCode = kIRInstIdFetch32; break;
    case kTypeFloat2 : instCode = kIRInstIdFetch64; break;
    case kTypeFloat3 : instCode = kIRInstIdFetch96; break;
    case kTypeFloat4 : instCode = kIRInstIdFetch128; break;
    case kTypeFloat8 : instCode = kIRInstIdFetch256; break;

    case kTypeQBool  : instCode = kIRInstIdFetch64; break;
    case kTypeQBool1 : instCode = kIRInstIdFetch64; break;
    case kTypeQBool2 : instCode = kIRInstIdFetch128; break;
    case kTypeQBool3 : instCode = kIRInstIdFetch192; break;
    case kTypeQBool4 : instCode = kIRInstIdFetch256; break;

    case kTypeDouble : instCode = kIRInstIdFetch64; break;
    case kTypeDouble1: instCode = kIRInstIdFetch64; break;
    case kTypeDouble2: instCode = kIRInstIdFetch128; break;
    case kTypeDouble3: instCode = kIRInstIdFetch192; break;
    case kTypeDouble4: instCode = kIRInstIdFetch256; break;

    default:
      return MPSL_TRACE_ERROR(kErrorInvalidState);
  }

  return getIR()->emitInst(getBlock(), instCode, dst, src);
}


Error AstToIR::emitStoreX(IRMem* dst, IRVar* src, uint32_t typeInfo) noexcept {
  uint32_t instCode = kIRInstIdNone;
  switch (typeInfo & (kTypeIdMask | kTypeVecMask)) {
    case kTypeBool   : instCode = kIRInstIdStore32; break;
    case kTypeBool1  : instCode = kIRInstIdStore32; break;
    case kTypeBool2  : instCode = kIRInstIdStore64; break;
    case kTypeBool3  : instCode = kIRInstIdStore96; break;
    case kTypeBool4  : instCode = kIRInstIdStore128; break;
    case kTypeBool8  : instCode = kIRInstIdStore256; break;

    case kTypeInt    : instCode = kIRInstIdStore32; break;
    case kTypeInt1   : instCode = kIRInstIdStore32; break;
    case kTypeInt2   : instCode = kIRInstIdStore64; break;
    case kTypeInt3   : instCode = kIRInstIdStore96; break;
    case kTypeInt4   : instCode = kIRInstIdStore128; break;
    case kTypeInt8   : instCode = kIRInstIdStore256; break;

    case kTypeFloat  : instCode = kIRInstIdStore32; break;
    case kTypeFloat1 : instCode = kIRInstIdStore32; break;
    case kTypeFloat2 : instCode = kIRInstIdStore64; break;
    case kTypeFloat3 : instCode = kIRInstIdStore96; break;
    case kTypeFloat4 : instCode = kIRInstIdStore128; break;
    case kTypeFloat8 : instCode = kIRInstIdStore256; break;

    case kTypeQBool  : instCode = kIRInstIdStore64; break;
    case kTypeQBool1 : instCode = kIRInstIdStore64; break;
    case kTypeQBool2 : instCode = kIRInstIdStore128; break;
    case kTypeQBool3 : instCode = kIRInstIdStore192; break;
    case kTypeQBool4 : instCode = kIRInstIdStore256; break;

    case kTypeDouble : instCode = kIRInstIdStore64; break;
    case kTypeDouble1: instCode = kIRInstIdStore64; break;
    case kTypeDouble2: instCode = kIRInstIdStore128; break;
    case kTypeDouble3: instCode = kIRInstIdStore192; break;
    case kTypeDouble4: instCode = kIRInstIdStore256; break;

    default:
      return MPSL_TRACE_ERROR(kErrorInvalidState);
  }

  MPSL_PROPAGATE(getIR()->emitInst(getBlock(), instCode, dst, src));
  return kErrorOk;
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
