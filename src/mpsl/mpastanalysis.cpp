// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MPSL_EXPORTS

// [Dependencies - MPSL]
#include "./mpastanalysis_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [mpsl::mpIsVarNodeType]
// ============================================================================

static MPSL_INLINE bool mpIsVarNodeType(uint32_t nodeType) noexcept {
  return nodeType == AstNode::kTypeVar || nodeType == AstNode::kTypeVarMemb;
}

// ============================================================================
// [mpsl::AstAnalysis - Construction / Destruction]
// ============================================================================

AstAnalysis::AstAnalysis(AstBuilder* ast, ErrorReporter* errorReporter) noexcept
  : AstVisitor<AstAnalysis>(ast),
    _errorReporter(errorReporter),
    _currentRet(nullptr),
    _unreachable(false) {}
AstAnalysis::~AstAnalysis() noexcept {}

// ============================================================================
// [mpsl::AstAnalysis - OnNode]
// ============================================================================

Error AstAnalysis::onProgram(AstProgram* node) noexcept {
  return onBlock(node);
}

Error AstAnalysis::onFunction(AstFunction* node) noexcept {
  bool isMain = node->getFunc()->eq("main", 4);

  // Link to the `_mainFunction` if this is `main()`.
  if (isMain) {
    if (_ast->_mainFunction != nullptr)
      return MPSL_TRACE_ERROR(kErrorInvalidState);
    _ast->_mainFunction = node;
  }

  AstSymbol* retSymb = _currentRet = node->getRet();
  Error err = node->hasBody() ? onNode(node->getBody()) : static_cast<Error>(kErrorOk);

  // Check whether the function returns if it has to.
  bool didReturn = isUnreachable();
  _currentRet = nullptr;
  _unreachable = false;
  MPSL_PROPAGATE(err);

  if (retSymb != nullptr && !didReturn)
    return _errorReporter->onError(kErrorInvalidProgram, node->getPosition(),
      "Function '%s()' has to return '%{Type}'.", node->getFunc()->getName(), retSymb->getTypeInfo());

  // Check whether the main function's return type matches the output.
  if (isMain) {
    AstSymbol* retPriv = _ast->getGlobalScope()->resolveSymbol(StringRef("@ret", 4));
    uint32_t mask = (kTypeIdMask | kTypeVecMask);

    uint32_t functionRTI = retSymb ? retSymb->getTypeInfo() & mask : static_cast<uint32_t>(kTypeVoid);
    uint32_t privateRTI = retPriv ? retPriv->getTypeInfo() & mask : static_cast<uint32_t>(kTypeVoid);

    if (functionRTI != privateRTI)
      return _errorReporter->onError(kErrorReturnMismatch, node->getPosition(),
        "The program's '%s()' returns '%{Type}', but the implementation requires '%{Type}'.", node->getFunc()->getName(), functionRTI, privateRTI);
  }

  return kErrorOk;
}

Error AstAnalysis::onBlock(AstBlock* node) noexcept {
  for (uint32_t i = 0, count = node->getLength(); i < count; i++) {
    MPSL_PROPAGATE(onNode(node->getAt(i)));
    MPSL_ASSERT(count == node->getLength());
  }

  return kErrorOk;
}

Error AstAnalysis::onBranch(AstBranch* node) noexcept {
  return onBlock(node);
}

Error AstAnalysis::onCondition(AstCondition* node) noexcept {
  if (node->hasExp()) {
    MPSL_PROPAGATE(onNode(node->getExp()));
    MPSL_PROPAGATE(boolCast(node, node->getExp()));
  }

  if (node->hasBody()) {
    bool prevUnreachable = _unreachable;
    MPSL_PROPAGATE(onNode(node->getBody()));
    _unreachable = prevUnreachable;
  }

  return kErrorOk;
}

Error AstAnalysis::onLoop(AstLoop* node) noexcept {
  if (node->hasInit()) {
    MPSL_PROPAGATE(onNode(node->getInit()));
  }

  if (node->hasIter()) {
    MPSL_PROPAGATE(onNode(node->getIter()));
  }

  if (node->hasCond()) {
    MPSL_PROPAGATE(onNode(node->getCond()));
    MPSL_PROPAGATE(boolCast(node, node->getCond()));
  }

  if (node->hasBody()) {
    bool prevUnreachable = _unreachable;
    MPSL_PROPAGATE(onNode(node->getBody()));
    _unreachable = prevUnreachable;
  }

  if (node->getNodeType() != AstNode::kTypeFor && !node->hasCond())
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  return kErrorOk;
}

