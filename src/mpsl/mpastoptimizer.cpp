// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MPSL_EXPORTS

// [Dependencies - MPSL]
#include "./mpastoptimizer_p.h"
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
  uint32_t i = 0, count = node->getLength();
  while (i < count) {
    AstCondition* cond = static_cast<AstCondition*>(node->getAt(i));
    if (cond->hasExp())
      MPSL_PROPAGATE(onNode(cond->getExp()));

    AstNode* exp = cond->getExp();
    bool alwaysTaken = false;

    if (exp == nullptr) {
      if (i != count - 1)
        return MPSL_TRACE_ERROR(kErrorInvalidState);

      // If this is the only node we remove the branch. This case will never
      // be produced by a parser or semantic analysis, but can be generated
      // by the optimizer in case that previous branches were removed.
      alwaysTaken = (i == 0);
    }
    else {
      uint32_t typeId = exp->getTypeInfo() & kTypeIdMask;
      if (exp->isImm()) {
        AstImm* imm = static_cast<AstImm*>(exp);
        alwaysTaken = mpGetBooleanValue(&imm->_value, typeId);

        if (!alwaysTaken) {
          // Never taken - just remove this branch and keep others.
          _ast->deleteNode(node->removeAt(i));
          count--;

          // Prevent incrementing `i`.
          continue;
        }
      }
    }

    if (cond->hasBody()) {
      bool prevUnreachable = _unreachable;
      MPSL_PROPAGATE(onNode(cond->getBody()));
      _unreachable = prevUnreachable;
    }

    if (alwaysTaken) {
      if (i == 0) {
        // If this is the first branch we will simplify `if/else (a) {b}` to `{b}`.
        AstNode* body = cond->unlinkBody();
        _ast->deleteNode(node->getParent()->replaceNode(node, body));
      }
      else {
        // Otherwise this will become the `else` clause.
        while (--count > i)
          _ast->deleteNode(node->removeAt(count));
      }
      return kErrorOk;
    }

    i++;
  }

  return kErrorOk;
}

Error AstOptimizer::onCondition(AstCondition* node) noexcept {
  MPSL_ASSERT(!"Reached");
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
      Value* value = &child->_value;

      uint32_t dTypeInfo = node->getTypeInfo();
      uint32_t sTypeInfo = child->getTypeInfo();

      if (op.isCast()) {
        MPSL_PROPAGATE(foldCast(node->getPosition(), value, dTypeInfo, value, sTypeInfo));
        child->setTypeInfo(dTypeInfo);

        node->unlinkChild();
        node->getParent()->replaceNode(node, child);

        _ast->deleteNode(node);
      }
      else {
        MPSL_PROPAGATE(foldUnaryOp(node->getPosition(), value, value, sTypeInfo, op.type));
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

        MPSL_PROPAGATE(foldUnaryOp(node->getPosition(), &sym->_value, &sym->_value, typeInfo, op.type));

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

      MPSL_PROPAGATE(
        foldBinaryOp(node->getPosition(),
          &lNode->_value,
          &lNode->_value, lNode->getTypeInfo(),
          &rNode->_value, rNode->getTypeInfo(), op.type));
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
            foldBinaryOp(node->getPosition(),
              &sym->_value,
              &sym->_value, left->getTypeInfo(),
              &rNode->_value, rNode->getTypeInfo(), op.type));

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

  return kErrorOk;
}

// ============================================================================
// [mpsl::AstOptimizer - Utilities - Cast]
// ============================================================================

#define COMB_DST_SRC(dstTypeId, srcTypeId) (((dstTypeId) << 4) | (srcTypeId))

