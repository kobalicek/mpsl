// [MPSL]
// Shader-Like Mathematical Expression JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MPSL_EXPORTS

// [Dependencies - MPSL]
#include "./mpast_p.h"
#include "./mphash_p.h"
#include "./mpmath_p.h"
#include "./mputils_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [mpsl::mpAstNodeSize]
// ============================================================================

struct AstNodeSize {
  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE uint32_t getNodeType() const noexcept { return _nodeType; }
  MPSL_INLINE uint32_t getNodeSize() const noexcept { return _nodeSize; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint8_t _nodeType;
  uint8_t _reserved;
  uint16_t _nodeSize;
};

#define ROW(type, size) { type, 0, static_cast<uint8_t>(size) }
static const AstNodeSize mpAstNodeSize[] = {
  ROW(kAstNodeNone     , 0                   ),
  ROW(kAstNodeProgram  , sizeof(AstProgram)  ),
  ROW(kAstNodeFunction , sizeof(AstFunction) ),
  ROW(kAstNodeBlock    , sizeof(AstBlock)    ),
  ROW(kAstNodeBranch   , sizeof(AstBranch)   ),
  ROW(kAstNodeCondition, sizeof(AstCondition)),
  ROW(kAstNodeFor      , sizeof(AstLoop)     ),
  ROW(kAstNodeWhile    , sizeof(AstLoop)     ),
  ROW(kAstNodeDoWhile  , sizeof(AstLoop)     ),
  ROW(kAstNodeBreak    , sizeof(AstBreak)    ),
  ROW(kAstNodeContinue , sizeof(AstContinue) ),
  ROW(kAstNodeReturn   , sizeof(AstReturn)   ),
  ROW(kAstNodeVarDecl  , sizeof(AstVarDecl)  ),
  ROW(kAstNodeVarMemb  , sizeof(AstVarMemb)  ),
  ROW(kAstNodeVar      , sizeof(AstVar)      ),
  ROW(kAstNodeImm      , sizeof(AstImm)      ),
  ROW(kAstNodeUnaryOp  , sizeof(AstUnaryOp)  ),
  ROW(kAstNodeBinaryOp , sizeof(AstBinaryOp) ),
  ROW(kAstNodeCall     , sizeof(AstCall)     )
};
#undef ROW

// ============================================================================
// [mpsl::AstBuilder - Construction / Destruction]
// ============================================================================

AstBuilder::AstBuilder(Allocator* allocator) noexcept
  : _allocator(allocator),
    _globalScope(nullptr),
    _programNode(nullptr),
    _mainFunction(nullptr) {}
AstBuilder::~AstBuilder() noexcept {}

// ============================================================================
// [mpsl::AstBuilder - Factory]
// ============================================================================

AstScope* AstBuilder::newScope(AstScope* parent, uint32_t scopeType) noexcept {
  void* p = _allocator->alloc(sizeof(AstScope));
  if (p == nullptr)
    return nullptr;
  return new(p) AstScope(this, parent, scopeType);
}

void AstBuilder::deleteScope(AstScope* scope) noexcept {
  scope->~AstScope();
  _allocator->release(scope, sizeof(AstScope));
}

AstSymbol* AstBuilder::newSymbol(const StringRef& key, uint32_t hVal, uint32_t symbolType, uint32_t scopeType) noexcept {
  size_t kLen = key.getLength();
  void* p = _allocator->alloc(sizeof(AstSymbol) + kLen + 1);

  if (p == nullptr)
    return nullptr;

  char* kStr = static_cast<char*>(p) + sizeof(AstSymbol);
  ::memcpy(kStr, key.getData(), kLen);

  kStr[kLen] = '\0';
  return new(p) AstSymbol(kStr, static_cast<uint32_t>(kLen), hVal, symbolType, scopeType);
}

void AstBuilder::deleteSymbol(AstSymbol* symbol) noexcept {
  size_t kLen = symbol->getLength();
  symbol->~AstSymbol();
  _allocator->release(symbol, sizeof(AstSymbol) + kLen + 1);
}

void AstBuilder::deleteNode(AstNode* node) noexcept {
  uint32_t length = node->getLength();
  AstNode** children = node->getChildren();

  for (uint32_t i = 0; i < length; i++) {
    AstNode* child = children[i];
    if (child != nullptr)
      deleteNode(child);
  }

  uint32_t nodeType = node->getNodeType();
  MPSL_ASSERT(mpAstNodeSize[nodeType].getNodeType() == nodeType);

  switch (nodeType) {
    case kAstNodeProgram  : static_cast<AstProgram*  >(node)->destroy(this); break;
    case kAstNodeFunction : static_cast<AstFunction* >(node)->destroy(this); break;
    case kAstNodeBlock    : static_cast<AstBlock*    >(node)->destroy(this); break;
    case kAstNodeBranch   : static_cast<AstBranch*   >(node)->destroy(this); break;
    case kAstNodeCondition: static_cast<AstCondition*>(node)->destroy(this); break;
    case kAstNodeFor      :
    case kAstNodeWhile    :
    case kAstNodeDoWhile  : static_cast<AstLoop*     >(node)->destroy(this); break;
    case kAstNodeBreak    : static_cast<AstBreak*    >(node)->destroy(this); break;
    case kAstNodeContinue : static_cast<AstContinue* >(node)->destroy(this); break;
    case kAstNodeReturn   : static_cast<AstReturn*   >(node)->destroy(this); break;
    case kAstNodeVarDecl  : static_cast<AstVarDecl*  >(node)->destroy(this); break;
    case kAstNodeVarMemb  : static_cast<AstVarMemb*  >(node)->destroy(this); break;
    case kAstNodeVar      : static_cast<AstVar*      >(node)->destroy(this); break;
    case kAstNodeImm      : static_cast<AstImm*      >(node)->destroy(this); break;
    case kAstNodeUnaryOp  : static_cast<AstUnaryOp*  >(node)->destroy(this); break;
    case kAstNodeBinaryOp : static_cast<AstBinaryOp* >(node)->destroy(this); break;
    case kAstNodeCall     : static_cast<AstCall*     >(node)->destroy(this); break;
  }

  _allocator->release(node, mpAstNodeSize[nodeType].getNodeSize());
}

// ============================================================================
// [mpsl::AstBuilder - Initialization]
// ============================================================================

Error AstBuilder::addProgramScope() noexcept {
  if (_globalScope == nullptr) {
    _globalScope = newScope(nullptr, kAstScopeGlobal);
    MPSL_NULLCHECK(_globalScope);
  }

  if (_programNode == nullptr) {
    _programNode = newNode<AstProgram>();
    MPSL_NULLCHECK(_programNode);
  }

  return kErrorOk;
}

// TODO: Not sure this is a right place for these functions.
Error AstBuilder::addBuiltInTypes(const TypeInfo* data, size_t count) noexcept {
  AstScope* scope = getGlobalScope();
  if (scope == nullptr)
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  for (uint32_t i = 0; i < kTypeCount; i++) {
    const TypeInfo& typeInfo = mpTypeInfo[i];
    uint32_t id = typeInfo.typeId;

    // 'void' is considered a keyword, skipped from registration.
    if (id == kTypeVoid)
      continue;

    size_t nLen = ::strlen(typeInfo.name);
    char buffer[16];

    ::memcpy(buffer, typeInfo.name, nLen);
    buffer[nLen + 0] = '\0';
    buffer[nLen + 1] = '\0';
    StringRef name(buffer, nLen);

    // Only register vector types if the type is not object.
    uint32_t j = 1;
    uint32_t jMax = typeInfo.maxElements;

    do {
      // Ban 5-7 element vectors, even if the vector supports up to 8.
      if (j >= 5 && j <= 7)
        continue;

      // Vector type suffix.
      uint32_t typeInfo = id;
      if (j > 1) {
        typeInfo |= j << kTypeVecShift;
        buffer[nLen] = static_cast<char>(j + '0');
        name._length = nLen + 1;
      }

      uint32_t hVal = HashUtils::hashString(name);
      AstSymbol* symbol = newSymbol(name, hVal, kAstSymbolTypeName, kAstScopeGlobal);
      MPSL_NULLCHECK(symbol);

      symbol->setDeclared();
      symbol->setTypeInfo(typeInfo);

      scope->putSymbol(symbol);
    } while (++j <= jMax);
  }

  return kErrorOk;
}

Error AstBuilder::addBuiltInConstants(const ConstInfo* data, size_t count) noexcept {
  AstScope* scope = getGlobalScope();
  if (scope == nullptr)
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  for (uint32_t i = 0; i < count; i++) {
    const ConstInfo& constInfo = data[i];

    StringRef name(constInfo.name, ::strlen(constInfo.name));
    uint32_t hVal = HashUtils::hashString(name);

    AstSymbol* symbol = newSymbol(name, hVal, kAstSymbolVariable, kAstScopeGlobal);
    MPSL_NULLCHECK(symbol);

    symbol->setTypeInfo(kTypeDouble | kTypeRead);
    symbol->setDeclared();
    symbol->setAssigned();
    symbol->_value.d[0] = constInfo.value;

    scope->putSymbol(symbol);
  }

  return kErrorOk;
}

Error AstBuilder::addBuiltInIntrinsics() noexcept {
  AstScope* scope = getGlobalScope();
  if (scope == nullptr)
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  for (uint32_t i = kOpNone + 1; i < kOpCount; i++) {
    const OpInfo& op = OpInfo::get(i);
    MPSL_ASSERT(op.type == i);

    if (!op.isIntrinsic())
      continue;

    StringRef name(op.name, ::strlen(op.name));
    uint32_t hVal = HashUtils::hashString(name);

    AstSymbol* symbol = newSymbol(name, hVal, kAstSymbolIntrinsic, kAstScopeGlobal);
    MPSL_NULLCHECK(symbol);

    symbol->setDeclared();
    symbol->setOpType(op.type);

    scope->putSymbol(symbol);
  }

  return kErrorOk;
}

Error AstBuilder::addBuiltInObject(uint32_t slot, const Layout* layout, AstSymbol** collidedSymbol) noexcept {
  AstScope* scope = getGlobalScope();
  if (scope == nullptr)
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  StringRef name;

  bool isAnonymous = !layout->hasName();
  char anonymousName[32];

  if (isAnonymous) {
    snprintf(anonymousName, 31, "@arg%u", static_cast<unsigned int>(slot));
    name.set(anonymousName, ::strlen(anonymousName));
    isAnonymous = true;
  }
  else {
    name.set(layout->getName(), layout->getNameLength());
  }

  uint32_t hVal = HashUtils::hashString(name);
  AstSymbol* symbol = scope->getSymbol(name, hVal);

  if (symbol != nullptr) {
    *collidedSymbol = symbol;
    return MPSL_TRACE_ERROR(kErrorSymbolCollision);
  }

  // Create the root object symbol even if it's anonymous.
  symbol = newSymbol(name, hVal, kAstSymbolVariable, kAstScopeGlobal);
  MPSL_NULLCHECK(symbol);

  symbol->setDeclared();
  symbol->setTypeInfo(kTypePtr);
  symbol->setDataSlot(slot);
  symbol->setLayout(layout);
  scope->putSymbol(symbol);

  // Create all members if the root is anonymous or a member has `kTypeDenest`.
  const Layout::Member* members = layout->getMembersArray();
  uint32_t count = layout->getMembersCount();

  // Filter to clear these flags as they are only used to define the layout.
  uint32_t kTypeInfoFilter = ~kTypeDenest;

  for (uint32_t i = 0; i < count; i++) {
    const Layout::Member* m = &members[i];
    uint32_t typeInfo = m->typeInfo;

    bool denest = isAnonymous || ((typeInfo & kTypeDenest) != 0) || (m->name[0] == '@');
    if (denest) {
      name.set(m->name, m->nameLength);
      hVal = HashUtils::hashString(name);

      symbol = scope->getSymbol(name, hVal);
      if (symbol != nullptr) {
        *collidedSymbol = symbol;
        return MPSL_TRACE_ERROR(kErrorSymbolCollision);
      }

      symbol = newSymbol(name, hVal, kAstSymbolVariable, kAstScopeGlobal);
      MPSL_NULLCHECK(symbol);

      // NOTE: Denested symbols don't have a data layout as they don't act
      // as objects, but variables. However, denested variables need data
      // offset, as it's then used to access memory of its data slot.
      symbol->setDeclared();
      symbol->setTypeInfo(m->typeInfo & kTypeInfoFilter);
      symbol->setDataSlot(slot);
      symbol->setDataOffset(m->offset);
      scope->putSymbol(symbol);
    }
  }

  return kErrorOk;
}

// ============================================================================
// [mpsl::AstBuilder - Dump]
// ============================================================================

Error AstBuilder::dump(StringBuilder& sb) noexcept {
  return AstDump(this, sb).onProgram(getProgramNode());
}

// ============================================================================
// [mpsl::AstScope - Construction / Destruction]
// ============================================================================

struct AstScopeReleaseHandler {
  MPSL_INLINE AstScopeReleaseHandler(AstBuilder* ast) noexcept : _ast(ast) {}
  MPSL_INLINE void release(AstSymbol* node) noexcept { _ast->deleteSymbol(node); }

