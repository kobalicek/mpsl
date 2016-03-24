// [MPSL]
// Shader-Like Mathematical Expression JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MPSL_EXPORTS

// [Dependencies - MPSL]
#include "./mpcompiler_x86_p.h"
#include "./mpmath_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

static MPSL_INLINE bool mpIsGp(const asmjit::Operand& op) {
  return op.isVar() && static_cast<const asmjit::X86Var&>(op).isGp();
}

// ============================================================================
// [mpsl::JitCompiler - Construction / Destruction]
// ============================================================================

JitCompiler::JitCompiler(Allocator* allocator, X86Compiler* c)
  : _allocator(allocator),
    _c(c),
    _functionBody(nullptr),
    _constPool(&c->_constAllocator) {

  const asmjit::CpuInfo& cpu = c->getRuntime()->getCpuInfo();
  _enableSSE4_1 = cpu.hasFeature(asmjit::CpuInfo::kX86FeatureSSE4_1);
}

JitCompiler::~JitCompiler() {}

// ============================================================================
// [mpsl::JitCompiler - Const Pool]
// ============================================================================

void JitCompiler::prepareConstPool() {
  if (!_constLabel.isInitialized()) {
    _constLabel = _c->newLabel();
    _constPtr = _c->newIntPtr("constPool");
    _c->setPriority(_constPtr, 2);

    asmjit::HLNode* prev = _c->setCursor(_functionBody);
    _c->lea(_constPtr, asmjit::x86::ptr(_constLabel));
    if (prev != _functionBody) _c->setCursor(prev);
  }
}

asmjit::X86Mem JitCompiler::getConstantU64(uint64_t value) {
  prepareConstPool();

  size_t offset;
  if (_constPool.add(&value, sizeof(uint64_t), offset) != asmjit::kErrorOk)
    return asmjit::X86Mem();

  return asmjit::x86::ptr(_constPtr, static_cast<int>(offset));
}

asmjit::X86Mem JitCompiler::getConstantU64AsPD(uint64_t value) {
  prepareConstPool();

  size_t offset;
  asmjit::Vec128 vec = asmjit::Vec128::fromSQ(value, 0);
  if (_constPool.add(&vec, sizeof(asmjit::Vec128), offset) != asmjit::kErrorOk)
    return asmjit::X86Mem();

  return asmjit::x86::ptr(_constPtr, static_cast<int>(offset));
}

asmjit::X86Mem JitCompiler::getConstantD64(double value) {
  DoubleBits bits;
  bits.d = value;
  return getConstantU64(bits.u);
}

asmjit::X86Mem JitCompiler::getConstantD64AsPD(double value) {
  DoubleBits bits;
  bits.d = value;
  return getConstantU64AsPD(bits.u);
}

// ============================================================================
// [mpsl::JitCompiler - Compile]
// ============================================================================

Error JitCompiler::compileIRAsFunc(IRBuilder* ir) {
  uint32_t i;
  uint32_t numSlots = ir->getNumSlots();

  asmjit::FuncBuilderX proto;
  proto.setRetT<unsigned int>();

  for (i = 0; i < numSlots; i++) {
    _data[i] = _c->newIntPtr("ptr%u", i);
    ir->getDataPtr(i)->setJitId(_data[i].getId());
    proto.addArgT<void*>();
  }

  _c->addFunc(proto);
  _functionBody = _c->getCursor();

  for (i = 0; i < numSlots; i++) {
    _c->setArg(i, _data[i]);
  }

  compileIRAsPart(ir);

  X86GpVar errCode = _c->newInt32("err");
  _c->xor_(errCode, errCode);
  _c->ret(errCode);

  _c->endFunc();

  if (_constLabel.isInitialized())
    _c->embedConstPool(_constLabel, _constPool);

  return kErrorOk;
}