Error AstOptimizer::foldCast(uint32_t position,
  Value* dVal, uint32_t dTypeInfo,
  const Value* sVal, uint32_t sTypeInfo) noexcept {

  Value tVal;
  if (dVal == sVal)
    sVal = static_cast<Value*>(::memcpy(&tVal, sVal, sizeof(Value)));

  uint32_t dTypeId = dTypeInfo & kTypeIdMask;
  uint32_t sTypeId = sTypeInfo & kTypeIdMask;

  uint32_t dVecSize = TypeInfo::elementsOf(dTypeInfo);
  uint32_t sVecSize = TypeInfo::elementsOf(sTypeInfo);

  uint32_t i = 0;
  uint32_t count = dVecSize;
  uint32_t fetchDouble = 0;

  if (dVecSize != sVecSize) {
    if (sVecSize == 1 && count > 1)
      count = 1;
    else
      return MPSL_TRACE_ERROR(kErrorInvalidState);
  }

  do {
    switch (COMB_DST_SRC(dTypeId, sTypeId)) {
      // 32-bit copy.
      case COMB_DST_SRC(kTypeBool  , kTypeBool  ):
      case COMB_DST_SRC(kTypeInt   , kTypeInt   ):
      case COMB_DST_SRC(kTypeFloat , kTypeFloat ): dVal->u[i] = sVal->u[i]; break;

      // 64-bit copy.
      case COMB_DST_SRC(kTypeQBool , kTypeQBool):
      case COMB_DST_SRC(kTypeDouble, kTypeDouble): dVal->q[i] = sVal->q[i]; break;

      // Cast to a boolean.
      case COMB_DST_SRC(kTypeBool  , kTypeQBool ): dVal->u[i] = sVal->q[i] ? kB32_1 : kB32_0; break;
      case COMB_DST_SRC(kTypeQBool , kTypeBool  ): dVal->u[i] = sVal->u[i] ? kB64_1 : kB64_0; break;

      // Cast to an integer.
      case COMB_DST_SRC(kTypeInt   , kTypeBool  ): dVal->i[i] = sVal->u[i] ? 1 : 0; break;
      case COMB_DST_SRC(kTypeInt   , kTypeQBool ): dVal->i[i] = sVal->q[i] ? 1 : 0; break;
      case COMB_DST_SRC(kTypeInt   , kTypeFloat ): dVal->i[i] = static_cast<int>(sVal->f[i]); break;
      case COMB_DST_SRC(kTypeInt   , kTypeDouble): dVal->i[i] = static_cast<int>(sVal->d[i]); break;

      // Cast to float.
      case COMB_DST_SRC(kTypeFloat , kTypeBool  ): dVal->f[i] = sVal->u[i] ? 1.0f : 0.0f; break;
      case COMB_DST_SRC(kTypeFloat , kTypeQBool ): dVal->f[i] = sVal->q[i] ? 1.0f : 0.0f; break;
      case COMB_DST_SRC(kTypeFloat , kTypeInt   ): dVal->f[i] = static_cast<float>(sVal->i[i]); break;
      case COMB_DST_SRC(kTypeFloat , kTypeDouble): dVal->f[i] = static_cast<float>(sVal->d[i]); break;

      // Cast to double.
      case COMB_DST_SRC(kTypeDouble, kTypeBool  ): dVal->d[i] = sVal->u[i] ? 1.0 : 0.0; break;
      case COMB_DST_SRC(kTypeDouble, kTypeQBool ): dVal->d[i] = sVal->q[i] ? 1.0 : 0.0; break;
      case COMB_DST_SRC(kTypeDouble, kTypeInt   ): dVal->d[i] = static_cast<double>(sVal->i[i]); break;
      case COMB_DST_SRC(kTypeDouble, kTypeFloat ): dVal->d[i] = static_cast<double>(sVal->f[i]); break;

      default:
        return MPSL_TRACE_ERROR(kErrorInvalidState);
    }
  } while (++i < count);

  // Expand scalar to vector.
  if (i < dVecSize) {
    switch (mpTypeInfo[dTypeId].size) {
      case 4: do { dVal->u[i] = dVal->u[0]; } while (++i < dVecSize); break;
      case 8: do { dVal->q[i] = dVal->q[0]; } while (++i < dVecSize); break;
    }
  }

  return kErrorOk;
}

#undef COMB_DST_SRC

// ============================================================================
// [mpsl::AstOptimizer - Utilities - UnaryOp]
// ============================================================================

#define COMB_SPEC(op, typeId) (((op) << 8) | (typeId))
#define COMB_PROC(code) do code while (++i < count); break

