// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
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

asmjit::X86Mem JitCompiler::getConstantByValue(const Value& value, uint32_t width) {
  prepareConstPool();

  size_t offset;
  if (_constPool.add(&value, width, offset) != asmjit::kErrorOk)
    return asmjit::X86Mem();

  return asmjit::x86::ptr(_constPtr, static_cast<int>(offset));
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
      if (inst->getInstCode() == kInstCodeJmp) {
        next = static_cast<IRBlock*>(inst->getOperand(0));
        if (next->isAssembled()) next = nullptr;
      }
      else if (inst->getInstCode() == kInstCodeJcc) {
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

    const InstInfo& info = mpInstInfo[inst->getInstCode() & kInstCodeMask];

    for (i = 0; i < count; i++) {
      IRObject* irOp = irOpArray[i];

      switch (irOp->getObjectType()) {
        case IRObject::kTypeVar: {
          IRVar* var = static_cast<IRVar*>(irOp);
          if (var->getReg() == kIRRegGP)
            asmOp[i] = varAsI32(var);
          if (var->getReg() == kIRRegSIMD)
            asmOp[i] = varAsXmm(var);
          break;
        }

        case IRObject::kTypeMem: {
          IRMem* mem = static_cast<IRMem*>(irOp);
          IRVar* base = mem->getBase();
          IRVar* index = mem->getIndex();

          // TODO: index.
          asmOp[i] = asmjit::x86::ptr(varAsPtr(base), mem->getOffset());
          break;
        }

        case IRObject::kTypeImm: {
          // TODO:
          IRImm* imm = static_cast<IRImm*>(irOp);

          if ((info.hasImm() && i == count - 1) || (info.isI32() && !info.isCvt()))
            asmOp[i] = asmjit::imm(imm->getValue().i[0]);
          else
            asmOp[i] = getConstantByValue(imm->getValue(), imm->getWidth());

          break;
        }

        case IRObject::kTypeBlock: {
          // TODO:
          break;
        }
      }
    }

#define OP_1(id) (kInstCode##id | kInstVec0)
#define OP_X(id) (kInstCode##id | kInstVec128)
#define OP_Y(id) (kInstCode##id | kInstVec256)
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

      case OP_1(Mov32): emit2x(asmjit::kX86InstIdMovd, asmOp[0], asmOp[1]); break;
      case OP_1(Mov64): emit2x(asmjit::kX86InstIdMovq, asmOp[0], asmOp[1]); break;
      case OP_1(Mov128): emit2x(asmjit::kX86InstIdMovaps, asmOp[0], asmOp[1]); break;

      case OP_1(Cvtitof): emit2x(asmjit::kX86InstIdCvtsi2ss, asmOp[0], asmOp[1]); break;
      case OP_1(Cvtitod): emit2x(asmjit::kX86InstIdCvtsi2sd, asmOp[0], asmOp[1]); break;

      case OP_1(Cvtftoi): emit2x(asmjit::kX86InstIdCvttss2si, asmOp[0], asmOp[1]); break;
      case OP_1(Cvtftod): emit2x(asmjit::kX86InstIdCvtss2sd, asmOp[0], asmOp[1]); break;

      case OP_1(Cvtdtoi): emit2x(asmjit::kX86InstIdCvttsd2si, asmOp[0], asmOp[1]); break;
      case OP_1(Cvtdtof): emit2x(asmjit::kX86InstIdCvtsd2ss, asmOp[0], asmOp[1]); break;

      case OP_1(Addi): emit3i(asmjit::kX86InstIdAdd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Addi): emit3i(asmjit::kX86InstIdPaddd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Addf): emit3f(asmjit::kX86InstIdAddss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Addf): emit3f(asmjit::kX86InstIdAddps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Addd): emit3d(asmjit::kX86InstIdAddsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Addd): emit3d(asmjit::kX86InstIdAddpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Subi): emit3i(asmjit::kX86InstIdSub, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Subi): emit3i(asmjit::kX86InstIdPsubd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Subf): emit3f(asmjit::kX86InstIdSubss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Subf): emit3f(asmjit::kX86InstIdSubps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Subd): emit3d(asmjit::kX86InstIdSubsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Subd): emit3d(asmjit::kX86InstIdSubpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Muli): emit3i(asmjit::kX86InstIdImul, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Muli): emit3i(asmjit::kX86InstIdPmulld, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Mulf): emit3f(asmjit::kX86InstIdMulss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Mulf): emit3f(asmjit::kX86InstIdMulps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Muld): emit3d(asmjit::kX86InstIdMulsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Muld): emit3d(asmjit::kX86InstIdMulpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Divf): emit3f(asmjit::kX86InstIdDivss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Divf): emit3f(asmjit::kX86InstIdDivps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Divd): emit3d(asmjit::kX86InstIdDivsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Divd): emit3d(asmjit::kX86InstIdDivpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Andi): emit3i(asmjit::kX86InstIdAnd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Andi): emit3i(asmjit::kX86InstIdPand, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Andf):
      case OP_X(Andf): emit3f(asmjit::kX86InstIdAndps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Andd):
      case OP_X(Andd): emit3d(asmjit::kX86InstIdAndpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Ori): emit3i(asmjit::kX86InstIdOr, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Ori): emit3i(asmjit::kX86InstIdPor, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Orf):
      case OP_X(Orf): emit3f(asmjit::kX86InstIdOrps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Ord):
      case OP_X(Ord): emit3d(asmjit::kX86InstIdOrpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Xori): emit3i(asmjit::kX86InstIdXor, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Xori): emit3i(asmjit::kX86InstIdPxor, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Xorf):
      case OP_X(Xorf): emit3f(asmjit::kX86InstIdXorps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Xord):
      case OP_X(Xord): emit3d(asmjit::kX86InstIdXorpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Minf): emit3f(asmjit::kX86InstIdMinss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Minf): emit3f(asmjit::kX86InstIdMinps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Mind): emit3d(asmjit::kX86InstIdMinsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Mind): emit3d(asmjit::kX86InstIdMinpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Maxf): emit3f(asmjit::kX86InstIdMaxss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Maxf): emit3f(asmjit::kX86InstIdMaxps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Maxd): emit3d(asmjit::kX86InstIdMaxsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Maxd): emit3d(asmjit::kX86InstIdMaxpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Sqrtf): emit2x(asmjit::kX86InstIdSqrtss, asmOp[0], asmOp[1]); break;
      case OP_X(Sqrtf): emit2x(asmjit::kX86InstIdSqrtps, asmOp[0], asmOp[1]); break;
      case OP_1(Sqrtd): emit2x(asmjit::kX86InstIdSqrtsd, asmOp[0], asmOp[1]); break;
      case OP_X(Sqrtd): emit2x(asmjit::kX86InstIdSqrtpd, asmOp[0], asmOp[1]); break;

      case OP_1(Cmpeqf): emit3f(asmjit::kX86InstIdCmpss, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpEQ); break;
      case OP_X(Cmpeqf): emit3f(asmjit::kX86InstIdCmpps, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpEQ); break;
      case OP_1(Cmpeqd): emit3d(asmjit::kX86InstIdCmpsd, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpEQ); break;
      case OP_X(Cmpeqd): emit3d(asmjit::kX86InstIdCmppd, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpEQ); break;

      case OP_1(Cmpnef): emit3f(asmjit::kX86InstIdCmpss, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpNEQ); break;
      case OP_X(Cmpnef): emit3f(asmjit::kX86InstIdCmpps, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpNEQ); break;
      case OP_1(Cmpned): emit3d(asmjit::kX86InstIdCmpsd, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpNEQ); break;
      case OP_X(Cmpned): emit3d(asmjit::kX86InstIdCmppd, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpNEQ); break;

      case OP_1(Cmpltf): emit3f(asmjit::kX86InstIdCmpss, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLT); break;
      case OP_X(Cmpltf): emit3f(asmjit::kX86InstIdCmpps, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLT); break;
      case OP_1(Cmpltd): emit3d(asmjit::kX86InstIdCmpsd, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLT); break;
      case OP_X(Cmpltd): emit3d(asmjit::kX86InstIdCmppd, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLT); break;

      case OP_1(Cmplef): emit3f(asmjit::kX86InstIdCmpss, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLE); break;
      case OP_X(Cmplef): emit3f(asmjit::kX86InstIdCmpps, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLE); break;
      case OP_1(Cmpled): emit3d(asmjit::kX86InstIdCmpsd, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLE); break;
      case OP_X(Cmpled): emit3d(asmjit::kX86InstIdCmppd, asmOp[0], asmOp[1], asmOp[2], asmjit::kX86CmpLE); break;

      case OP_1(Cmpgtf): emit3f(asmjit::kX86InstIdCmpss, asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLE); break;
      case OP_X(Cmpgtf): emit3f(asmjit::kX86InstIdCmpps, asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLE); break;
      case OP_1(Cmpgtd): emit3d(asmjit::kX86InstIdCmpsd, asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLE); break;
      case OP_X(Cmpgtd): emit3d(asmjit::kX86InstIdCmppd, asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLE); break;

      case OP_1(Cmpgef): emit3f(asmjit::kX86InstIdCmpss, asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLT); break;
      case OP_X(Cmpgef): emit3f(asmjit::kX86InstIdCmpps, asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLT); break;
      case OP_1(Cmpged): emit3d(asmjit::kX86InstIdCmpsd, asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLT); break;
      case OP_X(Cmpged): emit3d(asmjit::kX86InstIdCmppd, asmOp[0], asmOp[2], asmOp[1], asmjit::kX86CmpLT); break;

      case OP_1(Shuf):
      case OP_X(Shuf): emit3d(asmjit::kX86InstIdPshufd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Vaddb):
      case OP_X(Vaddb): emit3i(asmjit::kX86InstIdPaddb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vaddw):
      case OP_X(Vaddw): emit3i(asmjit::kX86InstIdPaddd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vaddd):
      case OP_X(Vaddd): emit3i(asmjit::kX86InstIdPaddq, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vaddq):
      case OP_X(Vaddq): emit3i(asmjit::kX86InstIdPaddq, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vaddssb):
      case OP_X(Vaddssb): emit3i(asmjit::kX86InstIdPaddsb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vaddusb):
      case OP_X(Vaddusb): emit3i(asmjit::kX86InstIdPaddusb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vaddssw):
      case OP_X(Vaddssw): emit3i(asmjit::kX86InstIdPaddsw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vaddusw):
      case OP_X(Vaddusw): emit3i(asmjit::kX86InstIdPaddusw, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Vsubb):
      case OP_X(Vsubb): emit3i(asmjit::kX86InstIdPsubb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vsubw):
      case OP_X(Vsubw): emit3i(asmjit::kX86InstIdPsubd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vsubd):
      case OP_X(Vsubd): emit3i(asmjit::kX86InstIdPsubq, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vsubq):
      case OP_X(Vsubq): emit3i(asmjit::kX86InstIdPsubq, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vsubssb):
      case OP_X(Vsubssb): emit3i(asmjit::kX86InstIdPsubsb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vsubusb):
      case OP_X(Vsubusb): emit3i(asmjit::kX86InstIdPsubusb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vsubssw):
      case OP_X(Vsubssw): emit3i(asmjit::kX86InstIdPsubsw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vsubusw):
      case OP_X(Vsubusw): emit3i(asmjit::kX86InstIdPsubusw, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Vmulw):
      case OP_X(Vmulw): emit3i(asmjit::kX86InstIdPmullw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vmulhsw):
      case OP_X(Vmulhsw): emit3i(asmjit::kX86InstIdPmulhw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vmulhuw):
      case OP_X(Vmulhuw): emit3i(asmjit::kX86InstIdPmulhuw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vmuld):
      case OP_X(Vmuld): emit3i(asmjit::kX86InstIdPmulld, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Vminsb):
      case OP_X(Vminsb): emit3i(asmjit::kX86InstIdPminsb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vminub):
      case OP_X(Vminub): emit3i(asmjit::kX86InstIdPminub, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vminsw):
      case OP_X(Vminsw): emit3i(asmjit::kX86InstIdPminsw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vminuw):
      case OP_X(Vminuw): emit3i(asmjit::kX86InstIdPminuw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vminsd):
      case OP_X(Vminsd): emit3i(asmjit::kX86InstIdPminsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vminud):
      case OP_X(Vminud): emit3i(asmjit::kX86InstIdPminud, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Vmaxsb):
      case OP_X(Vmaxsb): emit3i(asmjit::kX86InstIdPmaxsb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vmaxub):
      case OP_X(Vmaxub): emit3i(asmjit::kX86InstIdPmaxub, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vmaxsw):
      case OP_X(Vmaxsw): emit3i(asmjit::kX86InstIdPmaxsw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vmaxuw):
      case OP_X(Vmaxuw): emit3i(asmjit::kX86InstIdPmaxuw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vmaxsd):
      case OP_X(Vmaxsd): emit3i(asmjit::kX86InstIdPmaxsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vmaxud):
      case OP_X(Vmaxud): emit3i(asmjit::kX86InstIdPmaxud, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Vsllw):
      case OP_X(Vsllw): emit3i(asmjit::kX86InstIdPsllw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vsrlw):
      case OP_X(Vsrlw): emit3i(asmjit::kX86InstIdPsrlw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vsraw):
      case OP_X(Vsraw): emit3i(asmjit::kX86InstIdPsraw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vslld):
      case OP_X(Vslld): emit3i(asmjit::kX86InstIdPslld, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vsrld):
      case OP_X(Vsrld): emit3i(asmjit::kX86InstIdPsrld, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vsrad):
      case OP_X(Vsrad): emit3i(asmjit::kX86InstIdPsrad, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vsllq):
      case OP_X(Vsllq): emit3i(asmjit::kX86InstIdPsllq, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vsrlq):
      case OP_X(Vsrlq): emit3i(asmjit::kX86InstIdPsrlq, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Vmaddwd):
      case OP_X(Vmaddwd): emit3i(asmjit::kX86InstIdPmaddwd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Vcmpeqb):
      case OP_X(Vcmpeqb): emit3i(asmjit::kX86InstIdPcmpeqb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vcmpeqw):
      case OP_X(Vcmpeqw): emit3i(asmjit::kX86InstIdPcmpeqw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vcmpeqd):
      case OP_X(Vcmpeqd): emit3i(asmjit::kX86InstIdPcmpeqd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Vcmpgtb):
      case OP_X(Vcmpgtb): emit3i(asmjit::kX86InstIdPcmpgtb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vcmpgtw):
      case OP_X(Vcmpgtw): emit3i(asmjit::kX86InstIdPcmpgtw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vcmpgtd):
      case OP_X(Vcmpgtd): emit3i(asmjit::kX86InstIdPcmpgtd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Vmovsxbw):
      case OP_X(Vmovsxbw): emit3i(asmjit::kX86InstIdPmovsxbw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vmovzxbw):
      case OP_X(Vmovzxbw): emit3i(asmjit::kX86InstIdPmovzxbw, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Vmovsxwd):
      case OP_X(Vmovsxwd): emit3i(asmjit::kX86InstIdPmovsxwd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vmovzxwd):
      case OP_X(Vmovzxwd): emit3i(asmjit::kX86InstIdPmovzxwd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Vpacksswb):
      case OP_X(Vpacksswb): emit3i(asmjit::kX86InstIdPacksswb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vpackuswb):
      case OP_X(Vpackuswb): emit3i(asmjit::kX86InstIdPackuswb, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Vpackssdw):
      case OP_X(Vpackssdw): emit3i(asmjit::kX86InstIdPackssdw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Vpackusdw):
      case OP_X(Vpackusdw): emit3i(asmjit::kX86InstIdPackusdw, asmOp[0], asmOp[1], asmOp[2]); break;

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

void JitCompiler::emit3i(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2) {
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

    case asmjit::kX86InstIdPackusdw: {
      // TODO:
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

void JitCompiler::emit3f(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2) {
  if (o0.getId() != o1.getId())
    _c->emit(o1.isVar() ? asmjit::kX86InstIdMovaps : asmjit::kX86InstIdMovss, o0, o1);
  _c->emit(instId, o0, o2);
}

void JitCompiler::emit3f(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2, int imm) {
  if (o0.getId() != o1.getId())
    _c->emit(o1.isVar() ? asmjit::kX86InstIdMovaps : asmjit::kX86InstIdMovss, o0, o1);
  _c->emit(instId, o0, o2, imm);
}

void JitCompiler::emit3d(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2) {
  if (o0.getId() != o1.getId())
    _c->emit(o1.isVar() ? asmjit::kX86InstIdMovapd : asmjit::kX86InstIdMovsd, o0, o1);
  _c->emit(instId, o0, o2);
}

void JitCompiler::emit3d(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2, int imm) {
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
