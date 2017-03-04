// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MPSL_EXPORTS

// [Dependencies - MPSL]
#include "./mpastoptimizer_p.h"
#include "./mpfold_p.h"
#include "./mpmath_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [mpsl::AstOptimizer - Construction / Destruction]
// ============================================================================

AstOptimizer::AstOptimizer(AstBuilder* ast, ErrorReporter* errorReporter) noexcept
  : AstVisitor<AstOptimizer>(ast),
    _errorReporter(errorReporter),
    _currentRet(nullptr),
    _unreachable(false),
    _isConditional(false),
    _isLocalScope(false) {}
AstOptimizer::~AstOptimizer() noexcept {}

// ============================================================================
// [mpsl::AstOptimizer - Utilities]
// ============================================================================

static MPSL_INLINE bool mpGetBooleanValue(const Value* src, uint32_t typeId) noexcept {
  return mpTypeInfo[typeId].size == 4 ? src->u[0] != 0 : src->q[0] != 0;
}

static bool mpValueAsScalarDouble(double* dst, const Value* src, uint32_t typeInfo) noexcept {
  uint32_t typeId = typeInfo & kTypeIdMask;
  uint32_t count = TypeInfo::elementsOf(typeInfo);

  // Check whether all vectors are equivalent before converting to scalar.
  switch (mpTypeInfo[typeId].size) {
    case 4: {
      uint32_t v = src->u[0];
      for (uint32_t i = 1; i < count; i++)
        if (src->u[i] != v)
          return false;
      break;
    }

    case 8: {
      uint64_t v = src->q[0];
      for (uint32_t i = 1; i < count; i++)
        if (src->q[i] != v)
          return false;
      break;
    }
  }

  double v;
  switch (typeId) {
    case kTypeBool   : v = src->u[0] ? 1.0 : 0.0; break;
    case kTypeQBool  : v = src->q[0] ? 1.0 : 0.0; break;
    case kTypeInt    : v = static_cast<double>(src->i[0]); break;
    case kTypeFloat  : v = static_cast<double>(src->f[0]); break;
    case kTypeDouble : v = src->d[0]; break;

    default:
      return false;
  }

  *dst = v;
  return true;
}

// ============================================================================
// [mpsl::AstOptimizer - OnNode]
// ============================================================================

Error AstOptimizer::onProgram(AstProgram* node) noexcept {
  return onBlock(node);
}

Error AstOptimizer::onFunction(AstFunction* node) noexcept {
  AstSymbol* ret = node->getRet();

  _currentRet = ret;
  _isLocalScope = true;

  Error err = node->hasBody() ? onNode(node->getBody()) : static_cast<Error>(kErrorOk);

  _currentRet = nullptr;
  _unreachable = false;

  _isConditional = false;
  _isLocalScope = false;

  return err;
}

Error AstOptimizer::onBlock(AstBlock* node) noexcept {
  // Prevent removing nodes that are not stored in pure `AstBlock`. For example
  // function call inherits from `AstBlock`, but it needs each expression passed.
  bool alterable = node->getNodeType() == AstNode::kTypeBlock;

  uint32_t i = 0;
  uint32_t curCount = node->getLength();
  uint32_t oldCount;

  while (i < curCount) {
    if (isUnreachable() && alterable) {
_DeleteNode:
      _ast->deleteNode(node->removeAt(i));
      curCount--;
      continue;
    }

    bool wasUnreachable = isUnreachable();
    MPSL_PROPAGATE(onNode(node->getAt(i)));

    oldCount = curCount;
    curCount = node->getLength();

    if (curCount < oldCount) {
      if (!alterable)
        return MPSL_TRACE_ERROR(kErrorInvalidState);
      continue;
    }

    if (alterable && (wasUnreachable || node->getAt(i)->isImm()))
      goto _DeleteNode;

    i++;
  }

  return kErrorOk;
}