Error AstAnalysis::onBreak(AstBreak* node) noexcept {
  if (!node->getLoop())
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  _unreachable = true;
  return kErrorOk;
}

Error AstAnalysis::onContinue(AstContinue* node) noexcept {
  if (!node->getLoop())
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  _unreachable = true;
  return kErrorOk;
}

Error AstAnalysis::onReturn(AstReturn* node) noexcept {
  uint32_t retType = kTypeVoid;
  uint32_t srcType = kTypeVoid;

  if (_currentRet) {
    retType = _currentRet->getTypeInfo();
    node->setTypeInfo(retType);
  }

  AstNode* child = node->getChild();
  if (child != nullptr) {
    MPSL_PROPAGATE(onNode(child));
    child = node->getChild();
    srcType = child->getTypeInfo() & kTypeIdMask;
  }

  if (retType != srcType) {
    if (retType == kTypeVoid || srcType == kTypeVoid)
      return invalidCast(node->getPosition(), "Invalid return conversion", srcType, retType);
    MPSL_PROPAGATE(implicitCast(node, child, retType));
  }

  _unreachable = true;
  return kErrorOk;
}

Error AstAnalysis::onVarDecl(AstVarDecl* node) noexcept {
  if (node->hasChild()) {
    MPSL_PROPAGATE(onNode(node->getChild()));
    MPSL_PROPAGATE(implicitCast(node, node->getChild(), node->getSymbol()->getTypeInfo()));
  }

  return kErrorOk;
}

Error AstAnalysis::onVarMemb(AstVarMemb* node) noexcept {
  if (!node->hasChild())
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  MPSL_PROPAGATE(onNode(node->getChild()));
  AstNode* child = node->getChild();

  uint32_t typeInfo = child->getTypeInfo();
  uint32_t typeId = typeInfo & kTypeIdMask;

  if (TypeInfo::isPtrId(typeId)) {
    if (child->getNodeType() != AstNode::kTypeVar)
      return MPSL_TRACE_ERROR(kErrorInvalidState);

    // The field accesses an object's member.
    const AstSymbol* sym = static_cast<AstVar*>(child)->getSymbol();
    const Layout* layout = sym->getLayout();

    if (layout == nullptr)
      return MPSL_TRACE_ERROR(kErrorInvalidState);

    const Layout::Member* m = layout->getMember(node->getField());
    if (m == nullptr)
      return _errorReporter->onError(kErrorInvalidProgram, node->getPosition(),
        "Object '%s' doesn't have a member '%s'", sym->getName(), node->getField().getData());

    node->setTypeInfo(m->typeInfo | kTypeRef | (typeInfo & kTypeRW));
    node->setOffset(m->offset);
  }
  else {
    // The field should access/shuffle a vector.
    if ((typeInfo & kTypeVecMask) == 0)
      return _errorReporter->onError(kErrorInvalidProgram, node->getPosition(),
        "Type '%{Type}' doesn't have a member '%s'", typeInfo, node->getField().getData());

    // TODO: Field
  }

  return kErrorOk;
}

Error AstAnalysis::onVar(AstVar* node) noexcept {
  uint32_t typeInfo = node->getTypeInfo();
  if ((typeInfo & kTypeIdMask) == kTypeVoid)
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  typeInfo |= kTypeRef;
  node->setTypeInfo(typeInfo);

  return kErrorOk;
}

Error AstAnalysis::onImm(AstImm* node) noexcept {
  if (node->getTypeInfo() == kTypeVoid)
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  return kErrorOk;
}

