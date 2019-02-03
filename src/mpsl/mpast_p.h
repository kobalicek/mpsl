// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPAST_P_H
#define _MPSL_MPAST_P_H

// [Dependencies - MPSL]
#include "./mphash_p.h"
#include "./mplang_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [Forward Declarations]
// ============================================================================

class AstBuilder;
class AstScope;
class AstSymbol;

class AstNode;
class AstFunction;
class AstProgram;
class AstUnary;

// ============================================================================
// [mpsl::AstBuilder]
// ============================================================================

//! \internal
class AstBuilder {
public:
  MPSL_NONCOPYABLE(AstBuilder)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  AstBuilder(ZoneAllocator* allocator) noexcept;
  ~AstBuilder() noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline ZoneAllocator* allocator() const noexcept { return _allocator; }
  inline AstScope* globalScope() const noexcept { return _globalScope; }
  inline AstProgram* programNode() const noexcept { return _programNode; }
  inline AstFunction* mainFunction() const noexcept { return _mainFunction; }

  // --------------------------------------------------------------------------
  // [Factory]
  // --------------------------------------------------------------------------

  AstScope* newScope(AstScope* parent, uint32_t scopeType) noexcept;
  void deleteScope(AstScope* scope) noexcept;

  AstSymbol* newSymbol(const StringRef& key, uint32_t hashCode, uint32_t symbolType, uint32_t scopeType) noexcept;
  void deleteSymbol(AstSymbol* symbol) noexcept;

#define MPSL_ALLOC_AST_OBJECT(_Size_) \
  void* obj = _allocator->alloc(_Size_); \
  if (MPSL_UNLIKELY(obj == nullptr)) return nullptr

  template<typename T>
  inline T* newNode() noexcept {
    MPSL_ALLOC_AST_OBJECT(sizeof(T));
    return new(obj) T(this);
  }

  template<typename T, typename P0>
  inline T* newNode(P0 p0) noexcept {
    MPSL_ALLOC_AST_OBJECT(sizeof(T));
    return new(obj) T(this, p0);
  }

  template<typename T, typename P0, typename P1>
  inline T* newNode(P0 p0, P1 p1) noexcept {
    MPSL_ALLOC_AST_OBJECT(sizeof(T));
    return new(obj) T(this, p0, p1);
  }

#undef MPSL_ALLOC_AST_OBJECT

  void deleteNode(AstNode* node) noexcept;

  inline char* newString(const char* s, size_t size) noexcept {
    char* p = static_cast<char*>(_allocator->alloc(size + 1));
    if (MPSL_UNLIKELY(!p)) return p;

    ::memcpy(p, s, size);
    p[size] = '\0';
    return p;
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

  Error dump(String& sb) noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  ZoneAllocator* _allocator;             //!< Zone allocator.
  String _sb;                            //!< String builder to build possible output messages.
  AstScope* _globalScope;                //!< Global scope.
  AstProgram* _programNode;              //!< Root node.
  AstFunction* _mainFunction;            //!< Program `main()` node.
};

// ============================================================================
// [mpsl::AstSymbol]
// ============================================================================

class AstSymbol : public HashNode {
public:
  MPSL_NONCOPYABLE(AstSymbol)

  //! Symbol type.
  enum Type {
    kTypeNone       = 0,                 //!< Not used.
    kTypeTypeName   = 1,                 //!< Type-name.
    kTypeIntrinsic  = 2,                 //!< Intrinsic (converted to an operator internally).
    kTypeVariable   = 3,                 //!< Variable.
    kTypeFunction   = 4                  //!< Function.
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

  inline AstSymbol(const char* name, uint32_t nameSize, uint32_t hashCode, uint32_t symbolType, uint32_t scopeType) noexcept
    : HashNode(hashCode),
      _name(name),
      _nameSize(nameSize),
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

  inline bool eq(const StringRef& s) const noexcept {
    return eq(s.data(), s.size());
  }

  //! Get whether the symbol name is equal to string `data` of `size`.
  inline bool eq(const char* data, size_t size) const noexcept {
    return static_cast<size_t>(_nameSize) == size && ::memcmp(_name, data, size) == 0;
  }

  //! Get symbol name.
  inline const char* name() const noexcept { return _name; }
  //! Get symbol name size.
  inline uint32_t nameSize() const noexcept { return _nameSize; }
  //! Get hash value of the symbol name.
  inline uint32_t hashCode() const noexcept { return _hashCode; }

  //! Get node associated with the symbol (can be null for built-ins).
  inline AstNode* node() const noexcept { return _node; }
  //! Associate node with the symbol (basically the node that declares it).
  inline void setNode(AstNode* node) noexcept { _node = node; }