Error AstOptimizer::foldUnaryOp(uint32_t position, Value* dVal,
  const Value* sVal, uint32_t typeInfo, uint32_t op) noexcept {

  Value tVal;
  uint32_t typeId = typeInfo & kTypeIdMask;

  uint32_t i = 0;
  uint32_t count = TypeInfo::elementsOf(typeInfo);

  // The result is bool if the operator is '!'.
  if (op == kOpNot) {
    typeId = TypeInfo::sizeOf(typeId) == 4 ? kTypeBool : kTypeQBool;
    foldCast(position, &tVal, typeId | (typeInfo & ~kTypeIdMask), sVal, typeInfo);

    sVal = &tVal;
  }

  switch (COMB_SPEC(op, typeId)) {
    case COMB_SPEC(kOpPreInc   , kTypeInt   ):
    case COMB_SPEC(kOpPostInc  , kTypeInt   ): COMB_PROC({ dVal->u[i] = sVal->u[i] + 1U; });
    case COMB_SPEC(kOpPreInc   , kTypeFloat ):
    case COMB_SPEC(kOpPostInc  , kTypeFloat ): COMB_PROC({ dVal->f[i] = sVal->f[i] + static_cast<float>(1); });
    case COMB_SPEC(kOpPreInc   , kTypeDouble):
    case COMB_SPEC(kOpPostInc  , kTypeDouble): COMB_PROC({ dVal->d[i] = sVal->d[i] + static_cast<double>(1); });

    case COMB_SPEC(kOpPreDec   , kTypeInt   ):
    case COMB_SPEC(kOpPostDec  , kTypeInt   ): COMB_PROC({ dVal->u[i] = sVal->u[i] - 1U; });
    case COMB_SPEC(kOpPreDec   , kTypeFloat ):
    case COMB_SPEC(kOpPostDec  , kTypeFloat ): COMB_PROC({ dVal->f[i] = sVal->f[i] - static_cast<float>(1); });
    case COMB_SPEC(kOpPreDec   , kTypeDouble):
    case COMB_SPEC(kOpPostDec  , kTypeDouble): COMB_PROC({ dVal->d[i] = sVal->d[i] - static_cast<double>(1); });

    case COMB_SPEC(kOpBitNeg   , kTypeBool  ):
    case COMB_SPEC(kOpBitNeg   , kTypeInt   ):
    case COMB_SPEC(kOpBitNeg   , kTypeFloat ): COMB_PROC({ dVal->u[i] = ~sVal->u[i]; });
    case COMB_SPEC(kOpBitNeg   , kTypeQBool ):
    case COMB_SPEC(kOpBitNeg   , kTypeDouble): COMB_PROC({ dVal->q[i] = ~sVal->q[i]; });

    case COMB_SPEC(kOpNeg      , kTypeInt   ): COMB_PROC({ dVal->i[i] = -sVal->i[i]; });
    case COMB_SPEC(kOpNeg      , kTypeFloat ): COMB_PROC({ dVal->f[i] = -sVal->f[i]; });
    case COMB_SPEC(kOpNeg      , kTypeDouble): COMB_PROC({ dVal->d[i] = -sVal->d[i]; });

    case COMB_SPEC(kOpNot      , kTypeBool  ):
    case COMB_SPEC(kOpNeg      , kTypeBool  ): COMB_PROC({ dVal->i[i] = sVal->u[i] ^ kB32_1; });
    case COMB_SPEC(kOpNot      , kTypeQBool ):
    case COMB_SPEC(kOpNeg      , kTypeQBool ): COMB_PROC({ dVal->q[i] = sVal->q[i] ^ kB64_1; });

    case COMB_SPEC(kOpIsNan    , kTypeFloat ): COMB_PROC({ dVal->u[i] = mpIsNanF(sVal->f[i]) ? kB32_1 : kB32_0; });
    case COMB_SPEC(kOpIsNan    , kTypeDouble): COMB_PROC({ dVal->q[i] = mpIsNanD(sVal->d[i]) ? kB64_1 : kB64_0; });
    case COMB_SPEC(kOpIsInf    , kTypeFloat ): COMB_PROC({ dVal->u[i] = mpIsInfF(sVal->f[i]) ? kB32_1 : kB32_0; });
    case COMB_SPEC(kOpIsInf    , kTypeDouble): COMB_PROC({ dVal->q[i] = mpIsInfD(sVal->d[i]) ? kB64_1 : kB64_0; });
    case COMB_SPEC(kOpIsFinite , kTypeFloat ): COMB_PROC({ dVal->u[i] = mpIsFiniteF(sVal->f[i]) ? kB32_1 : kB32_0; });
    case COMB_SPEC(kOpIsFinite , kTypeDouble): COMB_PROC({ dVal->q[i] = mpIsFiniteD(sVal->d[i]) ? kB64_1 : kB64_0; });

    case COMB_SPEC(kOpSignBit  , kTypeInt   ): COMB_PROC({ dVal->u[i] = sVal->u[i] >> 31; });
    case COMB_SPEC(kOpSignBit  , kTypeFloat ): COMB_PROC({ dVal->f[i] = mpSignBitF(sVal->f[i]) ? 1.0f : 0.0f; });
    case COMB_SPEC(kOpSignBit  , kTypeDouble): COMB_PROC({ dVal->q[i] = mpSignBitD(sVal->d[i]) ? 1.0  : 0.0 ; });

    case COMB_SPEC(kOpTrunc    , kTypeFloat ): COMB_PROC({ dVal->f[i] = mpTruncF(sVal->f[i]); });
    case COMB_SPEC(kOpTrunc    , kTypeDouble): COMB_PROC({ dVal->d[i] = mpTruncD(sVal->d[i]); });
    case COMB_SPEC(kOpFloor    , kTypeFloat ): COMB_PROC({ dVal->f[i] = mpFloorF(sVal->f[i]); });
    case COMB_SPEC(kOpFloor    , kTypeDouble): COMB_PROC({ dVal->d[i] = mpFloorD(sVal->d[i]); });
    case COMB_SPEC(kOpRound    , kTypeFloat ): COMB_PROC({ dVal->f[i] = mpRoundF(sVal->f[i]); });
    case COMB_SPEC(kOpRound    , kTypeDouble): COMB_PROC({ dVal->d[i] = mpRoundD(sVal->d[i]); });
    case COMB_SPEC(kOpRoundEven, kTypeFloat ): COMB_PROC({ dVal->f[i] = mpRoundEvenF(sVal->f[i]); });
    case COMB_SPEC(kOpRoundEven, kTypeDouble): COMB_PROC({ dVal->d[i] = mpRoundEvenD(sVal->d[i]); });
    case COMB_SPEC(kOpCeil     , kTypeFloat ): COMB_PROC({ dVal->f[i] = mpCeilF(sVal->f[i]); });
    case COMB_SPEC(kOpCeil     , kTypeDouble): COMB_PROC({ dVal->d[i] = mpCeilD(sVal->d[i]); });

    case COMB_SPEC(kOpAbs      , kTypeInt   ): COMB_PROC({ dVal->i[i] = mpAbsI(sVal->i[i]); });
    case COMB_SPEC(kOpAbs      , kTypeFloat ): COMB_PROC({ dVal->f[i] = mpAbsF(sVal->f[i]); });
    case COMB_SPEC(kOpAbs      , kTypeDouble): COMB_PROC({ dVal->d[i] = mpAbsD(sVal->d[i]); });
    case COMB_SPEC(kOpExp      , kTypeFloat ): COMB_PROC({ dVal->f[i] = mpExpF(sVal->f[i]); });
    case COMB_SPEC(kOpExp      , kTypeDouble): COMB_PROC({ dVal->d[i] = mpExpD(sVal->d[i]); });
    case COMB_SPEC(kOpLog      , kTypeFloat ): COMB_PROC({ dVal->f[i] = mpLogF(sVal->f[i]); });
    case COMB_SPEC(kOpLog      , kTypeDouble): COMB_PROC({ dVal->d[i] = mpLogD(sVal->d[i]); });
    case COMB_SPEC(kOpLog2     , kTypeFloat ): COMB_PROC({ dVal->f[i] = mpLog2F(sVal->f[i]); });
    case COMB_SPEC(kOpLog2     , kTypeDouble): COMB_PROC({ dVal->d[i] = mpLog2D(sVal->d[i]); });
    case COMB_SPEC(kOpLog10    , kTypeFloat ): COMB_PROC({ dVal->f[i] = mpLog10F(sVal->f[i]); });
    case COMB_SPEC(kOpLog10    , kTypeDouble): COMB_PROC({ dVal->d[i] = mpLog10D(sVal->d[i]); });
    case COMB_SPEC(kOpSqrt     , kTypeFloat ): COMB_PROC({ dVal->f[i] = mpSqrtF(sVal->f[i]); });
    case COMB_SPEC(kOpSqrt     , kTypeDouble): COMB_PROC({ dVal->d[i] = mpSqrtD(sVal->d[i]); });

    case COMB_SPEC(kOpSin      , kTypeFloat ): COMB_PROC({ dVal->f[i] = mpSinF(sVal->f[i]); });
    case COMB_SPEC(kOpSin      , kTypeDouble): COMB_PROC({ dVal->d[i] = mpSinD(sVal->d[i]); });
    case COMB_SPEC(kOpCos      , kTypeFloat ): COMB_PROC({ dVal->f[i] = mpCosF(sVal->f[i]); });
    case COMB_SPEC(kOpCos      , kTypeDouble): COMB_PROC({ dVal->d[i] = mpCosD(sVal->d[i]); });
    case COMB_SPEC(kOpTan      , kTypeFloat ): COMB_PROC({ dVal->f[i] = mpTanF(sVal->f[i]); });
    case COMB_SPEC(kOpTan      , kTypeDouble): COMB_PROC({ dVal->d[i] = mpTanD(sVal->d[i]); });

    case COMB_SPEC(kOpAsin     , kTypeFloat ): COMB_PROC({ dVal->f[i] = mpAsinF(sVal->f[i]); });
    case COMB_SPEC(kOpAsin     , kTypeDouble): COMB_PROC({ dVal->d[i] = mpAsinD(sVal->d[i]); });
    case COMB_SPEC(kOpAcos     , kTypeFloat ): COMB_PROC({ dVal->f[i] = mpAcosF(sVal->f[i]); });
    case COMB_SPEC(kOpAcos     , kTypeDouble): COMB_PROC({ dVal->d[i] = mpAcosD(sVal->d[i]); });
    case COMB_SPEC(kOpAtan     , kTypeFloat ): COMB_PROC({ dVal->f[i] = mpAtanF(sVal->f[i]); });
    case COMB_SPEC(kOpAtan     , kTypeDouble): COMB_PROC({ dVal->d[i] = mpAtanD(sVal->d[i]); });

    default:
      return _errorReporter->onError(kErrorInvalidProgram, position,
        "Invalid unary operation '%s' on target type '%{Type}'.", mpOpInfo[op].name, typeInfo);
  }

  return kErrorOk;
}

