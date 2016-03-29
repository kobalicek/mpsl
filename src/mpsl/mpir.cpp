// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MPSL_EXPORTS

// [Dependencies - MPSL]
#include "./mpir_p.h"
#include "./mpmath_p.h"
#include "./mputils_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [mpsl::IRInstInfo]
// ============================================================================

#define ROW(inst, numOps, flags, name) \
  { \
    flags, numOps, name \
  }
#define I(x) kIRInstInfo##x
const IRInstInfo mpInstInfo[kIRInstIdCount] = {
  // +--------------+--+-----------------------------------------+------------+
  // |Instruction   |#N| Flags                                   | Name       |
  // +--------------+--+-----------------------------------------+------------+
  ROW(None          , 0, 0                                       , "<none>"),
  ROW(Jmp           , 1, I(Jxx)                                  , "Jmp"),
  ROW(Jcc           , 2, I(Jxx)                                  , "Jcc"),
  ROW(Call          , 0, I(Call)                                 , "Call"),
  ROW(Ret           , 0, I(Ret)                                  , "Ret"),
  ROW(Fetch32       , 2, I(Fetch)                                , "Fetch32"),
  ROW(Fetch64       , 2, I(Fetch)                                , "Fetch64"),
  ROW(Fetch96       , 2, I(Fetch)                                , "Fetch96"),
  ROW(Fetch128      , 2, I(Fetch)                                , "Fetch128"),
  ROW(Fetch192      , 2, I(Fetch)                                , "Fetch192"),
  ROW(Fetch256      , 2, I(Fetch)                                , "Fetch256"),
  ROW(Store32       , 2, I(Store)                                , "Store32"),
  ROW(Store64       , 2, I(Store)                                , "Store64"),
  ROW(Store96       , 2, I(Store)                                , "Store96"),
  ROW(Store128      , 2, I(Store)                                , "Store128"),
  ROW(Store192      , 2, I(Store)                                , "Store192"),
  ROW(Store256      , 2, I(Store)                                , "Store256"),
  ROW(Mov32         , 2, I(Mov)                                  , "Mov32"),
  ROW(Mov64         , 2, I(Mov)                                  , "Mov64"),
  ROW(Mov128        , 2, I(Mov)                                  , "Mov128"),
  ROW(Mov256        , 2, I(Mov)                                  , "Mov256"),
  ROW(Insert32      , 3, I(Fetch)                                , "Insert32"),
  ROW(Insert64      , 3, I(Fetch)                                , "Insert64"),
  ROW(Extract32     , 3, I(Store)                                , "Extract32"),
  ROW(Extract64     , 3, I(Store)                                , "Extract64"),
  ROW(CvtI32ToF32   , 2, I(I32) | I(F32) | I(Cvt)                , "CvtI32ToF32"),
  ROW(CvtI32ToF64   , 2, I(I32) | I(F64) | I(Cvt)                , "CvtI32ToF64"),
  ROW(CvtF32ToI32   , 2, I(I32) | I(F32) | I(Cvt)                , "CvtF32ToI32"),
  ROW(CvtF32ToF64   , 2, I(F32) | I(F64) | I(Cvt)                , "CvtF32ToF64"),
  ROW(CvtF64ToI32   , 2, I(I32) | I(F64) | I(Cvt)                , "CvtF64ToI32"),
  ROW(CvtF64ToF32   , 2, I(F32) | I(F64) | I(Cvt)                , "CvtF64ToF32"),
  ROW(CmpEqI32      , 3, I(I32)                                  , "CmpEqI32"),
  ROW(CmpEqF32      , 3, I(F32)                                  , "CmpEqF32"),
  ROW(CmpEqF64      , 3, I(F64)                                  , "CmpEqF64"),
  ROW(CmpNeI32      , 3, I(I32)                                  , "CmpNeI32"),
  ROW(CmpNeF32      , 3, I(F32)                                  , "CmpNeF32"),
  ROW(CmpNeF64      , 3, I(F64)                                  , "CmpNeF64"),
  ROW(CmpLtI32      , 3, I(I32)                                  , "CmpLtI32"),
  ROW(CmpLtF32      , 3, I(F32)                                  , "CmpLtF32"),
  ROW(CmpLtF64      , 3, I(F64)                                  , "CmpLtF64"),
  ROW(CmpLeI32      , 3, I(I32)                                  , "CmpLeI32"),
  ROW(CmpLeF32      , 3, I(F32)                                  , "CmpLeF32"),
  ROW(CmpLeF64      , 3, I(F64)                                  , "CmpLeF64"),
  ROW(CmpGtI32      , 3, I(I32)                                  , "CmpGtI32"),
  ROW(CmpGtF32      , 3, I(F32)                                  , "CmpGtF32"),
  ROW(CmpGtF64      , 3, I(F64)                                  , "CmpGtF64"),
  ROW(CmpGeI32      , 3, I(I32)                                  , "CmpGeI32"),
  ROW(CmpGeF32      , 3, I(F32)                                  , "CmpGeF32"),
  ROW(CmpGeF64      , 3, I(F64)                                  , "CmpGeF64"),
  ROW(BitNegI32     , 2, I(I32)                                  , "BitNegI32"),
  ROW(BitNegF32     , 2, I(F32)                                  , "BitNegF32"),
  ROW(BitNegF64     , 2, I(F64)                                  , "BitNegF64"),
  ROW(NegI32        , 2, I(I32)                                  , "NegI32"),
  ROW(NegF32        , 2, I(F32)                                  , "NegF32"),
  ROW(NegF64        , 2, I(F64)                                  , "NegF64"),
  ROW(NotI32        , 2, I(I32)                                  , "NotI32"),
  ROW(NotF32        , 2, I(F32)                                  , "NotF32"),
  ROW(NotF64        , 2, I(F64)                                  , "NotF64"),
  ROW(Lzcnt         , 2, I(I32)                                  , "Lzcnt"),
  ROW(Popcnt        , 2, I(I32)                                  , "Popcnt"),
  ROW(AddI32        , 3, I(I32)                                  , "AddI32"),
  ROW(AddF32        , 3, I(F32)                                  , "AddF32"),
  ROW(AddF64        , 3, I(F64)                                  , "AddF64"),
  ROW(SubI32        , 3, I(I32)                                  , "SubI32"),
  ROW(SubF32        , 3, I(F32)                                  , "SubF32"),
  ROW(SubF64        , 3, I(F64)                                  , "SubF64"),
  ROW(MulI32        , 3, I(I32)                                  , "MulI32"),
  ROW(MulF32        , 3, I(F32)                                  , "MulF32"),
  ROW(MulF64        , 3, I(F64)                                  , "MulF64"),
  ROW(DivI32        , 3, I(I32)                                  , "DivI32"),
  ROW(DivF32        , 3, I(F32)                                  , "DivF32"),
  ROW(DivF64        , 3, I(F64)                                  , "DivF64"),
  ROW(ModI32        , 3, I(I32) | I(Complex)                     , "ModI32"),
  ROW(ModF32        , 3, I(F32) | I(Complex)                     , "ModF32"),
  ROW(ModF64        , 3, I(F64) | I(Complex)                     , "ModF64"),
  ROW(AndI32        , 3, I(I32)                                  , "AndI32"),
  ROW(AndF32        , 3, I(F32)                                  , "AndF32"),
  ROW(AndF64        , 3, I(F64)                                  , "AndF64"),
  ROW(OrI32         , 3, I(I32)                                  , "OrI32"),
  ROW(OrF32         , 3, I(F32)                                  , "OrF32"),
  ROW(OrF64         , 3, I(F64)                                  , "OrF64"),
  ROW(XorI32        , 3, I(I32)                                  , "XorI32"),
  ROW(XorF32        , 3, I(F32)                                  , "XorF32"),
  ROW(XorF64        , 3, I(F64)                                  , "XorF64"),
  ROW(SarI32        , 3, I(I32)                                  , "SarI32"),
  ROW(ShrI32        , 3, I(I32)                                  , "ShrI32"),
  ROW(ShlI32        , 3, I(I32)                                  , "ShlI32"),
  ROW(RorI32        , 3, I(I32)                                  , "RorI32"),
  ROW(RolI32        , 3, I(I32)                                  , "RolI32"),
  ROW(MinI32        , 3, I(I32)                                  , "MinI32"),
  ROW(MinF32        , 3, I(F32)                                  , "MinF32"),
  ROW(MinF64        , 3, I(F64)                                  , "MinF64"),
  ROW(MaxI32        , 3, I(I32)                                  , "MaxI32"),
  ROW(MaxF32        , 3, I(F32)                                  , "MaxF32"),
  ROW(MaxF64        , 3, I(F64)                                  , "MaxF64"),
  ROW(IsNanF32      , 2, I(F32)                                  , "IsNanF32"),
  ROW(IsNanF64      , 2, I(F64)                                  , "IsNanF64"),
  ROW(IsInfF32      , 2, I(F32)                                  , "IsInfF32"),
  ROW(IsInfF64      , 2, I(F64)                                  , "IsInfF64"),
  ROW(IsFiniteF32   , 2, I(F32)                                  , "IsFiniteF32"),
  ROW(IsFiniteF64   , 2, I(F64)                                  , "IsFiniteF64"),
  ROW(SignBitI32    , 2, I(I32)                                  , "SignBitI32"),
  ROW(SignBitF32    , 2, I(F32)                                  , "SignBitF32"),
  ROW(SignBitF64    , 2, I(F64)                                  , "SignBitF64"),
  ROW(TruncF32      , 2, I(F32)                                  , "TruncF32"),
  ROW(TruncF64      , 2, I(F64)                                  , "TruncF64"),
  ROW(FloorF32      , 2, I(F32)                                  , "FloorF32"),
  ROW(FloorF64      , 2, I(F64)                                  , "FloorF64"),
  ROW(RoundF32      , 2, I(F32)                                  , "RoundF32"),
  ROW(RoundF64      , 2, I(F64)                                  , "RoundF64"),
  ROW(RoundEvenF32  , 2, I(F32)                                  , "RoundEvenF32"),
  ROW(RoundEvenF64  , 2, I(F64)                                  , "RoundEvenF64"),
  ROW(CeilF32       , 2, I(F32)                                  , "CeilF32"),
  ROW(CeilF64       , 2, I(F64)                                  , "CeilF64"),
  ROW(AbsI32        , 2, I(I32)                                  , "AbsI32"),
  ROW(AbsF32        , 2, I(F32)                                  , "AbsF32"),
  ROW(AbsF64        , 2, I(F64)                                  , "AbsF64"),
  ROW(ExpF32        , 2, I(F32) | I(Complex)                     , "ExpF32"),
  ROW(ExpF64        , 2, I(F64) | I(Complex)                     , "ExpF64"),
  ROW(LogF32        , 2, I(F32) | I(Complex)                     , "LogF32"),
  ROW(LogF64        , 2, I(F64) | I(Complex)                     , "LogF64"),
  ROW(Log2F32       , 2, I(F32) | I(Complex)                     , "Log2F32"),
  ROW(Log2F64       , 2, I(F64) | I(Complex)                     , "Log2F64"),
  ROW(Log10F32      , 2, I(F32) | I(Complex)                     , "Log10F32"),
  ROW(Log10F64      , 2, I(F64) | I(Complex)                     , "Log10F64"),
  ROW(SqrtF32       , 2, I(F32)                                  , "SqrtF32"),
  ROW(SqrtF64       , 2, I(F64)                                  , "SqrtF64"),
  ROW(SinF32        , 2, I(F32) | I(Complex)                     , "SinF32"),
  ROW(SinF64        , 2, I(F64) | I(Complex)                     , "SinF64"),
  ROW(CosF32        , 2, I(F32) | I(Complex)                     , "CosF32"),
  ROW(CosF64        , 2, I(F64) | I(Complex)                     , "CosF64"),
  ROW(TanF32        , 2, I(F32) | I(Complex)                     , "TanF32"),
  ROW(TanF64        , 2, I(F64) | I(Complex)                     , "TanF64"),
  ROW(AsinF32       , 2, I(F32) | I(Complex)                     , "AsinF32"),
  ROW(AsinF64       , 2, I(F64) | I(Complex)                     , "AsinF64"),
  ROW(AcosF32       , 2, I(F32) | I(Complex)                     , "AcosF32"),
  ROW(AcosF64       , 2, I(F64) | I(Complex)                     , "AcosF64"),
  ROW(AtanF32       , 2, I(F32) | I(Complex)                     , "AtanF32"),
  ROW(AtanF64       , 2, I(F64) | I(Complex)                     , "AtanF64"),
  ROW(PowF32        , 3, I(F32) | I(Complex)                     , "PowF32"),
  ROW(PowF64        , 3, I(F64) | I(Complex)                     , "PowF64"),
  ROW(Atan2F32      , 3, I(F32) | I(Complex)                     , "Atan2F32"),
  ROW(Atan2F64      , 3, I(F64) | I(Complex)                     , "Atan2F64"),
  ROW(CopySignF32   , 3, I(F32)                                  , "CopySignF32"),
  ROW(CopySignF64   , 3, I(F64)                                  , "CopySignF64"),
  ROW(Vabsb         , 2, I(I32)                                  , "vabsb"),
  ROW(Vabsw         , 2, I(I32)                                  , "vabsw"),
  ROW(Vabsd         , 2, I(I32)                                  , "vabsd"),
  ROW(Vaddb         , 3, I(I32)                                  , "vaddb"),
  ROW(Vaddw         , 3, I(I32)                                  , "vaddw"),
  ROW(Vaddd         , 3, I(I32)                                  , "vaddd"),
  ROW(Vaddq         , 3, I(I32)                                  , "vaddq"),
  ROW(Vaddssb       , 3, I(I32)                                  , "vaddssb"),
  ROW(Vaddusb       , 3, I(I32)                                  , "vaddusb"),
  ROW(Vaddssw       , 3, I(I32)                                  , "vaddssw"),
  ROW(Vaddusw       , 3, I(I32)                                  , "vaddusw"),
  ROW(Vsubb         , 3, I(I32)                                  , "vsubb"),
  ROW(Vsubw         , 3, I(I32)                                  , "vsubw"),
  ROW(Vsubd         , 3, I(I32)                                  , "vsubd"),
  ROW(Vsubq         , 3, I(I32)                                  , "vsubq"),
  ROW(Vsubssb       , 3, I(I32)                                  , "vsubssb"),
  ROW(Vsubusb       , 3, I(I32)                                  , "vsubusb"),
  ROW(Vsubssw       , 3, I(I32)                                  , "vsubssw"),
  ROW(Vsubusw       , 3, I(I32)                                  , "vsubusw"),
  ROW(Vmulw         , 3, I(I32)                                  , "vmulw"),
  ROW(Vmulhsw       , 3, I(I32)                                  , "vmulhsw"),
  ROW(Vmulhuw       , 3, I(I32)                                  , "vmulhuw"),
  ROW(Vmuld         , 3, I(I32)                                  , "vmuld"),
  ROW(Vminsb        , 3, I(I32)                                  , "vminsb"),
  ROW(Vminub        , 3, I(I32)                                  , "vminub"),
  ROW(Vminsw        , 3, I(I32)                                  , "vminsw"),
  ROW(Vminuw        , 3, I(I32)                                  , "vminuw"),
  ROW(Vminsd        , 3, I(I32)                                  , "vminsd"),
  ROW(Vminud        , 3, I(I32)                                  , "vminud"),
  ROW(Vmaxsb        , 3, I(I32)                                  , "vmaxsb"),
  ROW(Vmaxub        , 3, I(I32)                                  , "vmaxub"),
  ROW(Vmaxsw        , 3, I(I32)                                  , "vmaxsw"),
  ROW(Vmaxuw        , 3, I(I32)                                  , "vmaxuw"),
  ROW(Vmaxsd        , 3, I(I32)                                  , "vmaxsd"),
  ROW(Vmaxud        , 3, I(I32)                                  , "vmaxud"),
  ROW(Vsllw         , 3, I(I32)                                  , "vsllw"),
  ROW(Vsrlw         , 3, I(I32)                                  , "vsrlw"),
  ROW(Vsraw         , 3, I(I32)                                  , "vsraw"),
  ROW(Vslld         , 3, I(I32)                                  , "vslld"),
  ROW(Vsrld         , 3, I(I32)                                  , "vsrld"),
  ROW(Vsrad         , 3, I(I32)                                  , "vsrad"),
  ROW(Vsllq         , 3, I(I32)                                  , "vsllq"),
  ROW(Vsrlq         , 3, I(I32)                                  , "vsrlq"),
  ROW(Vcmpeqb       , 3, I(I32)                                  , "vcmpeqb"),
  ROW(Vcmpeqw       , 3, I(I32)                                  , "vcmpeqw"),
  ROW(Vcmpeqd       , 3, I(I32)                                  , "vcmpeqd"),
  ROW(Vcmpgtb       , 3, I(I32)                                  , "vcmpgtb"),
  ROW(Vcmpgtw       , 3, I(I32)                                  , "vcmpgtw"),
  ROW(Vcmpgtd       , 3, I(I32)                                  , "vcmpgtd")
};
#undef I
#undef ROW