  //! Get symbol type, see \ref AstSymbol::Type.
  inline uint32_t symbolType() const noexcept { return _symbolType; }
  //! Get whether the symbol is `kTypeTypeName`.
  inline uint32_t isTypeName() const noexcept { return _symbolType == kTypeTypeName; }
  //! Get whether the symbol is `kTypeIntrinsic`.
  inline uint32_t isIntrinsic() const noexcept { return _symbolType == kTypeIntrinsic; }
  //! Get whether the symbol is `kTypeVariable`.
  inline uint32_t isVariable() const noexcept { return _symbolType == kTypeVariable; }
  //! Get whether the symbol is `kTypeFunction`.
  inline uint32_t isFunction() const noexcept { return _symbolType == kTypeFunction; }

  //! Get symbol flags, see \ref AstSymbol::Flags.
  inline uint32_t symbolFlags() const noexcept { return _symbolFlags; }
  //! Get whether the symbol flags contain `flag`.
  inline bool hasSymbolFlag(uint32_t flag) const noexcept { return (_symbolFlags & flag) != 0; }

  inline void setSymbolFlag(uint32_t flag) noexcept { _symbolFlags |= static_cast<uint8_t>(flag); }
  inline void clearSymbolFlag(uint32_t flag) noexcept { _symbolFlags &= ~static_cast<uint8_t>(flag); }

  //! Get operator type, see \ref OpType.
  //!
  //! Only valid if symbol type is \ref kTypeIntrinsic.
  inline uint32_t opType() const noexcept { return _opType; }
  //! Set operator type to `opType`.
  inline void setOpType(uint32_t opType) noexcept {
    MPSL_ASSERT(_symbolType == kTypeIntrinsic);
    _opType = static_cast<uint8_t>(opType);
  }

  inline uint32_t dataSlot() const noexcept { return _dataSlot; }
  inline void setDataSlot(uint32_t slot) noexcept { _dataSlot = static_cast<uint8_t>(slot); }

  inline int32_t dataOffset() const noexcept { return _dataOffset; }
  inline void setDataOffset(int32_t offset) noexcept { _dataOffset = offset; }

  //! Check if the symbol is global (i.e. it was declared in a global scope).
  inline bool isGlobal() const noexcept { return hasSymbolFlag(kFlagIsGlobal); }
  //! Check if the symbol was declared.
  inline bool isDeclared() const noexcept { return hasSymbolFlag(kFlagIsDeclared); }
  //! Set the symbol to be declared (\ref kFlagIsDeclared).
  inline void setDeclared() noexcept { setSymbolFlag(kFlagIsDeclared); }

  //! Get whether the variable has assigned a constant value at the moment.
  //!
  //! If true, the `_value` is a valid constant that can be used to replace
  //! the variable node by a constant value.
  //!
  //! \note The value can change during AST traversal in case that the variable
  //! is mutable. The value never changes if the variable is declared in global
  //! scope (is that case it's always const) or it has been declared with const
  //! keyword (semantic analysis would refuse any modification to that variable).
  inline bool isAssigned() const noexcept { return hasSymbolFlag(kFlagIsAssigned); }
  //! Set symbol to be assigned (sets the \ref kFlagIsAssigned flag).
  inline void setAssigned() noexcept { setSymbolFlag(kFlagIsAssigned); }
  //! Set symbol to not be assigned (clears the \ref kFlagIsAssigned flag).
  inline void clearAssigned() noexcept { clearSymbolFlag(kFlagIsAssigned); }

  //! Set symbol type-info.
  inline uint32_t typeInfo() const noexcept { return _typeInfo; }
  //! Get symbol type-info.
  inline void setTypeInfo(uint32_t typeInfo) noexcept { _typeInfo = typeInfo; }

  //! Get the constant value, see `isAssigned()`.
  inline const Value& value() const noexcept { return _value; }
  //! Set `_isAssigned` to true and `_value` to `value`.
  inline void setValue(const Value& value) noexcept { _value = value; setAssigned(); }

  inline const Layout* layout() const noexcept { return _layout; }
  inline void setLayout(const Layout* layout) noexcept { _layout = layout; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  const char* _name;                     //!< Symbol name (key).
  uint32_t _nameSize;                    //!< Symbol name size.

  uint8_t _symbolType;                   //!< Type of the symbol, see \ref Type.
  uint8_t _symbolFlags;                  //!< Symbol flags, see \ref Flags.
  uint8_t _opType;                       //!< Operator type (if the symbol is \ref kTypeIntrinsic).
  uint8_t _dataSlot;                     //!< Data slot (only if the symbol is mapped to a data object).

  uint32_t _typeInfo;                    //!< TypeInfo of the symbol.
  int32_t _dataOffset;                   //!< Data offset (only if the symbol is mapped to a data object).

  AstNode* _node;                        //!< Node where the symbol is defined (nullptr if built-in)
  const Layout* _layout;                 //!< Link to the layout, only valid if the symbol is object.
  Value _value;                          //!< The current value of the symbol (in case the symbol is immediate).
};

// ============================================================================
// [mpsl::AstScope]
// ============================================================================

class AstScope {
public:
  MPSL_NONCOPYABLE(AstScope)

