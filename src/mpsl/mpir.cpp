// [MPSL]
// Shader-Like Mathematical Expression JIT Engine for C++.
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
#define F(flag) kIRInstFlag##flag
const IRInstInfo mpInstInfo[kIRInstCount] = {
  // +--------------+--+-----------------------------------------+------------+
  // |Instruction   |#N| Flags                                   | Name       |
  // +--------------+--+-----------------------------------------+------------+
  ROW(None          , 0, 0                                       , "<none>"),
  ROW(Jmp           , 1, F(Jxx)                                  , "Jmp"),
  ROW(Jcc           , 2, F(Jxx)                                  , "Jcc"),
  ROW(Call          , 0, F(Call)                                 , "Call"),
  ROW(Ret           , 0, F(Ret)                                  , "Ret"),
  ROW(Mov32         , 2, F(Mov)                                  , "Mov32"),
  ROW(Mov64         , 2, F(Mov)                                  , "Mov64"),
  ROW(Mov128        , 2, F(Mov)                                  , "Mov128"),
  ROW(Mov256        , 2, F(Mov)                                  , "Mov256"),
  ROW(MovI32        , 2, F(I32)                                  , "MovI32"),
  ROW(MovF32        , 2, F(F32)                                  , "MovF32"),
  ROW(MovF64        , 2, F(F64)                                  , "MovF64"),
  ROW(Fetch32       , 2, F(Fetch)                                , "Fetch32"),
  ROW(Fetch64       , 2, F(Fetch)                                , "Fetch64"),
  ROW(Fetch128      , 2, F(Fetch)                                , "Fetch128"),
  ROW(Fetch256      , 2, F(Fetch)                                , "Fetch256"),
  ROW(Store32       , 2, F(Store)                                , "Store32"),
  ROW(Store64       , 2, F(Store)                                , "Store64"),
  ROW(Store128      , 2, F(Store)                                , "Store128"),
  ROW(Store256      , 2, F(Store)                                , "Store256"),
  ROW(Insert32      , 3, F(Fetch)                                , "Insert32"),
  ROW(Insert64      , 3, F(Fetch)                                , "Insert64"),
  ROW(Extract32     , 3, F(Store)                                , "Extract32"),
  ROW(Extract64     , 3, F(Store)                                , "Extract64"),
  ROW(CvtI32ToF32   , 2, F(I32) | F(F32) | F(Cvt)                , "CvtI32ToF32"),
  ROW(CvtI32ToF64   , 2, F(I32) | F(F64) | F(Cvt)                , "CvtI32ToF64"),
  ROW(CvtF32ToI32   , 2, F(I32) | F(F32) | F(Cvt)                , "CvtF32ToI32"),
  ROW(CvtF32ToF64   , 2, F(F32) | F(F64) | F(Cvt)                , "CvtF32ToF64"),
  ROW(CvtF64ToI32   , 2, F(I32) | F(F64) | F(Cvt)                , "CvtF64ToI32"),
  ROW(CvtF64ToF32   , 2, F(F32) | F(F64) | F(Cvt)                , "CvtF64ToF32"),
  ROW(CmpEqI32      , 3, F(I32)                                  , "CmpEqI32"),
  ROW(CmpEqF32      , 3, F(F32)                                  , "CmpEqF32"),
  ROW(CmpEqF64      , 3, F(F64)                                  , "CmpEqF64"),
  ROW(CmpNeI32      , 3, F(I32)                                  , "CmpNeI32"),
  ROW(CmpNeF32      , 3, F(F32)                                  , "CmpNeF32"),
  ROW(CmpNeF64      , 3, F(F64)                                  , "CmpNeF64"),
  ROW(CmpLtI32      , 3, F(I32)                                  , "CmpLtI32"),
  ROW(CmpLtF32      , 3, F(F32)                                  , "CmpLtF32"),
  ROW(CmpLtF64      , 3, F(F64)                                  , "CmpLtF64"),
  ROW(CmpLeI32      , 3, F(I32)                                  , "CmpLeI32"),
  ROW(CmpLeF32      , 3, F(F32)                                  , "CmpLeF32"),
  ROW(CmpLeF64      , 3, F(F64)                                  , "CmpLeF64"),
  ROW(CmpGtI32      , 3, F(I32)                                  , "CmpGtI32"),
  ROW(CmpGtF32      , 3, F(F32)                                  , "CmpGtF32"),
  ROW(CmpGtF64      , 3, F(F64)                                  , "CmpGtF64"),
  ROW(CmpGeI32      , 3, F(I32)                                  , "CmpGeI32"),
  ROW(CmpGeF32      , 3, F(F32)                                  , "CmpGeF32"),
  ROW(CmpGeF64      , 3, F(F64)                                  , "CmpGeF64"),
  ROW(BitNegI32     , 2, F(I32)                                  , "BitNegI32"),
  ROW(BitNegF32     , 2, F(F32)                                  , "BitNegF32"),
  ROW(BitNegF64     , 2, F(F64)                                  , "BitNegF64"),
  ROW(NegI32        , 2, F(I32)                                  , "NegI32"),
  ROW(NegF32        , 2, F(F32)                                  , "NegF32"),
  ROW(NegF64        , 2, F(F64)                                  , "NegF64"),
  ROW(NotI32        , 2, F(I32)                                  , "NotI32"),
  ROW(NotF32        , 2, F(F32)                                  , "NotF32"),
  ROW(NotF64        , 2, F(F64)                                  , "NotF64"),
  ROW(AddI32        , 3, F(I32)                                  , "AddI32"),
  ROW(AddF32        , 3, F(F32)                                  , "AddF32"),
  ROW(AddF64        , 3, F(F64)                                  , "AddF64"),
  ROW(SubI32        , 3, F(I32)                                  , "SubI32"),
  ROW(SubF32        , 3, F(F32)                                  , "SubF32"),
  ROW(SubF64        , 3, F(F64)                                  , "SubF64"),
  ROW(MulI32        , 3, F(I32)                                  , "MulI32"),
  ROW(MulF32        , 3, F(F32)                                  , "MulF32"),
  ROW(MulF64        , 3, F(F64)                                  , "MulF64"),
  ROW(DivI32        , 3, F(I32)                                  , "DivI32"),
  ROW(DivF32        , 3, F(F32)                                  , "DivF32"),
  ROW(DivF64        , 3, F(F64)                                  , "DivF64"),
  ROW(ModI32        , 3, F(I32) | F(Complex)                     , "ModI32"),
  ROW(ModF32        , 3, F(F32) | F(Complex)                     , "ModF32"),
  ROW(ModF64        , 3, F(F64) | F(Complex)                     , "ModF64"),
  ROW(AndI32        , 3, F(I32)                                  , "AndI32"),
  ROW(AndF32        , 3, F(F32)                                  , "AndF32"),
  ROW(AndF64        , 3, F(F64)                                  , "AndF64"),
  ROW(OrI32         , 3, F(I32)                                  , "OrI32"),
  ROW(OrF32         , 3, F(F32)                                  , "OrF32"),
  ROW(OrF64         , 3, F(F64)                                  , "OrF64"),
  ROW(XorI32        , 3, F(I32)                                  , "XorI32"),
  ROW(XorF32        , 3, F(F32)                                  , "XorF32"),
  ROW(XorF64        , 3, F(F64)                                  , "XorF64"),
  ROW(SarI32        , 3, F(I32)                                  , "SarI32"),
  ROW(ShrI32        , 3, F(I32)                                  , "ShrI32"),
  ROW(ShlI32        , 3, F(I32)                                  , "ShlI32"),
  ROW(RorI32        , 3, F(I32)                                  , "RorI32"),
  ROW(RolI32        , 3, F(I32)                                  , "RolI32"),
  ROW(MinI32        , 3, F(I32)                                  , "MinI32"),
  ROW(MinF32        , 3, F(F32)                                  , "MinF32"),
  ROW(MinF64        , 3, F(F64)                                  , "MinF64"),
  ROW(MaxI32        , 3, F(I32)                                  , "MaxI32"),
  ROW(MaxF32        , 3, F(F32)                                  , "MaxF32"),
  ROW(MaxF64        , 3, F(F64)                                  , "MaxF64"),
  ROW(IsNanF32      , 2, F(F32)                                  , "IsNanF32"),
  ROW(IsNanF64      , 2, F(F64)                                  , "IsNanF64"),
  ROW(IsInfF32      , 2, F(F32)                                  , "IsInfF32"),
  ROW(IsInfF64      , 2, F(F64)                                  , "IsInfF64"),
  ROW(IsFiniteF32   , 2, F(F32)                                  , "IsFiniteF32"),
  ROW(IsFiniteF64   , 2, F(F64)                                  , "IsFiniteF64"),
  ROW(SignBitI32    , 2, F(I32)                                  , "SignBitI32"),
  ROW(SignBitF32    , 2, F(F32)                                  , "SignBitF32"),
  ROW(SignBitF64    , 2, F(F64)                                  , "SignBitF64"),
  ROW(TruncF32      , 2, F(F32)                                  , "TruncF32"),
  ROW(TruncF64      , 2, F(F64)                                  , "TruncF64"),
  ROW(FloorF32      , 2, F(F32)                                  , "FloorF32"),
  ROW(FloorF64      , 2, F(F64)                                  , "FloorF64"),
  ROW(RoundF32      , 2, F(F32)                                  , "RoundF32"),
  ROW(RoundF64      , 2, F(F64)                                  , "RoundF64"),
  ROW(RoundEvenF32  , 2, F(F32)                                  , "RoundEvenF32"),
  ROW(RoundEvenF64  , 2, F(F64)                                  , "RoundEvenF64"),
  ROW(CeilF32       , 2, F(F32)                                  , "CeilF32"),
  ROW(CeilF64       , 2, F(F64)                                  , "CeilF64"),
  ROW(AbsI32        , 2, F(I32)                                  , "AbsI32"),
  ROW(AbsF32        , 2, F(F32)                                  , "AbsF32"),
  ROW(AbsF64        , 2, F(F64)                                  , "AbsF64"),
  ROW(ExpF32        , 2, F(F32) | F(Complex)                     , "ExpF32"),
  ROW(ExpF64        , 2, F(F64) | F(Complex)                     , "ExpF64"),
  ROW(LogF32        , 2, F(F32) | F(Complex)                     , "LogF32"),
  ROW(LogF64        , 2, F(F64) | F(Complex)                     , "LogF64"),
  ROW(Log2F32       , 2, F(F32) | F(Complex)                     , "Log2F32"),
  ROW(Log2F64       , 2, F(F64) | F(Complex)                     , "Log2F64"),
  ROW(Log10F32      , 2, F(F32) | F(Complex)                     , "Log10F32"),
  ROW(Log10F64      , 2, F(F64) | F(Complex)                     , "Log10F64"),
  ROW(SqrtF32       , 2, F(F32)                                  , "SqrtF32"),
  ROW(SqrtF64       , 2, F(F64)                                  , "SqrtF64"),
  ROW(SinF32        , 2, F(F32) | F(Complex)                     , "SinF32"),
  ROW(SinF64        , 2, F(F64) | F(Complex)                     , "SinF64"),
  ROW(CosF32        , 2, F(F32) | F(Complex)                     , "CosF32"),
  ROW(CosF64        , 2, F(F64) | F(Complex)                     , "CosF64"),
  ROW(TanF32        , 2, F(F32) | F(Complex)                     , "TanF32"),
  ROW(TanF64        , 2, F(F64) | F(Complex)                     , "TanF64"),
  ROW(AsinF32       , 2, F(F32) | F(Complex)                     , "AsinF32"),
  ROW(AsinF64       , 2, F(F64) | F(Complex)                     , "AsinF64"),
  ROW(AcosF32       , 2, F(F32) | F(Complex)                     , "AcosF32"),
  ROW(AcosF64       , 2, F(F64) | F(Complex)                     , "AcosF64"),
  ROW(AtanF32       , 2, F(F32) | F(Complex)                     , "AtanF32"),
  ROW(AtanF64       , 2, F(F64) | F(Complex)                     , "AtanF64"),
  ROW(PowF32        , 3, F(F32) | F(Complex)                     , "PowF32"),
  ROW(PowF64        , 3, F(F64) | F(Complex)                     , "PowF64"),
  ROW(Atan2F32      , 3, F(F32) | F(Complex)                     , "Atan2F32"),
  ROW(Atan2F64      , 3, F(F64) | F(Complex)                     , "Atan2F64"),
  ROW(CopySignF32   , 3, F(F32)                                  , "CopySignF32"),
  ROW(CopySignF64   , 3, F(F64)                                  , "CopySignF64")
};
#undef F
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
  uint32_t vecLen = TypeInfo::getVectorSize(typeInfo);

  // Scalar integers are allocated in GP registers.
  if (typeId == kTypeInt && vecLen <= 1) {
    reg = kIRRegGP;
    width = 4;
  }
  // Everything else is allocated in SIMD registers.
  else {
    reg = kIRRegSIMD;
    width = TypeInfo::getSize(typeId) * vecLen;
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
  uint32_t inst = kIRInstNone;

  switch (mpMin<uint32_t>(dst->getWidth(), src->getWidth())) {
    case  4: inst = kIRInstMov32 ; break;
    case  8: inst = kIRInstMov64 ; break;
    case 16: inst = kIRInstMov128; break;
    case 32: inst = kIRInstMov256; break;

    default:
      return MPSL_TRACE_ERROR(kErrorInvalidState);
  }

  return emitInst(block, inst, dst, src);
}

