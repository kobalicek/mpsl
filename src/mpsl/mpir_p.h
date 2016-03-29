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
  //! General purpose register (32-bit scalar int or pointer).
  kIRRegGP = 1,
  //! SIMD register (128-bit or 256-bit).
  kIRRegSIMD = 2,
  //! Count of register types
  kIRRegCount = 3
};

// ============================================================================
// [mpsl::IRInstCode]
// ============================================================================

//! \internal
//!
//! IR instruction code and meta-data.
enum IRInstCode {
  // --------------------------------------------------------------------------
  // [Instruction-ID]
  // --------------------------------------------------------------------------

  //! Not used.
  kIRInstIdMask = 0x00003FFF,
  kIRInstIdNone = 0,

  kIRInstIdJmp,
  kIRInstIdJcc,
  kIRInstIdCall,
  kIRInstIdRet,

  kIRInstIdFetch32,
  kIRInstIdFetch64,
  kIRInstIdFetch96,
  kIRInstIdFetch128,
  kIRInstIdFetch192,
  kIRInstIdFetch256,

  kIRInstIdStore32,
  kIRInstIdStore64,
  kIRInstIdStore96,
  kIRInstIdStore128,
  kIRInstIdStore192,
  kIRInstIdStore256,

  kIRInstIdMov32,
  kIRInstIdMov64,
  kIRInstIdMov128,
  kIRInstIdMov256,

  kIRInstIdInsert32,
  kIRInstIdInsert64,

  kIRInstIdExtract32,
  kIRInstIdExtract64,

  kIRInstIdCvtI32ToF32,
  kIRInstIdCvtI32ToF64,
  kIRInstIdCvtF32ToI32,
  kIRInstIdCvtF32ToF64,
  kIRInstIdCvtF64ToI32,
  kIRInstIdCvtF64ToF32,

  kIRInstIdCmpEqI32,
  kIRInstIdCmpEqF32,
  kIRInstIdCmpEqF64,
  kIRInstIdCmpNeI32,
  kIRInstIdCmpNeF32,
  kIRInstIdCmpNeF64,
  kIRInstIdCmpLtI32,
  kIRInstIdCmpLtF32,
  kIRInstIdCmpLtF64,
  kIRInstIdCmpLeI32,
  kIRInstIdCmpLeF32,
  kIRInstIdCmpLeF64,
  kIRInstIdCmpGtI32,
  kIRInstIdCmpGtF32,
  kIRInstIdCmpGtF64,
  kIRInstIdCmpGeI32,
  kIRInstIdCmpGeF32,
  kIRInstIdCmpGeF64,

  kIRInstIdBitNegI32,
  kIRInstIdBitNegF32,
  kIRInstIdBitNegF64,
  kIRInstIdNegI32,
  kIRInstIdNegF32,
  kIRInstIdNegF64,
  kIRInstIdNotI32,
  kIRInstIdNotF32,
  kIRInstIdNotF64,
  kIRInstIdLzcntI32,
  kIRInstIdPopcntI32,

  kIRInstIdAddI32,
  kIRInstIdAddF32,
  kIRInstIdAddF64,
  kIRInstIdSubI32,
  kIRInstIdSubF32,
  kIRInstIdSubF64,
  kIRInstIdMulI32,
  kIRInstIdMulF32,
  kIRInstIdMulF64,
  kIRInstIdDivI32,
  kIRInstIdDivF32,
  kIRInstIdDivF64,
  kIRInstIdModI32,
  kIRInstIdModF32,
  kIRInstIdModF64,

  kIRInstIdAndI32,
  kIRInstIdAndF32,
  kIRInstIdAndF64,
  kIRInstIdOrI32,
  kIRInstIdOrF32,
  kIRInstIdOrF64,
  kIRInstIdXorI32,
  kIRInstIdXorF32,
  kIRInstIdXorF64,