// ============================================================================
// [mpsl::IRBuilder - Construction / Destruction]
// ============================================================================

IRBuilder::IRBuilder(Allocator* allocator, uint32_t numSlots) noexcept
  : _allocator(allocator),
    _mainBlock(nullptr),
    _blockFirst(nullptr),
    _blockLast(nullptr),
    _numSlots(numSlots),
    _blockIdGen(0),
    _varIdGen(0) {

  for (uint32_t i = 0; i < Globals::kMaxArgumentsCount; i++) {
    if (i < numSlots)
      _dataSlots[i] = newVar(kIRRegGP, kPointerWidth);
    else
      _dataSlots[i] = nullptr;
  }
}
IRBuilder::~IRBuilder() noexcept {}

// ============================================================================
// [mpsl::IRBuilder - Factory]
// ============================================================================

MPSL_INLINE void mpExpandTypeInfo(uint32_t typeInfo, uint32_t& reg, uint32_t& width) noexcept {
  uint32_t typeId = typeInfo & kTypeIdMask;
  uint32_t vecLen = TypeInfo::elementsOf(typeInfo);

  // Scalar integers are allocated in GP registers.
  if (typeId == kTypeInt && vecLen <= 1) {
    reg = kIRRegGP;
    width = 4;
  }
  // Everything else is allocated in SIMD registers.
  else {
    reg = kIRRegSIMD;
    width = TypeInfo::sizeOf(typeId) * vecLen;
  }
}

