// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPAST_P_H
#define _MPSL_MPAST_P_H

// [Dependencies - MPSL]
#include "./mpallocator_p.h"
#include "./mphash_p.h"
#include "./mplang_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [Forward Declarations]
// ============================================================================

struct AstBuilder;
struct AstScope;
struct AstSymbol;

struct AstNode;
struct AstFunction;
struct AstProgram;
struct AstUnary;

// ============================================================================
// [mpsl::AstBuilder]
// ============================================================================

//! \internal
struct AstBuilder {
  MPSL_NO_COPY(AstBuilder)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  AstBuilder(Allocator* allocator) noexcept;
  ~AstBuilder() noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE Allocator* getAllocator() const noexcept { return _allocator; }

  MPSL_INLINE AstScope* getGlobalScope() const noexcept { return _globalScope; }
  MPSL_INLINE AstProgram* getProgramNode() const noexcept { return _programNode; }
  MPSL_INLINE AstFunction* getMainFunction() const noexcept { return _mainFunction; }

  // --------------------------------------------------------------------------
  // [Factory]
  // --------------------------------------------------------------------------

  AstScope* newScope(AstScope* parent, uint32_t scopeType) noexcept;
  void deleteScope(AstScope* scope) noexcept;

  AstSymbol* newSymbol(const StringRef& key, uint32_t hVal, uint32_t symbolType, uint32_t scopeType) noexcept;
  void deleteSymbol(AstSymbol* symbol) noexcept;

#define MPSL_ALLOC_AST_OBJECT(_Size_) \
  void* obj = _allocator->alloc(_Size_); \
  if (MPSL_UNLIKELY(obj == nullptr)) return nullptr

  template<typename T>
  MPSL_INLINE T* newNode() noexcept {
    MPSL_ALLOC_AST_OBJECT(sizeof(T));
    return new(obj) T(this);
  }

  template<typename T, typename P0>
  MPSL_INLINE T* newNode(P0 p0) noexcept {
    MPSL_ALLOC_AST_OBJECT(sizeof(T));
    return new(obj) T(this, p0);
  }

  template<typename T, typename P0, typename P1>
  MPSL_INLINE T* newNode(P0 p0, P1 p1) noexcept {
    MPSL_ALLOC_AST_OBJECT(sizeof(T));
    return new(obj) T(this, p0, p1);
  }

#undef MPSL_ALLOC_AST_OBJECT

  void deleteNode(AstNode* node) noexcept;

  MPSL_INLINE char* newString(const char* s, size_t sLen) noexcept {
    return _allocator->allocString(s, sLen);
  }

  // --------------------------------------------------------------------------
  // [Initialization]
  // --------------------------------------------------------------------------

  Error addProgramScope() noexcept;
  Error addBuiltInTypes(const TypeInfo* data, size_t count) noexcept;
  Error addBuiltInConstants(const ConstInfo* data, size_t count) noexcept;
  Error addBuiltInIntrinsics() noexcept;
  Error addBuiltInObject(uint32_t slot, const Layout* layout, AstSymbol** collidedSymbol) noexcept;

  // --------------------------------------------------------------------------
  // [Dump]
  // --------------------------------------------------------------------------

  Error dump(StringBuilder& sb) noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Allocator.
  Allocator* _allocator;
  //! String builder to build possible output messages.
  StringBuilder _sb;

  //! Global scope.
  AstScope* _globalScope;
  //! Root node.
  AstProgram* _programNode;

  //! Program `main()` node.
  AstFunction* _mainFunction;
};

// ============================================================================
// [mpsl::AstSymbol]
// ============================================================================

struct AstSymbol : public HashNode {
  MPSL_NO_COPY(AstSymbol)

  // --------------------------------------------------------------------------
  // [Enums]
  // --------------------------------------------------------------------------

  //! Symbol type.
  enum Type {
    //! Not used.
    kTypeNone = 0,
    //! The symbol is a type-name.
    kTypeTypeName,
    //! The symbol is an intrinsic (converted to an operator internally).
    kTypeIntrinsic,
    //! The symbol is a variable.
    kTypeVariable,
    //! The symbol is a function.
    kTypeFunction
  };

  //! Symbol flags.
  enum Flags {
    //! The symbol was declared in global scope.
    kFlagIsGlobal   = 0x0001,

    //! The symbol was declared and can be used.
    //!
    //! If this flag is not set it means that the parser is parsing its assignment
    //! (code like "int x = ...") and the symbol can't be used at this time. It's
    //! for parser to make sure that the symbol is declared before it's used.
    kFlagIsDeclared = 0x0002,

