// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MPSL_EXPORTS

// [Dependencies - MPSL]
#include "./mpast_p.h"
#include "./mpformatutils_p.h"
#include "./mphash_p.h"
#include "./mpmath_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [Utilities]
// ============================================================================

static MPSL_INLINE bool mpIsVarNodeType(uint32_t nodeType) noexcept {
  return nodeType == AstNode::kTypeVar || nodeType == AstNode::kTypeVarMemb;
}

static MPSL_INLINE uint32_t mpIndexSwizzle(const char* s, uint32_t len, char c) noexcept {
  for (uint32_t i = 0; i < len; i++)
    if (s[i] == c)
      return i;

  MPSL_ASSERT(!"Invalid VectorIdentifiers data.");
  return 0;
}

static uint32_t mpParseSwizzle(uint8_t* indexesOut, uint32_t& highestIndexOut, const StringRef& str) noexcept {
  if (str.getLength() > 8)
    return 0;

  const char* swizzleIn = str.getData();
  uint32_t swizzleLen = static_cast<uint32_t>(str.getLength());

  for (uint32_t i = 0; i < MPSL_ARRAY_SIZE(mpVectorIdentifiers); i++) {
    const VectorIdentifiers& vi = mpVectorIdentifiers[i];
    uint32_t mask = vi.mask;

    uint32_t x = 0;
    uint32_t highestIndex = 0;

    do {
      // Refuse non-letter characters.
      uint32_t c = static_cast<uint8_t>(swizzleIn[x]);
      if (c < 'a' || c > 'z') break;

      // Refuse characters that don't exist in VectorIdentifiers record.
      uint32_t letterIndex = (c - 'a');
      if (!(mask & (1U << letterIndex))) break;

      // Matched.
      uint32_t componentIndex = static_cast<uint8_t>(mpIndexSwizzle(vi.letters, 8, c));
      if (highestIndex < componentIndex) highestIndex = componentIndex;
      indexesOut[x] = componentIndex;
    } while (++x < swizzleLen);

    if (x == swizzleLen) {
      highestIndexOut = highestIndex;
      return x;
    }
  }

  return 0;
}

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
  ROW(AstNode::kTypeNone     , 0                   ),
  ROW(AstNode::kTypeProgram  , sizeof(AstProgram)  ),
  ROW(AstNode::kTypeFunction , sizeof(AstFunction) ),
  ROW(AstNode::kTypeBlock    , sizeof(AstBlock)    ),
  ROW(AstNode::kTypeBranch   , sizeof(AstBranch)   ),
  ROW(AstNode::kTypeFor      , sizeof(AstLoop)     ),
  ROW(AstNode::kTypeWhile    , sizeof(AstLoop)     ),
  ROW(AstNode::kTypeDoWhile  , sizeof(AstLoop)     ),
  ROW(AstNode::kTypeBreak    , sizeof(AstBreak)    ),
  ROW(AstNode::kTypeContinue , sizeof(AstContinue) ),
  ROW(AstNode::kTypeReturn   , sizeof(AstReturn)   ),
  ROW(AstNode::kTypeVarDecl  , sizeof(AstVarDecl)  ),
  ROW(AstNode::kTypeVarMemb  , sizeof(AstVarMemb)  ),
  ROW(AstNode::kTypeVar      , sizeof(AstVar)      ),
  ROW(AstNode::kTypeImm      , sizeof(AstImm)      ),
  ROW(AstNode::kTypeUnaryOp  , sizeof(AstUnaryOp)  ),
  ROW(AstNode::kTypeBinaryOp , sizeof(AstBinaryOp) ),
  ROW(AstNode::kTypeCall     , sizeof(AstCall)     )
};
#undef ROW

// ============================================================================
// [mpsl::AstBuilder - Construction / Destruction]
// ============================================================================

AstBuilder::AstBuilder(ZoneHeap* heap) noexcept
  : _heap(heap),
    _globalScope(nullptr),
    _programNode(nullptr),
    _mainFunction(nullptr) {}
AstBuilder::~AstBuilder() noexcept {}

// ============================================================================
// [mpsl::AstBuilder - Factory]
// ============================================================================