IRVar* IRBuilder::newVar(uint32_t reg, uint32_t width) noexcept {
  IRVar* var = newObject<IRVar>(reg, width);
  if (var == nullptr) return nullptr;

  // Assign variable ID.
  var->_id = ++_varIdGen;

  return var;
}

IRVar* IRBuilder::newVarByTypeInfo(uint32_t typeInfo) noexcept {
  uint32_t reg;
  uint32_t width;
  mpExpandTypeInfo(typeInfo, reg, width);

  return newVar(reg, width);
}

IRMem* IRBuilder::newMem(IRVar* base, IRVar* index, int32_t offset) noexcept {
  IRMem* mem = newObject<IRMem>(base, index, offset);
  return mem;
}

IRImm* IRBuilder::newImm(const Value& value, uint32_t reg, uint32_t width) noexcept {
  return newObject<IRImm>(value, reg, width);
}

IRImm* IRBuilder::newImmByTypeInfo(const Value& value, uint32_t typeInfo) noexcept {
  uint32_t reg;
  uint32_t width;
  mpExpandTypeInfo(typeInfo, reg, width);

  IRImm* imm = newImm(value, reg, width);
  if (imm == nullptr) return nullptr;

  imm->setTypeInfo(typeInfo);
  return imm;
}

IRBlock* IRBuilder::newBlock() noexcept {
  IRBlock* block = newObject<IRBlock>();
  if (block == nullptr) return nullptr;

  // Assign block ID.
  block->_id = ++_blockIdGen;

  // Add to the `_blockList`.
  if (_blockLast != nullptr) {
    MPSL_ASSERT(_blockFirst != nullptr);
    block->_prevBlock = _blockLast;
    _blockLast->_nextBlock = block;
  }
  else {
    _blockFirst = _blockLast = block;
  }

  _blockLast = block;
  return block;
}