  // --------------------------------------------------------------------------
  // [Enums]
  // --------------------------------------------------------------------------

  enum Type {
    kTypeGlobal = 0,                     //!< Global scope.
    kTypeLocal  = 1,                     //!< Local scope.
    kTypeNested = 2                      //!< Nested scope (always statically allocated).
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  AstScope(AstBuilder* ast, AstScope* parent, uint32_t scopeType) noexcept;
  ~AstScope() noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get the scope context.
  inline AstBuilder* ast() const noexcept { return _ast; }
  //! Get the parent scope (or nullptr).
  inline AstScope* parent() const noexcept { return _parent; }
  //! Get scope type, see \ref AstScope::Type.
  inline uint32_t scopeType() const noexcept { return _scopeType; }

  // --------------------------------------------------------------------------
  // [Ops]
  // --------------------------------------------------------------------------

  //! Get the symbol defined only in this scope.
  inline AstSymbol* getSymbol(const StringRef& name, uint32_t hashCode) noexcept {
    return _symbols.get(name, hashCode);
  }

  //! Put a given symbol to this scope.
  //!
  //! \note The function doesn't care about duplicates. The correct flow is
  //! to call `resolveSymbol()` or `getSymbol()` and then `putSymbol()` based
  //! on the result. You should never call `putSymbol()` without checking if
  //! the symbol is already there.
  inline void putSymbol(AstSymbol* symbol) noexcept {
    _symbols.put(symbol);
  }

  //! Resolve the symbol by traversing all parent scopes if not found in this
  //! one. An optional `scopeOut` argument can be used to get scope where the
  //! `name` has been found.
  AstSymbol* resolveSymbol(const StringRef& name, uint32_t hashCode, AstScope** scopeOut = nullptr) noexcept;

  inline AstSymbol* resolveSymbol(const StringRef& name) noexcept {
    return resolveSymbol(name, HashUtils::hashString(name.data(), name.size()));
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  AstBuilder* _ast;                      //!< AST builder.
  AstScope* _parent;                     //!< Parent scope.
  Hash<StringRef, AstSymbol> _symbols;   //!< Symbols defined within this scope.
  uint32_t _scopeType;                   //!< Scope type, see \ref Type.
};

// ============================================================================
// [mpsl::AstNode]
// ============================================================================

class AstNode {
public:
  MPSL_NONCOPYABLE(AstNode)

  // --------------------------------------------------------------------------
  // [Enums]
  // --------------------------------------------------------------------------

  //! AST node type.
  enum Type {
    kTypeNone = 0,                       //!< Not used.

    kTypeProgram,                        //!< Node is `AstProgram`.
    kTypeFunction,                       //!< Node is `AstFunction`.
    kTypeBlock,                          //!< Node is `AstBlock`.

    kTypeBranch,                         //!< Node is `AstBranch`.
    kTypeFor,                            //!< Node is `AstLoop` describing `for () { ... }`.
    kTypeWhile,                          //!< Node is `AstLoop` describing `while () { ... }`.
    kTypeDoWhile,                        //!< Node is `AstLoop` describing `do { ... } while ()`.

    kTypeBreak,                          //!< Node is `AstBreak`.
    kTypeContinue,                       //!< Node is `AstContinue`.
    kTypeReturn,                         //!< Node is `AstReturn`.

    kTypeVarDecl,                        //!< Node is `AstVarDecl`.
    kTypeVarMemb,                        //!< Node is `AstVarMemb`.
    kTypeVar,                            //!< Node is `AstVar`.
    kTypeImm,                            //!< Node is `AstImm`.

    kTypeUnaryOp,                        //!< Node is `AstUnaryOp`.
    kTypeBinaryOp,                       //!< Node is `AstBinaryOp`.
    kTypeCall                            //!< Node is `AstCall`.
  };

  //! AST node flags.
  enum Flags {
    kFlagSideEffect = 0x01
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline AstNode(AstBuilder* ast, uint32_t nodeType, AstNode** children = nullptr, uint32_t size = 0) noexcept
    : _ast(ast),
      _parent(nullptr),
      _children(children),
      _nodeType(static_cast<uint8_t>(nodeType)),
      _nodeFlags(0),
      _opType(kOpNone),
      _reserved(0),
      _position(~static_cast<uint32_t>(0)),
      _typeInfo(kTypeVoid),
      _size(size) {}

