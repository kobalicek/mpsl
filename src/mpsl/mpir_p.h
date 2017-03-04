// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPIR_P_H
#define _MPSL_MPIR_P_H

// [Dependencies - MPSL]
#include "./mpast_p.h"
#include "./mphash_p.h"
#include "./mplang_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [Forward Declarations]
// ============================================================================

class IRBuilder;
class IRObject;
class IRBlock;
class IRReg;
class IRMem;
class IRImm;
class IRInst;

typedef ZoneVector<IRInst*> IRBody;
typedef ZoneVector<IRBlock*> IRBlocks;

// ============================================================================
// [mpsl::IRBuilder]
// ============================================================================

class IRBuilder {
public:
  MPSL_NONCOPYABLE(IRBuilder)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  IRBuilder(ZoneHeap* heap, uint32_t numSlots) noexcept;
  ~IRBuilder() noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE ZoneHeap* getHeap() const noexcept { return _heap; }

  MPSL_INLINE IRBlocks& getBlocks() noexcept { return _blocks; }
  MPSL_INLINE const IRBlocks& getBlocks() const noexcept { return _blocks; }

  MPSL_INLINE IRBlocks& getExits() noexcept { return _exits; }
  MPSL_INLINE const IRBlocks& getExits() const noexcept { return _exits; }

  MPSL_INLINE IRBlock* getEntry() const noexcept {
    MPSL_ASSERT(!_blocks.isEmpty());
    return _blocks[0];
  }

  MPSL_INLINE IRReg* getDataPtr(uint32_t slot) const noexcept {
    MPSL_ASSERT(slot < _numSlots);
    return _dataSlots[slot];
  }

  MPSL_INLINE uint32_t getNumSlots() const noexcept { return _numSlots; }

  // --------------------------------------------------------------------------
  // [Factory]
  // --------------------------------------------------------------------------

#define MPSL_ALLOC_IR_OBJECT(SIZE) \
  void* obj = _heap->alloc(SIZE); \
  if (MPSL_UNLIKELY(obj == nullptr)) \
    return nullptr

  template<typename T>
  MPSL_INLINE T* newObject() noexcept {
    MPSL_ALLOC_IR_OBJECT(sizeof(T));
    return new(obj) T(this);
  }

  template<typename T, typename P0>
  MPSL_INLINE T* newObject(P0 p0) noexcept {
    MPSL_ALLOC_IR_OBJECT(sizeof(T));
    return new(obj) T(this, p0);
  }

  template<typename T, typename P0, typename P1>
  MPSL_INLINE T* newObject(P0 p0, P1 p1) noexcept {
    MPSL_ALLOC_IR_OBJECT(sizeof(T));
    return new(obj) T(this, p0, p1);
  }

  template<typename T, typename P0, typename P1, typename P2>
  MPSL_INLINE T* newObject(P0 p0, P1 p1, P2 p2) noexcept {
    MPSL_ALLOC_IR_OBJECT(sizeof(T));
    return new(obj) T(this, p0, p1, p2);
  }
#undef MPSL_ALLOC_IR_OBJECT

  IRReg* newVar(uint32_t reg, uint32_t width) noexcept;
  IRReg* newVarByTypeInfo(uint32_t typeInfo) noexcept;

  IRMem* newMem(IRReg* base, IRReg* index, int32_t offset) noexcept;

  IRImm* newImm(const Value& value, uint32_t reg, uint32_t immSize) noexcept;
  IRImm* newImmByTypeInfo(const Value& value, uint32_t typeInfo) noexcept;

  IRBlock* newBlock() noexcept;
  Error connectBlocks(IRBlock* predecessor, IRBlock* successor) noexcept;

  MPSL_INLINE IRInst* _newInst(uint32_t instCode, uint32_t opCount) noexcept;
  MPSL_INLINE IRInst* newInst(uint32_t instCode, IRObject* o0) noexcept;
  MPSL_INLINE IRInst* newInst(uint32_t instCode, IRObject* o0, IRObject* o1) noexcept;
  MPSL_INLINE IRInst* newInst(uint32_t instCode, IRObject* o0, IRObject* o1, IRObject* o2) noexcept;