Error AstAnalysis::onUnaryOp(AstUnaryOp* node) noexcept {
  const OpInfo& op = OpInfo::get(node->getOp());

  if (!node->hasChild())
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  MPSL_PROPAGATE(onNode(node->getChild()));
  AstNode* child = node->getChild();

  if (op.isAssignment())
    MPSL_PROPAGATE(checkAssignment(child, op.type));

  if (op.isCast()) {
    uint32_t srcType = child->getTypeInfo();
    uint32_t dstType = node->getTypeInfo();

    uint32_t srcId = srcType & kTypeIdMask;
    uint32_t dstId = dstType & kTypeIdMask;

    // Refuse an explicit cast from void to any other type.
    if (srcId == kTypeVoid && dstId != kTypeVoid)
      return invalidCast(node->_position, "Invalid explicit cast", srcType, dstType);
  }
  else {
    uint32_t srcType = child->getTypeInfo();
    uint32_t dstType = srcType & ~(kTypeRef | kTypeWrite);

    uint32_t srcId = srcType & kTypeIdMask;
    uint32_t dstId = srcId;

    bool supported = (op.isIntOp()   && TypeInfo::isIntId(srcId))  |
                     (op.isBoolOp()  && TypeInfo::isBoolId(srcId)) |
                     (op.isFloatOp() && TypeInfo::isFloatId(srcId));

    if (!supported) {
      // Promote to double if this is a floating point operator or intrinsic.
      if (TypeInfo::isFloatId(dstId)) {
        dstId = kTypeDouble;
        dstType = dstId | (srcType & ~kTypeIdMask);

        MPSL_PROPAGATE(implicitCast(node, child, dstType));
        child = node->getChild();
      }
    }

    // TODO: This has been relaxed, but there should be some flag that can turn on.
    /*
    if (op.isBitwise()) {
      // Bitwise operation performed on `float` or `double` is invalid.
      if (!TypeInfo::isIntOrBoolId(typeId))
        return _errorReporter->onError(kErrorInvalidProgram, node->getPosition(),
          "Bitwise operation '%s' can't be performed on type '%{Type}'", op.name, typeInfo);
    }
    */

    if (op.isConditional()) {
      // Result of conditional operator is a boolean.
      dstId = TypeInfo::boolIdByTypeId(dstType & kTypeIdMask);
      dstType = dstId | (dstType & ~kTypeIdMask);
    }

    node->setTypeInfo(dstType | kTypeRead);
  }

  return kErrorOk;
}