Error AstOptimizer::onBranch(AstBranch* node) noexcept {
  if (node->hasCond()) {
    MPSL_PROPAGATE(onNode(node->getCond()));
    AstNode* cond = node->getCond();

    if (cond->isImm()) {
      uint32_t typeId = cond->getTypeInfo() & kTypeIdMask;

      AstImm* imm = cond->as<AstImm>();
      AstNode* alwaysTaken = nullptr;
      AstNode* neverTaken = nullptr;

      // Simplify if (cond) {a} else {b} to either {a} or {b}.
      if (mpGetBooleanValue(&imm->_value, typeId)) {
        if (node->hasThen())
          alwaysTaken = node->unlinkThen();
        if (node->hasElse())
          neverTaken = node->unlinkElse();
      }
      else {
        if (node->hasThen())
          neverTaken = node->unlinkThen();
        if (node->hasElse())
          alwaysTaken = node->unlinkElse();
      }

      if (neverTaken)
        _ast->deleteNode(neverTaken);

      _ast->deleteNode(
        node->getParent()->replaceNode(
          node, alwaysTaken));

      if (alwaysTaken) {
        bool prevUnreachable = _unreachable;
        MPSL_PROPAGATE(onNode(alwaysTaken));
        _unreachable = prevUnreachable;
      }

      return kErrorOk;
    }
  }

  if (node->hasThen()) {
    bool prevUnreachable = _unreachable;
    MPSL_PROPAGATE(onNode(node->getThen()));
    _unreachable = prevUnreachable;
  }

  if (node->hasElse()) {
    bool prevUnreachable = _unreachable;
    MPSL_PROPAGATE(onNode(node->getElse()));
    _unreachable = prevUnreachable;
  }

  return kErrorOk;
}

Error AstOptimizer::onLoop(AstLoop* node) noexcept {
  if (node->hasInit())
    MPSL_PROPAGATE(onNode(node->getInit()));

  if (node->hasIter())
    MPSL_PROPAGATE(onNode(node->getIter()));

  if (node->hasCond())
    MPSL_PROPAGATE(onNode(node->getCond()));

  if (node->hasBody()) {
    bool prevUnreachable = _unreachable;
    MPSL_PROPAGATE(onNode(node->getBody()));
    _unreachable = prevUnreachable;
  }

  return kErrorOk;
}

Error AstOptimizer::onBreak(AstBreak* node) noexcept {
  _unreachable = true;
  return kErrorOk;
}

Error AstOptimizer::onContinue(AstContinue* node) noexcept {
  _unreachable = true;
  return kErrorOk;
}

Error AstOptimizer::onReturn(AstReturn* node) noexcept {
  if (node->hasChild())
    MPSL_PROPAGATE(onNode(node->getChild()));

  _unreachable = true;
  return kErrorOk;
}

Error AstOptimizer::onVarDecl(AstVarDecl* node) noexcept {
  AstSymbol* sym = node->getSymbol();

  if (node->hasChild()) {
    MPSL_PROPAGATE(onNode(node->getChild()));
    AstNode* child = node->getChild();

    if (child->isImm())
      sym->setValue(static_cast<AstImm*>(child)->getValue());
  }

  return kErrorOk;
}

Error AstOptimizer::onVarMemb(AstVarMemb* node) noexcept {
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
    const AstSymbol* symbol = static_cast<AstVar*>(child)->getSymbol();
    const Layout* layout = symbol->getLayout();

    if (layout == nullptr)
      return MPSL_TRACE_ERROR(kErrorInvalidState);

    const Layout::Member* m = layout->getMember(node->getField());
    if (m == nullptr)
      return _errorReporter->onError(kErrorInvalidProgram, node->getPosition(),
        "Object '%s' doesn't have member '%s'", symbol->getName(), node->getField().getData());

    node->setTypeInfo(m->typeInfo | kTypeRef | (typeInfo & kTypeRW));
    node->setOffset(m->offset);
  }
  else {
    // The field should access/shuffle a vector.
    if ((typeInfo & kTypeVecMask) == 0)
      return _errorReporter->onError(kErrorInvalidProgram, node->getPosition(),
        "Type '%{Type}' doesn't have member '%s'", typeInfo, node->getField().getData());

    // TODO: Field
  }

  return kErrorOk;
}

