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

static MPSL_INLINE uint32_t mpGetVecFlags(uint32_t typeInfo) noexcept {
  if ((typeInfo & kTypeVecMask) < kTypeVec2)
    return 0;
  else
    return TypeInfo::widthOf(typeInfo) <= 16 ? kInstVec128 : kInstVec256;
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
    _block(nullptr),
    _functionLevel(0),
    _hasV256(false),
    _hiddenRet(nullptr),
    _currentRet(),
    _nestedFunctions(ir->getHeap()),
    _varMap(ir->getHeap()),
    _memMap(ir->getHeap()) {
  _hiddenRet = ast->getGlobalScope()->resolveSymbol(StringRef("@ret", 4));
}
AstToIR::~AstToIR() noexcept {}

// ============================================================================
// [mpsl::AstToIR - OnNode]
// ============================================================================

Error AstToIR::onProgram(AstProgram* node, Args& out) noexcept {
  AstNode** children = node->getChildren();
  uint32_t i, count = node->getLength();

  // Find the "main()" function and use it as an entry point.
  for (i = 0; i < count; i++) {
    AstNode* child = children[i];
    if (child->getNodeType() == AstNode::kTypeFunction) {
      AstFunction* func = static_cast<AstFunction*>(child);
      if (func->getFunc()->eq("main", 4)) {
        MPSL_PROPAGATE(_ir->initEntryBlock());
        _block = _ir->getMainBlock();
        return onFunction(func, out);
      }
    }
  }

  return MPSL_TRACE_ERROR(kErrorNoEntryPoint);
}

// NOTE: This is only called once per "main()". Other functions are simply
// inlined during the AST to IR translation.
Error AstToIR::onFunction(AstFunction* node, Args& out) noexcept {
  if (node->hasBody()) {
    MPSL_PROPAGATE(_nestedFunctions.put(node));
    MPSL_PROPAGATE(onNode(node->getBody(), out));
  }

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

    uint32_t typeInfo = _hiddenRet->getTypeInfo();
    uint32_t width = TypeInfo::widthOf(typeInfo);

    if (_functionLevel == 0) {
      IRPair<IRVar> var;
      MPSL_PROPAGATE(asVar(var, val.result, typeInfo));

      IRPair<IRMem> mem;
      MPSL_PROPAGATE(addrOfData(mem, DataSlot(_hiddenRet->getDataSlot(), _hiddenRet->getDataOffset()), width));
      MPSL_PROPAGATE(emitStore(mem, var, typeInfo));
    }
    else {
      _currentRet = val.result;
    }
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
    return out.hasValue() ? static_cast<Error>(kErrorOk) : MPSL_TRACE_ERROR(kErrorInvalidState);
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
  uint32_t typeInfo = node->getTypeInfo();
  const OpInfo& op = OpInfo::get(node->getOp());

  IRPair<IRVar> var;
  MPSL_PROPAGATE(asVar(var, tmp.result, typeInfo));

  // Special case for unary assignment - `(x)++` and `++(x)` like operators.
  if (op.isAssignment()) {
    // If the `value` is in memory then:
    //   1. Dereference it (already done).
    //   2. Perform increment or decrement op.
    //   3. Store back in memory.
    Value immContent;
    uint32_t instCode = kInstCodeNone;

    switch (typeInfo & kTypeIdMask) {
      case kTypeBool  : instCode = op.insti    ; break;
      case kTypeInt   : instCode = op.insti    ; immContent.i.set(1   ); break;
      case kTypeFloat : instCode = op.instf    ; immContent.f.set(1.0f); break;
      case kTypeDouble: instCode = op.instf + 1; immContent.d.set(1.0 ); break;
      default:
        return MPSL_TRACE_ERROR(kErrorInvalidState);
    }

    IRPair<IRObject> imm;
    MPSL_PROPAGATE(newImm(imm, immContent, typeInfo));

    IRPair<IRVar> result;
    if (out.dependsOnResult)
      MPSL_PROPAGATE(newVar(result, typeInfo));

    if (op.isPostAssignment()) {
      MPSL_PROPAGATE(emitMove(result, var, typeInfo));
      MPSL_PROPAGATE(emitInst3(instCode, var, var, imm, typeInfo));
      if (tmp.result.lo->isMem())
        MPSL_PROPAGATE(emitStore(reinterpret_cast<IRPair<IRMem>&>(tmp.result), var, typeInfo));
    }
    else {
      MPSL_PROPAGATE(emitInst3(instCode, var, var, imm, typeInfo));
      MPSL_PROPAGATE(emitMove(result, var, typeInfo));
      if (tmp.result.lo->isMem())
        MPSL_PROPAGATE(emitStore(reinterpret_cast<IRPair<IRMem>&>(tmp.result), var, typeInfo));
    }

    return out.result.set(result);
  }
  else {
    MPSL_PROPAGATE(newVar(out.result, typeInfo));

    if (op.isCast()) {
      // TODO: Vector casting doesn't work right now.
      uint32_t fromId = node->getChild()->getTypeInfo() & kTypeIdMask;
      uint32_t instCode = kInstCodeNone;

      switch (COMBINE_OP_CAST(typeInfo & kTypeIdMask, fromId)) {
        case COMBINE_OP_CAST(kTypeFloat , kTypeDouble): instCode = kInstCodeCvtdtof; break;
        case COMBINE_OP_CAST(kTypeFloat , kTypeInt   ): instCode = kInstCodeCvtdtoi; break;
        case COMBINE_OP_CAST(kTypeDouble, kTypeFloat ): instCode = kInstCodeCvtftod; break;
        case COMBINE_OP_CAST(kTypeDouble, kTypeInt   ): instCode = kInstCodeCvtftoi; break;
        case COMBINE_OP_CAST(kTypeInt   , kTypeFloat ): instCode = kInstCodeCvtitof; break;
        case COMBINE_OP_CAST(kTypeInt   , kTypeDouble): instCode = kInstCodeCvtitod; break;

        default:
          return MPSL_TRACE_ERROR(kErrorInvalidState);
      }
      MPSL_PROPAGATE(emitInst2(instCode, out.result, tmp.result, typeInfo));
    }
    else if (op.isSwizzle()) {
      uint32_t width = TypeInfo::widthOf(typeInfo);
      uint32_t swizzleMask = node->getSwizzleMask();

      Value swizzleValue;
      swizzleValue.q.set(0);

      if (needSplit(width)) {
        // TODO:
      }
      else {
        swizzleValue.i[0] = swizzleMask;
        IRImm* msk = getIR()->newImm(swizzleValue, kIRRegNone, 4);
        MPSL_PROPAGATE(getIR()->emitInst(getBlock(), kInstCodePshufd, out.result.lo, tmp.result.lo, msk));
      }
    }
    else {
      uint32_t instCode = op.getInstByTypeId(typeInfo & kTypeIdMask);
      if (MPSL_UNLIKELY(instCode == kInstCodeNone))
        return MPSL_TRACE_ERROR(kErrorInvalidState);
      MPSL_PROPAGATE(emitInst2(instCode, out.result, tmp.result, typeInfo));
    }
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

  uint32_t typeInfo = node->getTypeInfo();
  const OpInfo& op = OpInfo::get(node->getOp());

  IRPair<IRVar> result;
  IRPair<IRVar> lVar;
  IRPair<IRVar> rVar;
  MPSL_PROPAGATE(newVar(result, typeInfo));

  uint32_t instCode = op.getInstByTypeId(typeInfo & kTypeIdMask);
  if (op.isAssignment()) {
    if (op.getType() == kOpAssign) {
      // Pure assignment operator `=`.
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
    if (MPSL_UNLIKELY(instCode == kInstCodeNone))
      return MPSL_TRACE_ERROR(kErrorInvalidState);

    if (op.isShift()) {
      if (rValue.result.lo->isImm())
        rValue.result.hi = rValue.result.lo;

      MPSL_PROPAGATE(asVar(lVar, lValue.result, typeInfo));
      MPSL_PROPAGATE(emitInst3(instCode, result, lVar, rValue.result, typeInfo));
    }
    else {
      MPSL_PROPAGATE(asVar(lVar, lValue.result, typeInfo));
      MPSL_PROPAGATE(asVar(rVar, rValue.result, typeInfo));
      MPSL_PROPAGATE(emitInst3(instCode, result, lVar, rVar, typeInfo));
    }

    out.result.set(result);
  }

  return kErrorOk;
}

#undef COMBINE_OP_TYPE
#undef COMBINE_OP_CAST

Error AstToIR::onCall(AstCall* node, Args& out) noexcept {
  uint32_t i;

  AstSymbol* fSym = node->getSymbol();
  if (MPSL_UNLIKELY(!fSym))
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  AstFunction* func = static_cast<AstFunction*>(fSym->getNode());
  if (MPSL_UNLIKELY(!func) || func->getNodeType() != AstNode::kTypeFunction)
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  if (MPSL_UNLIKELY(_nestedFunctions.has(func)))
    return MPSL_TRACE_ERROR(kErrorRecursionNotAllowed);


  AstBlock* fArgsDecl = func->getArgs();
  bool argsUsed = func->hasBody();

  uint32_t fArgsCount = func->getArgs()->getLength();
  uint32_t cArgsCount = node->getLength();

  if (MPSL_UNLIKELY(fArgsCount < cArgsCount))
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  // Translate all function arguments to IR.
  for (i = 0; i < cArgsCount; i++) {
    Args value(argsUsed);

    AstVarDecl* argDecl = static_cast<AstVarDecl*>(fArgsDecl->getAt(i));
    MPSL_PROPAGATE(onNode(node->getAt(i), value));

    IRPair<IRVar> var;
    MPSL_PROPAGATE(asVar(var, value.result, argDecl->getTypeInfo()));

    mapVarToAst(argDecl->getSymbol(), var);
  }

  // Map all function arguments to `varMap`.
  while (i < fArgsCount) {
    Args value(argsUsed);

    AstVarDecl* argDecl = static_cast<AstVarDecl*>(fArgsDecl->getAt(i));
    MPSL_PROPAGATE(onNode(argDecl, value));

    IRPair<IRVar> var;
    MPSL_PROPAGATE(asVar(var, value.result, argDecl->getTypeInfo()));

    mapVarToAst(argDecl->getSymbol(), var);
    i++;
  }

  // Emit the function body.
  if (func->hasBody()) {
    _functionLevel++;
    MPSL_PROPAGATE(_nestedFunctions.put(func));
    MPSL_PROPAGATE(onNode(func->getBody(), out));

    _functionLevel--;
    _nestedFunctions.del(func);
    out.result = _currentRet;
  }

  return kErrorOk;
}

// ============================================================================
// [mpsl::AstToIR - Utilities]
// ============================================================================

Error AstToIR::mapVarToAst(AstSymbol* sym, IRPair<IRVar> var) noexcept {
  IRPair<IRVar>* existing;

  if (!_varMap.get(sym, &existing))
    return _varMap.put(sym, var);

  *existing = var;
  return kErrorOk;
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

Error AstToIR::asVar(IRPair<IRObject>& out, IRPair<IRObject> in, uint32_t typeInfo) noexcept {
  if (in.lo == nullptr && in.hi == nullptr)
    return out.set(nullptr, nullptr);

  uint32_t ti[2] = { typeInfo, kTypeVoid };
  uint32_t width = TypeInfo::widthOf(typeInfo);

  if (needSplit(width))
    mpSplitTypeInfo(ti[0], ti[1], typeInfo);

  for (uint32_t i = 0; i < 2; i++) {
    IRObject* inObj = in.obj[i];

    if (!inObj) {
      out.obj[i] = nullptr;
      continue;
    }

    switch (inObj->getObjectType()) {
      case IRObject::kTypeVar: {
        out.obj[i] = static_cast<IRVar*>(inObj);
        break;
      }

      case IRObject::kTypeMem: {
        typeInfo = ti[i];

        IRMem* mem = static_cast<IRMem*>(inObj);
        IRVar* var = getIR()->newVarByTypeInfo(typeInfo);

        MPSL_NULLCHECK(var);
        MPSL_PROPAGATE(emitFetchX(var, mem, typeInfo));

        out.obj[i] = var;
        break;
      }

      case IRObject::kTypeImm: {
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

    MPSL_PROPAGATE(getIR()->emitInst(block, instCode | mpGetVecFlags(loTI), o0.lo, o1.lo));
    MPSL_PROPAGATE(getIR()->emitInst(block, instCode | mpGetVecFlags(hiTI), o0.hi, o1.hi));
    return kErrorOk;
  }
  else {
    return getIR()->emitInst(block, instCode | mpGetVecFlags(typeInfo), o0.lo, o1.lo);
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

    MPSL_PROPAGATE(getIR()->emitInst(block, instCode | mpGetVecFlags(loTI), o0.lo, o1.lo, o2.lo));
    MPSL_PROPAGATE(getIR()->emitInst(block, instCode | mpGetVecFlags(hiTI), o0.hi, o1.hi, o2.hi));
    return kErrorOk;
  }
  else {
    return getIR()->emitInst(block, instCode | mpGetVecFlags(typeInfo), o0.lo, o1.lo, o2.lo);
  }
}

Error AstToIR::emitFetchX(IRVar* dst, IRMem* src, uint32_t typeInfo) noexcept {
  uint32_t instCode = kInstCodeNone;
  switch (typeInfo & (kTypeIdMask | kTypeVecMask)) {
    case kTypeBool   : instCode = kInstCodeFetch32; break;
    case kTypeBool1  : instCode = kInstCodeFetch32; break;
    case kTypeBool2  : instCode = kInstCodeFetch64; break;
    case kTypeBool3  : instCode = kInstCodeFetch96; break;
    case kTypeBool4  : instCode = kInstCodeFetch128; break;
    case kTypeBool8  : instCode = kInstCodeFetch256; break;

    case kTypeInt    : instCode = kInstCodeFetch32; break;
    case kTypeInt1   : instCode = kInstCodeFetch32; break;
    case kTypeInt2   : instCode = kInstCodeFetch64; break;
    case kTypeInt3   : instCode = kInstCodeFetch96; break;
    case kTypeInt4   : instCode = kInstCodeFetch128; break;
    case kTypeInt8   : instCode = kInstCodeFetch256; break;

    case kTypeFloat  : instCode = kInstCodeFetch32; break;
    case kTypeFloat1 : instCode = kInstCodeFetch32; break;
    case kTypeFloat2 : instCode = kInstCodeFetch64; break;
    case kTypeFloat3 : instCode = kInstCodeFetch96; break;
    case kTypeFloat4 : instCode = kInstCodeFetch128; break;
    case kTypeFloat8 : instCode = kInstCodeFetch256; break;

    case kTypeQBool  : instCode = kInstCodeFetch64; break;
    case kTypeQBool1 : instCode = kInstCodeFetch64; break;
    case kTypeQBool2 : instCode = kInstCodeFetch128; break;
    case kTypeQBool3 : instCode = kInstCodeFetch192; break;
    case kTypeQBool4 : instCode = kInstCodeFetch256; break;

    case kTypeDouble : instCode = kInstCodeFetch64; break;
    case kTypeDouble1: instCode = kInstCodeFetch64; break;
    case kTypeDouble2: instCode = kInstCodeFetch128; break;
    case kTypeDouble3: instCode = kInstCodeFetch192; break;
    case kTypeDouble4: instCode = kInstCodeFetch256; break;

    default:
      return MPSL_TRACE_ERROR(kErrorInvalidState);
  }

  return getIR()->emitInst(getBlock(), instCode, dst, src);
}

Error AstToIR::emitStoreX(IRMem* dst, IRVar* src, uint32_t typeInfo) noexcept {
  uint32_t instCode = kInstCodeNone;
  switch (typeInfo & (kTypeIdMask | kTypeVecMask)) {
    case kTypeBool   : instCode = kInstCodeStore32; break;
    case kTypeBool1  : instCode = kInstCodeStore32; break;
    case kTypeBool2  : instCode = kInstCodeStore64; break;
    case kTypeBool3  : instCode = kInstCodeStore96; break;
    case kTypeBool4  : instCode = kInstCodeStore128; break;
    case kTypeBool8  : instCode = kInstCodeStore256; break;

    case kTypeInt    : instCode = kInstCodeStore32; break;
    case kTypeInt1   : instCode = kInstCodeStore32; break;
    case kTypeInt2   : instCode = kInstCodeStore64; break;
    case kTypeInt3   : instCode = kInstCodeStore96; break;
    case kTypeInt4   : instCode = kInstCodeStore128; break;
    case kTypeInt8   : instCode = kInstCodeStore256; break;

    case kTypeFloat  : instCode = kInstCodeStore32; break;
    case kTypeFloat1 : instCode = kInstCodeStore32; break;
    case kTypeFloat2 : instCode = kInstCodeStore64; break;
    case kTypeFloat3 : instCode = kInstCodeStore96; break;
    case kTypeFloat4 : instCode = kInstCodeStore128; break;
    case kTypeFloat8 : instCode = kInstCodeStore256; break;

    case kTypeQBool  : instCode = kInstCodeStore64; break;
    case kTypeQBool1 : instCode = kInstCodeStore64; break;
    case kTypeQBool2 : instCode = kInstCodeStore128; break;
    case kTypeQBool3 : instCode = kInstCodeStore192; break;
    case kTypeQBool4 : instCode = kInstCodeStore256; break;

    case kTypeDouble : instCode = kInstCodeStore64; break;
    case kTypeDouble1: instCode = kInstCodeStore64; break;
    case kTypeDouble2: instCode = kInstCodeStore128; break;
    case kTypeDouble3: instCode = kInstCodeStore192; break;
    case kTypeDouble4: instCode = kInstCodeStore256; break;

    default:
      return MPSL_TRACE_ERROR(kErrorInvalidState);
  }

  MPSL_PROPAGATE(getIR()->emitInst(getBlock(), instCode, dst, src));
  return kErrorOk;
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