Error AstAnalysis::onBinaryOp(AstBinaryOp* node) noexcept {
  const OpInfo& op = OpInfo::get(node->getOp());

  if (!node->hasLeft() || !node->hasRight())
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  MPSL_PROPAGATE(onNode(node->getLeft()));
  MPSL_PROPAGATE(onNode(node->getRight()));

  if (op.isAssignment())
    MPSL_PROPAGATE(checkAssignment(node->getLeft(), op.type));

  // Minimal evaluation requires both operands to be casted to bool.
  if (op.isLogical()) {
    MPSL_PROPAGATE(boolCast(node, node->getLeft()));
    MPSL_PROPAGATE(boolCast(node, node->getRight()));
  }

  for (;;) {
    AstNode* left = node->getLeft();
    AstNode* right = node->getRight();

    uint32_t lTypeInfo = left->getTypeInfo();
    uint32_t rTypeInfo = right->getTypeInfo();

    uint32_t lTypeId = lTypeInfo & kTypeIdMask;
    uint32_t rTypeId = rTypeInfo & kTypeIdMask;

    uint32_t dstTypeInfo = lTypeInfo;

    if (op.isShift()) {
      // Bit shift and rotation can only be done on integers.
      if (!TypeInfo::isIntId(lTypeId))
        return _errorReporter->onError(kErrorInvalidProgram, node->getPosition(),
          "Bitwise operation '%s' can't be performed on type '%{Type}'.", op.name, lTypeId);

      if (!TypeInfo::isIntId(rTypeId))
        return _errorReporter->onError(kErrorInvalidProgram, node->getPosition(),
          "Bitwise operation '%s' can't be specified by type '%{Type}'.", op.name, rTypeId);

      // Right operand of bit shift and rotation must be scalar.
      if (TypeInfo::elementsOf(rTypeInfo) > 1)
        return _errorReporter->onError(kErrorInvalidProgram, node->getPosition(),
          "Bitwise operation '%s' requires right operand to be scalar, not '%{Type}'.", op.name, rTypeId);
    }
    else {
      // Promote `int` to `double` in case that the operator is only defined
      // for floating point operand(s).
      if (op.isFloatOnly() && (lTypeId != rTypeId || !TypeInfo::isFloatId(lTypeId))) {
        // This kind of conversion should never happen on assignment. This
        // should remain a runtime check to prevent undefined behavior in
        // case of a bug in the `OpInfo table.
        if (op.isAssignment())
          return MPSL_TRACE_ERROR(kErrorInvalidState);

        MPSL_PROPAGATE(implicitCast(node, left , kTypeDouble | (lTypeInfo & ~kTypeIdMask)));
        MPSL_PROPAGATE(implicitCast(node, right, kTypeDouble | (rTypeInfo & ~kTypeIdMask)));
        continue;
      }

      if (lTypeId != rTypeId) {
        bool rightToLeft = true;
        bool leftToRight = true;

        if (op.isAssignment())
          leftToRight = false;

        if (mpCanImplicitCast(lTypeId, rTypeId) && rightToLeft) {
          // Cast type on the right side to the type on the left side.
          AstUnaryOp* castNode = _ast->newNode<AstUnaryOp>(kOpCast, dstTypeInfo);
          MPSL_NULLCHECK(castNode);

          node->injectNode(right, castNode);
          continue;
        }

        if (mpCanImplicitCast(rTypeId, lTypeId) && leftToRight) {
          // Cast type on the left side to the type on the right side.
          dstTypeInfo = rTypeInfo;

          AstUnaryOp* castNode = _ast->newNode<AstUnaryOp>(kOpCast, dstTypeInfo);
          MPSL_NULLCHECK(castNode);

          node->injectNode(left, castNode);
          continue;
        }

        return invalidCast(node->_position, "Invalid implicit cast", rTypeInfo, lTypeInfo);
      }

      // Check whether both operands have the same number of vector elements.
      // In case of mismatch the scalar operand should be promoted to vector
      // if the operator is not an assignment. All other cases are errors.
      uint32_t lVec = TypeInfo::elementsOf(lTypeInfo);
      uint32_t rVec = TypeInfo::elementsOf(rTypeInfo);

      if (lVec != rVec) {
        if (lVec == 1) {
          if (op.isAssignment())
            return _errorReporter->onError(kErrorInvalidProgram, node->getPosition(),
              "Vector size mismatch '%{Type}' vs '%{Type}'.", lTypeId, rTypeId);

          // Promote left operand to a vector of `rVec` elements.
          AstUnaryOp* swizzleNode = _ast->newNode<AstUnaryOp>(kOpSwizzle, lTypeId | (rVec << kTypeVecShift) | kTypeRead);
          MPSL_NULLCHECK(swizzleNode);

          node->injectNode(left, swizzleNode);
          continue;
        }
        else if (rVec == 1) {
          // Promote right operand to a vector of `lVec` elements.
          AstUnaryOp* swizzleNode = _ast->newNode<AstUnaryOp>(kOpSwizzle, rTypeId | (lVec << kTypeVecShift) | kTypeRead);
          MPSL_NULLCHECK(swizzleNode);

          node->injectNode(right, swizzleNode);
          continue;
        }
        else {
          return _errorReporter->onError(kErrorInvalidProgram, node->getPosition(),
            "Vector size mismatch '%{Type}' vs '%{Type}'.", lTypeId, rTypeId);
        }
      }
    }

    if (op.isConditional()) {
      // Result of a conditional is a boolean.
      dstTypeInfo = TypeInfo::boolIdByTypeId(lTypeId & kTypeIdMask) | kTypeRead;
    }
    else {
      // Results in a new temporary, clear the reference/write flags.
      dstTypeInfo = (dstTypeInfo | kTypeRead) & ~(kTypeRef | kTypeWrite);
    }

    node->setTypeInfo(dstTypeInfo);
    break;
  }

  return kErrorOk;
}

Error AstAnalysis::onCall(AstCall* node) noexcept {
  AstSymbol* sym = node->getSymbol();
  uint32_t count = node->getLength();

  // Transform an intrinsic function into unary or binary operator.
  if (sym->isIntrinsic()) {
    const OpInfo& op = OpInfo::get(sym->getOpType());

    uint32_t reqArgs = op.getOpCount();
    if (count != reqArgs)
      return _errorReporter->onError(kErrorInvalidProgram, node->getPosition(),
        "Function '%s()' requires %u argument(s) (%u provided).", sym->getName(), reqArgs, count);

    AstNode* newNode;
    if (reqArgs == 1) {
      AstUnaryOp* unary = _ast->newNode<AstUnaryOp>(op.type);
      MPSL_NULLCHECK(unary);

      unary->setChild(node->removeAt(0));
      newNode = unary;
    }
    else {
      AstBinaryOp* binary = _ast->newNode<AstBinaryOp>(op.type);
      MPSL_NULLCHECK(binary);

      binary->setRight(node->removeAt(1));
      binary->setLeft(node->removeAt(0));
      newNode = binary;
    }

    newNode->setPosition(node->getPosition());
    _ast->deleteNode(node->getParent()->replaceNode(node, newNode));

    return onNode(newNode);
  }

  AstFunction* decl = static_cast<AstFunction*>(sym->getNode());
  if (decl == nullptr || decl->getNodeType() != AstNode::kTypeFunction)
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  if (decl->getRet())
    node->setTypeInfo(decl->getRet()->getTypeInfo());

  AstBlock* declArgs = decl->getArgs();
  if (count != declArgs->getLength())
    return _errorReporter->onError(kErrorInvalidProgram, node->getPosition(),
      "Function '%s()' requires %u argument(s) (%u provided).", sym->getName(), declArgs->getLength(), count);

  for (uint32_t i = 0; i < count; i++) {
    MPSL_PROPAGATE(onNode(node->getAt(i)));
    MPSL_PROPAGATE(implicitCast(node, node->getAt(i), declArgs->getAt(i)->getTypeInfo()));
  }

  return kErrorOk;
}