  inline void destroy(AstBuilder* ast) noexcept {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  template<typename T>
  inline T* as() noexcept { return static_cast<T*>(this); }

  template<typename T>
  inline const T* as() const noexcept { return static_cast<const T*>(this); }

  //! Get the `AstBuilder` instance that created this node.
  inline AstBuilder* ast() const noexcept { return _ast; }

  //! Check if the node has a parent.
  inline bool hasParent() const noexcept { return _parent != nullptr; }
  //! Get the parent node.
  inline AstNode* parent() const noexcept { return _parent; }

  //! Get whether the node is empty (has no children).
  inline bool empty() const noexcept { return !_size; }
  //! Get size of the children array.
  inline uint32_t size() const noexcept { return _size; }
  //! Get children array.
  inline AstNode** children() const noexcept { return reinterpret_cast<AstNode**>(_children); }

  //! Get node type.
  inline uint32_t nodeType() const noexcept { return _nodeType; }
  //! Get whether the node is a loop (for, do, while, ...).
  inline bool isLoop() const noexcept { return _nodeType >= kTypeFor && _nodeType <= kTypeDoWhile; }
  //! Get whether the node is `AstVar`.
  inline bool isVar() const noexcept { return _nodeType == kTypeVar; }
  //! Get whether the node is `AstImm`.
  inline bool isImm() const noexcept { return _nodeType == kTypeImm; }

  //! Get node flags.
  inline uint32_t nodeFlags() const noexcept { return _nodeFlags; }
  //! Get whether the node has a `flag` set.
  inline bool hasNodeFlag(uint32_t flag) const noexcept { return (static_cast<uint32_t>(_nodeFlags) & flag) != 0; }
  //! Set node flags.
  inline void setNodeFlags(uint32_t flags) noexcept { _nodeFlags = static_cast<uint8_t>(flags); }
  //! Add node flags.
  inline void addNodeFlags(uint32_t flags) noexcept { _nodeFlags |= static_cast<uint8_t>(flags); }

  //! Get operator type.
  inline uint32_t opType() const noexcept { return _opType; }
  //! Set operator type.
  inline void setOpType(uint32_t opType) noexcept { _opType = static_cast<uint8_t>(opType); }

  //! Get whether the node has associated position in source code.
  inline bool hasPosition() const noexcept { return _position != ~static_cast<uint32_t>(0); }
  //! Get source code position of the node.
  inline uint32_t position() const noexcept { return _position; }
  //! Set source code position of the node.
  inline void setPosition(uint32_t position) noexcept { _position = position; }
  //! Reset source code position of the node.
  inline void resetPosition() noexcept { _position = ~static_cast<uint32_t>(0); }

  //! Get type-info.
  inline uint32_t typeInfo() const noexcept { return _typeInfo; }
  //! Set type-info.
  inline void setTypeInfo(uint32_t typeInfo) noexcept { _typeInfo = typeInfo; }

  // --------------------------------------------------------------------------
  // [Children]
  // --------------------------------------------------------------------------

  inline AstNode* childAt(uint32_t index) const noexcept {
    MPSL_ASSERT(index < _size);
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

  AstBuilder* _ast;                      //!< AST builder.
  AstNode* _parent;                      //!< Parent node.
  AstNode** _children;                   //!< Child nodes.

  uint8_t _nodeType;                     //!< Node type, see \ref Type.
  uint8_t _nodeFlags;                    //!< Node flags, see \ref Flags.
  uint8_t _opType;                       //!< Operator, see `OpType`.
  uint8_t _reserved;                     //!< Reserved (padding).

  uint32_t _position;                    //!< Position (in chars) from the beginning of the source.
  uint32_t _typeInfo;                    //!< Type-info of the node (initially kTypeVoid).
  uint32_t _size;                        //!< Count of child-nodes.
};

#define MPSL_AST_CHILD(INDEX, NODE_T, NAME, NAME_UP)                          \
  inline NODE_T* NAME() const noexcept { return _##NAME; }                    \
  inline NODE_T* set##NAME_UP(NODE_T* node) noexcept {                        \
    return static_cast<NODE_T*>(replaceAt(INDEX, node));                      \
  }                                                                           \
                                                                              \
  inline NODE_T* unlink##NAME_UP() noexcept {                                 \
    NODE_T* node = _##NAME;                                                   \
                                                                              \
    MPSL_ASSERT(node != nullptr);                                             \
    MPSL_ASSERT(node->_parent == this);                                       \
                                                                              \
    node->_parent = nullptr;                                                  \
    _##NAME = nullptr;                                                        \
                                                                              \
    return node;                                                              \
  }                                                                           \
                                                                              \
  NODE_T* _##NAME

// ============================================================================
// [mpsl::AstBlock]
// ============================================================================

class AstBlock : public AstNode {
public:
  MPSL_NONCOPYABLE(AstBlock)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline AstBlock(AstBuilder* ast, uint32_t nodeType = kTypeBlock) noexcept
    : AstNode(ast, nodeType),
      _capacity(0) {}