#undef COMP_PROC
#undef COMP_SPEC

// ============================================================================
// [mpsl::AstOptimizer - Utilities - BinaryOp]
// ============================================================================

#define COMB_SPEC(op, typeId) (((op) << 8) | (typeId))
#define COMB_PROC(code) do code while (++i < count); break
#define IDIV_CHECK(value) do { if (!value) goto _IDivByZero; } while(0)

Error AstOptimizer::foldBinaryOp(uint32_t position, Value* dst,
  const Value* lVal, uint32_t lTypeInfo,
  const Value* rVal, uint32_t rTypeInfo, uint32_t op) noexcept {

  uint32_t typeId = lTypeInfo & kTypeIdMask;
  uint32_t i = 0;
  uint32_t count = TypeInfo::elementsOf(rTypeInfo);

  // Keep only operator identifier without `Assign` part (except for `=`).
  op = OpInfo::get(op).altType;

  switch (COMB_SPEC(op, typeId)) {
    case COMB_SPEC(kOpAssign  , kTypeBool  ):
    case COMB_SPEC(kOpAssign  , kTypeInt   ):
    case COMB_SPEC(kOpAssign  , kTypeFloat ): COMB_PROC({ dst->u[i] = rVal->u[i];  });
    case COMB_SPEC(kOpAssign  , kTypeQBool ):
    case COMB_SPEC(kOpAssign  , kTypeDouble): COMB_PROC({ dst->q[i] = rVal->q[i];  });

    case COMB_SPEC(kOpEq      , kTypeBool  ):
    case COMB_SPEC(kOpEq      , kTypeInt   ): COMB_PROC({ dst->u[i] = lVal->u[i] == rVal->u[i] ? kB32_1 : kB32_0; });
    case COMB_SPEC(kOpEq      , kTypeFloat ): COMB_PROC({ dst->u[i] = lVal->f[i] == rVal->f[i] ? kB32_1 : kB32_0; });
    case COMB_SPEC(kOpEq      , kTypeQBool ): COMB_PROC({ dst->q[i] = lVal->q[i] == rVal->q[i] ? kB64_1 : kB64_0; });
    case COMB_SPEC(kOpEq      , kTypeDouble): COMB_PROC({ dst->q[i] = lVal->d[i] == rVal->d[i] ? kB64_1 : kB64_0; });

    case COMB_SPEC(kOpNe      , kTypeBool  ):
    case COMB_SPEC(kOpNe      , kTypeInt   ): COMB_PROC({ dst->u[i] = lVal->u[i] != rVal->u[i] ? kB32_1 : kB32_0; });
    case COMB_SPEC(kOpNe      , kTypeFloat ): COMB_PROC({ dst->u[i] = lVal->f[i] != rVal->f[i] ? kB32_1 : kB32_0; });
    case COMB_SPEC(kOpNe      , kTypeQBool ): COMB_PROC({ dst->q[i] = lVal->q[i] != rVal->q[i] ? kB64_1 : kB64_0; });
    case COMB_SPEC(kOpNe      , kTypeDouble): COMB_PROC({ dst->q[i] = lVal->d[i] != rVal->d[i] ? kB64_1 : kB64_0; });

    case COMB_SPEC(kOpLt      , kTypeInt   ): COMB_PROC({ dst->u[i] = lVal->i[i] <  rVal->i[i] ? kB32_1 : kB32_0; });
    case COMB_SPEC(kOpLt      , kTypeFloat ): COMB_PROC({ dst->u[i] = lVal->f[i] <  rVal->f[i] ? kB32_1 : kB32_0; });
    case COMB_SPEC(kOpLt      , kTypeDouble): COMB_PROC({ dst->q[i] = lVal->d[i] <  rVal->d[i] ? kB64_1 : kB64_0; });

    case COMB_SPEC(kOpLe      , kTypeInt   ): COMB_PROC({ dst->u[i] = lVal->i[i] <= rVal->i[i] ? kB32_1 : kB32_0; });
    case COMB_SPEC(kOpLe      , kTypeFloat ): COMB_PROC({ dst->u[i] = lVal->f[i] <= rVal->f[i] ? kB32_1 : kB32_0; });
    case COMB_SPEC(kOpLe      , kTypeDouble): COMB_PROC({ dst->q[i] = lVal->d[i] <= rVal->d[i] ? kB64_1 : kB64_0; });

    case COMB_SPEC(kOpGt      , kTypeInt   ): COMB_PROC({ dst->u[i] = lVal->i[i] >  rVal->i[i] ? kB32_1 : kB32_0; });
    case COMB_SPEC(kOpGt      , kTypeFloat ): COMB_PROC({ dst->u[i] = lVal->f[i] >  rVal->f[i] ? kB32_1 : kB32_0; });
    case COMB_SPEC(kOpGt      , kTypeDouble): COMB_PROC({ dst->q[i] = lVal->d[i] >  rVal->d[i] ? kB64_1 : kB64_0; });

    case COMB_SPEC(kOpGe      , kTypeInt   ): COMB_PROC({ dst->u[i] = lVal->i[i] >= rVal->i[i] ? kB32_1 : kB32_0; });
    case COMB_SPEC(kOpGe      , kTypeFloat ): COMB_PROC({ dst->u[i] = lVal->f[i] >= rVal->f[i] ? kB32_1 : kB32_0; });
    case COMB_SPEC(kOpGe      , kTypeDouble): COMB_PROC({ dst->q[i] = lVal->d[i] >= rVal->d[i] ? kB64_1 : kB64_0; });

    case COMB_SPEC(kOpAdd     , kTypeInt   ): COMB_PROC({ dst->u[i] = lVal->u[i] + rVal->u[i]; });
    case COMB_SPEC(kOpAdd     , kTypeFloat ): COMB_PROC({ dst->f[i] = lVal->f[i] + rVal->f[i]; });
    case COMB_SPEC(kOpAdd     , kTypeDouble): COMB_PROC({ dst->d[i] = lVal->d[i] + rVal->d[i]; });

    case COMB_SPEC(kOpSub     , kTypeInt   ): COMB_PROC({ dst->u[i] = lVal->u[i] - rVal->u[i]; });
    case COMB_SPEC(kOpSub     , kTypeFloat ): COMB_PROC({ dst->f[i] = lVal->f[i] - rVal->f[i]; });
    case COMB_SPEC(kOpSub     , kTypeDouble): COMB_PROC({ dst->d[i] = lVal->d[i] - rVal->d[i]; });

    case COMB_SPEC(kOpMul     , kTypeInt   ): COMB_PROC({ dst->i[i] = lVal->i[i] * rVal->i[i]; });
    case COMB_SPEC(kOpMul     , kTypeFloat ): COMB_PROC({ dst->f[i] = lVal->f[i] * rVal->f[i]; });
    case COMB_SPEC(kOpMul     , kTypeDouble): COMB_PROC({ dst->d[i] = lVal->d[i] * rVal->d[i]; });

    case COMB_SPEC(kOpDiv     , kTypeInt   ): COMB_PROC({ int32_t v = rVal->i[i]; IDIV_CHECK(v); dst->i[i] = lVal->i[i] / v; });
    case COMB_SPEC(kOpDiv     , kTypeFloat ): COMB_PROC({ dst->f[i] = lVal->f[i] / rVal->f[i]; });
    case COMB_SPEC(kOpDiv     , kTypeDouble): COMB_PROC({ dst->d[i] = lVal->d[i] / rVal->d[i]; });

    case COMB_SPEC(kOpMod     , kTypeInt   ): COMB_PROC({ int32_t v = rVal->i[i]; IDIV_CHECK(v); dst->i[i] = lVal->i[i] % v; });
    case COMB_SPEC(kOpMod     , kTypeFloat ): COMB_PROC({ dst->f[i] = mpModF(lVal->f[i], rVal->f[i]); });
    case COMB_SPEC(kOpMod     , kTypeDouble): COMB_PROC({ dst->d[i] = mpModD(lVal->d[i], rVal->d[i]); });

    case COMB_SPEC(kOpAnd     , kTypeBool  ):
    case COMB_SPEC(kOpLogAnd  , kTypeBool  ):
    case COMB_SPEC(kOpAnd     , kTypeInt   ):
    case COMB_SPEC(kOpAnd     , kTypeFloat ): COMB_PROC({ dst->u[i] = lVal->u[i] & rVal->u[i]; });
    case COMB_SPEC(kOpAnd     , kTypeQBool ):
    case COMB_SPEC(kOpLogAnd  , kTypeQBool ):
    case COMB_SPEC(kOpAnd     , kTypeDouble): COMB_PROC({ dst->q[i] = lVal->q[i] & rVal->q[i]; });

    case COMB_SPEC(kOpOr      , kTypeBool  ):
    case COMB_SPEC(kOpLogOr   , kTypeBool  ):
    case COMB_SPEC(kOpOr      , kTypeInt   ):
    case COMB_SPEC(kOpOr      , kTypeFloat ): COMB_PROC({ dst->u[i] = lVal->u[i] | rVal->u[i]; });
    case COMB_SPEC(kOpOr      , kTypeQBool ):
    case COMB_SPEC(kOpLogOr   , kTypeQBool ):
    case COMB_SPEC(kOpOr      , kTypeDouble): COMB_PROC({ dst->q[i] = lVal->q[i] | rVal->q[i]; });

    case COMB_SPEC(kOpXor     , kTypeBool  ):
    case COMB_SPEC(kOpXor     , kTypeInt   ):
    case COMB_SPEC(kOpXor     , kTypeFloat ): COMB_PROC({ dst->u[i] = lVal->u[i] ^ rVal->u[i]; });
    case COMB_SPEC(kOpXor     , kTypeQBool ):
    case COMB_SPEC(kOpXor     , kTypeDouble): COMB_PROC({ dst->q[i] = lVal->q[i] ^ rVal->q[i]; });

    case COMB_SPEC(kOpSll     , kTypeInt   ): COMB_PROC({ dst->u[i] = lVal->u[i] << (rVal->u[i] & 0x1F); });
    case COMB_SPEC(kOpSrl     , kTypeInt   ): COMB_PROC({ dst->u[i] = lVal->u[i] >> (rVal->u[i] & 0x1F); });
    case COMB_SPEC(kOpSra     , kTypeInt   ): COMB_PROC({ dst->i[i] = lVal->i[i] >> (rVal->i[i] & 0x1F); });
    case COMB_SPEC(kOpRol     , kTypeInt   ): COMB_PROC({ dst->u[i] = mpBitRol(lVal->u[i], rVal->u[i] & 0x1F); });
    case COMB_SPEC(kOpRor     , kTypeInt   ): COMB_PROC({ dst->u[i] = mpBitRor(lVal->u[i], rVal->u[i] & 0x1F); });

    case COMB_SPEC(kOpMin     , kTypeInt   ): COMB_PROC({ dst->i[i] = mpMin(lVal->i[i], rVal->i[i]); });
    case COMB_SPEC(kOpMin     , kTypeFloat ): COMB_PROC({ dst->f[i] = mpMin(lVal->f[i], rVal->f[i]); });
    case COMB_SPEC(kOpMin     , kTypeDouble): COMB_PROC({ dst->d[i] = mpMin(lVal->d[i], rVal->d[i]); });
    case COMB_SPEC(kOpMax     , kTypeInt   ): COMB_PROC({ dst->i[i] = mpMax(lVal->i[i], rVal->i[i]); });
    case COMB_SPEC(kOpMax     , kTypeFloat ): COMB_PROC({ dst->f[i] = mpMax(lVal->f[i], rVal->f[i]); });
    case COMB_SPEC(kOpMax     , kTypeDouble): COMB_PROC({ dst->d[i] = mpMax(lVal->d[i], rVal->d[i]); });

    case COMB_SPEC(kOpPow     , kTypeFloat ): COMB_PROC({ dst->f[i] = mpPowF(lVal->f[i], rVal->f[i]); });
    case COMB_SPEC(kOpPow     , kTypeDouble): COMB_PROC({ dst->d[i] = mpPowD(lVal->d[i], rVal->d[i]); });
    case COMB_SPEC(kOpAtan2   , kTypeFloat ): COMB_PROC({ dst->f[i] = mpAtan2F(lVal->f[i], rVal->f[i]); });
    case COMB_SPEC(kOpAtan2   , kTypeDouble): COMB_PROC({ dst->d[i] = mpAtan2D(lVal->d[i], rVal->d[i]); });
    case COMB_SPEC(kOpCopySign, kTypeFloat ): COMB_PROC({ dst->f[i] = mpCopySignF(lVal->f[i], rVal->f[i]); });
    case COMB_SPEC(kOpCopySign, kTypeDouble): COMB_PROC({ dst->d[i] = mpCopySignD(lVal->d[i], rVal->d[i]); });

    default:
      return MPSL_TRACE_ERROR(kErrorInvalidState);
  }
  return kErrorOk;

_IDivByZero:
  return _errorReporter->onError(kErrorIntegerDivisionByZero, position,
    "Integer division by zero: %V %s %V.",
      lTypeInfo, lVal, OpInfo::get(op).name, rTypeInfo, rVal);
}

#undef IDIV_CHECK
#undef COMB_PROC
#undef COMB_SPEC

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