void IRBuilder::deleteObject(IRObject* obj) noexcept {
  size_t objectSize = 0;

  switch (obj->getObjectType()) {
    case kIRObjectVar: {
      objectSize = sizeof(IRVar);
      break;
    }

    case kIRObjectImm: {
      objectSize = sizeof(IRImm);
      break;
    }

    case kIRObjectBlock: {
      IRBlock* block = static_cast<IRBlock*>(obj);
      IRInst* inst = block->getFirstChild();

      while (inst != nullptr) {
        IRInst* next = inst->getNext();
        deleteObject(inst);
        inst = next;
      }

      // Delete from `_blockList`.
      IRBlock* prev = block->_prevBlock;
      IRBlock* next = block->_nextBlock;

      if (prev != nullptr) {
        MPSL_ASSERT(block != _blockFirst);
        prev->_nextBlock = next;
      }
      else {
        MPSL_ASSERT(block == _blockFirst);
        _blockFirst = next;
      }

      if (next != nullptr) {
        MPSL_ASSERT(block != _blockLast);
        next->_prevBlock = prev;
      }
      else {
        MPSL_ASSERT(block == _blockLast);
        _blockLast = prev;
      }

      objectSize = sizeof(IRBlock);
      break;
    }

    case kIRObjectInst: {
      IRInst* inst = static_cast<IRInst*>(obj);

      IRObject** opArray = inst->getOpArray();
      uint32_t count = inst->getOpCount();

      for (uint32_t i = 0; i < count; i++) {
        IRObject* op = opArray[i];
        op->_usageCount--;
      }

      objectSize = sizeof(IRInst);
      break;
    }

    default:
      MPSL_ASSERT(!"Reached");
  }

  _allocator->release(obj, objectSize);
}

