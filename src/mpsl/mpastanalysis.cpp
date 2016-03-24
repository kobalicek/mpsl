// [MPSL]
// Shader-Like Mathematical Expression JIT Engine for C++.
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
// [mpsl::AstAnalysis - Construction / Destruction]
// ============================================================================

AstAnalysis::AstAnalysis(AstBuilder* ast, ErrorReporter* errorReporter) noexcept
  : AstVisitor(ast),
    _errorReporter(errorReporter),
    _currentRet(nullptr),
    _unreachable(false) {}
AstAnalysis::~AstAnalysis() noexcept {}

// ============================================================================
// [mpsl::AstAnalysis - OnNode]
// ============================================================================

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

  if (node->getNodeType() != kAstNodeFor && !node->hasCond())
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

  if (TypeInfo::isObjectId(typeId)) {
    if (child->getNodeType() != kAstNodeVar)
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
    uint32_t dstType = node->getTypeInfo();
    uint32_t srcType = child->getTypeInfo();

    uint32_t dstId = dstType & kTypeIdMask;
    uint32_t srcId = srcType & kTypeIdMask;

    // Refuse an explicit cast from void to any other type.
    if (srcId == kTypeVoid && dstId != kTypeVoid)
      return invalidCast(node->_position, "Invalid explicit cast", srcType, dstType);
  }
  else {
    uint32_t typeInfo = child->getTypeInfo() & ~(kTypeRef | kTypeWrite);
    uint32_t typeId = typeInfo & kTypeIdMask;

    // Only `float` or `double` can be used.
    if (op.isFloatOnly()) {
      if (!TypeInfo::isRealId(typeId)) {
        typeId = kTypeDouble;
        typeInfo = typeId | (typeInfo & ~kTypeIdMask);

        MPSL_PROPAGATE(implicitCast(node, child, typeInfo));
        child = node->getChild();
      }
    }

    // Bitwise operation performed on `float` or `double` is invalid.
    if (op.isBitwise()) {
      if (!TypeInfo::isIntOrBoolId(typeId))
        return _errorReporter->onError(kErrorInvalidProgram, node->getPosition(),
          "Bitwise operation '%s' can't be performed on type '%{Type}'", op.name, typeInfo);
    }
    // Result of a conditional is `__bool32` or `__bool64`.
    else if (op.isCondition()) {
      uint32_t size = TypeInfo::getSize(typeInfo & kTypeIdMask);

      typeId = size == 4 ? kTypeBool32 : kTypeBool64;
      typeInfo = typeId | (typeInfo & ~kTypeIdMask);
    }

    node->setTypeInfo(typeInfo | kTypeRead);
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

  // Minimal evaluation requires both operands to be casted to `__bool`.
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

    if (op.isFloatOnly() && (lTypeId != rTypeId || !TypeInfo::isRealId(lTypeId))) {
      MPSL_PROPAGATE(implicitCast(node, left , kTypeDouble | (lTypeInfo & ~kTypeIdMask)));
      MPSL_PROPAGATE(implicitCast(node, right, kTypeDouble | (rTypeInfo & ~kTypeIdMask)));
      continue;
    }

    // Bit shift/rotation operator does never change the left operand type,
    // unless it's a `__bool`, which would be implicitly casted to `int`.
    if (op.isBitShift()) {
      if (TypeInfo::isBoolId(lTypeId)) {
        lTypeId = kTypeInt;
        lTypeInfo = kTypeInt | (lTypeInfo & ~kTypeIdMask);
      }

      if (!TypeInfo::isIntId(lTypeId))
        return _errorReporter->onError(kErrorInvalidProgram, node->getPosition(),
          "Bitwise operation '%s' can't be performed on type '%{Type}'.", op.name, lTypeId);

      if (!TypeInfo::isIntId(rTypeId))
        return _errorReporter->onError(kErrorInvalidProgram, node->getPosition(),
          "Bitwise operation '%s' can't be specified by type '%{Type}'.", op.name, rTypeId);
    }

    uint32_t result = lTypeInfo;
    if (lTypeId != rTypeId) {
      bool rightToLeft = true;
      bool leftToRight = true;

      if (op.isAssignment())
        leftToRight = false;

      if (mpCanImplicitCast(lTypeId, rTypeId) && rightToLeft) {
        // Cast type on the right side to the type on the left side.
        AstUnaryOp* castNode = _ast->newNode<AstUnaryOp>(kOpCast, result);
        node->injectNode(right, castNode);
        continue;
      }

      if (mpCanImplicitCast(rTypeId, lTypeId) && leftToRight) {
        // Cast type on the left side to the type on the right side.
        result = rTypeInfo;

        AstUnaryOp* castNode = _ast->newNode<AstUnaryOp>(kOpCast, result);
        node->injectNode(left, castNode);
        continue;
      }

      return invalidCast(node->_position, "Invalid implicit cast", rTypeInfo, lTypeInfo);
    }

    // Result of a conditional is `__bool32` or `__bool64`.
    if (op.isCondition()) {
      uint32_t size = TypeInfo::getSize(lTypeId & kTypeIdMask);
      result = (size == 4 ? kTypeBool32 : kTypeBool64) | kTypeRead;
    }
    else {
      // Results in a new temporary, clear the reference/write flags.
      result = (result | kTypeRead) & ~(kTypeRef | kTypeWrite);
    }

    node->setTypeInfo(result);
    break;
  }

  return kErrorOk;
}

Error AstAnalysis::onCall(AstCall* node) noexcept {
  AstSymbol* sym = node->getSymbol();
  uint32_t count = node->getLength();

  // Transform an intrinsic function into unary or binary operator.
  if (sym->getSymbolType() == kAstSymbolIntrinsic) {
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
  if (decl == nullptr || decl->getNodeType() != kAstNodeFunction)
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
  node->addNodeFlags(kAstNodeHasSideEffect);

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
    case 4: return implicitCast(node, child, kTypeBool32);
    case 8: return implicitCast(node, child, kTypeBool64);

    default:
      return _errorReporter->onError(kErrorInvalidProgram, node->_position,
        "%s from '%{Type}' to '__bool'.", "Invalid bool cast", child->getTypeInfo());
  }
}

Error AstAnalysis::invalidCast(uint32_t position, const char* msg, uint32_t fromTypeInfo, uint32_t toTypeInfo) noexcept {
  return _errorReporter->onError(kErrorInvalidProgram, position,
    "%s from '%{Type}' to '%{Type}'.", msg, fromTypeInfo, toTypeInfo);
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"