  // --------------------------------------------------------------------------
  // [Ops]
  // --------------------------------------------------------------------------

  //! Reserve the capacity of the AstBlock so one more node can be added into it.
  //!
  //! \note This has to be called before you use `appendNode()` or `insertAt()`.
  //! The reason is that it's easier to deal with possible allocation failure
  //! here (before the node to be added is created) than after the node is
  //! created, but failed to add into the block.
  Error willAdd() noexcept;

  //! Append the given `node` to the block.
  //!
  //! \note You have to call `willAdd()` before you use `appendNode()` for every
  //! node you want to add to the block.
  inline void appendNode(AstNode* node) noexcept {
    MPSL_ASSERT(node != nullptr);
    MPSL_ASSERT(node->parent() == nullptr);

    // We expect `willAdd()` to be called before `appendNode()`.
    MPSL_ASSERT(_size < _capacity);

    node->_parent = this;

    _children[_size] = node;
    _size++;
  }

  //! Insert the given `node` to the block at index `i`.
  //!
  //! \note You have to call `willAdd()` before you use `insertAt()` for every
  //! node you want to add to the block.
  inline void insertAt(uint32_t i, AstNode* node) noexcept {
    MPSL_ASSERT(node != nullptr);
    MPSL_ASSERT(node->parent() == nullptr);

    // We expect `willAdd()` to be called before `insertAt()`.
    MPSL_ASSERT(_size < _capacity);

    AstNode** p = children();
    uint32_t j = _size;

    node->_parent = this;
    while (i < j) {
      p[j] = p[j - 1];
      j--;
    }

    p[j] = node;
    _size++;
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

class AstUnary : public AstNode {
public:
  MPSL_NONCOPYABLE(AstUnary)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline AstUnary(AstBuilder* ast, uint32_t nodeType) noexcept
    : AstNode(ast, nodeType, &_child, 1),
      _child(nullptr) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline AstNode** children() const noexcept { return (AstNode**)&_child; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  MPSL_AST_CHILD(0, AstNode, child, Child);
};

// ============================================================================
// [mpsl::AstBinary]
// ============================================================================

class AstBinary : public AstNode {
public:
  MPSL_NONCOPYABLE(AstBinary)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline AstBinary(AstBuilder* ast, uint32_t nodeType) noexcept
    : AstNode(ast, nodeType, &_left, 2),
      _left(nullptr),
      _right(nullptr) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline AstNode** children() const noexcept { return (AstNode**)&_left; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  MPSL_AST_CHILD(0, AstNode, left, Left);
  MPSL_AST_CHILD(1, AstNode, right, Right);
};

// ============================================================================
// [mpsl::AstProgram]
// ============================================================================

class AstProgram : public AstBlock {
public:
  MPSL_NONCOPYABLE(AstProgram)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline AstProgram(AstBuilder* ast) noexcept
    : AstBlock(ast, kTypeProgram) {}
};

// ============================================================================
// [mpsl::AstFunction]
// ============================================================================

class AstFunction : public AstNode {
public:
  MPSL_NONCOPYABLE(AstFunction)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline AstFunction(AstBuilder* ast) noexcept
    : AstNode(ast, kTypeFunction, (AstNode**)&_args, 2),
      _args(nullptr),
      _body(nullptr),
      _scope(nullptr),
      _func(nullptr),
      _ret(nullptr) {}

  inline void destroy(AstBuilder* ast) noexcept {
    ast->deleteScope(scope());
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline AstNode** children() const noexcept { return (AstNode**)&_args; }

  inline AstScope* scope() const noexcept { return _scope; }
  inline void setScope(AstScope* scope) noexcept { _scope = scope; }

  inline AstSymbol* func() const noexcept { return _func; }
  inline void setFunc(AstSymbol* func) noexcept { _func = func; }

  inline AstSymbol* ret() const noexcept { return _ret; }
  inline void setRet(AstSymbol* ret) noexcept { _ret = ret; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  MPSL_AST_CHILD(0, AstBlock, args, Args);
  MPSL_AST_CHILD(1, AstBlock, body, Body);

  AstScope* _scope;
  AstSymbol* _func;
  AstSymbol* _ret;
};

// ============================================================================
// [mpsl::AstBranch]
// ============================================================================

class AstBranch : public AstNode {
public:
  MPSL_NONCOPYABLE(AstBranch)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline AstBranch(AstBuilder* ast) noexcept
    : AstNode(ast, kTypeBranch, &_condition, 3),
      _condition(nullptr),
      _thenBody(nullptr),
      _elseBody(nullptr) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline AstNode** children() const noexcept { return (AstNode**)&_condition; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  MPSL_AST_CHILD(0, AstNode, condition, Condition);
  MPSL_AST_CHILD(1, AstNode, thenBody, ThenBody);
  MPSL_AST_CHILD(2, AstNode, elseBody, ElseBody);
};

// ============================================================================
// [mpsl::AstLoop]
// ============================================================================

class AstLoop : public AstNode {
public:
  MPSL_NONCOPYABLE(AstLoop)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline AstLoop(AstBuilder* ast, uint32_t nodeType) noexcept
    : AstNode(ast, nodeType, &_forInit, 4),
      _forInit(nullptr),
      _forIter(nullptr),
      _condition(nullptr),
      _body(nullptr) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline AstNode** children() const noexcept { return (AstNode**)&_forInit; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  MPSL_AST_CHILD(0, AstNode, forInit, ForInit);
  MPSL_AST_CHILD(1, AstNode, forIter, ForIter);
  MPSL_AST_CHILD(2, AstNode, condition, Condition);
  MPSL_AST_CHILD(3, AstBlock, body, Body);
};

// ============================================================================
// [mpsl::AstFlow]
// ============================================================================

class AstFlow : public AstNode {
public:
  MPSL_NONCOPYABLE(AstFlow)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline AstFlow(AstBuilder* ast, uint32_t nodeType) noexcept
    : AstNode(ast, nodeType) {}

  // --------------------------------------------------------------------------
  // [Interface]
  // --------------------------------------------------------------------------

  AstLoop* loop() const noexcept;
};

// ============================================================================
// [mpsl::AstBreak]
// ============================================================================

class AstBreak : public AstFlow {
public:
  MPSL_NONCOPYABLE(AstBreak)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline AstBreak(AstBuilder* ast) noexcept
    : AstFlow(ast, kTypeBreak) {}
};

// ============================================================================
// [mpsl::AstContinue]
// ============================================================================

class AstContinue : public AstFlow {
public:
  MPSL_NONCOPYABLE(AstContinue)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline AstContinue(AstBuilder* ast) noexcept
    : AstFlow(ast, kTypeContinue) {}
};

// ============================================================================
// [mpsl::AstReturn]
// ============================================================================

class AstReturn : public AstUnary {
public:
  MPSL_NONCOPYABLE(AstReturn)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline AstReturn(AstBuilder* ast) noexcept
    : AstUnary(ast, kTypeReturn) {}
};

// ============================================================================
// [mpsl::AstVarDecl]
// ============================================================================

class AstVarDecl : public AstUnary {
public:
  MPSL_NONCOPYABLE(AstVarDecl)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline AstVarDecl(AstBuilder* ast) noexcept
    : AstUnary(ast, kTypeVarDecl),
      _symbol(nullptr) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline AstSymbol* symbol() const noexcept { return _symbol; }
  inline void setSymbol(AstSymbol* symbol) noexcept { _symbol = symbol; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  AstSymbol* _symbol;
};

// ============================================================================
// [mpsl::AstVarMemb]
// ============================================================================

class AstVarMemb : public AstUnary {
public:
  MPSL_NONCOPYABLE(AstVarMemb)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline AstVarMemb(AstBuilder* ast) noexcept
    : AstUnary(ast, kTypeVarMemb),
      _field(nullptr, 0),
      _offset(0) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline const StringRef& field() const noexcept { return _field; }
  inline void setField(const char* s, size_t size) noexcept { _field.set(s, size); }

  inline int32_t offset() const noexcept { return _offset; }
  inline void setOffset(int offset) noexcept { _offset = offset; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  StringRef _field;                      //!< Accessoe name.
  int32_t _offset;                       //!< Member offset if this is a member access.
};

// ============================================================================
// [mpsl::AstVar]
// ============================================================================

class AstVar : public AstNode {
public:
  MPSL_NONCOPYABLE(AstVar)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline AstVar(AstBuilder* ast) noexcept
    : AstNode(ast, kTypeVar),
      _symbol(nullptr) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline AstSymbol* symbol() const noexcept { return _symbol; }
  inline void setSymbol(AstSymbol* symbol) noexcept { _symbol = symbol; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  AstSymbol* _symbol;
};

// ============================================================================
// [mpsl::AstImm]
// ============================================================================

class AstImm : public AstNode {
public:
  MPSL_NONCOPYABLE(AstImm)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline AstImm(AstBuilder* ast) noexcept
    : AstNode(ast, kTypeImm) {
    ::memset(&_value, 0, sizeof(_value));
    _slot = 0;
  }

  inline AstImm(AstBuilder* ast, const Value& value, uint32_t typeInfo) noexcept
    : AstNode(ast, kTypeImm) {
    _typeInfo = typeInfo;
    _value = value;
    _slot = 0;
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline const Value& value() const noexcept { return _value; }
  inline void setValue(const Value& value) noexcept { _value = value; }

  inline uint32_t slot() const noexcept { return _slot; }
  inline void setSlot(uint32_t slot) noexcept { _slot = slot; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  Value _value;
  uint32_t _slot;
};

// ============================================================================
// [mpsl::AstUnaryOp]
// ============================================================================

class AstUnaryOp : public AstUnary {
public:
  MPSL_NONCOPYABLE(AstUnaryOp)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline AstUnaryOp(AstBuilder* ast, uint32_t opType) noexcept
    : AstUnary(ast, kTypeUnaryOp),
      _swizzleUInt64(0) {
    setOpType(opType);
  }

  inline AstUnaryOp(AstBuilder* ast, uint32_t opType, uint32_t typeInfo) noexcept
    : AstUnary(ast, kTypeUnaryOp),
      _swizzleUInt64(0) {
    setOpType(opType);
    setTypeInfo(typeInfo);
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline uint8_t* swizzleArray() noexcept { return _swizzleArray; }
  inline const uint8_t* swizzleArray() const noexcept { return _swizzleArray; }
  inline uint32_t swizzleCount() const noexcept { return TypeInfo::elementsOf(typeInfo()); }

  inline uint32_t swizzleIndex(size_t position) const noexcept {
    MPSL_ASSERT(position < ASMJIT_ARRAY_SIZE(_swizzleArray));
    return _swizzleArray[position];
  }

  inline void setSwizzleIndex(uint32_t position, uint32_t value) noexcept {
    MPSL_ASSERT(position < ASMJIT_ARRAY_SIZE(_swizzleArray));
    MPSL_ASSERT(value <= 0xFF);
    _swizzleArray[position] = static_cast<uint8_t>(value);
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  union {
    uint8_t _swizzleArray[8];
    uint64_t _swizzleUInt64;
  };
};

// ============================================================================
// [mpsl::AstBinaryOp]
// ============================================================================

class AstBinaryOp : public AstBinary {
public:
  MPSL_NONCOPYABLE(AstBinaryOp)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline AstBinaryOp(AstBuilder* ast, uint32_t opType) noexcept
    : AstBinary(ast, kTypeBinaryOp) {
    setOpType(opType);
  }
};

// ============================================================================
// [mpsl::AstCall]
// ============================================================================

class AstCall : public AstBlock {
public:
  MPSL_NONCOPYABLE(AstCall)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline AstCall(AstBuilder* ast) noexcept
    : AstBlock(ast, kTypeCall),
      _symbol(nullptr) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline AstSymbol* symbol() const noexcept { return _symbol; }
  inline void setSymbol(AstSymbol* symbol) noexcept { _symbol = symbol; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  AstSymbol* _symbol;
};

// ============================================================================
// [mpsl::AstVisitor]
// ============================================================================

template<typename Impl>
class AstVisitor {
public:
  MPSL_NONCOPYABLE(AstVisitor<Impl>)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline AstVisitor(AstBuilder* ast) noexcept : _ast(ast) {}
  inline ~AstVisitor() noexcept {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline AstBuilder* ast() const noexcept { return _ast; }

  // --------------------------------------------------------------------------
  // [OnNode]
  // --------------------------------------------------------------------------

  MPSL_NOINLINE Error onNode(AstNode* node) noexcept {
    switch (node->nodeType()) {
      case AstNode::kTypeProgram  : return static_cast<Impl*>(this)->onProgram  (static_cast<AstProgram*  >(node));
      case AstNode::kTypeFunction : return static_cast<Impl*>(this)->onFunction (static_cast<AstFunction* >(node));
      case AstNode::kTypeBlock    : return static_cast<Impl*>(this)->onBlock    (static_cast<AstBlock*    >(node));
      case AstNode::kTypeBranch   : return static_cast<Impl*>(this)->onBranch   (static_cast<AstBranch*   >(node));
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
class AstVisitorWithArgs {
public:
  MPSL_NONCOPYABLE(AstVisitorWithArgs<Impl, Args>)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline AstVisitorWithArgs(AstBuilder* ast) noexcept : _ast(ast) {}
  inline ~AstVisitorWithArgs() noexcept {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline AstBuilder* ast() const noexcept { return _ast; }

  // --------------------------------------------------------------------------
  // [OnNode]
  // --------------------------------------------------------------------------

  MPSL_NOINLINE Error onNode(AstNode* node, Args args) noexcept {
    switch (node->nodeType()) {
      case AstNode::kTypeProgram  : return static_cast<Impl*>(this)->onProgram  (static_cast<AstProgram*  >(node), args);
      case AstNode::kTypeFunction : return static_cast<Impl*>(this)->onFunction (static_cast<AstFunction* >(node), args);
      case AstNode::kTypeBlock    : return static_cast<Impl*>(this)->onBlock    (static_cast<AstBlock*    >(node), args);
      case AstNode::kTypeBranch   : return static_cast<Impl*>(this)->onBranch   (static_cast<AstBranch*   >(node), args);
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

class AstDump : public AstVisitor<AstDump> {
public:
  MPSL_NONCOPYABLE(AstDump)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  inline AstDump(AstBuilder* ast, String& sb) noexcept
    : AstVisitor<AstDump>(ast),
      _sb(sb),
      _level(0) {}
  inline ~AstDump() noexcept {}

  // --------------------------------------------------------------------------
  // [OnNode]
  // --------------------------------------------------------------------------

  Error onProgram(AstProgram* node) noexcept;
  Error onFunction(AstFunction* node) noexcept;
  Error onBlock(AstBlock* node) noexcept;
  Error onBranch(AstBranch* node) noexcept;
  Error onLoop(AstLoop* node) noexcept;
  Error onBreak(AstBreak* node) noexcept;
  Error onContinue(AstContinue* node) noexcept;
  Error onReturn(AstReturn* node) noexcept;
  Error onVarDecl(AstVarDecl* node) noexcept;
  Error onVarMemb(AstVarMemb* node) noexcept;
  Error onVar(AstVar* node) noexcept;
  Error onImm(AstImm* node) noexcept;
  Error onUnaryOp(AstUnaryOp* node) noexcept;
  Error onBinaryOp(AstBinaryOp* node) noexcept;
  Error onCall(AstCall* node) noexcept;

  // --------------------------------------------------------------------------
  // [Helpers]
  // --------------------------------------------------------------------------

  Error info(const char* fmt, ...) noexcept;
  Error nest(const char* fmt, ...) noexcept;
  Error denest() noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  String& _sb;
  uint32_t _level;
};

// ============================================================================
// [mpsl::AstAnalysis]
// ============================================================================

//! \internal
//!
//! Visitor that does semantic analysis.
class AstAnalysis : public AstVisitor<AstAnalysis> {
public:
  MPSL_NONCOPYABLE(AstAnalysis)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  AstAnalysis(AstBuilder* ast, ErrorReporter* errorReporter) noexcept;
  ~AstAnalysis() noexcept;

  // --------------------------------------------------------------------------
  // [OnNode]
  // --------------------------------------------------------------------------

  Error onProgram(AstProgram* node) noexcept;
  Error onFunction(AstFunction* node) noexcept;
  Error onBlock(AstBlock* node) noexcept;
  Error onBranch(AstBranch* node) noexcept;
  Error onLoop(AstLoop* node) noexcept;
  Error onBreak(AstBreak* node) noexcept;
  Error onContinue(AstContinue* node) noexcept;
  Error onReturn(AstReturn* node) noexcept;
  Error onVarDecl(AstVarDecl* node) noexcept;
  Error onVarMemb(AstVarMemb* node) noexcept;
  Error onVar(AstVar* node) noexcept;
  Error onImm(AstImm* node) noexcept;
  Error onUnaryOp(AstUnaryOp* node) noexcept;
  Error onBinaryOp(AstBinaryOp* node) noexcept;
  Error onCall(AstCall* node) noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  inline AstSymbol* currentRet() const noexcept { return _currentRet; }
  inline bool isUnreachable() const noexcept { return _unreachable; }

  // --------------------------------------------------------------------------
  // [CheckAssignment]
  // --------------------------------------------------------------------------

  Error checkAssignment(AstNode* node, uint32_t op) noexcept;

  // --------------------------------------------------------------------------
  // [Cast]
  // --------------------------------------------------------------------------

  //! Perform an implicit cast.
  uint32_t implicitCast(AstNode* node, AstNode* child, uint32_t type) noexcept;

  //! Perform an internal cast to `bool` or `__qbool`.
  uint32_t boolCast(AstNode* node, AstNode* child) noexcept;

  // TODO: Move to `AstBuilder::onInvalidCast()`.
  //! Report an invalid implicit or explicit cast.
  Error invalidCast(uint32_t position, const char* msg, uint32_t fromTypeInfo, uint32_t toTypeInfo) noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  ErrorReporter* _errorReporter;
  AstSymbol* _currentRet;

  bool _unreachable;
};

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPAST_P_H
