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
  return mpTypeInfo[typeId].size() == 4 ? src->u[0] != 0 : src->q[0] != 0;
}

static bool mpValueAsScalarDouble(double* dst, const Value* src, uint32_t typeInfo) noexcept {
  uint32_t typeId = typeInfo & kTypeIdMask;
  uint32_t count = TypeInfo::elementsOf(typeInfo);

  // Check whether all vectors are equivalent before converting to scalar.
  switch (mpTypeInfo[typeId].size()) {
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
  AstSymbol* ret = node->ret();

  _currentRet = ret;
  _isLocalScope = true;

  Error err = node->body() ? onNode(node->body()) : static_cast<Error>(kErrorOk);

  _currentRet = nullptr;
  _unreachable = false;

  _isConditional = false;
  _isLocalScope = false;

  return err;
}

Error AstOptimizer::onBlock(AstBlock* node) noexcept {
  // Prevent removing nodes that are not stored in pure `AstBlock`. For example
  // function call inherits from `AstBlock`, but it needs each expression passed.
  bool alterable = node->nodeType() == AstNode::kTypeBlock;

  uint32_t i = 0;
  uint32_t curCount = node->size();
  uint32_t oldCount;

  while (i < curCount) {
    if (isUnreachable() && alterable) {
_DeleteNode:
      _ast->deleteNode(node->removeAt(i));
      curCount--;
      continue;
    }

    bool wasUnreachable = isUnreachable();
    MPSL_PROPAGATE(onNode(node->childAt(i)));

    oldCount = curCount;
    curCount = node->size();

    if (curCount < oldCount) {
      if (!alterable)
        return MPSL_TRACE_ERROR(kErrorInvalidState);
      continue;
    }

    if (alterable && (wasUnreachable || node->childAt(i)->isImm()))
      goto _DeleteNode;

    i++;
  }

  return kErrorOk;
}

Error AstOptimizer::onBranch(AstBranch* node) noexcept {
  if (node->condition()) {
    MPSL_PROPAGATE(onNode(node->condition()));
    AstNode* condition = node->condition();

    if (condition->isImm()) {
      uint32_t typeId = condition->typeInfo() & kTypeIdMask;

      AstImm* imm = condition->as<AstImm>();
      AstNode* alwaysTaken = nullptr;
      AstNode* neverTaken = nullptr;

      // Simplify if (condition) {a} else {b} to either {a} or {b}.
      if (mpGetBooleanValue(&imm->_value, typeId)) {
        if (node->thenBody())
          alwaysTaken = node->unlinkThenBody();
        if (node->elseBody())
          neverTaken = node->unlinkElseBody();
      }
      else {
        if (node->thenBody())
          neverTaken = node->unlinkThenBody();
        if (node->elseBody())
          alwaysTaken = node->unlinkElseBody();
      }

      if (neverTaken)
        _ast->deleteNode(neverTaken);

      _ast->deleteNode(
        node->parent()->replaceNode(
          node, alwaysTaken));

      if (alwaysTaken) {
        bool prevUnreachable = _unreachable;
        MPSL_PROPAGATE(onNode(alwaysTaken));
        _unreachable = prevUnreachable;
      }

      return kErrorOk;
    }
  }

  if (node->thenBody()) {
    bool prevUnreachable = _unreachable;
    MPSL_PROPAGATE(onNode(node->thenBody()));
    _unreachable = prevUnreachable;
  }

  if (node->elseBody()) {
    bool prevUnreachable = _unreachable;
    MPSL_PROPAGATE(onNode(node->elseBody()));
    _unreachable = prevUnreachable;
  }

  return kErrorOk;
}

Error AstOptimizer::onLoop(AstLoop* node) noexcept {
  if (node->forInit())
    MPSL_PROPAGATE(onNode(node->forInit()));

  if (node->forIter())
    MPSL_PROPAGATE(onNode(node->forIter()));

  if (node->condition())
    MPSL_PROPAGATE(onNode(node->condition()));

  if (node->body()) {
    bool prevUnreachable = _unreachable;
    MPSL_PROPAGATE(onNode(node->body()));
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
  if (node->child())
    MPSL_PROPAGATE(onNode(node->child()));

  _unreachable = true;
  return kErrorOk;
}

Error AstOptimizer::onVarDecl(AstVarDecl* node) noexcept {
  AstSymbol* sym = node->symbol();

  if (node->child()) {
    MPSL_PROPAGATE(onNode(node->child()));
    AstNode* child = node->child();

    if (child->isImm())
      sym->setValue(static_cast<AstImm*>(child)->value());
  }

  return kErrorOk;
}

Error AstOptimizer::onVarMemb(AstVarMemb* node) noexcept {
  if (!node->child())
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  MPSL_PROPAGATE(onNode(node->child()));
  AstNode* child = node->child();

  uint32_t typeInfo = child->typeInfo();
  uint32_t typeId = typeInfo & kTypeIdMask;

  if (TypeInfo::isPtrId(typeId)) {
    if (child->nodeType() != AstNode::kTypeVar)
      return MPSL_TRACE_ERROR(kErrorInvalidState);

    // The field accesses an object's member.
    const AstSymbol* symbol = static_cast<AstVar*>(child)->symbol();
    const Layout* layout = symbol->layout();

    if (layout == nullptr)
      return MPSL_TRACE_ERROR(kErrorInvalidState);

    const Layout::Member* m = layout->member(node->field());
    if (m == nullptr)
      return _errorReporter->onError(kErrorInvalidProgram, node->position(),
        "Object '%s' doesn't have member '%s'", symbol->name(), node->field().data());

    node->setTypeInfo(m->typeInfo | kTypeRef | (typeInfo & kTypeRW));
    node->setOffset(m->offset);
  }
  else {
    // The field should access/shuffle a vector.
    if ((typeInfo & kTypeVecMask) == 0)
      return _errorReporter->onError(kErrorInvalidProgram, node->position(),
        "Type '%{Type}' doesn't have member '%s'", typeInfo, node->field().data());

    // TODO: Field
  }

  return kErrorOk;
}

Error AstOptimizer::onVar(AstVar* node) noexcept {
  AstSymbol* sym = node->symbol();
  uint32_t typeInfo = node->typeInfo();

  if (!isUnreachable() &&
      !isConditional() &&
      sym->isAssigned() &&
      !node->hasNodeFlag(AstNode::kFlagSideEffect)) {
    typeInfo |= kTypeRead;
    typeInfo &= ~(kTypeWrite | kTypeRef);

    AstImm* imm = _ast->newNode<AstImm>(sym->value(), typeInfo);
    _ast->deleteNode(node->parent()->replaceNode(node, imm));
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
  const OpInfo& op = OpInfo::get(node->opType());

  MPSL_PROPAGATE(onNode(node->child()));
  AstNode* child = node->child();

  if (!isUnreachable()) {
    if (child->isImm()) {
      AstImm* child = static_cast<AstImm*>(node->child());
      Value& value = child->_value;

      uint32_t dTypeInfo = node->typeInfo();
      uint32_t sTypeInfo = child->typeInfo();

      if (op.isCast()) {
        MPSL_PROPAGATE(
          Fold::foldCast(value, dTypeInfo, value, sTypeInfo));

        child->setTypeInfo(dTypeInfo);
        node->unlinkChild();
        node->parent()->replaceNode(node, child);
        _ast->deleteNode(node);
      }
      else if (op.isSwizzle()) {
        MPSL_PROPAGATE(
          Fold::foldSwizzle(node->swizzleArray(), value, value, dTypeInfo));

        child->setTypeInfo(dTypeInfo);
        node->unlinkChild();
        node->parent()->replaceNode(node, child);
        _ast->deleteNode(node);
      }
      else {
        MPSL_PROPAGATE(
          Fold::foldUnaryOp(op.type(), value, value, sTypeInfo));

        child->setTypeInfo(dTypeInfo);
        node->unlinkChild();
        node->parent()->replaceNode(node, child);
        _ast->deleteNode(node);
      }
    }
    // Evaluate an assignment.
    else if (!isConditional() && op.isAssignment() && child->isVar()) {
      AstSymbol* sym = static_cast<AstVar*>(child)->symbol();
      if (sym->isAssigned()) {
        Value newValue = sym->value();
        uint32_t typeInfo = child->typeInfo() & ~(kTypeRef | kTypeWrite);

        MPSL_PROPAGATE(
          Fold::foldUnaryOp(op.type(), sym->_value, sym->_value, typeInfo));

        // Only `(.)++` and `(.)--` operators keep the previous value.
        if (!(op.flags() & kOpFlagAssignPost))
          newValue = sym->value();

        AstImm* newNode = _ast->newNode<AstImm>(newValue, typeInfo);
        MPSL_NULLCHECK(newNode);

        AstNode* oldNode = node->parent()->replaceNode(node, newNode);
        _ast->deleteNode(oldNode);
      }
    }
    // Simplify `-(-(x))` -> `x` and `~(~(x))` -> `x`.
    else if (child->nodeType() == AstNode::kTypeUnaryOp && node->opType() == child->opType()) {
      if (node->opType() == kOpNeg || node->opType() == kOpBitNeg) {
        AstNode* childOfChild = static_cast<AstUnaryOp*>(child)->unlinkChild();
        node->parent()->replaceNode(node, childOfChild);
        _ast->deleteNode(node);
      }
    }

  }

  return kErrorOk;
}

Error AstOptimizer::onBinaryOp(AstBinaryOp* node) noexcept {
  const OpInfo& op = OpInfo::get(node->opType());

  AstNode* left = node->left();
  AstNode* right = node->right();

  if (op.isAssignment())
    left->addNodeFlags(AstNode::kFlagSideEffect);

  MPSL_PROPAGATE(onNode(left));
  left = node->left();

  // Support minimal evaluation of `&&` and `||` operators.
  if (left->isImm() && op.isLogical()) {
    AstImm* lVal = static_cast<AstImm*>(left);
    AstNode* result = nullptr;

    bool b = mpGetBooleanValue(&lVal->_value, lVal->typeInfo() & kTypeIdMask);
    if ((op.type() == kOpLogAnd &&  b) || // Fold `true  && x` -> `x`.
        (op.type() == kOpLogOr  && !b)) { // Fold `false || x` -> `x`.
      result = node->unlinkRight();
    }
    else if (
        (op.type() == kOpLogAnd &&  b) || // Fold `false && x` -> `false`.
        (op.type() == kOpLogOr  && !b)) { // Fold `true  || x` -> `true`.
      result = node->unlinkLeft();
    }

    if (result == nullptr)
      return MPSL_TRACE_ERROR(kErrorInvalidState);

    _ast->deleteNode(node->parent()->replaceNode(node, result));
    return onNode(result);
  }

  MPSL_PROPAGATE(onNode(right));
  right = node->right();

  if (!isUnreachable()) {
    uint32_t typeInfo = node->typeInfo();

    bool lIsImm = left->isImm();
    bool rIsImm = right->isImm();

    // If both nodes are values it's easy, just fold them into a single one.
    if (lIsImm && rIsImm) {
      AstImm* lNode = static_cast<AstImm*>(left);
      AstImm* rNode = static_cast<AstImm*>(right);

      MPSL_PROPAGATE(Fold::foldBinaryOp(op.type(),
          lNode->_value,
          lNode->_value, lNode->typeInfo(),
          rNode->_value, rNode->typeInfo()));
      lNode->setTypeInfo(typeInfo | kTypeRead);

      node->unlinkLeft();
      node->parent()->replaceNode(node, lNode);

      _ast->deleteNode(node);
    }
    // There is still a little optimization opportunity.
    else if (lIsImm) {
      AstImm* lNode = static_cast<AstImm*>(left);
      double val;

      if (TypeInfo::isIntOrFPType(typeInfo) && mpValueAsScalarDouble(&val, &lNode->_value, left->typeInfo())) {
        if ((val == 0.0 && (op.flags() & kOpFlagNopIfL0)) ||
            (val == 1.0 && (op.flags() & kOpFlagNopIfL1))) {
          node->unlinkRight();
          node->parent()->replaceNode(node, right);

          _ast->deleteNode(node);
        }
      }
    }
    else if (rIsImm) {
      AstImm* rNode = static_cast<AstImm*>(right);

      // Evaluate an assignment.
      if (!isConditional() && op.isAssignment() && left->isVar()) {
        AstSymbol* sym = static_cast<AstVar*>(left)->symbol();
        if (op.type() == kOpAssign || sym->isAssigned()) {
          MPSL_PROPAGATE(
            Fold::foldBinaryOp(op.type(),
              sym->_value,
              sym->_value, left->typeInfo(),
              rNode->_value, rNode->typeInfo()));

          typeInfo = (left->typeInfo() & ~(kTypeRef | kTypeWrite)) | kTypeRead;
          sym->setAssigned();

          AstImm* newNode = _ast->newNode<AstImm>(sym->value(), typeInfo);
          MPSL_NULLCHECK(newNode);

          AstNode* oldNode = node->parent()->replaceNode(node, newNode);
          _ast->deleteNode(oldNode);
        }
      }
      else {
        double val;
        if (TypeInfo::isIntOrFPType(typeInfo) && mpValueAsScalarDouble(&val, &rNode->_value, right->typeInfo())) {
          if ((val == 0.0 && (op.flags() & kOpFlagNopIfR0)) ||
              (val == 1.0 && (op.flags() & kOpFlagNopIfR1))) {
            node->unlinkLeft();
            node->parent()->replaceNode(node, left);

            _ast->deleteNode(node);
          }
        }
      }
    }
  }

  return kErrorOk;
}

Error AstOptimizer::onCall(AstCall* node) noexcept {
  AstSymbol* sym = node->symbol();
  uint32_t i, count = node->size();

  for (i = 0; i < count; i++)
    MPSL_PROPAGATE(onNode(node->childAt(i)));

  // TODO:

  return kErrorOk;
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