AstScope* AstBuilder::newScope(AstScope* parent, uint32_t scopeType) noexcept {
  void* p = _heap->alloc(sizeof(AstScope));
  if (MPSL_UNLIKELY(p == nullptr))
    return nullptr;
  return new(p) AstScope(this, parent, scopeType);
}

void AstBuilder::deleteScope(AstScope* scope) noexcept {
  scope->~AstScope();
  _heap->release(scope, sizeof(AstScope));
}

AstSymbol* AstBuilder::newSymbol(const StringRef& key, uint32_t hVal, uint32_t symbolType, uint32_t scopeType) noexcept {
  size_t kLen = key.getLength();
  void* p = _heap->alloc(sizeof(AstSymbol) + kLen + 1);

  if (MPSL_UNLIKELY(p == nullptr))
    return nullptr;

  char* kStr = static_cast<char*>(p) + sizeof(AstSymbol);
  ::memcpy(kStr, key.getData(), kLen);

  kStr[kLen] = '\0';
  return new(p) AstSymbol(kStr, static_cast<uint32_t>(kLen), hVal, symbolType, scopeType);
}

void AstBuilder::deleteSymbol(AstSymbol* symbol) noexcept {
  size_t kLen = symbol->getLength();
  symbol->~AstSymbol();
  _heap->release(symbol, sizeof(AstSymbol) + kLen + 1);
}

void AstBuilder::deleteNode(AstNode* node) noexcept {
  uint32_t length = node->getLength();
  AstNode** children = node->getChildren();

  for (uint32_t i = 0; i < length; i++) {
    AstNode* child = children[i];
    if (child) deleteNode(child);
  }

  uint32_t nodeType = node->getNodeType();
  MPSL_ASSERT(mpAstNodeSize[nodeType].getNodeType() == nodeType);

  switch (nodeType) {
    case AstNode::kTypeProgram  : static_cast<AstProgram*  >(node)->destroy(this); break;
    case AstNode::kTypeFunction : static_cast<AstFunction* >(node)->destroy(this); break;
    case AstNode::kTypeBlock    : static_cast<AstBlock*    >(node)->destroy(this); break;
    case AstNode::kTypeBranch   : static_cast<AstBranch*   >(node)->destroy(this); break;
    case AstNode::kTypeFor      :
    case AstNode::kTypeWhile    :
    case AstNode::kTypeDoWhile  : static_cast<AstLoop*     >(node)->destroy(this); break;
    case AstNode::kTypeBreak    : static_cast<AstBreak*    >(node)->destroy(this); break;
    case AstNode::kTypeContinue : static_cast<AstContinue* >(node)->destroy(this); break;
    case AstNode::kTypeReturn   : static_cast<AstReturn*   >(node)->destroy(this); break;
    case AstNode::kTypeVarDecl  : static_cast<AstVarDecl*  >(node)->destroy(this); break;
    case AstNode::kTypeVarMemb  : static_cast<AstVarMemb*  >(node)->destroy(this); break;
    case AstNode::kTypeVar      : static_cast<AstVar*      >(node)->destroy(this); break;
    case AstNode::kTypeImm      : static_cast<AstImm*      >(node)->destroy(this); break;
    case AstNode::kTypeUnaryOp  : static_cast<AstUnaryOp*  >(node)->destroy(this); break;
    case AstNode::kTypeBinaryOp : static_cast<AstBinaryOp* >(node)->destroy(this); break;
    case AstNode::kTypeCall     : static_cast<AstCall*     >(node)->destroy(this); break;
  }

  _heap->release(node, mpAstNodeSize[nodeType].getNodeSize());
}

// ============================================================================
// [mpsl::AstBuilder - Initialization]
// ============================================================================

Error AstBuilder::addProgramScope() noexcept {
  if (_globalScope == nullptr) {
    _globalScope = newScope(nullptr, AstScope::kTypeGlobal);
    MPSL_NULLCHECK(_globalScope);
  }

  if (_programNode == nullptr) {
    _programNode = newNode<AstProgram>();
    MPSL_NULLCHECK(_programNode);
  }

  return kErrorOk;
}

