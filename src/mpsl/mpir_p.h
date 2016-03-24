// [MPSL]
// Shader-Like Mathematical Expression JIT Engine for C++.
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
// [mpsl::IRObjectType]
// ============================================================================

//! \internal
//!
//! Type of \ref IRObject.
enum IRObjectType {
  //! Not used.
  kIRObjectNone = 0,

  //! The object is \ref IRVar.
  kIRObjectVar,
  //! The object is \ref IRMem.
  kIRObjectMem,
  //! The object is \ref IRImm.
  kIRObjectImm,

  //! The object is \ref IRInst.
  kIRObjectInst,
  //! The object is \ref IRBlock.
  kIRObjectBlock
};

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
  //! GP register (32-bit scalar int or pointer).
  kIRRegGP,
  //! SIMD register (128-bit or 256-bit).
  kIRRegSIMD,
  //! Count of register types
  kIRRegCount
};

// ============================================================================
// [mpsl::IRInstCode]
// ============================================================================

//! \internal
//!
//! IR instruction code.
enum IRInstCode {
  //! Not used.
  kIRInstMask = 0x0FFF,
  kIRInstNone = 0,

  kIRInstJmp,
  kIRInstJcc,
  kIRInstCall,
  kIRInstRet,

  kIRInstMov32,
  kIRInstMov64,
  kIRInstMov128,
  kIRInstMov256,

  kIRInstMovI32,
  kIRInstMovF32,
  kIRInstMovF64,

  kIRInstFetch32,
  kIRInstFetch64,
  kIRInstFetch128,
  kIRInstFetch256,

  kIRInstStore32,
  kIRInstStore64,
  kIRInstStore128,
  kIRInstStore256,

  kIRInstInsert32,
  kIRInstInsert64,

  kIRInstExtract32,
  kIRInstExtract64,

  kIRInstCvtI32ToF32,
  kIRInstCvtI32ToF64,

  kIRInstCvtF32ToI32,
  kIRInstCvtF32ToF64,

  kIRInstCvtF64ToI32,
  kIRInstCvtF64ToF32,

  kIRInstCmpEqI32,
  kIRInstCmpEqF32,
  kIRInstCmpEqF64,

  kIRInstCmpNeI32,
  kIRInstCmpNeF32,
  kIRInstCmpNeF64,

  kIRInstCmpLtI32,
  kIRInstCmpLtF32,
  kIRInstCmpLtF64,

  kIRInstCmpLeI32,
  kIRInstCmpLeF32,
  kIRInstCmpLeF64,

  kIRInstCmpGtI32,
  kIRInstCmpGtF32,
  kIRInstCmpGtF64,

  kIRInstCmpGeI32,
  kIRInstCmpGeF32,
  kIRInstCmpGeF64,

  kIRInstBitNegI32,
  kIRInstBitNegF32,
  kIRInstBitNegF64,

  kIRInstNegI32,
  kIRInstNegF32,
  kIRInstNegF64,

  kIRInstNotI32,
  kIRInstNotF32,
  kIRInstNotF64,

  kIRInstAddI32,
  kIRInstAddF32,
  kIRInstAddF64,

  kIRInstSubI32,
  kIRInstSubF32,
  kIRInstSubF64,

  kIRInstMulI32,
  kIRInstMulF32,
  kIRInstMulF64,

  kIRInstDivI32,
  kIRInstDivF32,
  kIRInstDivF64,

  kIRInstModI32,
  kIRInstModF32,
  kIRInstModF64,

  kIRInstAndI32,
  kIRInstAndF32,
  kIRInstAndF64,

  kIRInstOrI32,
  kIRInstOrF32,
  kIRInstOrF64,

  kIRInstXorI32,
  kIRInstXorF32,
  kIRInstXorF64,

  kIRInstSarI32,
  kIRInstShrI32,
  kIRInstShlI32,
  kIRInstRorI32,
  kIRInstRolI32,

  kIRInstMinI32,
  kIRInstMinF32,
  kIRInstMinF64,

  kIRInstMaxI32,
  kIRInstMaxF32,
  kIRInstMaxF64,

  kIRInstIsNanF32,
  kIRInstIsNanF64,

  kIRInstIsInfF32,
  kIRInstIsInfF64,

  kIRInstIsFiniteF32,
  kIRInstIsFiniteF64,

  kIRInstSignBitI32,
  kIRInstSignBitF32,
  kIRInstSignBitF64,

  kIRInstTruncF32,
  kIRInstTruncF64,

  kIRInstFloorF32,
  kIRInstFloorF64,

  kIRInstRoundF32,
  kIRInstRoundF64,

  kIRInstRoundEvenF32,
  kIRInstRoundEvenF64,

  kIRInstCeilF32,
  kIRInstCeilF64,

  kIRInstAbsI32,
  kIRInstAbsF32,
  kIRInstAbsF64,

  kIRInstExpF32,
  kIRInstExpF64,

  kIRInstLogF32,
  kIRInstLogF64,