// ============================================================================
// [mpsl::IRBuilder - Init]
// ============================================================================

Error IRBuilder::initMainBlock() noexcept {
  MPSL_ASSERT(_mainBlock == nullptr);

  _mainBlock = newBlock();
  MPSL_NULLCHECK(_mainBlock);

  _mainBlock->_blockData._blockType = kIRBlockEntry;
  _mainBlock->_usageCount = 1;

  return kErrorOk;
}

// ============================================================================
// [mpsl::IRBuilder - Emit]
// ============================================================================

Error IRBuilder::emitInst(IRBlock* block, uint32_t instCode, IRObject* o0) noexcept {
  IRInst* node = newInst(instCode, o0);
  MPSL_NULLCHECK(node);

  block->append(node);
  return kErrorOk;
}

Error IRBuilder::emitInst(IRBlock* block, uint32_t instCode, IRObject* o0, IRObject* o1) noexcept {
  IRInst* node = newInst(instCode, o0, o1);
  MPSL_NULLCHECK(node);

  block->append(node);
  return kErrorOk;
}

Error IRBuilder::emitInst(IRBlock* block, uint32_t instCode, IRObject* o0, IRObject* o1, IRObject* o2) noexcept {
  IRInst* node = newInst(instCode, o0, o1, o2);
  MPSL_NULLCHECK(node);

  block->append(node);
  return kErrorOk;
}