// TODO: Not sure this is the right place for these functions.
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
      AstSymbol* symbol = newSymbol(name, hVal, AstSymbol::kTypeTypeName, AstScope::kTypeGlobal);
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

    AstSymbol* symbol = newSymbol(name, hVal, AstSymbol::kTypeVariable, AstScope::kTypeGlobal);
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

    AstSymbol* symbol = newSymbol(name, hVal, AstSymbol::kTypeIntrinsic, AstScope::kTypeGlobal);
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

  if (symbol) {
    *collidedSymbol = symbol;
    return MPSL_TRACE_ERROR(kErrorSymbolCollision);
  }

  // Create the root object symbol even if it's anonymous.
  symbol = newSymbol(name, hVal, AstSymbol::kTypeVariable, AstScope::kTypeGlobal);
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
      if (symbol) {
        *collidedSymbol = symbol;
        return MPSL_TRACE_ERROR(kErrorSymbolCollision);
      }

      symbol = newSymbol(name, hVal, AstSymbol::kTypeVariable, AstScope::kTypeGlobal);
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

class AstScopeReleaseHandler {
public:
  MPSL_INLINE AstScopeReleaseHandler(AstBuilder* ast) noexcept : _ast(ast) {}
  MPSL_INLINE void release(AstSymbol* node) noexcept { _ast->deleteSymbol(node); }

  AstBuilder* _ast;
};

AstScope::AstScope(AstBuilder* ast, AstScope* parent, uint32_t scopeType) noexcept
  : _ast(ast),
    _parent(parent),
    _symbols(ast->getHeap()),
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

  if (scopeOut) *scopeOut = scope;
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
    if (child != refNode) continue;

    children[i] = node;
    refNode->_parent = nullptr;

    if (node) node->_parent = this;
    return refNode;
  }

  return nullptr;
}

AstNode* AstNode::replaceAt(uint32_t index, AstNode* node) noexcept {
  AstNode* child = getAt(index);
  _children[index] = node;

  if (child) child->_parent = nullptr;
  if (node) node->_parent = this;

  return child;
}