Error AstOptimizer::onVar(AstVar* node) noexcept {
  AstSymbol* sym = node->getSymbol();
  uint32_t typeInfo = node->getTypeInfo();

  if (!isUnreachable() &&
      !isConditional() &&
      sym->isAssigned() &&
      !node->hasNodeFlag(AstNode::kFlagSideEffect)) {
    typeInfo |= kTypeRead;
    typeInfo &= ~(kTypeWrite | kTypeRef);

    AstImm* imm = _ast->newNode<AstImm>(sym->getValue(), typeInfo);
    _ast->deleteNode(node->getParent()->replaceNode(node, imm));
  }
  else {
    typeInfo |= kTypeRef;
    node->setTypeInfo(typeInfo);
  }

  return kErrorOk;
}

Error AstOptimizer::onImm(AstImm* node) noexcept {
  return kErrorOk;
}

Error AstOptimizer::onUnaryOp(AstUnaryOp* node) noexcept {
  const OpInfo& op = OpInfo::get(node->getOp());

  MPSL_PROPAGATE(onNode(node->getChild()));
  AstNode* child = node->getChild();

  if (!isUnreachable()) {
    if (child->isImm()) {
      AstImm* child = static_cast<AstImm*>(node->getChild());
      Value& value = child->_value;

      uint32_t dTypeInfo = node->getTypeInfo();
      uint32_t sTypeInfo = child->getTypeInfo();

      if (op.isCast()) {
        MPSL_PROPAGATE(
          Fold::foldCast(value, dTypeInfo, value, sTypeInfo));

        child->setTypeInfo(dTypeInfo);
        node->unlinkChild();
        node->getParent()->replaceNode(node, child);
        _ast->deleteNode(node);
      }
      else if (op.isSwizzle()) {
        MPSL_PROPAGATE(
          Fold::foldSwizzle(node->getSwizzleArray(), value, value, dTypeInfo));

        child->setTypeInfo(dTypeInfo);
        node->unlinkChild();
        node->getParent()->replaceNode(node, child);
        _ast->deleteNode(node);
      }
      else {
        MPSL_PROPAGATE(
          Fold::foldUnaryOp(op.type, value, value, sTypeInfo));

        child->setTypeInfo(dTypeInfo);
        node->unlinkChild();
        node->getParent()->replaceNode(node, child);
        _ast->deleteNode(node);
      }
    }
    // Evaluate an assignment.
    else if (!isConditional() && op.isAssignment() && child->isVar()) {
      AstSymbol* sym = static_cast<AstVar*>(child)->getSymbol();
      if (sym->isAssigned()) {
        Value newValue = sym->getValue();
        uint32_t typeInfo = child->getTypeInfo() & ~(kTypeRef | kTypeWrite);

        MPSL_PROPAGATE(
          Fold::foldUnaryOp(op.type, sym->_value, sym->_value, typeInfo));

        // Only `(.)++` and `(.)--` operators keep the previous value.
        if (!(op.flags & kOpFlagAssignPost))
          newValue = sym->getValue();

        AstImm* newNode = _ast->newNode<AstImm>(newValue, typeInfo);
        MPSL_NULLCHECK(newNode);

        AstNode* oldNode = node->getParent()->replaceNode(node, newNode);
        _ast->deleteNode(oldNode);
      }
    }
    // Simplify `-(-(x))` -> `x` and `~(~(x))` -> `x`.
    else if (child->getNodeType() == AstNode::kTypeUnaryOp && node->getOp() == child->getOp()) {
      if (node->getOp() == kOpNeg || node->getOp() == kOpBitNeg) {
        AstNode* childOfChild = static_cast<AstUnaryOp*>(child)->unlinkChild();
        node->getParent()->replaceNode(node, childOfChild);
        _ast->deleteNode(node);
      }
    }

  }

  return kErrorOk;
}