  kIRInstIdSarI32,
  kIRInstIdShrI32,
  kIRInstIdShlI32,
  kIRInstIdRorI32,
  kIRInstIdRolI32,

  kIRInstIdMinI32,
  kIRInstIdMinF32,
  kIRInstIdMinF64,
  kIRInstIdMaxI32,
  kIRInstIdMaxF32,
  kIRInstIdMaxF64,

  kIRInstIdIsNanF32,
  kIRInstIdIsNanF64,
  kIRInstIdIsInfF32,
  kIRInstIdIsInfF64,
  kIRInstIdIsFiniteF32,
  kIRInstIdIsFiniteF64,

  kIRInstIdSignBitI32,
  kIRInstIdSignBitF32,
  kIRInstIdSignBitF64,

  kIRInstIdTruncF32,
  kIRInstIdTruncF64,
  kIRInstIdFloorF32,
  kIRInstIdFloorF64,
  kIRInstIdRoundF32,
  kIRInstIdRoundF64,
  kIRInstIdRoundEvenF32,
  kIRInstIdRoundEvenF64,
  kIRInstIdCeilF32,
  kIRInstIdCeilF64,

  kIRInstIdAbsI32,
  kIRInstIdAbsF32,
  kIRInstIdAbsF64,
  kIRInstIdExpF32,
  kIRInstIdExpF64,
  kIRInstIdLogF32,
  kIRInstIdLogF64,
  kIRInstIdLog2F32,
  kIRInstIdLog2F64,
  kIRInstIdLog10F32,
  kIRInstIdLog10F64,
  kIRInstIdSqrtF32,
  kIRInstIdSqrtF64,

  kIRInstIdSinF32,
  kIRInstIdSinF64,
  kIRInstIdCosF32,
  kIRInstIdCosF64,
  kIRInstIdTanF32,
  kIRInstIdTanF64,
  kIRInstIdAsinF32,
  kIRInstIdAsinF64,
  kIRInstIdAcosF32,
  kIRInstIdAcosF64,
  kIRInstIdAtanF32,
  kIRInstIdAtanF64,
  kIRInstIdPowF32,
  kIRInstIdPowF64,
  kIRInstIdAtan2F32,
  kIRInstIdAtan2F64,
  kIRInstIdCopySignF32,
  kIRInstIdCopySignF64,

  kIRInstIdVabsb,
  kIRInstIdVabsw,
  kIRInstIdVabsd,
  kIRInstIdVaddb,
  kIRInstIdVaddw,
  kIRInstIdVaddd,
  kIRInstIdVaddq,
  kIRInstIdVaddssb,
  kIRInstIdVaddusb,
  kIRInstIdVaddssw,
  kIRInstIdVaddusw,
  kIRInstIdVsubb,
  kIRInstIdVsubw,
  kIRInstIdVsubd,
  kIRInstIdVsubq,
  kIRInstIdVsubssb,
  kIRInstIdVsubusb,
  kIRInstIdVsubssw,
  kIRInstIdVsubusw,
  kIRInstIdVmulw,
  kIRInstIdVmulhsw,
  kIRInstIdVmulhuw,
  kIRInstIdVmuld,
  kIRInstIdVminsb,
  kIRInstIdVminub,
  kIRInstIdVminsw,
  kIRInstIdVminuw,
  kIRInstIdVminsd,
  kIRInstIdVminud,
  kIRInstIdVmaxsb,
  kIRInstIdVmaxub,
  kIRInstIdVmaxsw,
  kIRInstIdVmaxuw,
  kIRInstIdVmaxsd,
  kIRInstIdVmaxud,
  kIRInstIdVsllw,
  kIRInstIdVsrlw,
  kIRInstIdVsraw,
  kIRInstIdVslld,
  kIRInstIdVsrld,
  kIRInstIdVsrad,
  kIRInstIdVsllq,
  kIRInstIdVsrlq,
  kIRInstIdVcmpeqb,
  kIRInstIdVcmpeqw,
  kIRInstIdVcmpeqd,
  kIRInstIdVcmpgtb,
  kIRInstIdVcmpgtw,
  kIRInstIdVcmpgtd,