AstNode* AstNode::injectNode(AstNode* refNode, AstUnary* node) noexcept {
  MPSL_ASSERT(refNode != nullptr && refNode->getParent() == this);
  MPSL_ASSERT(node != nullptr && node->getParent() == nullptr);

  uint32_t length = _length;
  AstNode** children = getChildren();

  for (uint32_t i = 0; i < length; i++) {
    AstNode* child = children[i];
    if (child != refNode) continue;

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

  ZoneHeap* heap = self->getAst()->getHeap();

  AstNode** oldArray = self->getChildren();
  AstNode** newArray = static_cast<AstNode**>(heap->alloc(newCapacity * sizeof(AstNode), newCapacity));

  MPSL_NULLCHECK(newArray);
  newCapacity /= sizeof(AstNode*);

  self->_children = newArray;
  self->_capacity = static_cast<uint32_t>(newCapacity);

  if (oldCapacity != 0) {
    ::memcpy(newArray, oldArray, length * sizeof(AstNode*));
    heap->release(oldArray, oldCapacity * sizeof(AstNode*));
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
  nest("If");
  if (node->hasCond())
    MPSL_PROPAGATE(onNode(node->getCond()));
  else
    MPSL_PROPAGATE(_sb.appendString("(no condition)"));
  denest();

  if (node->hasThen()) {
    nest("Then");
    MPSL_PROPAGATE(onNode(node->getThen()));
    denest();
  }

  if (node->hasElse()) {
    nest("Else");
    MPSL_PROPAGATE(onNode(node->getElse()));
    denest();
  }
}

Error AstDump::onLoop(AstLoop* node) noexcept {
  uint32_t nodeType = node->getNodeType();

  nest(nodeType == AstNode::kTypeFor     ? "For"   :
       nodeType == AstNode::kTypeWhile   ? "While" :
       nodeType == AstNode::kTypeDoWhile ? "Do"    : "<unknown>");

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

  if (nodeType == AstNode::kTypeDoWhile) {
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
    sym ? sym->getName() : static_cast<const char*>(nullptr), node->getTypeInfo());

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

  if (node->getOp() == kOpCast) {
    nest("(%{Type})", typeInfo);
  }
  else if (node->getOp() == kOpSwizzle) {
    char swizzle[32];
    FormatUtils::formatSwizzleArray(swizzle, node->getSwizzleArray(), TypeInfo::elementsOf(typeInfo));
    nest("(.%s) [%{Type}]", swizzle, typeInfo);
  }
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
  FormatUtils::vformat(_sb, fmt, ap);
  _sb.appendChar('\n');

  va_end(ap);
  return kErrorOk;
}

Error AstDump::nest(const char* fmt, ...) noexcept {
  va_list ap;
  va_start(ap, fmt);

  _sb.appendChars(' ', static_cast<size_t>(_level) * 2);
  FormatUtils::vformat(_sb, fmt, ap);
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
    if (_ast->_mainFunction)
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

  if (retSymb && !didReturn)
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
  if (node->hasCond()) {
    MPSL_PROPAGATE(onNode(node->getCond()));
    MPSL_PROPAGATE(boolCast(node, node->getCond()));
  }

  bool prevUnreachable = _unreachable;
  bool thenUnreachable = prevUnreachable;
  bool elseUnreachable = prevUnreachable;

  if (node->hasThen()) {
    _unreachable = prevUnreachable;
    MPSL_PROPAGATE(onNode(node->getThen()));
    thenUnreachable = _unreachable;
  }

  if (node->hasElse()) {
    _unreachable = prevUnreachable;
    MPSL_PROPAGATE(onNode(node->getElse()));
    elseUnreachable = _unreachable;
  }

  _unreachable = prevUnreachable || (thenUnreachable & elseUnreachable);
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
  if (child) {
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
  uint32_t typeInfo = node->getSymbol()->getTypeInfo();
  node->setTypeInfo(typeInfo);

  if (node->hasChild()) {
    MPSL_PROPAGATE(onNode(node->getChild()));
    MPSL_PROPAGATE(implicitCast(node, node->getChild(), typeInfo));
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

    // Member access.
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
    // Swizzle operation.
    if ((typeInfo & kTypeVecMask) == 0)
      return _errorReporter->onError(kErrorInvalidProgram, node->getPosition(),
        "Type '%{Type}' doesn't have a member '%s'", typeInfo, node->getField().getData());

    uint8_t swizzle[8];
    uint32_t highestIndex;

    uint32_t count = mpParseSwizzle(swizzle, highestIndex, node->getField());
    if (!count || highestIndex >= TypeInfo::widthOf(typeInfo))
      return _errorReporter->onError(kErrorInvalidProgram, node->getPosition(),
        "Type '%{Type}' cannot be swizzled as '%s'", typeInfo, node->getField().getData());

    if (count <= 1)
      typeInfo = typeId | kTypeRO;
    else
      typeInfo = typeId | kTypeRO | (count << kTypeVecShift);

    AstUnaryOp* swizzleNode = _ast->newNode<AstUnaryOp>(kOpSwizzle, typeInfo);
    swizzleNode->setChild(node->unlinkChild());
    ::memcpy(swizzleNode->_swizzleArray, swizzle, count);

    node->getParent()->replaceNode(node, swizzleNode);
    _ast->deleteNode(node);
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
        srcId = kTypeDouble;

        dstType = dstId | (srcType & ~kTypeIdMask);
        srcType = dstType;

        MPSL_PROPAGATE(implicitCast(node, child, dstType));
        child = node->getChild();
      }
      else {
        return _errorReporter->onError(kErrorInvalidProgram, node->getPosition(),
          "Operator '%s' doesn't support argument of type '%{Type}'", op.name, srcType);
      }
    }

    // DSP-specific checks.
    if (op.isDSP64() && (TypeInfo::widthOf(srcType) % 8) != 0) {
      return _errorReporter->onError(kErrorInvalidProgram, node->getPosition(),
        "Operator '%s' doesn't support packed odd vectors, '%{Type}' is odd", op.name, srcType);
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
        // case of a bug in the `OpInfo` table.
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

      // DSP-specific checks.
      if (op.isDSP64() && (TypeInfo::widthOf(dstTypeInfo) % 8) != 0) {
        return _errorReporter->onError(kErrorInvalidProgram, node->getPosition(),
          "Operator '%s' doesn't support packed odd vectors, '%{Type}' is odd", op.name, dstTypeInfo);
      }
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