Error IRBuilder::emitMove(IRBlock* block, IRVar* dst, IRVar* src) noexcept {
  uint32_t inst = kIRInstIdNone;

  switch (mpMin<uint32_t>(dst->getWidth(), src->getWidth())) {
    case  4: inst = kIRInstIdMov32 ; break;
    case  8: inst = kIRInstIdMov64 ; break;
    case 16: inst = kIRInstIdMov128; break;
    case 32: inst = kIRInstIdMov256; break;

    default:
      return MPSL_TRACE_ERROR(kErrorInvalidState);
  }

  return emitInst(block, inst, dst, src);
}

Error IRBuilder::emitFetch(IRBlock* block, IRVar* dst, IRObject* src) noexcept {
  uint32_t inst = kIRInstIdNone;

  if (src->isImm() || src->isMem()) {
    switch (dst->getWidth()) {
      case  4: inst = kIRInstIdFetch32 ; break;
      case  8: inst = kIRInstIdFetch64 ; break;
      case 16: inst = kIRInstIdFetch128; break;
      case 32: inst = kIRInstIdFetch256; break;
    }
  }

  if (inst == kIRInstIdNone)
    return MPSL_TRACE_ERROR(kErrorInvalidState);

  return emitInst(block, inst, dst, src);
}

// ============================================================================
// [mpsl::IRBuilder - Dump]
// ============================================================================