// ============================================================================
// [mpsl::AstAnalysis - CheckAssignment]
// ============================================================================

Error AstAnalysis::checkAssignment(AstNode* node, uint32_t op) noexcept {
  if (!mpIsVarNodeType(node->getNodeType()))
    return _errorReporter->onError(kErrorInvalidProgram, node->getPosition(),
      "Can't assign '%s' to a non-variable.", mpOpInfo[op].name);

  uint32_t typeInfo = node->getTypeInfo();
  if (!(typeInfo & kTypeWrite))
    return _errorReporter->onError(kErrorInvalidProgram, node->getPosition(),
      "Can't assign '%s' to a non-writable variable.", mpOpInfo[op].name);

  // Assignment has always a side-effect - used by optimizer to not remove code
  // that has to be executed.
  node->addNodeFlags(AstNode::kFlagSideEffect);

  return kErrorOk;
}

// ============================================================================
// [mpsl::AstAnalysis - Cast]
// ============================================================================

Error AstAnalysis::implicitCast(AstNode* node, AstNode* child, uint32_t typeInfo) noexcept {
  uint32_t childInfo = child->getTypeInfo();

  // First implicit cast type-id, vector/scalar cast will be checked later on.
  uint32_t aTypeId = typeInfo  & kTypeIdMask;
  uint32_t bTypeId = childInfo & kTypeIdMask;

  bool implicitCast = false;

  // Try to cast to `aType`.
  if (aTypeId != bTypeId) {
    implicitCast = mpCanImplicitCast(aTypeId, bTypeId);
    if (!implicitCast)
      return invalidCast(node->_position, "Invalid implicit cast", childInfo, typeInfo);
  }

  // Try to cast to vector from scalar.
  uint32_t aAttr = typeInfo  & (kTypeAttrMask & ~(kTypeRW | kTypeRef));
  uint32_t bAttr = childInfo & (kTypeAttrMask & ~(kTypeRW | kTypeRef));

  if (aAttr != bAttr) {
    if ((aAttr & kTypeVecMask) != 0 && (bAttr & kTypeVecMask) <= kTypeVec1)
      implicitCast = true;
    else
      return invalidCast(node->_position, "Invalid implicit cast", childInfo, typeInfo);
  }

  if (implicitCast) {
    AstUnaryOp* castNode = _ast->newNode<AstUnaryOp>(kOpCast, typeInfo);
    node->injectNode(child, castNode);
  }

  return kErrorOk;
}

uint32_t AstAnalysis::boolCast(AstNode* node, AstNode* child) noexcept {
  uint32_t size = mpTypeInfo[child->getTypeInfo() & kTypeIdMask].size;

  switch (size) {
    case 4: return implicitCast(node, child, kTypeBool);
    case 8: return implicitCast(node, child, kTypeQBool);

    default:
      return _errorReporter->onError(kErrorInvalidProgram, node->_position,
        "%s from '%{Type}' to 'bool'.", "Invalid boolean cast", child->getTypeInfo());
  }
}

Error AstAnalysis::invalidCast(uint32_t position, const char* msg, uint32_t fromTypeInfo, uint32_t toTypeInfo) noexcept {
  return _errorReporter->onError(kErrorInvalidProgram, position,
    "%s from '%{Type}' to '%{Type}'.", msg, fromTypeInfo, toTypeInfo);
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
