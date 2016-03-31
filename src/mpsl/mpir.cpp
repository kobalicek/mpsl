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

  // Assign a variable ID.
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
    case IRObject::kTypeVar: {
      objectSize = sizeof(IRVar);
      break;
    }

    case IRObject::kTypeImm: {
      objectSize = sizeof(IRImm);
      break;
    }

    case IRObject::kTypeBlock: {
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

    case IRObject::kTypeInst: {
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
  uint32_t inst = kInstCodeNone;

  switch (mpMin<uint32_t>(dst->getWidth(), src->getWidth())) {
    case  4: inst = kInstCodeMov32 ; break;
    case  8: inst = kInstCodeMov64 ; break;
    case 16: inst = kInstCodeMov128; break;
    case 32: inst = kInstCodeMov256; break;

    default:
      return MPSL_TRACE_ERROR(kErrorInvalidState);
  }

  return emitInst(block, inst, dst, src);
}

Error IRBuilder::emitFetch(IRBlock* block, IRVar* dst, IRObject* src) noexcept {
  uint32_t inst = kInstCodeNone;

  if (src->isImm() || src->isMem()) {
    switch (dst->getWidth()) {
      case  4: inst = kInstCodeFetch32 ; break;
      case  8: inst = kInstCodeFetch64 ; break;
      case 16: inst = kInstCodeFetch128; break;
      case 32: inst = kInstCodeFetch256; break;
    }
  }

  if (inst == kInstCodeNone)
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
      uint32_t code = inst->getInstCode() & kInstCodeMask;
      uint32_t vec = inst->getInstCode() & kInstVecMask;

      sb.appendFormat("  %s", mpInstInfo[code].name);

      if (vec == kInstVec128) sb.appendString("@128");
      if (vec == kInstVec256) sb.appendString("@256");

      IRObject** opArray = inst->getOpArray();
      uint32_t i, count = inst->getOpCount();

      for (i = 0; i < count; i++) {
        IRObject* op = opArray[i];

        if (i == 0)
          sb.appendString(" ");
        else
          sb.appendString(", ");

        switch (op->getObjectType()) {
          case IRObject::kTypeVar: {
            IRVar* var = static_cast<IRVar*>(op);
            sb.appendFormat("%%%u", var->getId());
            break;
          }

          case IRObject::kTypeMem: {
            IRMem* mem = static_cast<IRMem*>(op);
            sb.appendFormat("[%%%u + %d]",
              mem->getBase()->getId(),
              static_cast<int>(mem->getOffset()));
            break;
          }

          case IRObject::kTypeImm: {
            IRImm* imm = static_cast<IRImm*>(op);
            Utils::formatValue(sb, imm->getTypeInfo(), &imm->_value);
            break;
          }

          case IRObject::kTypeBlock: {
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
