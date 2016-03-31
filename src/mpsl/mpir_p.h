// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPIR_P_H
#define _MPSL_MPIR_P_H

// [Dependencies - MPSL]
#include "./mpallocator_p.h"
#include "./mpast_p.h"
#include "./mphash_p.h"
#include "./mplang_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [Forward Declarations]
// ============================================================================

struct IRBuilder;
struct IRObject;
struct IRVar;
struct IRImm;
struct IRInst;
struct IRBlock;

// ============================================================================
// [mpsl::IRBlockType]
// ============================================================================

enum IRBlockType {
  //! Basic block.
  kIRBlockBasic = 0,
  //! Entry point.
  kIRBlockEntry
};

// ============================================================================
// [mpsl::IRRegType]
// ============================================================================

//! \internal
//!
//! Type of IR register (used by variables and immediates).
enum IRRegType {
  //! Invalid register (not used).
  kIRRegNone = 0,
  //! General purpose register (32-bit scalar int or pointer).
  kIRRegGP = 1,
  //! SIMD register (128-bit or 256-bit).
  kIRRegSIMD = 2,
  //! Count of register types
  kIRRegCount = 3
};

// ============================================================================
// [mpsl::IRObject]
// ============================================================================

//! Base class for all IR objects.
struct IRObject {
  MPSL_NO_COPY(IRObject)

  // --------------------------------------------------------------------------
  // [Enums]
  // --------------------------------------------------------------------------

  //! Type of \ref IRObject.
  enum Type {
    //! Not used.
    kTypeNone = 0,

    //! The object is \ref IRVar.
    kTypeVar,
    //! The object is \ref IRMem.
    kTypeMem,
    //! The object is \ref IRImm.
    kTypeImm,

    //! The object is \ref IRInst.
    kTypeInst,
    //! The object is \ref IRBlock.
    kTypeBlock
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
    _usageCount = 0;
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get the object type, see \ref IRObjectType.
  MPSL_INLINE uint32_t getObjectType() const noexcept { return _anyData._objectType; }

  //! Get whether the `IRObject` is `IRVar`.
  MPSL_INLINE bool isVar() const noexcept { return getObjectType() == kTypeVar; }
  //! Get whether the `IRObject` is `IRMem`.
  MPSL_INLINE bool isMem() const noexcept { return getObjectType() == kTypeMem; }
  //! Get whether the `IRObject` is `IRImm`.
  MPSL_INLINE bool isImm() const noexcept { return getObjectType() == kTypeImm; }
  //! Get whether the `IRObject` is `IRInst`.
  MPSL_INLINE bool isInst() const noexcept { return getObjectType() == kTypeInst; }
  //! Get whether the `IRObject` is `IRBlock`.
  MPSL_INLINE bool isBlock() const noexcept { return getObjectType() == kTypeBlock; }

  //! Get the object ID.
  MPSL_INLINE uint32_t getId() const noexcept { return _id; }