  kIRInstIdCount,

  // --------------------------------------------------------------------------
  // [Vector]
  // --------------------------------------------------------------------------

  // Part of the instruction. Specifies how wide the operation is, maps well to
  // current architectures (SSE/AVX).

  kIRInstVMask = 0x0000C000,
  //! The operation is scalar.
  kIRInstV0    = 0x00000000,
  //! The operation is 128-bit wide.
  kIRInstV128  = 0x00004000,
  //! The operation is 256-bit wide.
  kIRInstV256  = 0x00008000,

  // --------------------------------------------------------------------------
  // [Use]
  // --------------------------------------------------------------------------

  // TODO: Not implemented.

  // Use flags are not part of the IR instruction itself, however, they provide
  // some useful hints for code-generator and IR lowering phases. For example
  // when MPSL doesn't use the full width of the SIMD register it annotates it
  // to make certain optimizations possible and to prevent emitting code which
  // result won't be used (for example it's easier to multiply int2 vector than
  // int4 vector on pure SSE2 target - i.e. without using SSE4.1).

  kIRInstUseMask = 0x00FF0000,
  kIRInstUse32_0 = 0x00010000,
  kIRInstUse32_1 = 0x00020000,
  kIRInstUse32_2 = 0x00040000,
  kIRInstUse32_3 = 0x00080000,
  kIRInstUse32_4 = 0x00100000,
  kIRInstUse32_5 = 0x00200000,
  kIRInstUse32_6 = 0x00400000,
  kIRInstUse32_7 = 0x00800000
};

// ============================================================================
// [mpsl::IRInstFlags]
// ============================================================================

enum IRInstFlags {
  kIRInstInfoI32     = 0x0001, //! Works with I32 operand(s).
  kIRInstInfoF32     = 0x0002, //! Works with F32 operand(s).
  kIRInstInfoF64     = 0x0004, //! Works with D32 operand(s).
  kIRInstInfoSIMD    = 0x0008,
  kIRInstInfoFetch   = 0x0010,
  kIRInstInfoStore   = 0x0020,
  kIRInstInfoMov     = 0x0040,
  kIRInstInfoCvt     = 0x0080,
  kIRInstInfoJxx     = 0x0100,
  kIRInstInfoRet     = 0x0200,
  kIRInstInfoCall    = 0x0400,
  kIRInstInfoComplex = 0x8000
};

// ============================================================================
// [mpsl::IRInstInfo]
// ============================================================================

struct IRInstInfo {
  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE bool isI32() const noexcept { return flags & kIRInstInfoI32; }
  MPSL_INLINE bool isF32() const noexcept { return flags & kIRInstInfoF32; }
  MPSL_INLINE bool isF64() const noexcept { return flags & kIRInstInfoF64; }
  MPSL_INLINE bool isCvt() const noexcept { return flags & kIRInstInfoCvt; }

  MPSL_INLINE bool isFetch() const noexcept { return flags & kIRInstInfoFetch; }
  MPSL_INLINE bool isStore() const noexcept { return flags & kIRInstInfoStore; }

  MPSL_INLINE bool isJxx() const noexcept { return flags & kIRInstInfoJxx; }
  MPSL_INLINE bool isRet() const noexcept { return flags & kIRInstInfoRet; }
  MPSL_INLINE bool isCall() const noexcept { return flags & kIRInstInfoCall; }
  MPSL_INLINE bool isComplex() const noexcept { return flags & kIRInstInfoComplex; }

  MPSL_INLINE bool getNumOps() const noexcept { return numOps; }
  MPSL_INLINE const char* getName() const noexcept { return name; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint16_t flags;
  uint8_t numOps;
  char name[13];
};
extern const IRInstInfo mpInstInfo[kIRInstIdCount];

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