MPSL_NOAPI Error IRBuilder::dump(StringBuilder& sb) noexcept {
  IRBlock* block = _blockFirst;

  while (block != nullptr) {
    sb.appendFormat(".B%u\n", block->getId());

    IRInst* inst = block->getFirstChild();
    while (inst != nullptr) {
      uint32_t code = inst->getInstCode() & kIRInstIdMask;
      uint32_t vec = inst->getInstCode() & kIRInstVMask;

      sb.appendFormat("  %s", mpInstInfo[code].name);

      if (vec == kIRInstV128) sb.appendString("@128");
      if (vec == kIRInstV256) sb.appendString("@256");

      IRObject** opArray = inst->getOpArray();
      uint32_t i, count = inst->getOpCount();

      for (i = 0; i < count; i++) {
        IRObject* op = opArray[i];

        if (i == 0)
          sb.appendString(" ");
        else
          sb.appendString(", ");

        switch (op->getObjectType()) {
          case kIRObjectVar: {
            IRVar* var = static_cast<IRVar*>(op);
            sb.appendFormat("%%%u", var->getId());
            break;
          }

          case kIRObjectMem: {
            IRMem* mem = static_cast<IRMem*>(op);
            sb.appendFormat("[%%%u + %d]",
              mem->getBase()->getId(),
              static_cast<int>(mem->getOffset()));
            break;
          }

          case kIRObjectImm: {
            IRImm* imm = static_cast<IRImm*>(op);
            Utils::formatValue(sb, imm->getTypeInfo(), &imm->_value);
            break;
          }

          case kIRObjectBlock: {
            IRBlock* block = static_cast<IRBlock*>(op);
            sb.appendFormat("B%u", block->getId());
            break;
          }
        }
      }

      sb.appendString("\n");
      inst = inst->getNext();
    }

    block = block->_nextBlock;
  }

  return kErrorOk;
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