Error IRBuilder::emitFetch(IRBlock* block, IRVar* dst, IRObject* src) noexcept {
  uint32_t inst = kIRInstNone;

  if (src->isImm() || src->isMem()) {
    switch (dst->getWidth()) {
      case  4: inst = kIRInstFetch32 ; break;
      case  8: inst = kIRInstFetch64 ; break;
      case 16: inst = kIRInstFetch128; break;
      case 32: inst = kIRInstFetch256; break;
    }
  }

  if (inst == kIRInstNone)
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
      uint32_t code = inst->getInstCode() & kIRInstMask;
      uint32_t vec = inst->getInstCode() & kIRInstVecMask;

      sb.appendFormat("  %s", mpInstInfo[code].name);

      if (vec == kIRInstVec128) sb.appendString("_128");
      if (vec == kIRInstVec256) sb.appendString("_256");

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
            sb.appendFormat("[%%%u + %d]", mem->getBase()->getId(), mem->getOffset());
            break;
          }

          case kIRObjectImm: {
            IRImm* imm = static_cast<IRImm*>(op);
            Utils::formatValue(sb, imm->getTypeInfo(), &imm->_value);
            break;
          }

          case kIRObjectBlock: {
            IRBlock* block = static_cast<IRBlock*>(op);
            sb.appendFormat(".B%u", block->getId());
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