  void deleteInst(IRInst* obj) noexcept;
  void deleteObject(IRObject* obj) noexcept;
  MPSL_INLINE void derefObject(IRObject* obj) noexcept;

  // --------------------------------------------------------------------------
  // [Init]
  // --------------------------------------------------------------------------

  Error initEntry() noexcept;

  // --------------------------------------------------------------------------
  // [Emit]
  // --------------------------------------------------------------------------

  Error emitInst(IRBlock* block, uint32_t instCode, IRObject* o0) noexcept;
  Error emitInst(IRBlock* block, uint32_t instCode, IRObject* o0, IRObject* o1) noexcept;
  Error emitInst(IRBlock* block, uint32_t instCode, IRObject* o0, IRObject* o1, IRObject* o2) noexcept;

  Error emitMove(IRBlock* block, IRReg* dst, IRReg* src) noexcept;
  // TODO: Probably remove.
  Error emitFetch(IRBlock* block, IRReg* dst, IRObject* src) noexcept;

  // --------------------------------------------------------------------------
  // [Dump]
  // --------------------------------------------------------------------------

  Error dump(StringBuilder& sb) noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  ZoneHeap* _heap;                       //!< Memory heap used to allocate IR objects.
  IRBlocks _blocks;                      //!< IR basic blocks.
  IRBlocks _exits;                       //!< IR basic blocks without successors (exits).

  //! Entry point arguments.
  IRReg* _dataSlots[Globals::kMaxArgumentsCount];
  uint32_t _numSlots;                    //!< Number of entry-point arguments.

  uint32_t _blockIdGen;                  //!< Block ID generator.
  uint32_t _varIdGen;                    //!< Variable ID generator.
};

// ============================================================================
// [mpsl::IRObject]
// ============================================================================

//! Base class for all IR objects.
class IRObject {
public:
  MPSL_NONCOPYABLE(IRObject)

  //! Type of \ref IRObject.
  enum Type {
    kTypeNone  = 0,                      //!< Not used.
    kTypeReg   = 1,                      //!< The object is \ref IRReg.
    kTypeMem   = 2,                      //!< The object is \ref IRMem.
    kTypeImm   = 3,                      //!< The object is \ref IRImm.
    kTypeBlock = 4                       //!< The object is \ref IRBlock.
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE IRObject(IRBuilder* ir, uint32_t objectType) noexcept {
    _anyData._objectType = static_cast<uint8_t>(objectType);
    _anyData._reserved[0] = 0;
    _anyData._reserved[1] = 0;
    _anyData._reserved[2] = 0;

    _id = 0;
    _refCount = 0;
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get the object type, see \ref IRObjectType.
  MPSL_INLINE uint32_t getObjectType() const noexcept { return _anyData._objectType; }

  //! Get whether the `IRObject` is `IRReg`.
  MPSL_INLINE bool isReg() const noexcept { return getObjectType() == kTypeReg; }
  //! Get whether the `IRObject` is `IRMem`.
  MPSL_INLINE bool isMem() const noexcept { return getObjectType() == kTypeMem; }
  //! Get whether the `IRObject` is `IRImm`.
  MPSL_INLINE bool isImm() const noexcept { return getObjectType() == kTypeImm; }
  //! Get whether the `IRObject` is `IRBlock`.
  MPSL_INLINE bool isBlock() const noexcept { return getObjectType() == kTypeBlock; }

  //! Get the object ID.
  MPSL_INLINE uint32_t getId() const noexcept { return _id; }

  //! Get register type, only \ref IRReg and \ref IRImm.
  MPSL_INLINE uint32_t getReg() const noexcept {
    MPSL_ASSERT(isReg() || isImm());
    // This member is the same for IRReg and IRImm.
    return _varData._reg;
  }

  MPSL_INLINE uint32_t getRefCount() noexcept { return _refCount; }
  MPSL_INLINE void addRef() noexcept { _refCount++; }

  template<typename T>
  MPSL_INLINE T* as() noexcept { return static_cast<T*>(this); }
  template<typename T>
  MPSL_INLINE const T* as() const noexcept { return static_cast<const T*>(this); }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  struct AnyData {
    uint8_t _objectType;                 //!< Type of the IRObject, see \ref IRObjectType.
    uint8_t _reserved[7];                //!< \internal.
  };

  struct VarData {
    uint8_t _objectType;                 //!< Type of the IRObject, see \ref IRObjectType.
    uint8_t _reg;                        //!< Type of the IRReg's register, see \ref IRRegType.
    uint8_t _width;                      //!< Register width used (in bytes).
    uint8_t _reserved[1];                //!< \internal
    uint32_t _jitId;                     //!< JIT compiler associated ID.
  };

  struct ImmData {
    uint8_t _objectType;                 //!< Type of the IRObject, see \ref IRObjectType.
    uint8_t _reg;                        //!< Type of this object, see \ref IRRegType.
    uint8_t _width;                      //!< Size of the immediate in bytes.
    uint8_t _reserved[1];                //!< \internal
    uint32_t _typeInfo;                  //!< Type information of the immediate (for debugging).
  };

  struct BlockData {
    uint8_t _objectType;                 //!< Type of the IRObject, see \ref IRObjectType.
    uint8_t _blockType;                  //!< Type of the IRBlock.
    uint8_t _isAssembled;                //!< Whether the block has been assembled by JIT compiler.
    uint8_t _reserved[1];                //!< \internal.
    uint32_t _jitId;                     //!< JIT compiler associated ID.
  };

  union {
    AnyData _anyData;
    VarData _varData;
    ImmData _immData;
    BlockData _blockData;
  };

  //! ID of the IRObject.
  //!
  //! NOTE: IDs are used by IRBlock, IRReg, and IRImm objects. They have all
  //! their own ID generator so IDs can collide between different object types.
  uint32_t _id;

  //! Increased every time this IRObject is used as an operand.
  uint32_t _refCount;
};

MPSL_INLINE void IRBuilder::derefObject(IRObject* obj) noexcept {
  if (--obj->_refCount == 0)
    deleteObject(obj);
}

// ============================================================================
// [mpsl::IRReg]
// ============================================================================

class IRReg : public IRObject {
public:
  MPSL_NONCOPYABLE(IRReg)

