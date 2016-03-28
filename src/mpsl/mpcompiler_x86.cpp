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

  _tmpXmm0 = _c->newXmm("tmpXmm0");
  _tmpXmm1 = _c->newXmm("tmpXmm1");

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
      if (inst->getInstCode() == kIRInstIdJmp) {
        next = static_cast<IRBlock*>(inst->getOperand(0));
        if (next->isAssembled()) next = nullptr;
      }
      else if (inst->getInstCode() == kIRInstIdJcc) {
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

    const IRInstInfo& info = mpInstInfo[inst->getInstCode() & kIRInstIdMask];

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

#define OP_1(id) (kIRInstId##id | kIRInstV0)
#define OP_X(id) (kIRInstId##id | kIRInstV128)
#define OP_Y(id) (kIRInstId##id | kIRInstV256)
    switch (inst->getInstCode()) {
      case OP_1(Fetch32):
      case OP_1(Store32):
        if ((mpIsGp(asmOp[0]) && (asmOp[1].isMem() || mpIsGp(asmOp[1]))) ||
            (mpIsGp(asmOp[1]) && (asmOp[0].isMem())))
          _c->emit(asmjit::kX86InstIdMov, asmOp[0], asmOp[1]);
        else
          _c->emit(asmjit::kX86InstIdMovd, asmOp[0], asmOp[1]);
        break;

      case OP_1(Fetch64):
      case OP_1(Store64):
        _c->emit(asmjit::kX86InstIdMovq, asmOp[0], asmOp[1]);
        break;

      case OP_1(Fetch96): {
        X86Mem mem = static_cast<X86Mem&>(asmOp[1]);
        _c->emit(asmjit::kX86InstIdMovq, asmOp[0], mem);
        mem.adjust(8);
        _c->emit(asmjit::kX86InstIdMovd, _tmpXmm0, mem);
        _c->emit(asmjit::kX86InstIdPunpcklqdq, asmOp[0], _tmpXmm0);
        break;
      }

      case OP_1(Store96): {
        X86Mem mem = static_cast<X86Mem&>(asmOp[0]);
        _c->emit(asmjit::kX86InstIdMovq, mem, asmOp[1]);
        _c->emit(asmjit::kX86InstIdPshufd, _tmpXmm0, asmOp[1], X86Util::shuffle(1, 0, 3, 2));
        mem.adjust(8);
        _c->emit(asmjit::kX86InstIdMovd, mem, _tmpXmm0);
        break;
      }

      case OP_1(Fetch128):
      case OP_1(Store128):
        _c->emit(asmjit::kX86InstIdMovups, asmOp[0], asmOp[1]);
        break;

      case OP_1(Mov32): emit2_Any(asmjit::kX86InstIdMovd, asmOp[0], asmOp[1]); break;
      case OP_1(Mov64): emit2_Any(asmjit::kX86InstIdMovq, asmOp[0], asmOp[1]); break;
      case OP_1(Mov128): emit2_Any(asmjit::kX86InstIdMovaps, asmOp[0], asmOp[1]); break;

      case OP_1(CvtI32ToF32): emit2_Any(asmjit::kX86InstIdCvtsi2ss, asmOp[0], asmOp[1]); break;
      case OP_1(CvtI32ToF64): emit2_Any(asmjit::kX86InstIdCvtsi2sd, asmOp[0], asmOp[1]); break;

      case OP_1(CvtF32ToI32): emit2_Any(asmjit::kX86InstIdCvttss2si, asmOp[0], asmOp[1]); break;
      case OP_1(CvtF32ToF64): emit2_Any(asmjit::kX86InstIdCvtss2sd, asmOp[0], asmOp[1]); break;

      case OP_1(CvtF64ToI32): emit2_Any(asmjit::kX86InstIdCvttsd2si, asmOp[0], asmOp[1]); break;
      case OP_1(CvtF64ToF32): emit2_Any(asmjit::kX86InstIdCvtsd2ss, asmOp[0], asmOp[1]); break;

      case OP_1(CmpEqF32): emit3_F32(asmjit::kX86InstIdCmpss, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpEQ); break;
      case OP_X(CmpEqF32): emit3_F32(asmjit::kX86InstIdCmpps, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpEQ); break;
      case OP_1(CmpEqF64): emit3_F64(asmjit::kX86InstIdCmpsd, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpEQ); break;
      case OP_X(CmpEqF64): emit3_F64(asmjit::kX86InstIdCmppd, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpEQ); break;

      case OP_1(CmpNeF32): emit3_F32(asmjit::kX86InstIdCmpss, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpNEQ); break;
      case OP_X(CmpNeF32): emit3_F32(asmjit::kX86InstIdCmpps, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpNEQ); break;
      case OP_1(CmpNeF64): emit3_F64(asmjit::kX86InstIdCmpsd, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpNEQ); break;
      case OP_X(CmpNeF64): emit3_F64(asmjit::kX86InstIdCmppd, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpNEQ); break;

      case OP_1(CmpLtF32): emit3_F32(asmjit::kX86InstIdCmpss, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLT); break;
      case OP_X(CmpLtF32): emit3_F32(asmjit::kX86InstIdCmpps, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLT); break;
      case OP_1(CmpLtF64): emit3_F64(asmjit::kX86InstIdCmpsd, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLT); break;
      case OP_X(CmpLtF64): emit3_F64(asmjit::kX86InstIdCmppd, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLT); break;

      case OP_1(CmpLeF32): emit3_F32(asmjit::kX86InstIdCmpss, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLE); break;
      case OP_X(CmpLeF32): emit3_F32(asmjit::kX86InstIdCmpps, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLE); break;
      case OP_1(CmpLeF64): emit3_F64(asmjit::kX86InstIdCmpsd, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLE); break;
      case OP_X(CmpLeF64): emit3_F64(asmjit::kX86InstIdCmppd, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLE); break;

      case OP_1(CmpGtF32): emit3_F32(asmjit::kX86InstIdCmpss, asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLE); break;
      case OP_X(CmpGtF32): emit3_F32(asmjit::kX86InstIdCmpps, asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLE); break;
      case OP_1(CmpGtF64): emit3_F64(asmjit::kX86InstIdCmpsd, asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLE); break;
      case OP_X(CmpGtF64): emit3_F64(asmjit::kX86InstIdCmppd, asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLE); break;

      case OP_1(CmpGeF32): emit3_F32(asmjit::kX86InstIdCmpss, asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLT); break;
      case OP_X(CmpGeF32): emit3_F32(asmjit::kX86InstIdCmpps, asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLT); break;
      case OP_1(CmpGeF64): emit3_F64(asmjit::kX86InstIdCmpsd, asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLT); break;
      case OP_X(CmpGeF64): emit3_F64(asmjit::kX86InstIdCmppd, asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLT); break;

      case OP_1(AddI32): emit3_I32(asmjit::kX86InstIdAdd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(AddI32): emit3_I32(asmjit::kX86InstIdPaddd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(AddF32): emit3_F32(asmjit::kX86InstIdAddss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(AddF32): emit3_F32(asmjit::kX86InstIdAddps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(AddF64): emit3_F64(asmjit::kX86InstIdAddsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(AddF64): emit3_F64(asmjit::kX86InstIdAddpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(SubI32): emit3_I32(asmjit::kX86InstIdSub, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(SubI32): emit3_I32(asmjit::kX86InstIdPsubd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(SubF32): emit3_F32(asmjit::kX86InstIdSubss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(SubF32): emit3_F32(asmjit::kX86InstIdSubps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(SubF64): emit3_F64(asmjit::kX86InstIdSubsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(SubF64): emit3_F64(asmjit::kX86InstIdSubpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(MulI32): emit3_I32(asmjit::kX86InstIdImul, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(MulI32): emit3_I32(asmjit::kX86InstIdPmulld, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(MulF32): emit3_F32(asmjit::kX86InstIdMulss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(MulF32): emit3_F32(asmjit::kX86InstIdMulps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(MulF64): emit3_F64(asmjit::kX86InstIdMulsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(MulF64): emit3_F64(asmjit::kX86InstIdMulpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(DivF32): emit3_F32(asmjit::kX86InstIdDivss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(DivF32): emit3_F32(asmjit::kX86InstIdDivps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(DivF64): emit3_F64(asmjit::kX86InstIdDivsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(DivF64): emit3_F64(asmjit::kX86InstIdDivpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(AndI32): emit3_I32(asmjit::kX86InstIdAnd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(AndI32): emit3_I32(asmjit::kX86InstIdPand, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(AndF32):
      case OP_X(AndF32): emit3_F32(asmjit::kX86InstIdAndps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(AndF64):
      case OP_X(AndF64): emit3_F64(asmjit::kX86InstIdAndpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(OrI32): emit3_I32(asmjit::kX86InstIdOr, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(OrI32): emit3_I32(asmjit::kX86InstIdPor, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(OrF32):
      case OP_X(OrF32): emit3_F32(asmjit::kX86InstIdOrps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(OrF64):
      case OP_X(OrF64): emit3_F64(asmjit::kX86InstIdOrpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(XorI32): emit3_I32(asmjit::kX86InstIdXor, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(XorI32): emit3_I32(asmjit::kX86InstIdPxor, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(XorF32):
      case OP_X(XorF32): emit3_F32(asmjit::kX86InstIdXorps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(XorF64):
      case OP_X(XorF64): emit3_F64(asmjit::kX86InstIdXorpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(MinF32): emit3_F32(asmjit::kX86InstIdMinss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(MinF32): emit3_F32(asmjit::kX86InstIdMinps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(MinF64): emit3_F64(asmjit::kX86InstIdMinsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(MinF64): emit3_F64(asmjit::kX86InstIdMinpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(MaxF32): emit3_F32(asmjit::kX86InstIdMaxss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(MaxF32): emit3_F32(asmjit::kX86InstIdMaxps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(MaxF64): emit3_F64(asmjit::kX86InstIdMaxsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(MaxF64): emit3_F64(asmjit::kX86InstIdMaxpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(SqrtF32): emit2_Any(asmjit::kX86InstIdSqrtss, asmOp[0], asmOp[1]); break;
      case OP_X(SqrtF32): emit2_Any(asmjit::kX86InstIdSqrtps, asmOp[0], asmOp[1]); break;
      case OP_1(SqrtF64): emit2_Any(asmjit::kX86InstIdSqrtsd, asmOp[0], asmOp[1]); break;
      case OP_X(SqrtF64): emit2_Any(asmjit::kX86InstIdSqrtpd, asmOp[0], asmOp[1]); break;

      default:
        // TODO:
        MPSL_ASSERT(!"Implemented");
    }
#undef OP_Y
#undef OP_X
#undef OP_1

    inst = inst->getNext();
  }

  block->setAssembled();
  return kErrorOk;
}

void JitCompiler::emit3_I32(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2) {
  // Intercept instructions that are disabled for the current target and
  // substitute them with a sequential code that is compatible. It's easier
  // to deal with it here than dealing with it in `compileBasicBlock()`.
  switch (instId) {
    case asmjit::kX86InstIdPmulld: {
      if (_enableSSE4_1)
        break;

      if (o0.getId() != o1.getId())
        _c->emit(asmjit::kX86InstIdMovaps, o0, o1);

      _c->emit(asmjit::kX86InstIdPshufd, _tmpXmm0, o1, X86Util::shuffle(2, 3, 0, 1));
      _c->emit(asmjit::kX86InstIdPshufd, _tmpXmm1, o2, X86Util::shuffle(2, 3, 0, 1));

      _c->emit(asmjit::kX86InstIdPmuludq, _tmpXmm0, _tmpXmm1);
      _c->emit(asmjit::kX86InstIdPmuludq, o0, o2);
      _c->emit(asmjit::kX86InstIdShufps, o0, _tmpXmm0, X86Util::shuffle(0, 2, 0, 2));
      return;
    }
  }

  // Instruction `instId` is emitted as is.
  if (o0.getId() != o1.getId()) {
    if (mpIsGp(o0) && mpIsGp(o1))
      _c->emit(asmjit::kX86InstIdMov, o0, o1);
    else
      _c->emit(asmjit::kX86InstIdMovaps, o0, o1);
  }
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
  MPSL_ASSERT(id != kInvalidValue);

  return _c->getIntPtrById(id);
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