Error AstOptimizer::onBinaryOp(AstBinaryOp* node) noexcept {
  const OpInfo& op = OpInfo::get(node->getOp());

  AstNode* left = node->getLeft();
  AstNode* right = node->getRight();

  if (op.isAssignment())
    left->addNodeFlags(AstNode::kFlagSideEffect);

  MPSL_PROPAGATE(onNode(left));
  left = node->getLeft();

  // Support minimal evaluation of `&&` and `||` operators.
  if (left->isImm() && op.isLogical()) {
    AstImm* lVal = static_cast<AstImm*>(left);
    AstNode* result = nullptr;

    bool b = mpGetBooleanValue(&lVal->_value, lVal->getTypeInfo() & kTypeIdMask);
    if ((op.type == kOpLogAnd &&  b) || // Fold `true  && x` -> `x`.
        (op.type == kOpLogOr  && !b)) { // Fold `false || x` -> `x`.
      result = node->unlinkRight();
    }
    else if (
        (op.type == kOpLogAnd &&  b) || // Fold `false && x` -> `false`.
        (op.type == kOpLogOr  && !b)) { // Fold `true  || x` -> `true`.
      result = node->unlinkLeft();
    }

    if (result == nullptr)
      return MPSL_TRACE_ERROR(kErrorInvalidState);

    _ast->deleteNode(node->getParent()->replaceNode(node, result));
    return onNode(result);
  }

  MPSL_PROPAGATE(onNode(right));
  right = node->getRight();

  if (!isUnreachable()) {
    uint32_t typeInfo = node->getTypeInfo();

    bool lIsImm = left->isImm();
    bool rIsImm = right->isImm();

    // If both nodes are values it's easy, just fold them into a single one.
    if (lIsImm && rIsImm) {
      AstImm* lNode = static_cast<AstImm*>(left);
      AstImm* rNode = static_cast<AstImm*>(right);

      MPSL_PROPAGATE(Fold::foldBinaryOp(op.type,
          lNode->_value,
          lNode->_value, lNode->getTypeInfo(),
          rNode->_value, rNode->getTypeInfo()));
      lNode->setTypeInfo(typeInfo | kTypeRead);

      node->unlinkLeft();
      node->getParent()->replaceNode(node, lNode);

      _ast->deleteNode(node);
    }
    // There is still a little optimization opportunity.
    else if (lIsImm) {
      AstImm* lNode = static_cast<AstImm*>(left);
      double val;

      if (TypeInfo::isIntOrFPType(typeInfo) && mpValueAsScalarDouble(&val, &lNode->_value, left->getTypeInfo())) {
        if ((val == 0.0 && (op.flags & kOpFlagNopIfL0)) ||
            (val == 1.0 && (op.flags & kOpFlagNopIfL1))) {
          node->unlinkRight();
          node->getParent()->replaceNode(node, right);

          _ast->deleteNode(node);
        }
      }
    }
    else if (rIsImm) {
      AstImm* rNode = static_cast<AstImm*>(right);

      // Evaluate an assignment.
      if (!isConditional() && op.isAssignment() && left->isVar()) {
        AstSymbol* sym = static_cast<AstVar*>(left)->getSymbol();
        if (op.type == kOpAssign || sym->isAssigned()) {
          MPSL_PROPAGATE(
            Fold::foldBinaryOp(op.type,
              sym->_value,
              sym->_value, left->getTypeInfo(),
              rNode->_value, rNode->getTypeInfo()));

          typeInfo = (left->getTypeInfo() & ~(kTypeRef | kTypeWrite)) | kTypeRead;
          sym->setAssigned();

          AstImm* newNode = _ast->newNode<AstImm>(sym->getValue(), typeInfo);
          MPSL_NULLCHECK(newNode);

          AstNode* oldNode = node->getParent()->replaceNode(node, newNode);
          _ast->deleteNode(oldNode);
        }
      }
      else {
        double val;
        if (TypeInfo::isIntOrFPType(typeInfo) && mpValueAsScalarDouble(&val, &rNode->_value, right->getTypeInfo())) {
          if ((val == 0.0 && (op.flags & kOpFlagNopIfR0)) ||
              (val == 1.0 && (op.flags & kOpFlagNopIfR1))) {
            node->unlinkLeft();
            node->getParent()->replaceNode(node, left);

            _ast->deleteNode(node);
          }
        }
      }
    }
  }

  return kErrorOk;
}

Error AstOptimizer::onCall(AstCall* node) noexcept {
  AstSymbol* sym = node->getSymbol();
  uint32_t i, count = node->getLength();

  for (i = 0; i < count; i++)
    MPSL_PROPAGATE(onNode(node->getAt(i)));

  // TODO:

  return kErrorOk;
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