  //! IR register kind.
  enum Kind {
    kKindNone  = 0,                      //!< Invalid register (not used).
    kKindGp    = 1,                      //!< General purpose register (32-bit or 64-bit).
    kKindVec   = 2,                      //!< Vector register (64-bit, 128-bit or 256-bit).
    kKindCount = 3                       //!< Count of register types
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE IRReg(IRBuilder* ir, uint32_t reg, uint32_t width) noexcept
    : IRObject(ir, kTypeReg) {
    _varData._reg = static_cast<uint8_t>(reg);
    _varData._width = static_cast<uint8_t>(width);
    _varData._jitId = kInvalidRegId;
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE uint32_t getReg() const noexcept { return _varData._reg; }
  MPSL_INLINE uint32_t getWidth() const noexcept { return _varData._width; }

  MPSL_INLINE uint32_t getJitId() const noexcept { return _varData._jitId; }
  MPSL_INLINE void setJitId(uint32_t id) noexcept { _varData._jitId = id; }
};

// ============================================================================
// [mpsl::IRMem]
// ============================================================================

class IRMem : public IRObject {
public:
  MPSL_NONCOPYABLE(IRMem)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE IRMem(IRBuilder* ir, IRReg* base, IRReg* index, int32_t offset) noexcept
    : IRObject(ir, kTypeMem),
      _base(base),
      _index(index),
      _offset(offset) {

    if (base) base->addRef();
    if (index) index->addRef();
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get if the memory operand has a base register.
  MPSL_INLINE bool hasBase() const noexcept { return _base != nullptr; }
  //! Get base register.
  MPSL_INLINE IRReg* getBase() const noexcept { return _base; }

  //! Get if the memory operand has an index register.
  MPSL_INLINE bool hasIndex() const noexcept { return _index != nullptr; }
  //! Get index register.
  MPSL_INLINE IRReg* getIndex() const noexcept { return _index; }

  //! Get if the memory operand contains an offset.
  MPSL_INLINE bool hasOffset() const noexcept { return _offset != 0; }
  //! Get immediate offset.
  MPSL_INLINE int32_t getOffset() const noexcept { return _offset; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  IRReg* _base;
  IRReg* _index;
  int32_t _offset;
};

// ============================================================================
// [mpsl::IRImm]
// ============================================================================

class IRImm : public IRObject {
public:
  MPSL_NONCOPYABLE(IRImm)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE IRImm(IRBuilder* ir) noexcept
    : IRObject(ir, kTypeImm) {}

  MPSL_INLINE IRImm(IRBuilder* ir, const Value& value, uint32_t reg, uint32_t width) noexcept
    : IRObject(ir, kTypeImm),
      _value(value) {
    _immData._reg = static_cast<uint8_t>(reg);
    _immData._width = static_cast<uint8_t>(width);

    // Not associated by default.
    _immData._typeInfo = kTypeVoid;
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE uint32_t getReg() const noexcept { return _immData._reg; }
  MPSL_INLINE uint32_t getWidth() const noexcept { return _immData._width; }

  MPSL_INLINE uint32_t getTypeInfo() const noexcept { return _immData._typeInfo; }
  MPSL_INLINE void setTypeInfo(uint32_t ti) noexcept { _immData._typeInfo = ti; }

  MPSL_INLINE const Value& getValue() const noexcept { return _value; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Immediate value.
  Value _value;
};

// ============================================================================
// [mpsl::IRInst]
// ============================================================================

class IRInst {
public:
  MPSL_NONCOPYABLE(IRInst)

  enum { kMaxOperands = 4 };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE IRInst(IRBuilder* ir, uint32_t instCode, uint32_t opCount) noexcept
    : _instCode(instCode),
      _opCount(opCount) {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE uint32_t getInstCode() const noexcept { return _instCode; }
  MPSL_INLINE uint32_t getOpCount() const noexcept { return _opCount; }
  MPSL_INLINE IRObject** getOpArray() const noexcept { return (IRObject**)&_opArray; }

  MPSL_INLINE IRObject* getOperand(size_t index) const noexcept {
    MPSL_ASSERT(index < static_cast<size_t>(getOpCount()));
    return _opArray[index];
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Instruction code in case this is an \ref IRInst.
  uint32_t _instCode;
  //! Count of operands.
  uint32_t _opCount;

  //! Instruction operands array (maximum 4).
  IRObject* _opArray[kMaxOperands];
};

MPSL_INLINE IRInst* IRBuilder::_newInst(uint32_t instCode, uint32_t opCount) noexcept {
  size_t size = (sizeof(IRInst) - sizeof(IRObject*)) + sizeof(IRObject*) * opCount;
  void* inst = _heap->alloc(size);

  if (inst == nullptr)
    return nullptr;

  return new(inst) IRInst(this, instCode, opCount);
}

MPSL_INLINE IRInst* IRBuilder::newInst(uint32_t instCode, IRObject* o0) noexcept {
  IRInst* inst = _newInst(instCode, 1);
  if (inst == nullptr) return nullptr;

  inst->_opArray[0] = o0;
  o0->addRef();

  return inst;
}

MPSL_INLINE IRInst* IRBuilder::newInst(uint32_t instCode, IRObject* o0, IRObject* o1) noexcept {
  IRInst* inst = _newInst(instCode, 2);
  if (inst == nullptr) return nullptr;

  inst->_opArray[0] = o0;
  inst->_opArray[1] = o1;

  o0->addRef();
  o1->addRef();

  return inst;
}

MPSL_INLINE IRInst* IRBuilder::newInst(uint32_t instCode, IRObject* o0, IRObject* o1, IRObject* o2) noexcept {
  IRInst* inst = _newInst(instCode, 3);
  if (inst == nullptr) return nullptr;

  inst->_opArray[0] = o0;
  inst->_opArray[1] = o1;
  inst->_opArray[2] = o2;

  o0->addRef();
  o1->addRef();
  o2->addRef();

  return inst;
}

// ============================================================================
// [mpsl::IRBlock]
// ============================================================================

class IRBlock : public IRObject {
public:
  MPSL_NONCOPYABLE(IRBlock)

  //! Block kind.
  enum Kind {
    kKindBasic = 0,                      //!< Basic block.
    kKindEntry = 1,                      //!< Entry block (no predecessors).
    kKindExit  = 2                       //!< Exit block (no successors).
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE IRBlock(IRBuilder* ir) noexcept
    : IRObject(ir, kTypeBlock),
      _ir(ir),
      _body(),
      _requiresFixup(false) {
    _blockData._blockType = kKindBasic;
    _blockData._isAssembled = false;
    _blockData._jitId = kInvalidRegId;
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE bool isEmpty() const noexcept { return _body.isEmpty(); }
  MPSL_INLINE uint32_t getBlockType() const noexcept { return _blockData._blockType; }

  MPSL_INLINE bool isAssembled() const noexcept { return _blockData._isAssembled != 0; }
  MPSL_INLINE void setAssembled() noexcept { _blockData._isAssembled = true; }

  MPSL_INLINE uint32_t getJitId() const noexcept { return _blockData._jitId; }
  MPSL_INLINE void setJitId(uint32_t id) noexcept { _blockData._jitId = id; }

  MPSL_INLINE bool hasPredecessors() const noexcept { return !_predecessors.isEmpty(); }
  MPSL_INLINE IRBlocks& getPredecessors() noexcept { return _predecessors; }
  MPSL_INLINE const IRBlocks& getPredecessors() const noexcept { return _predecessors; }

  MPSL_INLINE bool hasSuccessors() const noexcept { return !_successors.isEmpty(); }
  MPSL_INLINE IRBlocks& getSuccessors() noexcept { return _successors; }
  MPSL_INLINE const IRBlocks& getSuccessors() const noexcept { return _successors; }

  MPSL_INLINE Error addSuccessor(IRBlock* successor) noexcept { return _ir->connectBlocks(this, successor); }
  MPSL_INLINE Error addPredecessor(IRBlock* predecessor) noexcept { return _ir->connectBlocks(predecessor, this); }

  MPSL_INLINE IRBody& getBody() noexcept { return _body; }
  MPSL_INLINE const IRBody& getBody() const noexcept { return _body; }

  MPSL_INLINE Error append(IRInst* inst) noexcept { return _body.append(_ir->_heap, inst); }
  MPSL_INLINE Error prepend(IRInst* inst) noexcept { return _body.prepend(_ir->_heap, inst); }

  MPSL_INLINE void neuterAt(size_t i) noexcept {
    MPSL_ASSERT(i < _body.getLength());
    _body[i] = nullptr;
    _requiresFixup = true;
  }

  void _fixupAfterNeutering() noexcept;

  MPSL_INLINE void fixupAfterNeutering() noexcept {
    if (_requiresFixup)
      _fixupAfterNeutering();
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  IRBuilder* _ir;                        //!< IR builder.
  IRBlocks _predecessors;                //!< Block's predecessors.
  IRBlocks _successors;                  //!< Block's successors.
  IRBody _body;                          //!< Block body (instructions).
  bool _requiresFixup;                   //!< Body contains nulls and must be fixed.
};

// ============================================================================
// [mpsl::IRPair]
// ============================================================================

//! A pair of two IR objects.
//!
//! Used to split 256-bit wide operations to 128-bit in case the target doesn't
//! support 256-bit wide registers, which is relevant for all architectures
//! before AVX2 and ARM.
template<typename T>
class IRPair {
public:
  MPSL_INLINE IRPair() noexcept
    : lo(nullptr),
      hi(nullptr) {}

  MPSL_INLINE IRPair(const IRPair<T>& other) noexcept
    : lo(other.lo),
      hi(other.hi) {}

  MPSL_INLINE Error set(T* lo, T* hi = nullptr) noexcept {
    this->lo = lo;
    this->hi = hi;
    return kErrorOk;
  }

  template<typename U>
  MPSL_INLINE Error set(const IRPair<U>& pair) noexcept {
    this->lo = static_cast<T*>(pair.lo);
    this->hi = static_cast<T*>(pair.hi);
    return kErrorOk;
  }

  MPSL_INLINE operator IRPair<IRObject>&() noexcept {
    return *reinterpret_cast<IRPair<IRObject>*>(this);
  }
  MPSL_INLINE operator const IRPair<IRObject>&() const noexcept {
    return *reinterpret_cast<const IRPair<IRObject>*>(this);
  }

  MPSL_INLINE void reset() noexcept {
    this->lo = nullptr;
    this->hi = nullptr;
  }

  union {
    struct {
      T* lo;
      T* hi;
    };

    T* obj[2];
  };
};

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPIR_P_H