  //! Get register type, only \ref IRVar and \ref IRImm.
  MPSL_INLINE uint32_t getReg() const noexcept {
    MPSL_ASSERT(isVar() || isImm());
    // This member is the same for IRVar and IRImm.
    return _varData._reg;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  struct AnyData {
    //! Type of the IRObject, see \ref IRObjectType.
    uint8_t _objectType;
    //! \internal.
    uint8_t _reserved[7];
  };

  struct VarData {
    //! Type of the IRObject, see \ref IRObjectType.
    uint8_t _objectType;
    //! Type of the IRVar's register, see \ref IRRegType.
    uint8_t _reg;
    //! Register width used (in bytes).
    uint8_t _width;
    //! \internal
    uint8_t _reserved[1];
    //! JIT compiler associated ID.
    uint32_t _jitId;
  };

  struct ImmData {
    //! Type of the IRObject, see \ref IRObjectType.
    uint8_t _objectType;
    //! Type of this object, see \ref IRVarType.
    uint8_t _reg;
    //! Size of the immediate in bytes.
    uint8_t _width;
    //! \internal
    uint8_t _reserved[1];

    //! Type information of the immediate (for debugging).
    uint32_t _typeInfo;
  };

  struct InstData {
    //! Type of the IRObject, see \ref IRObjectType.
    uint8_t _objectType;
    //! \internal
    uint8_t _reserved[1];
    //! Count of operands.
    uint16_t _opCount;

    //! Instruction code in case this is an \ref IRInst.
    uint32_t _instCode;
  };

  struct BlockData {
    //! Type of the IRObject, see \ref IRObjectType.
    uint8_t _objectType;
    //! Type of the IRBlock.
    uint8_t _blockType;
    //! Whether the block has been assembled by JIT compiler.
    uint8_t _isAssembled;
    //! \internal.
    uint8_t _reserved[1];
    //! JIT compiler associated ID.
    uint32_t _jitId;
  };

  union {
    AnyData _anyData;
    VarData _varData;
    ImmData _immData;
    InstData _instData;
    BlockData _blockData;
  };

  //! ID of the IRObject.
  //!
  //! NOTE: IDs are used by IRBlock, IRVar, and IRImm objects. They have all
  //! their own ID generator so IDs can collide between different object types.
  uint32_t _id;

  //! Increased every time this IRObject is used as an operand.
  uint32_t _usageCount;
};

// ============================================================================
// [mpsl::IRVar]
// ============================================================================

struct IRVar : public IRObject {
  MPSL_NO_COPY(IRVar)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE IRVar(IRBuilder* ir, uint32_t reg, uint32_t width) noexcept
    : IRObject(ir, kTypeVar) {
    _varData._reg = static_cast<uint8_t>(reg);
    _varData._width = static_cast<uint8_t>(width);
    _varData._jitId = kInvalidValue;
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

struct IRMem : public IRObject {
  MPSL_NO_COPY(IRMem)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE IRMem(IRBuilder* ir, IRVar* base, IRVar* index, int32_t offset) noexcept
    : IRObject(ir, kTypeMem),
      _base(base),
      _index(index),
      _offset(offset) {

    if (base != nullptr) base->_usageCount++;
    if (index != nullptr) index->_usageCount++;
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get the base address (always a pointer).
  MPSL_INLINE IRVar* getBase() const noexcept { return _base; }
  //! Get the index to access, `nullptr` if there is no index.
  MPSL_INLINE IRVar* getIndex() const noexcept { return _index; }
  //! Get the displacement.
  MPSL_INLINE int32_t getOffset() const noexcept { return _offset; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  IRVar* _base;
  IRVar* _index;
  int32_t _offset;
};

// ============================================================================
// [mpsl::IRImm]
// ============================================================================

struct IRImm : public IRObject {
  MPSL_NO_COPY(IRImm)

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

struct IRInst : public IRObject {
  MPSL_NO_COPY(IRInst)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE IRInst(IRBuilder* ir, uint32_t instCode, uint32_t opCount) noexcept
    : IRObject(ir, kTypeInst),
      _block(nullptr),
      _prev(nullptr),
      _next(nullptr) {

    _instData._opCount = static_cast<uint16_t>(opCount);
    _instData._instCode = instCode;
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE bool hasBlock() const noexcept { return _block != nullptr; }
  MPSL_INLINE IRBlock* getBlock() const noexcept { return _block; }

  MPSL_INLINE bool hasPrev() const noexcept { return _prev != nullptr; }
  MPSL_INLINE IRInst* getPrev() const noexcept { return _prev; }

  MPSL_INLINE bool hasNext() const noexcept { return _next != nullptr; }
  MPSL_INLINE IRInst* getNext() const noexcept { return _next; }

  MPSL_INLINE uint32_t getInstCode() const noexcept { return _instData._instCode; }
  MPSL_INLINE uint32_t getOpCount() const noexcept { return _instData._opCount; }
  MPSL_INLINE IRObject** getOpArray() const noexcept { return (IRObject**)&_opArray; }

  MPSL_INLINE IRObject* getOperand(size_t index) const noexcept {
    MPSL_ASSERT(index < static_cast<size_t>(getOpCount()));
    return _opArray[index];
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Get the block this node is part of.
  IRBlock* _block;
  //! Get the previous node in the `_block`.
  IRInst* _prev;
  //! Get the next node in the `_block`.
  IRInst* _next;

  IRObject* _opArray[1];
};

// ============================================================================
// [mpsl::IRBlock]
// ============================================================================

struct IRBlock : IRObject {
  MPSL_NO_COPY(IRBlock)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE IRBlock(IRBuilder* ir) noexcept
    : IRObject(ir, kTypeBlock),
      _prevBlock(nullptr),
      _nextBlock(nullptr),
      _firstChild(nullptr),
      _lastChild(nullptr) {

    _blockData._blockType = kIRBlockBasic;
    _blockData._isAssembled = false;
    _blockData._jitId = kInvalidValue;
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE bool isEmpty() const noexcept { return _firstChild == nullptr; }
  MPSL_INLINE uint32_t getBlockType() const noexcept { return _blockData._blockType; }

  MPSL_INLINE bool isAssembled() const noexcept { return _blockData._isAssembled != 0; }
  MPSL_INLINE void setAssembled() noexcept { _blockData._isAssembled = true; }

  MPSL_INLINE uint32_t getJitId() const noexcept { return _blockData._jitId; }
  MPSL_INLINE void setJitId(uint32_t id) noexcept { _blockData._jitId = id; }

  MPSL_INLINE IRInst* getFirstChild() const noexcept { return _firstChild; }
  MPSL_INLINE IRInst* getLastChild() const noexcept { return _lastChild; }

  MPSL_INLINE IRInst* append(IRInst* inst) noexcept {
    MPSL_ASSERT(inst->_block == nullptr);
    MPSL_ASSERT(inst->_prev == nullptr);
    MPSL_ASSERT(inst->_next == nullptr);

    IRInst* last = _lastChild;
    inst->_block = this;

    if (last == nullptr) {
      _firstChild = _lastChild = inst;
    }
    else {
      MPSL_ASSERT(last->_next == nullptr);

      last->_next = inst;
      inst->_prev = last;

      _lastChild = inst;
    }

    return inst;
  }

  MPSL_INLINE IRInst* prepend(IRInst* inst) noexcept {
    MPSL_ASSERT(inst->_block == nullptr);
    MPSL_ASSERT(inst->_prev == nullptr);
    MPSL_ASSERT(inst->_next == nullptr);

    IRInst* first = _firstChild;
    inst->_block = this;

    if (first == nullptr) {
      _firstChild = _lastChild = inst;
    }
    else {
      MPSL_ASSERT(first->_prev == nullptr);

      first->_prev = inst;
      inst->_next = first;

      _firstChild = inst;
    }

    return inst;
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Previous block in IRBuilder's block list.
  IRBlock* _prevBlock;
  //! Next block in IRBuilder's block list.
  IRBlock* _nextBlock;

  //! First instruction in the block.
  IRInst* _firstChild;
  //! Last instruction in the block.
  IRInst* _lastChild;
};

// ============================================================================
// [mpsl::IRPair]
// ============================================================================

//! A pair of two IR objects.
//!
//! Used to split 256-bit wide operations to 128-bit in case the target
//! doesn't support 256-bit wide registers (all pre-AVX architectures, ARM).
template<typename T>
struct IRPair {
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

// ============================================================================
// [mpsl::IRBuilder]
// ============================================================================

struct IRBuilder {
  MPSL_NO_COPY(IRBuilder)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_NOAPI IRBuilder(Allocator* allocator, uint32_t numSlots) noexcept;
  MPSL_NOAPI ~IRBuilder() noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE Allocator* getAllocator() const noexcept { return _allocator; }
  MPSL_INLINE IRBlock* getMainBlock() const noexcept { return _mainBlock; }

  MPSL_INLINE IRVar* getDataPtr(uint32_t slot) const noexcept {
    MPSL_ASSERT(slot < _numSlots);
    return _dataSlots[slot];
  }

  MPSL_INLINE uint32_t getNumSlots() const noexcept { return _numSlots; }

  // --------------------------------------------------------------------------
  // [Factory]
  // --------------------------------------------------------------------------

#define MPSL_ALLOC_IR_OBJECT(_Size_) \
  void* obj = _allocator->alloc(_Size_); \
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

  MPSL_NOAPI IRVar* newVar(uint32_t reg, uint32_t width) noexcept;
  MPSL_NOAPI IRVar* newVarByTypeInfo(uint32_t typeInfo) noexcept;

  MPSL_NOAPI IRMem* newMem(IRVar* base, IRVar* index, int32_t offset) noexcept;

  MPSL_NOAPI IRImm* newImm(const Value& value, uint32_t reg, uint32_t immSize) noexcept;
  MPSL_NOAPI IRImm* newImmByTypeInfo(const Value& value, uint32_t typeInfo) noexcept;

  MPSL_NOAPI IRBlock* newBlock() noexcept;

  MPSL_INLINE IRInst* _newInst(uint32_t instCode, uint32_t opCount) noexcept {
    size_t size = (sizeof(IRInst) - sizeof(IRObject*)) + sizeof(IRObject*) * opCount;
    void* inst = _allocator->alloc(size);

    if (inst == nullptr)
      return nullptr;

    return new(inst) IRInst(this, instCode, opCount);
  }

  MPSL_INLINE IRInst* newInst(uint32_t instCode, IRObject* o0) noexcept {
    IRInst* inst = _newInst(instCode, 1);
    if (inst == nullptr) return nullptr;

    inst->_opArray[0] = o0;
    o0->_usageCount++;

    return inst;
  }

  MPSL_INLINE IRInst* newInst(uint32_t instCode, IRObject* o0, IRObject* o1) noexcept {
    IRInst* inst = _newInst(instCode, 2);
    if (inst == nullptr) return nullptr;

    inst->_opArray[0] = o0;
    inst->_opArray[1] = o1;

    o0->_usageCount++;
    o1->_usageCount++;

    return inst;
  }

  MPSL_INLINE IRInst* newInst(uint32_t instCode, IRObject* o0, IRObject* o1, IRObject* o2) noexcept {
    IRInst* inst = _newInst(instCode, 3);
    if (inst == nullptr) return nullptr;

    inst->_opArray[0] = o0;
    inst->_opArray[1] = o1;
    inst->_opArray[2] = o2;

    o0->_usageCount++;
    o1->_usageCount++;
    o2->_usageCount++;

    return inst;
  }

  MPSL_NOAPI void deleteObject(IRObject* obj) noexcept;

  // --------------------------------------------------------------------------
  // [Init]
  // --------------------------------------------------------------------------

  MPSL_NOAPI Error initMainBlock() noexcept;

  // --------------------------------------------------------------------------
  // [Emit]
  // --------------------------------------------------------------------------

  MPSL_NOAPI Error emitInst(IRBlock* block, uint32_t instCode, IRObject* o0) noexcept;
  MPSL_NOAPI Error emitInst(IRBlock* block, uint32_t instCode, IRObject* o0, IRObject* o1) noexcept;
  MPSL_NOAPI Error emitInst(IRBlock* block, uint32_t instCode, IRObject* o0, IRObject* o1, IRObject* o2) noexcept;

  MPSL_NOAPI Error emitMove(IRBlock* block, IRVar* dst, IRVar* src) noexcept;
  // TODO: Probably remove.
  MPSL_NOAPI Error emitFetch(IRBlock* block, IRVar* dst, IRObject* src) noexcept;

  // --------------------------------------------------------------------------
  // [Dump]
  // --------------------------------------------------------------------------

  MPSL_NOAPI Error dump(StringBuilder& sb) noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Memory allocator used to allocate `IRObject`s.
  Allocator* _allocator;
  //! Main block.
  IRBlock* _mainBlock;
  //! First block of managed by `IRBuilder`.
  IRBlock* _blockFirst;
  //! Last block of managed by `IRBuilder`.
  IRBlock* _blockLast;

  //! Entry point arguments.
  IRVar* _dataSlots[Globals::kMaxArgumentsCount];
  //! Number of entry-point arguments.
  uint32_t _numSlots;

  //! Block ID generator.
  uint32_t _blockIdGen;
  //! Variable ID generator.
  uint32_t _varIdGen;
};

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPIR_P_H