Error JitCompiler::compileIRAsPart(IRBuilder* ir) {
  // Compile the entry block and all other reacheable blocks.
  IRBlock* block = ir->getMainBlock();
  MPSL_PROPAGATE(compileConsecutiveBlocks(block));

  // Compile all remaining, reacheable, and non-assembled blocks.
  block = ir->_blockFirst;
  while (block != nullptr) {
    if (!block->isAssembled())
      compileConsecutiveBlocks(block);
    block = block->_nextBlock;
  }

  return kErrorOk;
}

Error JitCompiler::compileConsecutiveBlocks(IRBlock* block) {
  while (block != nullptr) {
    IRInst* inst = block->getLastChild();
    IRBlock* next = nullptr;

    if (inst != nullptr) {
      if (inst->getInstCode() == kIRInstJmp) {
        next = static_cast<IRBlock*>(inst->getOperand(0));
        if (next->isAssembled()) next = nullptr;
      }
      else if (inst->getInstCode() == kIRInstJcc) {
        next = static_cast<IRBlock*>(inst->getOperand(1));
        if (next->isAssembled()) {
          next = static_cast<IRBlock*>(inst->getOperand(2));
          if (next->isAssembled()) next = nullptr;
        }
      }
    }

    compileBasicBlock(block, next);
    block = next;
  }

  return kErrorOk;
}