  kIRInstLog2F32,
  kIRInstLog2F64,

  kIRInstLog10F32,
  kIRInstLog10F64,

  kIRInstSqrtF32,
  kIRInstSqrtF64,

  kIRInstSinF32,
  kIRInstSinF64,

  kIRInstCosF32,
  kIRInstCosF64,

  kIRInstTanF32,
  kIRInstTanF64,

  kIRInstAsinF32,
  kIRInstAsinF64,

  kIRInstAcosF32,
  kIRInstAcosF64,

  kIRInstAtanF32,
  kIRInstAtanF64,

  kIRInstPowF32,
  kIRInstPowF64,

  kIRInstAtan2F32,
  kIRInstAtan2F64,

  kIRInstCopySignF32,
  kIRInstCopySignF64,

  kIRInstCount,

  kIRInstVecMask = 0xF000,
  kIRInstVec128 = 0x1000,
  kIRInstVec256 = 0x2000
};

// ============================================================================
// [mpsl::IRInstFlags]
// ============================================================================

enum IRInstFlags {
  kIRInstFlagI32     = 0x0001,
  kIRInstFlagF32     = 0x0002,
  kIRInstFlagF64     = 0x0004,
  kIRInstFlagMov     = 0x0010,
  kIRInstFlagCvt     = 0x0020,
  kIRInstFlagFetch   = 0x0040,
  kIRInstFlagStore   = 0x0080,
  kIRInstFlagJxx     = 0x0100,
  kIRInstFlagRet     = 0x0200,
  kIRInstFlagCall    = 0x0400,
  kIRInstFlagComplex = 0x8000
};

// ============================================================================
// [mpsl::IRInstInfo]
// ============================================================================

struct IRInstInfo {
  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE bool isI32() const noexcept { return flags & kIRInstFlagI32; }
  MPSL_INLINE bool isF32() const noexcept { return flags & kIRInstFlagF32; }
  MPSL_INLINE bool isF64() const noexcept { return flags & kIRInstFlagF64; }
  MPSL_INLINE bool isCvt() const noexcept { return flags & kIRInstFlagCvt; }

  MPSL_INLINE bool isFetch() const noexcept { return flags & kIRInstFlagFetch; }
  MPSL_INLINE bool isStore() const noexcept { return flags & kIRInstFlagStore; }

  MPSL_INLINE bool isJxx() const noexcept { return flags & kIRInstFlagJxx; }
  MPSL_INLINE bool isRet() const noexcept { return flags & kIRInstFlagRet; }
  MPSL_INLINE bool isCall() const noexcept { return flags & kIRInstFlagCall; }
  MPSL_INLINE bool isComplex() const noexcept { return flags & kIRInstFlagComplex; }

  MPSL_INLINE bool getNumOps() const noexcept { return numOps; }
  MPSL_INLINE const char* getName() const noexcept { return name; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint16_t flags;
  uint8_t numOps;
  char name[13];
};
extern const IRInstInfo mpInstInfo[kIRInstCount];

// ============================================================================
// [mpsl::IRObject]
// ============================================================================

//! Base class for all IR objects.
struct IRObject {
  MPSL_NO_COPY(IRObject)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE IRObject(IRBuilder* ir, uint32_t objectType) noexcept
    : _ir(ir) {

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

  //! Get context that created this IRObject.
  MPSL_INLINE IRBuilder* getIR() const noexcept { return _ir; }

  //! Get the object type, see \ref IRObjectType.
  MPSL_INLINE uint32_t getObjectType() const noexcept { return _anyData._objectType; }

  //! Get whether the `IRObject` is `IRVar`.
  MPSL_INLINE bool isVar() const noexcept { return getObjectType() == kIRObjectVar; }
  //! Get whether the `IRObject` is `IRMem`.
  MPSL_INLINE bool isMem() const noexcept { return getObjectType() == kIRObjectMem; }
  //! Get whether the `IRObject` is `IRImm`.
  MPSL_INLINE bool isImm() const noexcept { return getObjectType() == kIRObjectImm; }
  //! Get whether the `IRObject` is `IRInst`.
  MPSL_INLINE bool isInst() const noexcept { return getObjectType() == kIRObjectInst; }
  //! Get whether the `IRObject` is `IRBlock`.
  MPSL_INLINE bool isBlock() const noexcept { return getObjectType() == kIRObjectBlock; }

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

  //! IRBuilder that created this object.
  IRBuilder* _ir;

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
    : IRObject(ir, kIRObjectVar) {
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
    : IRObject(ir, kIRObjectMem),
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
    : IRObject(ir, kIRObjectImm) {}

  MPSL_INLINE IRImm(IRBuilder* ir, const Value& value, uint32_t reg, uint32_t width) noexcept
    : IRObject(ir, kIRObjectImm),
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
    : IRObject(ir, kIRObjectInst),
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
    : IRObject(ir, kIRObjectBlock),
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
  if (obj == nullptr) return nullptr

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