    kFlagIsAssigned = 0x0004
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstSymbol(const char* name, uint32_t length, uint32_t hVal, uint32_t symbolType, uint32_t scopeType) noexcept
    : HashNode(hVal),
      _name(name),
      _length(length),
      _symbolType(static_cast<uint8_t>(symbolType)),
      _symbolFlags(scopeType == /* AstScope::kTypeGlobal */ 0 ? uint8_t(kFlagIsGlobal) : uint8_t(0)),
      _opType(kOpNone),
      _dataSlot(kInvalidDataSlot),
      _typeInfo(kTypeVoid),
      _dataOffset(0),
      _node(nullptr),
      _layout(nullptr),
      _value() {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE bool eq(const StringRef& s) const noexcept {
    return eq(s.getData(), s.getLength());
  }

  //! Get whether the symbol name is equal to string `s` of `len`.
  MPSL_INLINE bool eq(const char* s, size_t len) const noexcept {
    return static_cast<size_t>(_length) == len && ::memcmp(_name, s, len) == 0;
  }

  //! Get symbol name.
  MPSL_INLINE const char* getName() const noexcept { return _name; }
  //! Get symbol name length.
  MPSL_INLINE uint32_t getLength() const noexcept { return _length; }
  //! Get hash value of the symbol name.
  MPSL_INLINE uint32_t getHVal() const noexcept { return _hVal; }

  //! Check if the symbol has associated node with it.
  MPSL_INLINE bool hasNode() const noexcept { return _node != nullptr; }
  //! Get node associated with the symbol (can be `nullptr` for built-ins).
  MPSL_INLINE AstNode* getNode() const noexcept { return _node; }
  //! Associate node with the symbol (basically the node that declares it).
  MPSL_INLINE void setNode(AstNode* node) noexcept { _node = node; }

  //! Get symbol type, see \ref AstSymbol::Type.
  MPSL_INLINE uint32_t getSymbolType() const noexcept { return _symbolType; }
  //! Get whether the symbol is `kTypeTypeName`.
  MPSL_INLINE uint32_t isTypeName() const noexcept { return _symbolType == kTypeTypeName; }
  //! Get whether the symbol is `kTypeIntrinsic`.
  MPSL_INLINE uint32_t isIntrinsic() const noexcept { return _symbolType == kTypeIntrinsic; }
  //! Get whether the symbol is `kTypeVariable`.
  MPSL_INLINE uint32_t isVariable() const noexcept { return _symbolType == kTypeVariable; }
  //! Get whether the symbol is `kTypeFunction`.
  MPSL_INLINE uint32_t isFunction() const noexcept { return _symbolType == kTypeFunction; }

  //! Get symbol flags, see \ref AstSymbol::Flags.
  MPSL_INLINE uint32_t getSymbolFlags() const noexcept { return _symbolFlags; }

  MPSL_INLINE bool hasSymbolFlag(uint32_t flag) const noexcept { return (_symbolFlags & flag) != 0; }
  MPSL_INLINE void setSymbolFlag(uint32_t flag) noexcept { _symbolFlags |= static_cast<uint8_t>(flag); }
  MPSL_INLINE void clearSymbolFlag(uint32_t flag) noexcept { _symbolFlags &= ~static_cast<uint8_t>(flag); }

  //! Get operator type, see \ref OpType.
  //!
  //! Only valid if symbol type is \ref kTypeIntrinsic.
  MPSL_INLINE uint32_t getOpType() const noexcept { return _opType; }
  //! Set operator type to `opType`.
  MPSL_INLINE void setOpType(uint32_t opType) noexcept {
    MPSL_ASSERT(_symbolType == kTypeIntrinsic);
    _opType = static_cast<uint8_t>(opType);
  }

  MPSL_INLINE uint32_t getDataSlot() const noexcept { return _dataSlot; }
  MPSL_INLINE void setDataSlot(uint32_t slot) noexcept { _dataSlot = static_cast<uint8_t>(slot); }

  MPSL_INLINE int32_t getDataOffset() const noexcept { return _dataOffset; }
  MPSL_INLINE void setDataOffset(int32_t offset) noexcept { _dataOffset = offset; }

  //! Check if the symbol is global (i.e. it was declared in a global scope).
  MPSL_INLINE bool isGlobal() const noexcept { return hasSymbolFlag(kFlagIsGlobal); }
  //! Check if the symbol was declared.
  MPSL_INLINE bool isDeclared() const noexcept { return hasSymbolFlag(kFlagIsDeclared); }
  //! Set the symbol to be declared (\ref kFlagIsDeclared).
  MPSL_INLINE void setDeclared() noexcept { setSymbolFlag(kFlagIsDeclared); }

  //! Get whether the variable has assigned a constant value at the moment.
  //!
  //! If true, the `_value` is a valid constant that can be used to replace
  //! the variable node by a constant value.
  //!
  //! NOTE: The value can change during AST traversal in case that the variable
  //! is mutable. The value never changes if the variable is declared in global
  //! scope (is that case it's always const) or it has been declared with const
  //! keyword (semantic analysis would refuse any modification to that variable).
  MPSL_INLINE bool isAssigned() const noexcept { return hasSymbolFlag(kFlagIsAssigned); }
  //! Set symbol to be assigned (sets the \ref kFlagIsAssigned flag).
  MPSL_INLINE void setAssigned() noexcept { setSymbolFlag(kFlagIsAssigned); }
  //! Set symbol to not be assigned (clears the \ref kFlagIsAssigned flag).
  MPSL_INLINE void clearAssigned() noexcept { clearSymbolFlag(kFlagIsAssigned); }

  //! Set symbol type-info.
  MPSL_INLINE uint32_t getTypeInfo() const noexcept { return _typeInfo; }
  //! Get symbol type-info.
  MPSL_INLINE void setTypeInfo(uint32_t typeInfo) noexcept { _typeInfo = typeInfo; }

  //! Get the constant value, see `isAssigned()`.
  MPSL_INLINE const Value& getValue() const noexcept { return _value; }
  //! Set `_isAssigned` to true and `_value` to `value`.
  MPSL_INLINE void setValue(const Value& value) noexcept { _value = value; setAssigned(); }

  MPSL_INLINE const Layout* getLayout() const noexcept { return _layout; }
  MPSL_INLINE void setLayout(const Layout* layout) noexcept { _layout = layout; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Symbol name (key).
  const char* _name;
  //! Symbol name length.
  uint32_t _length;

  //! Type of the symbol, see \ref AstSymbolType.
  uint8_t _symbolType;
  //! Symbol flags, see \ref AstSymbolFlags.
  uint8_t _symbolFlags;
  //! Operator type (if the symbol is \ref kTypeIntrinsic).
  uint8_t _opType;
  //! Data slot (only if the symbol is mapped to a data object).
  uint8_t _dataSlot;

  //! TypeInfo of the symbol.
  uint32_t _typeInfo;
  //! Data offset (only if the symbol is mapped to a data object).
  int32_t _dataOffset;

  //! Node where the symbol is defined (NULL if it's built-in or was defined
  //! before program parsing). This is mostly used by error reporting.
  AstNode* _node;

  //! Link to the layout, only valid if the symbol is object.
  const Layout* _layout;

  //! The current value of the symbol (in case the symbol is immediate).
  Value _value;
};

// ============================================================================
// [mpsl::AstScope]
// ============================================================================

struct AstScope {
  MPSL_NO_COPY(AstScope)

  // --------------------------------------------------------------------------
  // [Enums]
  // --------------------------------------------------------------------------

  enum Type {
    //! Global scope.
    kTypeGlobal = 0,

    //! Local scope.
    kTypeLocal = 1,

    //! Nested scope.
    //!
    //! Always statically allocated and merged with the local scope before the
    //! scope is destroyed.
    kTypeNested = 2
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_NOAPI AstScope(AstBuilder* ast, AstScope* parent, uint32_t scopeType) noexcept;
  MPSL_NOAPI ~AstScope() noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get the scope context.
  MPSL_INLINE AstBuilder* getAst() const noexcept { return _ast; }
  //! Get the parent scope (or nullptr).
  MPSL_INLINE AstScope* getParent() const noexcept { return _parent; }
  //! Get scope type, see \ref AstScope::Type.
  MPSL_INLINE uint32_t getScopeType() const noexcept { return _scopeType; }

  // --------------------------------------------------------------------------
  // [Ops]
  // --------------------------------------------------------------------------

  //! Get the symbol defined only in this scope.
  MPSL_INLINE AstSymbol* getSymbol(const StringRef& name, uint32_t hVal) noexcept {
    return _symbols.get(name, hVal);
  }

  //! Put a given symbol to this scope.
  //!
  //! NOTE: The function doesn't care about duplicates. The correct flow is
  //! to call `resolveSymbol()` or `getSymbol()` and then `putSymbol()` based
  //! on the result. You should never call `putSymbol()` without checking if
  //! the symbol is already there.
  MPSL_INLINE void putSymbol(AstSymbol* symbol) noexcept {
    _symbols.put(symbol);
  }

  //! Resolve the symbol by traversing all parent scopes if not found in this
  //! one. An optional `scopeOut` argument can be used to get scope where the
  //! `name` has been found.
  MPSL_NOAPI AstSymbol* resolveSymbol(const StringRef& name, uint32_t hVal, AstScope** scopeOut = nullptr) noexcept;

  MPSL_INLINE AstSymbol* resolveSymbol(const StringRef& name) noexcept {
    return resolveSymbol(name, HashUtils::hashString(name.getData(), name.getLength()));
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! AST builder.
  AstBuilder* _ast;
  //! Parent scope.
  AstScope* _parent;

  //! Symbols defined in this scope.
  Hash<StringRef, AstSymbol> _symbols;

  //! Scope type, see \ref AstScope::Type.
  uint32_t _scopeType;
};

// ============================================================================
// [mpsl::AstNode]
// ============================================================================

#define MPSL_AST_CHILD(_Index_, _Type_, _Name_, _Memb_) \
  MPSL_INLINE bool has##_Name_() const noexcept { return _Memb_ != nullptr; } \
  MPSL_INLINE _Type_* get##_Name_() const noexcept { return _Memb_; } \
  \
  MPSL_INLINE _Type_* set##_Name_(_Type_* node) noexcept { \
    return static_cast<_Type_*>(replaceAt(_Index_, node)); \
  } \
  \
  MPSL_INLINE _Type_* unlink##_Name_() noexcept { \
    _Type_* node = _Memb_; \
    \
    MPSL_ASSERT(node != nullptr); \
    MPSL_ASSERT(node->_parent == this); \
    \
    node->_parent = nullptr; \
    _Memb_ = nullptr; \
    \
    return node; \
  } \
  \
  _Type_* _Memb_

struct AstNode {
  MPSL_NO_COPY(AstNode)

  // --------------------------------------------------------------------------
  // [Enums]
  // --------------------------------------------------------------------------

  //! AST node type.
  enum Type {
    //! Not used.
    kTypeNone = 0,

    //! Node is `AstProgram`.
    kTypeProgram,
    //! Node is `AstFunction`.
    kTypeFunction,
    //! Node is `AstBlock`.
    kTypeBlock,

    //! Node is `AstBranch`.
    kTypeBranch,
    //! Node is `AstCondition`.
    kTypeCondition,

    //! Node is `AstLoop` describing `for () { ... }`.
    kTypeFor,
    //! Node is `AstLoop` describing `while () { ... }`.
    kTypeWhile,
    //! Node is `AstLoop` describing `do { ... } while ()`.
    kTypeDoWhile,

    //! Node is `AstBreak`.
    kTypeBreak,
    //! Node is `AstContinue`.
    kTypeContinue,
    //! Node is `AstReturn`.
    kTypeReturn,

    //! Node is `AstVarDecl`.
    kTypeVarDecl,
    //! Node is `AstVarMemb`.
    kTypeVarMemb,
    //! Node is `AstVar`.
    kTypeVar,
    //! Node is `AstImm`.
    kTypeImm,

    //! Node is `AstUnaryOp`.
    kTypeUnaryOp,
    //! Node is `AstBinaryOp`.
    kTypeBinaryOp,
    //! Node is `AstCall`.
    kTypeCall
  };

  //! AST node flags.
  enum Flags {
    kFlagSideEffect = 0x01
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstNode(AstBuilder* ast, uint32_t nodeType, AstNode** children = nullptr, uint32_t length = 0) noexcept
    : _ast(ast),
      _parent(nullptr),
      _children(children),
      _nodeType(static_cast<uint8_t>(nodeType)),
      _nodeFlags(0),
      _nodeSize(0),
      _op(kOpNone),
      _position(~static_cast<uint32_t>(0)),
      _typeInfo(kTypeVoid),
      _length(length) {}

  MPSL_INLINE void destroy(AstBuilder* ast) noexcept {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get the `AstBuilder` instance that created this node.
  MPSL_INLINE AstBuilder* getAst() const noexcept { return _ast; }

  //! Check if the node has a parent.
  MPSL_INLINE bool hasParent() const noexcept { return _parent != nullptr; }
  //! Get the parent node.
  MPSL_INLINE AstNode* getParent() const noexcept { return _parent; }

  //! Get whether the node has children.
  //!
  //! NOTE: Nodes that always have children (even if they are implicitly set
  //! to nullptr) always return `true`. This function if usefuly mostly if the
  //! node is of `AstBlock` type.
  MPSL_INLINE bool hasChildren() const noexcept { return _length != 0; }
  //! Get children array.
  MPSL_INLINE AstNode** getChildren() const noexcept { return reinterpret_cast<AstNode**>(_children); }
  //! Get length of the children array.
  MPSL_INLINE uint32_t getLength() const noexcept { return _length; }

  //! Get node type.
  MPSL_INLINE uint32_t getNodeType() const noexcept { return _nodeType; }
  //! Get whether the node is a loop (for, do, while, ...).
  MPSL_INLINE bool isLoop() const noexcept { return _nodeType >= kTypeFor && _nodeType <= kTypeDoWhile; }
  //! Get whether the node is `AstVar`.
  MPSL_INLINE bool isVar() const noexcept { return _nodeType == kTypeVar; }
  //! Get whether the node is `AstImm`.
  MPSL_INLINE bool isImm() const noexcept { return _nodeType == kTypeImm; }

  //! Get whether the node has flag `flag`.
  MPSL_INLINE bool hasNodeFlag(uint32_t flag) const noexcept {
    return (static_cast<uint32_t>(_nodeFlags) & flag) != 0;
  }
  //! Get node flags.
  MPSL_INLINE uint32_t getNodeFlags() const noexcept {
    return _nodeFlags;
  }
  //! Set node flags.
  MPSL_INLINE void setNodeFlags(uint32_t flags) noexcept {
    _nodeFlags = static_cast<uint8_t>(flags);
  }
  //! Add node flags.
  MPSL_INLINE void addNodeFlags(uint32_t flags) noexcept {
    _nodeFlags |= static_cast<uint8_t>(flags);
  }

  //! Get node size (in bytes).
  MPSL_INLINE uint32_t getNodeSize() const noexcept { return _nodeSize; }

  //! Get op.
  MPSL_INLINE uint32_t getOp() const noexcept { return _op; }
  //! Set op.
  MPSL_INLINE void setOp(uint32_t op) noexcept { _op = static_cast<uint8_t>(op); }

  //! Get whether the node has associated position in source code.
  MPSL_INLINE bool hasPosition() const noexcept {
    return _position != ~static_cast<uint32_t>(0);
  }
  //! Get source code position of the node.
  MPSL_INLINE uint32_t getPosition() const noexcept {
    return _position;
  }
  //! Set source code position of the node.
  MPSL_INLINE void setPosition(uint32_t position) noexcept {
    _position = position;
  }
  //! Reset source code position of the node.
  MPSL_INLINE void resetPosition() noexcept {
    _position = ~static_cast<uint32_t>(0);
  }

  //! Get type-info.
  MPSL_INLINE uint32_t getTypeInfo() const noexcept { return _typeInfo; }
  //! Set type-info.
  MPSL_INLINE void setTypeInfo(uint32_t typeInfo) noexcept { _typeInfo = typeInfo; }

  // --------------------------------------------------------------------------
  // [Children]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstNode* getAt(uint32_t index) const noexcept {
    MPSL_ASSERT(index < _length);
    return _children[index];
  }

  //! Replace `refNode` by `node`.
  AstNode* replaceNode(AstNode* refNode, AstNode* node) noexcept;
  //! Replace node at index `index` by `node`.
  AstNode* replaceAt(uint32_t index, AstNode* node) noexcept;

  //! Inject `node` between this node and `refNode`.
  AstNode* injectNode(AstNode* refNode, AstUnary* node) noexcept;
  //! Inject `node` between this node and node at index `index`.
  AstNode* injectAt(uint32_t index, AstUnary* node) noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! AST builder.
  AstBuilder* _ast;
  //! Parent node.
  AstNode* _parent;
  //! Child nodes.
  AstNode** _children;

  //! Node type, see `AstNode::Type`.
  uint8_t _nodeType;
  //! Node flags, see `AstNode::Flags`.
  uint8_t _nodeFlags;
  //! Node size in bytes for `Allocator`.
  uint8_t _nodeSize;
  //! Operator, see `OpType`.
  uint8_t _op;

  //! Position (in characters) to the beginning of the program (default -1).
  uint32_t _position;

  //! Type-info of the node (mostly the result type).
  //!
  //! Initially set to `kTypeVoid`. It remains void only if there is no
  //! result after the node is evaluated. The type-info is either parsed
  //! (variables/numbers/cast) or deduced by using semantic analysis.
  uint32_t _typeInfo;

  //! Count of child-nodes.
  uint32_t _length;
};

// ============================================================================
// [mpsl::AstBlock]
// ============================================================================

struct AstBlock : public AstNode {
  MPSL_NO_COPY(AstBlock)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstBlock(AstBuilder* ast, uint32_t nodeType = kTypeBlock) noexcept
    : AstNode(ast, nodeType),
      _capacity(0) {}

  // --------------------------------------------------------------------------
  // [Ops]
  // --------------------------------------------------------------------------

  //! Reserve the capacity of the AstBlock so one more node can be added into it.
  //!
  //! NOTE: This has to be called before you use `appendNode()` or `insertAt()`.
  //! The reason is that it's easier to deal with possible allocation failure
  //! here (before the node to be added is created) than after the node is
  //! created, but failed to add into the block.
  Error willAdd() noexcept;

  //! Append the given `node` to the block.
  //!
  //! NOTE: You have to call `willAdd()` before you use `appendNode()` for every
  //! node you want to add to the block.
  MPSL_INLINE void appendNode(AstNode* node) noexcept {
    MPSL_ASSERT(node != nullptr);
    MPSL_ASSERT(node->getParent() == nullptr);

    // We expect `willAdd()` to be called before `appendNode()`.
    MPSL_ASSERT(_length < _capacity);

    node->_parent = this;

    _children[_length] = node;
    _length++;
  }

  //! Insert the given `node` to the block at index `i`.
  //!
  //! NOTE: You have to call `willAdd()` before you use `insertAt()` for every
  //! node you want to add to the block.
  MPSL_INLINE void insertAt(uint32_t i, AstNode* node) noexcept {
    MPSL_ASSERT(node != nullptr);
    MPSL_ASSERT(node->getParent() == nullptr);

    // We expect `willAdd()` to be called before `insertAt()`.
    MPSL_ASSERT(_length < _capacity);

    AstNode** p = getChildren();
    uint32_t j = _length;

    node->_parent = this;
    while (i < j) {
      p[j] = p[j - 1];
      j--;
    }

    p[j] = node;
    _length++;
  }

  //! Remove the given `node`.
  AstNode* removeNode(AstNode* node) noexcept;
  //! Remove the node at index `index`.
  AstNode* removeAt(uint32_t index) noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint32_t _capacity;
};

// ============================================================================
// [mpsl::AstUnary]
// ============================================================================

struct AstUnary : public AstNode {
  MPSL_NO_COPY(AstUnary)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstUnary(AstBuilder* ast, uint32_t nodeType) noexcept
    : AstNode(ast, nodeType, &_child, 1),
      _child(nullptr) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstNode** getChildren() const noexcept { return (AstNode**)&_child; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  MPSL_AST_CHILD(0, AstNode, Child, _child);
};

// ============================================================================
// [mpsl::AstBinary]
// ============================================================================

struct AstBinary : public AstNode {
  MPSL_NO_COPY(AstBinary)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstBinary(AstBuilder* ast, uint32_t nodeType) noexcept
    : AstNode(ast, nodeType, &_left, 2),
      _left(nullptr),
      _right(nullptr) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstNode** getChildren() const noexcept { return (AstNode**)&_left; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  MPSL_AST_CHILD(0, AstNode, Left, _left);
  MPSL_AST_CHILD(1, AstNode, Right, _right);
};

// ============================================================================
// [mpsl::AstProgram]
// ============================================================================

struct AstProgram : public AstBlock {
  MPSL_NO_COPY(AstProgram)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstProgram(AstBuilder* ast) noexcept
    : AstBlock(ast, kTypeProgram) {}
};

// ============================================================================
// [mpsl::AstFunction]
// ============================================================================

struct AstFunction : public AstNode {
  MPSL_NO_COPY(AstFunction)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstFunction(AstBuilder* ast) noexcept
    : AstNode(ast, kTypeFunction, (AstNode**)&_args, 2),
      _scope(nullptr),
      _func(nullptr),
      _ret(nullptr),
      _args(nullptr),
      _body(nullptr) {}

  MPSL_INLINE void destroy(AstBuilder* ast) noexcept {
    ast->deleteScope(getScope());
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstNode** getChildren() const noexcept { return (AstNode**)&_args; }

  MPSL_INLINE AstScope* getScope() const noexcept { return _scope; }
  MPSL_INLINE void setScope(AstScope* scope) noexcept { _scope = scope; }

  MPSL_INLINE AstSymbol* getFunc() const noexcept { return _func; }
  MPSL_INLINE void setFunc(AstSymbol* func) noexcept { _func = func; }

  MPSL_INLINE AstSymbol* getRet() const noexcept { return _ret; }
  MPSL_INLINE void setRet(AstSymbol* ret) noexcept { _ret = ret; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  MPSL_AST_CHILD(0, AstBlock, Args, _args);
  MPSL_AST_CHILD(1, AstBlock, Body, _body);

  AstScope* _scope;
  AstSymbol* _func;
  AstSymbol* _ret;
};

// ============================================================================
// [mpsl::AstBranch]
// ============================================================================

struct AstBranch : public AstBlock {
  MPSL_NO_COPY(AstBranch)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstBranch(AstBuilder* ast) noexcept
    : AstBlock(ast, kTypeBranch) {}
};

// ============================================================================
// [mpsl::AstCondition]
// ============================================================================

struct AstCondition : public AstNode {
  MPSL_NO_COPY(AstCondition)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstCondition(AstBuilder* ast) noexcept
    : AstNode(ast, kTypeCondition, &_exp, 2),
      _exp(nullptr),
      _body(nullptr) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstNode** getChildren() const noexcept { return (AstNode**)&_exp; }

  MPSL_INLINE bool isIf() const noexcept { return _exp != nullptr; }
  MPSL_INLINE bool isElse() const noexcept { return _exp == nullptr; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  MPSL_AST_CHILD(0, AstNode, Exp, _exp);
  MPSL_AST_CHILD(1, AstBlock, Body, _body);
};

// ============================================================================
// [mpsl::AstLoop]
// ============================================================================

struct AstLoop : public AstNode {
  MPSL_NO_COPY(AstLoop)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstLoop(AstBuilder* ast, uint32_t nodeType) noexcept
    : AstNode(ast, nodeType, &_init, 4),
      _init(nullptr),
      _iter(nullptr),
      _cond(nullptr),
      _body(nullptr) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstNode** getChildren() const noexcept { return (AstNode**)&_init; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  MPSL_AST_CHILD(0, AstNode, Init, _init);
  MPSL_AST_CHILD(1, AstNode, Iter, _iter);
  MPSL_AST_CHILD(2, AstNode, Cond, _cond);
  MPSL_AST_CHILD(3, AstBlock, Body, _body);
};

// ============================================================================
// [mpsl::AstFlow]
// ============================================================================

struct AstFlow : public AstNode {
  MPSL_NO_COPY(AstFlow)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstFlow(AstBuilder* ast, uint32_t nodeType) noexcept
    : AstNode(ast, nodeType) {}

  // --------------------------------------------------------------------------
  // [Interface]
  // --------------------------------------------------------------------------

  AstLoop* getLoop() const noexcept;
};

// ============================================================================
// [mpsl::AstBreak]
// ============================================================================

struct AstBreak : public AstFlow {
  MPSL_NO_COPY(AstBreak)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstBreak(AstBuilder* ast) noexcept
    : AstFlow(ast, kTypeBreak) {}
};

// ============================================================================
// [mpsl::AstContinue]
// ============================================================================

struct AstContinue : public AstFlow {
  MPSL_NO_COPY(AstContinue)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstContinue(AstBuilder* ast) noexcept
    : AstFlow(ast, kTypeContinue) {}
};

// ============================================================================
// [mpsl::AstReturn]
// ============================================================================

struct AstReturn : public AstUnary {
  MPSL_NO_COPY(AstReturn)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstReturn(AstBuilder* ast) noexcept
    : AstUnary(ast, kTypeReturn) {}
};

// ============================================================================
// [mpsl::AstVarDecl]
// ============================================================================

struct AstVarDecl : public AstUnary {
  MPSL_NO_COPY(AstVarDecl)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstVarDecl(AstBuilder* ast) noexcept
    : AstUnary(ast, kTypeVarDecl),
      _symbol(nullptr) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstSymbol* getSymbol() const noexcept { return _symbol; }
  MPSL_INLINE void setSymbol(AstSymbol* symbol) noexcept { _symbol = symbol; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  AstSymbol* _symbol;
};

// ============================================================================
// [mpsl::AstVarMemb]
// ============================================================================

struct AstVarMemb : public AstUnary {
  MPSL_NO_COPY(AstVarMemb)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstVarMemb(AstBuilder* ast) noexcept
    : AstUnary(ast, kTypeVarMemb),
      _field(nullptr, 0),
      _offset(0) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE const StringRef& getField() const noexcept { return _field; }
  MPSL_INLINE void setField(const char* s, size_t sLen) noexcept { _field.set(s, sLen); }

  MPSL_INLINE int32_t getOffset() const noexcept { return _offset; }
  MPSL_INLINE void setOffset(int offset) noexcept { _offset = offset; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  StringRef _field;
  //! Offset to the member if object (or a single field access of vector type).
  int32_t _offset;
};

// ============================================================================
// [mpsl::AstVar]
// ============================================================================

struct AstVar : public AstNode {
  MPSL_NO_COPY(AstVar)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstVar(AstBuilder* ast) noexcept
    : AstNode(ast, kTypeVar),
      _symbol(nullptr) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstSymbol* getSymbol() const noexcept { return _symbol; }
  MPSL_INLINE void setSymbol(AstSymbol* symbol) noexcept { _symbol = symbol; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  AstSymbol* _symbol;
};

// ============================================================================
// [mpsl::AstImm]
// ============================================================================

struct AstImm : public AstNode {
  MPSL_NO_COPY(AstImm)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstImm(AstBuilder* ast) noexcept
    : AstNode(ast, kTypeImm) {
    ::memset(&_value, 0, sizeof(_value));
    _slot = 0;
  }

  MPSL_INLINE AstImm(AstBuilder* ast, const Value& value, uint32_t typeInfo) noexcept
    : AstNode(ast, kTypeImm) {
    _typeInfo = typeInfo;
    _value = value;
    _slot = 0;
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE const Value& getValue() const noexcept { return _value; }
  MPSL_INLINE void setValue(const Value& value) noexcept { _value = value; }

  MPSL_INLINE uint32_t getSlot() const noexcept { return _slot; }
  MPSL_INLINE void setSlot(uint32_t slot) noexcept { _slot = slot; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  Value _value;
  uint32_t _slot;
};

// ============================================================================
// [mpsl::AstUnaryOp]
// ============================================================================

struct AstUnaryOp : public AstUnary {
  MPSL_NO_COPY(AstUnaryOp)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstUnaryOp(AstBuilder* ast, uint32_t op) noexcept
    : AstUnary(ast, kTypeUnaryOp) {
    setOp(op);
  }

  MPSL_INLINE AstUnaryOp(AstBuilder* ast, uint32_t op, uint32_t typeInfo) noexcept
    : AstUnary(ast, kTypeUnaryOp) {
    setOp(op);
    setTypeInfo(typeInfo);
  }
};

// ============================================================================
// [mpsl::AstBinaryOp]
// ============================================================================

struct AstBinaryOp : public AstBinary {
  MPSL_NO_COPY(AstBinaryOp)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstBinaryOp(AstBuilder* ast, uint32_t op) noexcept
    : AstBinary(ast, kTypeBinaryOp) {
    setOp(op);
  }
};

// ============================================================================
// [mpsl::AstCall]
// ============================================================================

struct AstCall : public AstBlock {
  MPSL_NO_COPY(AstCall)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstCall(AstBuilder* ast) noexcept
    : AstBlock(ast, kTypeCall),
      _symbol(nullptr) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstSymbol* getSymbol() const noexcept { return _symbol; }
  MPSL_INLINE void setSymbol(AstSymbol* symbol) noexcept { _symbol = symbol; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  AstSymbol* _symbol;
};

// ============================================================================
// [mpsl::AstVisitor]
// ============================================================================

template<typename Impl>
struct AstVisitor {
  MPSL_NO_COPY(AstVisitor<Impl>)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstVisitor(AstBuilder* ast) noexcept : _ast(ast) {}
  MPSL_INLINE ~AstVisitor() noexcept {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstBuilder* getAst() const noexcept { return _ast; }

  // --------------------------------------------------------------------------
  // [OnNode]
  // --------------------------------------------------------------------------

  MPSL_NOINLINE Error onNode(AstNode* node) noexcept {
    switch (node->getNodeType()) {
      case AstNode::kTypeProgram  : return static_cast<Impl*>(this)->onProgram  (static_cast<AstProgram*  >(node));
      case AstNode::kTypeFunction : return static_cast<Impl*>(this)->onFunction (static_cast<AstFunction* >(node));
      case AstNode::kTypeBlock    : return static_cast<Impl*>(this)->onBlock    (static_cast<AstBlock*    >(node));
      case AstNode::kTypeBranch   : return static_cast<Impl*>(this)->onBranch   (static_cast<AstBranch*   >(node));
      case AstNode::kTypeCondition: return static_cast<Impl*>(this)->onCondition(static_cast<AstCondition*>(node));
      case AstNode::kTypeFor      :
      case AstNode::kTypeWhile    :
      case AstNode::kTypeDoWhile  : return static_cast<Impl*>(this)->onLoop     (static_cast<AstLoop*     >(node));
      case AstNode::kTypeBreak    : return static_cast<Impl*>(this)->onBreak    (static_cast<AstBreak*    >(node));
      case AstNode::kTypeContinue : return static_cast<Impl*>(this)->onContinue (static_cast<AstContinue* >(node));
      case AstNode::kTypeReturn   : return static_cast<Impl*>(this)->onReturn   (static_cast<AstReturn*   >(node));
      case AstNode::kTypeVarDecl  : return static_cast<Impl*>(this)->onVarDecl  (static_cast<AstVarDecl*  >(node));
      case AstNode::kTypeVarMemb  : return static_cast<Impl*>(this)->onVarMemb  (static_cast<AstVarMemb*  >(node));
      case AstNode::kTypeVar      : return static_cast<Impl*>(this)->onVar      (static_cast<AstVar*      >(node));
      case AstNode::kTypeImm      : return static_cast<Impl*>(this)->onImm      (static_cast<AstImm*      >(node));
      case AstNode::kTypeUnaryOp  : return static_cast<Impl*>(this)->onUnaryOp  (static_cast<AstUnaryOp*  >(node));
      case AstNode::kTypeBinaryOp : return static_cast<Impl*>(this)->onBinaryOp (static_cast<AstBinaryOp* >(node));
      case AstNode::kTypeCall     : return static_cast<Impl*>(this)->onCall     (static_cast<AstCall*     >(node));

      default:
        return MPSL_TRACE_ERROR(kErrorInvalidState);
    }
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  AstBuilder* _ast;
};

// ============================================================================
// [mpsl::AstVisitorWithData]
// ============================================================================

template<typename Impl, typename Args>
struct AstVisitorWithArgs {
  MPSL_NO_COPY(AstVisitorWithArgs<Impl, Args>)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstVisitorWithArgs(AstBuilder* ast) noexcept : _ast(ast) {}
  MPSL_INLINE ~AstVisitorWithArgs() noexcept {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstBuilder* getAst() const noexcept { return _ast; }

  // --------------------------------------------------------------------------
  // [OnNode]
  // --------------------------------------------------------------------------

  MPSL_NOINLINE Error onNode(AstNode* node, Args args) noexcept {
    switch (node->getNodeType()) {
      case AstNode::kTypeProgram  : return static_cast<Impl*>(this)->onProgram  (static_cast<AstProgram*  >(node), args);
      case AstNode::kTypeFunction : return static_cast<Impl*>(this)->onFunction (static_cast<AstFunction* >(node), args);
      case AstNode::kTypeBlock    : return static_cast<Impl*>(this)->onBlock    (static_cast<AstBlock*    >(node), args);
      case AstNode::kTypeBranch   : return static_cast<Impl*>(this)->onBranch   (static_cast<AstBranch*   >(node), args);
      case AstNode::kTypeCondition: return static_cast<Impl*>(this)->onCondition(static_cast<AstCondition*>(node), args);
      case AstNode::kTypeFor      :
      case AstNode::kTypeWhile    :
      case AstNode::kTypeDoWhile  : return static_cast<Impl*>(this)->onLoop     (static_cast<AstLoop*     >(node), args);
      case AstNode::kTypeBreak    : return static_cast<Impl*>(this)->onBreak    (static_cast<AstBreak*    >(node), args);
      case AstNode::kTypeContinue : return static_cast<Impl*>(this)->onContinue (static_cast<AstContinue* >(node), args);
      case AstNode::kTypeReturn   : return static_cast<Impl*>(this)->onReturn   (static_cast<AstReturn*   >(node), args);
      case AstNode::kTypeVarDecl  : return static_cast<Impl*>(this)->onVarDecl  (static_cast<AstVarDecl*  >(node), args);
      case AstNode::kTypeVarMemb  : return static_cast<Impl*>(this)->onVarMemb  (static_cast<AstVarMemb*  >(node), args);
      case AstNode::kTypeVar      : return static_cast<Impl*>(this)->onVar      (static_cast<AstVar*      >(node), args);
      case AstNode::kTypeImm      : return static_cast<Impl*>(this)->onImm      (static_cast<AstImm*      >(node), args);
      case AstNode::kTypeUnaryOp  : return static_cast<Impl*>(this)->onUnaryOp  (static_cast<AstUnaryOp*  >(node), args);
      case AstNode::kTypeBinaryOp : return static_cast<Impl*>(this)->onBinaryOp (static_cast<AstBinaryOp* >(node), args);
      case AstNode::kTypeCall     : return static_cast<Impl*>(this)->onCall     (static_cast<AstCall*     >(node), args);

      default:
        return MPSL_TRACE_ERROR(kErrorInvalidState);
    }
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  AstBuilder* _ast;
};

// ============================================================================
// [mpsl::AstDump]
// ============================================================================

struct AstDump : public AstVisitor<AstDump> {
  MPSL_NO_COPY(AstDump)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstDump(AstBuilder* ast, StringBuilder& sb) noexcept
    : AstVisitor<AstDump>(ast),
      _sb(sb),
      _level(0) {}
  MPSL_INLINE ~AstDump() noexcept {}

  // --------------------------------------------------------------------------
  // [OnNode]
  // --------------------------------------------------------------------------

  MPSL_NOAPI Error onProgram(AstProgram* node) noexcept;
  MPSL_NOAPI Error onFunction(AstFunction* node) noexcept;
  MPSL_NOAPI Error onBlock(AstBlock* node) noexcept;
  MPSL_NOAPI Error onBranch(AstBranch* node) noexcept;
  MPSL_NOAPI Error onCondition(AstCondition* node) noexcept;
  MPSL_NOAPI Error onLoop(AstLoop* node) noexcept;
  MPSL_NOAPI Error onBreak(AstBreak* node) noexcept;
  MPSL_NOAPI Error onContinue(AstContinue* node) noexcept;
  MPSL_NOAPI Error onReturn(AstReturn* node) noexcept;
  MPSL_NOAPI Error onVarDecl(AstVarDecl* node) noexcept;
  MPSL_NOAPI Error onVarMemb(AstVarMemb* node) noexcept;
  MPSL_NOAPI Error onVar(AstVar* node) noexcept;
  MPSL_NOAPI Error onImm(AstImm* node) noexcept;
  MPSL_NOAPI Error onUnaryOp(AstUnaryOp* node) noexcept;
  MPSL_NOAPI Error onBinaryOp(AstBinaryOp* node) noexcept;
  MPSL_NOAPI Error onCall(AstCall* node) noexcept;

  // --------------------------------------------------------------------------
  // [Helpers]
  // --------------------------------------------------------------------------

  MPSL_NOAPI Error info(const char* fmt, ...) noexcept;
  MPSL_NOAPI Error nest(const char* fmt, ...) noexcept;
  MPSL_NOAPI Error denest() noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  StringBuilder& _sb;
  uint32_t _level;
};

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPAST_P_H