Error JitCompiler::compileBasicBlock(IRBlock* block, IRBlock* next) {
  IRInst* inst = block->getFirstChild();
  asmjit::Operand asmOp[3];

  while (inst != nullptr) {
    IRObject** irOpArray = inst->getOpArray();
    uint32_t i, count = inst->getOpCount();

    const IRInstInfo& info = mpInstInfo[inst->getInstCode() & kIRInstMask];

    for (i = 0; i < count; i++) {
      IRObject* irOp = irOpArray[i];

      switch (irOp->getObjectType()) {
        case kIRObjectVar: {
          IRVar* var = static_cast<IRVar*>(irOp);
          if (var->getReg() == kIRRegGP)
            asmOp[i] = varAsI32(var);
          if (var->getReg() == kIRRegSIMD)
            asmOp[i] = varAsXmm(var);
          break;
        }

        case kIRObjectMem: {
          IRMem* mem = static_cast<IRMem*>(irOp);
          IRVar* base = mem->getBase();
          IRVar* index = mem->getIndex();

          // TODO: index.
          asmOp[i] = asmjit::x86::ptr(varAsPtr(base), mem->getOffset());
          break;
        }

        case kIRObjectImm: {
          // TODO:
          IRImm* imm = static_cast<IRImm*>(irOp);

          if (info.isI32() && !info.isCvt())
            ;
          else
            asmOp[i] = getConstantU64(imm->_value.q[0]);

          break;
        }

        case kIRObjectBlock: {
          // TODO:
          break;
        }
      }
    }

    switch (inst->getInstCode()) {
      case kIRInstFetch32:
      case kIRInstStore32:
        if ((mpIsGp(asmOp[0]) && (asmOp[1].isMem() || mpIsGp(asmOp[1]))) ||
            (mpIsGp(asmOp[1]) && (asmOp[0].isMem())))
          _c->emit(asmjit::kX86InstIdMov, asmOp[0], asmOp[1]);
        else
          _c->emit(asmjit::kX86InstIdMovd, asmOp[0], asmOp[1]);
        break;

      case kIRInstFetch64:
      case kIRInstStore64:
        _c->emit(asmjit::kX86InstIdMovq, asmOp[0], asmOp[1]);
        break;

      case kIRInstFetch128:
      case kIRInstStore128:
        _c->emit(asmjit::kX86InstIdMovups, asmOp[0], asmOp[1]);
        break;

      case kIRInstFetch256:
      case kIRInstStore256:
        break;

      case kIRInstMov32                     : emit2_Any(asmjit::kX86InstIdMovd     , asmOp[0], asmOp[1]); break;
      case kIRInstMov64                     : emit2_Any(asmjit::kX86InstIdMovq     , asmOp[0], asmOp[1]); break;
      case kIRInstMov128                    : emit2_Any(asmjit::kX86InstIdMovaps   , asmOp[0], asmOp[1]); break;
      case kIRInstMov256                    : emit2_Any(asmjit::kX86InstIdVmovaps  , asmOp[0], asmOp[1]); break;

      case kIRInstCvtI32ToF32               : emit2_Any(asmjit::kX86InstIdCvtsi2ss , asmOp[0], asmOp[1]); break;
      case kIRInstCvtI32ToF64               : emit2_Any(asmjit::kX86InstIdCvtsi2sd , asmOp[0], asmOp[1]); break;

      case kIRInstCvtF32ToI32               : emit2_Any(asmjit::kX86InstIdCvttss2si, asmOp[0], asmOp[1]); break;
      case kIRInstCvtF32ToF64               : emit2_Any(asmjit::kX86InstIdCvtss2sd , asmOp[0], asmOp[1]); break;

      case kIRInstCvtF64ToI32               : emit2_Any(asmjit::kX86InstIdCvttsd2si, asmOp[0], asmOp[1]); break;
      case kIRInstCvtF64ToF32               : emit2_Any(asmjit::kX86InstIdCvtsd2ss , asmOp[0], asmOp[1]); break;

      case kIRInstCmpEqF32                  : emit3_F32(asmjit::kX86InstIdCmpss    , asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpEQ); break;
      case kIRInstCmpEqF32   | kIRInstVec128: emit3_F32(asmjit::kX86InstIdCmpps    , asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpEQ); break;
      case kIRInstCmpEqF64                  : emit3_F64(asmjit::kX86InstIdCmpsd    , asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpEQ); break;
      case kIRInstCmpEqF64   | kIRInstVec128: emit3_F64(asmjit::kX86InstIdCmppd    , asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpEQ); break;

      case kIRInstCmpNeF32                  : emit3_F32(asmjit::kX86InstIdCmpss    , asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpNEQ); break;
      case kIRInstCmpNeF32   | kIRInstVec128: emit3_F32(asmjit::kX86InstIdCmpps    , asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpNEQ); break;
      case kIRInstCmpNeF64                  : emit3_F64(asmjit::kX86InstIdCmpsd    , asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpNEQ); break;
      case kIRInstCmpNeF64   | kIRInstVec128: emit3_F64(asmjit::kX86InstIdCmppd    , asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpNEQ); break;

      case kIRInstCmpLtF32                  : emit3_F32(asmjit::kX86InstIdCmpss    , asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLT); break;
      case kIRInstCmpLtF32   | kIRInstVec128: emit3_F32(asmjit::kX86InstIdCmpps    , asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLT); break;
      case kIRInstCmpLtF64                  : emit3_F64(asmjit::kX86InstIdCmpsd    , asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLT); break;
      case kIRInstCmpLtF64   | kIRInstVec128: emit3_F64(asmjit::kX86InstIdCmppd    , asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLT); break;

      case kIRInstCmpLeF32                  : emit3_F32(asmjit::kX86InstIdCmpss    , asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLE); break;
      case kIRInstCmpLeF32   | kIRInstVec128: emit3_F32(asmjit::kX86InstIdCmpps    , asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLE); break;
      case kIRInstCmpLeF64                  : emit3_F64(asmjit::kX86InstIdCmpsd    , asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLE); break;
      case kIRInstCmpLeF64   | kIRInstVec128: emit3_F64(asmjit::kX86InstIdCmppd    , asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLE); break;

      case kIRInstCmpGtF32                  : emit3_F32(asmjit::kX86InstIdCmpss    , asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLE); break;
      case kIRInstCmpGtF32   | kIRInstVec128: emit3_F32(asmjit::kX86InstIdCmpps    , asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLE); break;
      case kIRInstCmpGtF64                  : emit3_F64(asmjit::kX86InstIdCmpsd    , asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLE); break;
      case kIRInstCmpGtF64   | kIRInstVec128: emit3_F64(asmjit::kX86InstIdCmppd    , asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLE); break;

      case kIRInstCmpGeF32                  : emit3_F32(asmjit::kX86InstIdCmpss    , asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLT); break;
      case kIRInstCmpGeF32   | kIRInstVec128: emit3_F32(asmjit::kX86InstIdCmpps    , asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLT); break;
      case kIRInstCmpGeF64                  : emit3_F64(asmjit::kX86InstIdCmpsd    , asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLT); break;
      case kIRInstCmpGeF64   | kIRInstVec128: emit3_F64(asmjit::kX86InstIdCmppd    , asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLT); break;

      case kIRInstAddI32                    : emit3_I32(asmjit::kX86InstIdAdd      , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstAddF32                    : emit3_F32(asmjit::kX86InstIdAddss    , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstAddF32     | kIRInstVec128: emit3_F32(asmjit::kX86InstIdAddps    , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstAddF64                    : emit3_F64(asmjit::kX86InstIdAddsd    , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstAddF64     | kIRInstVec128: emit3_F64(asmjit::kX86InstIdAddpd    , asmOp[0], asmOp[1], asmOp[2]); break;

      case kIRInstSubI32                    : emit3_I32(asmjit::kX86InstIdSub      , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstSubF32                    : emit3_F32(asmjit::kX86InstIdSubss    , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstSubF32     | kIRInstVec128: emit3_F32(asmjit::kX86InstIdSubps    , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstSubF64                    : emit3_F64(asmjit::kX86InstIdSubsd    , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstSubF64     | kIRInstVec128: emit3_F64(asmjit::kX86InstIdSubpd    , asmOp[0], asmOp[1], asmOp[2]); break;

      case kIRInstMulI32                    : emit3_I32(asmjit::kX86InstIdImul     , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstMulF32                    : emit3_F32(asmjit::kX86InstIdMulss    , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstMulF32     | kIRInstVec128: emit3_F32(asmjit::kX86InstIdMulps    , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstMulF64                    : emit3_F64(asmjit::kX86InstIdMulsd    , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstMulF64     | kIRInstVec128: emit3_F64(asmjit::kX86InstIdMulpd    , asmOp[0], asmOp[1], asmOp[2]); break;

      case kIRInstDivF32                    : emit3_F32(asmjit::kX86InstIdDivss    , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstDivF32     | kIRInstVec128: emit3_F32(asmjit::kX86InstIdDivps    , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstDivF64                    : emit3_F64(asmjit::kX86InstIdDivsd    , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstDivF64     | kIRInstVec128: emit3_F64(asmjit::kX86InstIdDivpd    , asmOp[0], asmOp[1], asmOp[2]); break;

      case kIRInstAndI32                    : emit3_I32(asmjit::kX86InstIdAnd      , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstAndF32                    :
      case kIRInstAndF32     | kIRInstVec128: emit3_F32(asmjit::kX86InstIdAndps    , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstAndF64                    :
      case kIRInstAndF64     | kIRInstVec128: emit3_F64(asmjit::kX86InstIdAndpd    , asmOp[0], asmOp[1], asmOp[2]); break;

      case kIRInstOrI32                     : emit3_I32(asmjit::kX86InstIdOr       , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstOrF32                     :
      case kIRInstOrF32      | kIRInstVec128: emit3_F32(asmjit::kX86InstIdOrps     , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstOrF64                     :
      case kIRInstOrF64      | kIRInstVec128: emit3_F64(asmjit::kX86InstIdOrpd     , asmOp[0], asmOp[1], asmOp[2]); break;

      case kIRInstXorI32                    : emit3_I32(asmjit::kX86InstIdXor      , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstXorF32                    :
      case kIRInstXorF32     | kIRInstVec128: emit3_F32(asmjit::kX86InstIdXorps    , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstXorF64                    :
      case kIRInstXorF64     | kIRInstVec128: emit3_F64(asmjit::kX86InstIdXorpd    , asmOp[0], asmOp[1], asmOp[2]); break;

      case kIRInstMinF32                    : emit3_F32(asmjit::kX86InstIdMinss    , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstMinF32     | kIRInstVec128: emit3_F32(asmjit::kX86InstIdMinps    , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstMinF64                    : emit3_F64(asmjit::kX86InstIdMinsd    , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstMinF64     | kIRInstVec128: emit3_F64(asmjit::kX86InstIdMinpd    , asmOp[0], asmOp[1], asmOp[2]); break;

      case kIRInstMaxF32                    : emit3_F32(asmjit::kX86InstIdMaxss    , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstMaxF32     | kIRInstVec128: emit3_F32(asmjit::kX86InstIdMaxps    , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstMaxF64                    : emit3_F64(asmjit::kX86InstIdMaxsd    , asmOp[0], asmOp[1], asmOp[2]); break;
      case kIRInstMaxF64     | kIRInstVec128: emit3_F64(asmjit::kX86InstIdMaxpd    , asmOp[0], asmOp[1], asmOp[2]); break;

      case kIRInstSqrtF32                   : emit2_Any(asmjit::kX86InstIdSqrtss   , asmOp[0], asmOp[1]); break;
      case kIRInstSqrtF32    | kIRInstVec128: emit2_Any(asmjit::kX86InstIdSqrtps   , asmOp[0], asmOp[1]); break;
      case kIRInstSqrtF64                   : emit2_Any(asmjit::kX86InstIdSqrtsd   , asmOp[0], asmOp[1]); break;
      case kIRInstSqrtF64    | kIRInstVec128: emit2_Any(asmjit::kX86InstIdSqrtpd   , asmOp[0], asmOp[1]); break;

      default:
        // TODO:
        MPSL_ASSERT(!"Implemented");
    }

    inst = inst->getNext();
  }

  block->setAssembled();
  return kErrorOk;
}

void JitCompiler::emit3_I32(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2) {
  if (o0.getId() != o1.getId())
    _c->emit(asmjit::kX86InstIdMov, o0, o1);
  _c->emit(instId, o0, o2);
}

void JitCompiler::emit3_F32(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2) {
  if (o0.getId() != o1.getId())
    _c->emit(o1.isVar() ? asmjit::kX86InstIdMovaps : asmjit::kX86InstIdMovss, o0, o1);
  _c->emit(instId, o0, o2);
}

void JitCompiler::emit3_F32(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2, int imm) {
  if (o0.getId() != o1.getId())
    _c->emit(o1.isVar() ? asmjit::kX86InstIdMovaps : asmjit::kX86InstIdMovss, o0, o1);
  _c->emit(instId, o0, o2, imm);
}

void JitCompiler::emit3_F64(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2) {
  if (o0.getId() != o1.getId())
    _c->emit(o1.isVar() ? asmjit::kX86InstIdMovapd : asmjit::kX86InstIdMovsd, o0, o1);
  _c->emit(instId, o0, o2);
}

void JitCompiler::emit3_F64(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2, int imm) {
  if (o0.getId() != o1.getId())
    _c->emit(o1.isVar() ? asmjit::kX86InstIdMovapd : asmjit::kX86InstIdMovsd, o0, o1);
  _c->emit(instId, o0, o2, imm);
}

X86GpVar JitCompiler::varAsPtr(IRVar* irVar) {
  uint32_t id = irVar->getJitId();

  if (id == kInvalidValue) {
    X86GpVar gp = _c->newIntPtr("%%%u", irVar->getId());
    irVar->setJitId(gp.getId());
    return gp;
  }
  else {
    return _c->getIntPtrById(id);
  }
}

X86GpVar JitCompiler::varAsI32(IRVar* irVar) {
  uint32_t id = irVar->getJitId();

  if (id == kInvalidValue) {
    X86GpVar gp = _c->newInt32("%%%u", irVar->getId());
    irVar->setJitId(gp.getId());
    return gp;
  }
  else {
    return _c->getInt32ById(id);
  }
}

X86XmmVar JitCompiler::varAsXmm(IRVar* irVar) {
  uint32_t id = irVar->getJitId();

  if (id == kInvalidValue) {
    X86XmmVar xmm = _c->newXmm("%%%u", irVar->getId());
    irVar->setJitId(xmm.getId());
    return xmm;
  }
  else {
    return _c->getXmmById(id);
  }
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
