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
class IRVar;
class IRImm;
class IRInst;
class IRBlock;

typedef ZoneVector<IRInst*> IRBody;

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
class IRObject {
public:
  MPSL_NONCOPYABLE(IRObject)

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

class IRVar : public IRObject {
public:
  MPSL_NONCOPYABLE(IRVar)

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

class IRMem : public IRObject {
public:
  MPSL_NONCOPYABLE(IRMem)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE IRMem(IRBuilder* ir, IRVar* base, IRVar* index, int32_t offset) noexcept
    : IRObject(ir, kTypeMem),
      _base(base),
      _index(index),
      _offset(offset) {

    if (base) base->_usageCount++;
    if (index) index->_usageCount++;
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get the base address (always a pointer).
  MPSL_INLINE IRVar* getBase() const noexcept { return _base; }
  //! Get the index to access, null if there is no index.
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

// ============================================================================
// [mpsl::IRBlock]
// ============================================================================

class IRBlock : public IRObject {
public:
  MPSL_NONCOPYABLE(IRBlock)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE IRBlock(IRBuilder* ir) noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE bool isEmpty() const noexcept { return _body.isEmpty(); }
  MPSL_INLINE uint32_t getBlockType() const noexcept { return _blockData._blockType; }

  MPSL_INLINE bool isAssembled() const noexcept { return _blockData._isAssembled != 0; }
  MPSL_INLINE void setAssembled() noexcept { _blockData._isAssembled = true; }

  MPSL_INLINE uint32_t getJitId() const noexcept { return _blockData._jitId; }
  MPSL_INLINE void setJitId(uint32_t id) noexcept { _blockData._jitId = id; }

  MPSL_INLINE IRBody& getBody() noexcept { return _body; }
  MPSL_INLINE const IRBody& getBody() const noexcept { return _body; }

  MPSL_INLINE Error append(IRInst* inst) noexcept { return _body.append(inst); }
  MPSL_INLINE Error prepend(IRInst* inst) noexcept { return _body.prepend(inst); }

  MPSL_INLINE void neuterAt(size_t i) noexcept {
    MPSL_ASSERT(i < _body.getLength());
    _body[i] = nullptr;
    _neutered = true;
  }

  MPSL_NOAPI void _fixupAfterNeutering() noexcept;

  MPSL_INLINE void fixupAfterNeutering() noexcept {
    if (_neutered)
      _fixupAfterNeutering();
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Previous block in IRBuilder's block list.
  IRBlock* _prevBlock;
  //! Next block in IRBuilder's block list.
  IRBlock* _nextBlock;

  //! Block body (instructions).
  IRBody _body;
  //! Body contains nulls and should be updated.
  bool _neutered;
};

// ============================================================================
// [mpsl::IRPair]
// ============================================================================

//! A pair of two IR objects.
//!
//! Used to split 256-bit wide operations to 128-bit in case the target
//! doesn't support 256-bit wide registers (all pre-AVX architectures, ARM).
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

// ============================================================================
// [mpsl::IRBuilder]
// ============================================================================

class IRBuilder {
public:
  MPSL_NONCOPYABLE(IRBuilder)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_NOAPI IRBuilder(ZoneHeap* heap, uint32_t numSlots) noexcept;
  MPSL_NOAPI ~IRBuilder() noexcept;

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE ZoneHeap* getHeap() const noexcept { return _heap; }
  MPSL_INLINE IRBlock* getMainBlock() const noexcept { return _entryBlock; }

  MPSL_INLINE IRVar* getDataPtr(uint32_t slot) const noexcept {
    MPSL_ASSERT(slot < _numSlots);
    return _dataSlots[slot];
  }

  MPSL_INLINE uint32_t getNumSlots() const noexcept { return _numSlots; }

  // --------------------------------------------------------------------------
  // [Factory]
  // --------------------------------------------------------------------------

#define MPSL_ALLOC_IR_OBJECT(_Size_) \
  void* obj = _heap->alloc(_Size_); \
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
    void* inst = _heap->alloc(size);

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

  MPSL_NOAPI void deleteInst(IRInst* obj) noexcept;
  MPSL_NOAPI void deleteObject(IRObject* obj) noexcept;

  // --------------------------------------------------------------------------
  // [Init]
  // --------------------------------------------------------------------------

  MPSL_NOAPI Error initEntryBlock() noexcept;

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

  //! Memory heap used to allocate `IRObject`s.
  ZoneHeap* _heap;
  //! Main block.
  IRBlock* _entryBlock;
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

MPSL_INLINE IRBlock::IRBlock(IRBuilder* ir) noexcept
  : IRObject(ir, kTypeBlock),
    _prevBlock(nullptr),
    _nextBlock(nullptr),
    _body(ir->getHeap()),
    _neutered(false) {
  _blockData._blockType = kIRBlockBasic;
  _blockData._isAssembled = false;
  _blockData._jitId = kInvalidValue;
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPIR_P_H