  AstBuilder* _ast;
};

AstScope::AstScope(AstBuilder* ast, AstScope* parent, uint32_t scopeType) noexcept
  : _ast(ast),
    _parent(parent),
    _symbols(ast->getAllocator()),
    _scopeType(static_cast<uint8_t>(scopeType)) {}

AstScope::~AstScope() noexcept {
  AstScopeReleaseHandler handler(_ast);
  _symbols.reset(handler);
}

// ============================================================================
// [mpsl::AstScope - Ops]
// ============================================================================

AstSymbol* AstScope::resolveSymbol(const StringRef& name, uint32_t hVal, AstScope** scopeOut) noexcept {
  AstScope* scope = this;
  AstSymbol* symbol;

  do {
    symbol = scope->_symbols.get(name, hVal);
  } while (symbol == nullptr && (scope = scope->getParent()) != nullptr);

  if (scopeOut != nullptr)
    *scopeOut = scope;

  return symbol;
}

// ============================================================================
// [mpsl::AstNode - Ops]
// ============================================================================

AstNode* AstNode::replaceNode(AstNode* refNode, AstNode* node) noexcept {
  MPSL_ASSERT(refNode != nullptr);
  MPSL_ASSERT(refNode->getParent() == this);
  MPSL_ASSERT(node == nullptr || !node->hasParent());

  uint32_t length = _length;
  AstNode** children = getChildren();

  for (uint32_t i = 0; i < length; i++) {
    AstNode* child = children[i];

    if (child != refNode)
      continue;

    children[i] = node;
    refNode->_parent = nullptr;

    if (node != nullptr)
      node->_parent = this;

    return refNode;
  }

  return nullptr;
}

AstNode* AstNode::replaceAt(uint32_t index, AstNode* node) noexcept {
  AstNode* child = getAt(index);
  _children[index] = node;

  if (child != nullptr)
    child->_parent = nullptr;

  if (node != nullptr)
    node->_parent = this;

  return child;
}

AstNode* AstNode::injectNode(AstNode* refNode, AstUnary* node) noexcept {
  MPSL_ASSERT(refNode != nullptr && refNode->getParent() == this);
  MPSL_ASSERT(node != nullptr && node->getParent() == nullptr);

  uint32_t length = _length;
  AstNode** children = getChildren();

  for (uint32_t i = 0; i < length; i++) {
    AstNode* child = children[i];

    if (child != refNode)
      continue;

    children[i] = node;
    refNode->_parent = node;

    node->_parent = this;
    node->_child = refNode;

    return refNode;
  }

  return nullptr;
}

AstNode* AstNode::injectAt(uint32_t index, AstUnary* node) noexcept {
  AstNode* child = getAt(index);

  MPSL_ASSERT(node != nullptr && node->getParent() == nullptr);
  MPSL_ASSERT(child != nullptr);

  _children[index] = node;
  child->_parent = node;

  node->_parent = this;
  node->_child = child;

  return child;
}

// ============================================================================
// [mpsl::AstBlock - Ops]
// ============================================================================

static Error mpBlockNodeGrow(AstBlock* self) noexcept {
  size_t oldCapacity = self->_capacity;
  size_t newCapacity = oldCapacity;

  size_t length = self->_length;
  MPSL_ASSERT(oldCapacity == length);

  // Grow, we prefer growing quickly until we reach 128 and then 1024 nodes. We
  // don't expect to reach these limits in the most used expressions; only test
  // cases can exploit this assumption.
  //
  // Growing schema:
  //   0..4..8..16..32..64..128..256..384..512..640..768..896..1024..[+256]
  if (newCapacity == 0)
    newCapacity = 4;
  else if (newCapacity < 128)
    newCapacity *= 2;
  else if (newCapacity < 1024)
    newCapacity += 128;
  else
    newCapacity += 256;

  Allocator* allocator = self->getAst()->getAllocator();

  AstNode** oldArray = self->getChildren();
  AstNode** newArray = static_cast<AstNode**>(allocator->alloc(newCapacity * sizeof(AstNode), newCapacity));

  MPSL_NULLCHECK(newArray);
  newCapacity /= sizeof(AstNode*);

  self->_children = newArray;
  self->_capacity = static_cast<uint32_t>(newCapacity);

  if (oldCapacity != 0) {
    ::memcpy(newArray, oldArray, length * sizeof(AstNode*));
    allocator->release(oldArray, oldCapacity * sizeof(AstNode*));
  }

  return kErrorOk;
}

Error AstBlock::willAdd() noexcept {
  // Grow if needed.
  if (_length == _capacity)
    MPSL_PROPAGATE(mpBlockNodeGrow(this));
  return kErrorOk;
}

AstNode* AstBlock::removeNode(AstNode* node) noexcept {
  MPSL_ASSERT(node != nullptr);
  MPSL_ASSERT(node->getParent() == this);

  AstNode** p = getChildren();
  AstNode** pEnd = p + _length;

  while (p != pEnd) {
    if (p[0] == node)
      goto _Found;
    p++;
  }

  // If removeNode() has been called we expect the node to be found. Otherwise
  // there is a bug somewhere.
  MPSL_ASSERT(!"Reached");
  return nullptr;

_Found:
  _length--;
  ::memmove(p, p + 1, static_cast<size_t>(pEnd - p - 1) * sizeof(AstNode*));

  node->_parent = nullptr;
  return node;
}

AstNode* AstBlock::removeAt(uint32_t index) noexcept {
  MPSL_ASSERT(index < _length);

  if (index >= _length)
    return nullptr;

  AstNode** p = getChildren() + index;
  AstNode* oldNode = p[0];

  _length--;
  ::memmove(p, p + 1, static_cast<size_t>(_length - index) * sizeof(AstNode*));

  oldNode->_parent = nullptr;
  return oldNode;
}

// ============================================================================
// [mpsl::AstFlow - Interface]
// ============================================================================

AstLoop* AstFlow::getLoop() const noexcept {
  AstNode* node = const_cast<AstFlow*>(this);
  for (;;) {
    node = node->getParent();

    if (node == nullptr)
      return nullptr;

    if (node->isLoop())
      return static_cast<AstLoop*>(node);
  }
}

// ============================================================================
// [mpsl::AstDump - OnNode]
// ============================================================================

Error AstDump::onProgram(AstProgram* node) noexcept {
  return onBlock(node);
}

Error AstDump::onFunction(AstFunction* node) noexcept {
  AstSymbol* funcSumbol = node->getFunc();
  AstSymbol* retSymbol = node->getRet();

  nest("%s() [Decl]", funcSumbol ? funcSumbol->getName() : static_cast<const char*>(nullptr));

  if (retSymbol) {
    nest("RetType");
    info("%s", retSymbol->getName());
    denest();
  }

  AstBlock* args = node->getArgs();
  if (args && args->getLength() > 0) {
    nest("Args");
    MPSL_PROPAGATE(onNode(args));
    denest();
  }

  if (node->hasBody())
    MPSL_PROPAGATE(onNode(node->getBody()));

  return denest();
}

Error AstDump::onBlock(AstBlock* node) noexcept {
  AstNode** children = node->getChildren();
  uint32_t i, count = node->getLength();

  for (i = 0; i < count; i++)
    MPSL_PROPAGATE(onNode(children[i]));

  return kErrorOk;
}

Error AstDump::onBranch(AstBranch* node) noexcept {
  return onBlock(node);
}

Error AstDump::onCondition(AstCondition* node) noexcept {
  // Get the type of the cond 'if', 'else if' or 'else'.
  const char* condName = "Cond";

  if (node->hasParent()) {
    if (static_cast<AstBlock*>(node->getParent())->getAt(0) == node)
      condName = "If";
    else if (node->hasExp())
      condName = "Else If";
    else
      condName = "Else";
  }

  nest("%s", condName);

  if (node->hasExp()) {
    nest("Exp");
    MPSL_PROPAGATE(onNode(node->getExp()));
    denest();
  }

  if (node->hasBody()) {
    nest("Body");
    MPSL_PROPAGATE(onNode(node->getBody()));
    denest();
  }

  return denest();
}

Error AstDump::onLoop(AstLoop* node) noexcept {
  uint32_t nodeType = node->getNodeType();

  nest(nodeType == kAstNodeFor     ? "For"   :
       nodeType == kAstNodeWhile   ? "While" :
       nodeType == kAstNodeDoWhile ? "Do"    : "<unknown>");

  if (node->hasInit()) {
    nest("Init");
    MPSL_PROPAGATE(onNode(node->getInit()));
    denest();
  }

  if (node->hasIter()) {
    nest("Iter");
    MPSL_PROPAGATE(onNode(node->getIter()));
    denest();
  }

  if (nodeType == kAstNodeDoWhile) {
    if (node->hasBody())
      MPSL_PROPAGATE(onNode(node->getBody()));

    if (node->hasCond()) {
      nest("Cond");
      MPSL_PROPAGATE(onNode(node->getCond()));
      denest();
    }
  }
  else {
    if (node->hasCond()) {
      nest("Cond");
      MPSL_PROPAGATE(onNode(node->getCond()));
      denest();
    }

    if (node->hasBody())
      MPSL_PROPAGATE(onNode(node->getBody()));
  }

  return denest();
}

Error AstDump::onBreak(AstBreak* node) noexcept {
  return info("Break");
}

Error AstDump::onContinue(AstContinue* node) noexcept {
  return info("Continue");
}

Error AstDump::onReturn(AstReturn* node) noexcept {
  nest("Return");
  if (node->hasChild())
    MPSL_PROPAGATE(onNode(node->getChild()));
  return denest();
}

Error AstDump::onVarDecl(AstVarDecl* node) noexcept {
  AstSymbol* sym = node->getSymbol();

  nest("%s [VarDecl:%{Type}]",
    sym ? sym->getName() : static_cast<const char*>(nullptr),
    sym ? sym->getTypeInfo() : kTypeVoid);

  if (node->hasChild())
    MPSL_PROPAGATE(onNode(node->getChild()));

  return denest();
}

Error AstDump::onVarMemb(AstVarMemb* node) noexcept {
  uint32_t typeInfo = node->getTypeInfo();

  nest(".%s [%{Type}]", node->getField().getData(), typeInfo);
  if (node->hasChild())
    MPSL_PROPAGATE(onNode(node->getChild()));
  return denest();
}

Error AstDump::onVar(AstVar* node) noexcept {
  AstSymbol* sym = node->getSymbol();
  return info("%s [%{Type}]",
    sym ? sym->getName() : static_cast<const char*>(nullptr),
    node->getTypeInfo());
}

Error AstDump::onImm(AstImm* node) noexcept {
  return info("%{Value} [%{Type}]", node->getTypeInfo(), &node->_value, node->getTypeInfo());
}

Error AstDump::onUnaryOp(AstUnaryOp* node) noexcept {
  uint32_t typeInfo = node->getTypeInfo();

  if (node->getOp() == kOpCast)
    nest("(%{Type})", typeInfo);
  else
    nest("%s [%{Type}]", OpInfo::get(node->getOp()).name, typeInfo);

  if (node->hasChild())
    MPSL_PROPAGATE(onNode(node->getChild()));

  return denest();
}

Error AstDump::onBinaryOp(AstBinaryOp* node) noexcept {
  nest("%s [%{Type}]", OpInfo::get(node->getOp()).name, node->getTypeInfo());

  if (node->hasLeft())
    MPSL_PROPAGATE(onNode(node->getLeft()));

  if (node->hasRight())
    MPSL_PROPAGATE(onNode(node->getRight()));

  return denest();
}

Error AstDump::onCall(AstCall* node) noexcept {
  AstSymbol* sym = node->getSymbol();

  nest("%s() [%{Type}]",
    sym ? sym->getName() : static_cast<const char*>(nullptr),
    node->getTypeInfo());

  onBlock(node);
  return denest();
}

// ============================================================================
// [mpsl::AstDump - Helpers]
// ============================================================================

Error AstDump::info(const char* fmt, ...) noexcept {
  va_list ap;
  va_start(ap, fmt);

  _sb.appendChars(' ', static_cast<size_t>(_level) * 2);
  Utils::vformat(_sb, fmt, ap);
  _sb.appendChar('\n');

  va_end(ap);
  return kErrorOk;
}

Error AstDump::nest(const char* fmt, ...) noexcept {
  va_list ap;
  va_start(ap, fmt);

  _sb.appendChars(' ', static_cast<size_t>(_level) * 2);
  Utils::vformat(_sb, fmt, ap);
  _sb.appendChar('\n');

  va_end(ap);
  _level++;

  return kErrorOk;
}

Error AstDump::denest() noexcept {
  MPSL_ASSERT(_level > 0);
  _level--;

  return kErrorOk;
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
