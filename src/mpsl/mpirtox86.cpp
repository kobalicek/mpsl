// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MPSL_EXPORTS

// [Dependencies - MPSL]
#include "./mpirtox86_p.h"
#include "./mpmath_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

static MPSL_INLINE bool mpIsGp(const asmjit::Operand& op) {
  return op.isVar() && static_cast<const asmjit::X86Var&>(op).isGp();
}

// ============================================================================
// [mpsl::IRToX86 - Construction / Destruction]
// ============================================================================

IRToX86::IRToX86(Allocator* allocator, X86Compiler* c)
  : _allocator(allocator),
    _c(c),
    _functionBody(nullptr),
    _constPool(&c->_constAllocator) {

  _tmpXmm0 = _c->newXmm("tmpXmm0");
  _tmpXmm1 = _c->newXmm("tmpXmm1");

  const asmjit::CpuInfo& cpu = c->getRuntime()->getCpuInfo();
  _enableSSE4_1 = cpu.hasFeature(asmjit::CpuInfo::kX86FeatureSSE4_1);
}

IRToX86::~IRToX86() {}

// ============================================================================
// [mpsl::IRToX86 - Const Pool]
// ============================================================================

void IRToX86::prepareConstPool() {
  if (!_constLabel.isInitialized()) {
    _constLabel = _c->newLabel();
    _constPtr = _c->newIntPtr("constPool");
    _c->setPriority(_constPtr, 2);

    asmjit::HLNode* prev = _c->setCursor(_functionBody);
    _c->lea(_constPtr, asmjit::x86::ptr(_constLabel));
    if (prev != _functionBody) _c->setCursor(prev);
  }
}

asmjit::X86Mem IRToX86::getConstantU64(uint64_t value) {
  prepareConstPool();

  size_t offset;
  if (_constPool.add(&value, sizeof(uint64_t), offset) != asmjit::kErrorOk)
    return asmjit::X86Mem();

  return asmjit::x86::ptr(_constPtr, static_cast<int>(offset));
}

asmjit::X86Mem IRToX86::getConstantU64AsPD(uint64_t value) {
  prepareConstPool();

  size_t offset;
  asmjit::Vec128 vec = asmjit::Vec128::fromSQ(value, 0);
  if (_constPool.add(&vec, sizeof(asmjit::Vec128), offset) != asmjit::kErrorOk)
    return asmjit::X86Mem();

  return asmjit::x86::ptr(_constPtr, static_cast<int>(offset));
}

asmjit::X86Mem IRToX86::getConstantD64(double value) {
  DoubleBits bits;
  bits.d = value;
  return getConstantU64(bits.u);
}

asmjit::X86Mem IRToX86::getConstantD64AsPD(double value) {
  DoubleBits bits;
  bits.d = value;
  return getConstantU64AsPD(bits.u);
}

asmjit::X86Mem IRToX86::getConstantByValue(const Value& value, uint32_t width) {
  prepareConstPool();

  size_t offset;
  if (_constPool.add(&value, width, offset) != asmjit::kErrorOk)
    return asmjit::X86Mem();

  return asmjit::x86::ptr(_constPtr, static_cast<int>(offset));
}

// ============================================================================
// [mpsl::IRToX86 - Compile]
// ============================================================================

Error IRToX86::compileIRAsFunc(IRBuilder* ir) {
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

Error IRToX86::compileIRAsPart(IRBuilder* ir) {
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

Error IRToX86::compileConsecutiveBlocks(IRBlock* block) {
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

Error IRToX86::compileBasicBlock(IRBlock* block, IRBlock* next) {
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

      case OP_1(Addf): emit3f(asmjit::kX86InstIdAddss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Addf): emit3f(asmjit::kX86InstIdAddps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Addd): emit3d(asmjit::kX86InstIdAddsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Addd): emit3d(asmjit::kX86InstIdAddpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Subf): emit3f(asmjit::kX86InstIdSubss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Subf): emit3f(asmjit::kX86InstIdSubps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Subd): emit3d(asmjit::kX86InstIdSubsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Subd): emit3d(asmjit::kX86InstIdSubpd, asmOp[0], asmOp[1], asmOp[2]); break;

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

      case OP_1(Pshufd):
      case OP_X(Pshufd): emit3d(asmjit::kX86InstIdPshufd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pmovsxbw):
      case OP_X(Pmovsxbw): emit3i(asmjit::kX86InstIdPmovsxbw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmovzxbw):
      case OP_X(Pmovzxbw): emit3i(asmjit::kX86InstIdPmovzxbw, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pmovsxwd):
      case OP_X(Pmovsxwd): emit3i(asmjit::kX86InstIdPmovsxwd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmovzxwd):
      case OP_X(Pmovzxwd): emit3i(asmjit::kX86InstIdPmovzxwd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Packsswb):
      case OP_X(Packsswb): emit3i(asmjit::kX86InstIdPacksswb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Packuswb):
      case OP_X(Packuswb): emit3i(asmjit::kX86InstIdPackuswb, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Packssdw):
      case OP_X(Packssdw): emit3i(asmjit::kX86InstIdPackssdw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Packusdw):
      case OP_X(Packusdw): emit3i(asmjit::kX86InstIdPackusdw, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Paddb):
      case OP_X(Paddb): emit3i(asmjit::kX86InstIdPaddb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Paddw):
      case OP_X(Paddw): emit3i(asmjit::kX86InstIdPaddw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Paddd): emit3i(asmjit::kX86InstIdAdd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Paddd): emit3i(asmjit::kX86InstIdPaddd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Paddq):
      case OP_X(Paddq): emit3i(asmjit::kX86InstIdPaddq, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Paddssb):
      case OP_X(Paddssb): emit3i(asmjit::kX86InstIdPaddsb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Paddusb):
      case OP_X(Paddusb): emit3i(asmjit::kX86InstIdPaddusb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Paddssw):
      case OP_X(Paddssw): emit3i(asmjit::kX86InstIdPaddsw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Paddusw):
      case OP_X(Paddusw): emit3i(asmjit::kX86InstIdPaddusw, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Psubb):
      case OP_X(Psubb): emit3i(asmjit::kX86InstIdPsubb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psubw):
      case OP_X(Psubw): emit3i(asmjit::kX86InstIdPsubw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psubd): emit3i(asmjit::kX86InstIdSub, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Psubd): emit3i(asmjit::kX86InstIdPsubd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psubq):
      case OP_X(Psubq): emit3i(asmjit::kX86InstIdPsubq, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psubssb):
      case OP_X(Psubssb): emit3i(asmjit::kX86InstIdPsubsb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psubusb):
      case OP_X(Psubusb): emit3i(asmjit::kX86InstIdPsubusb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psubssw):
      case OP_X(Psubssw): emit3i(asmjit::kX86InstIdPsubsw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psubusw):
      case OP_X(Psubusw): emit3i(asmjit::kX86InstIdPsubusw, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pmulw):
      case OP_X(Pmulw): emit3i(asmjit::kX86InstIdPmullw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmulhsw):
      case OP_X(Pmulhsw): emit3i(asmjit::kX86InstIdPmulhw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmulhuw):
      case OP_X(Pmulhuw): emit3i(asmjit::kX86InstIdPmulhuw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmuld): emit3i(asmjit::kX86InstIdImul, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Pmuld): emit3i(asmjit::kX86InstIdPmulld, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pminsb):
      case OP_X(Pminsb): emit3i(asmjit::kX86InstIdPminsb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pminub):
      case OP_X(Pminub): emit3i(asmjit::kX86InstIdPminub, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pminsw):
      case OP_X(Pminsw): emit3i(asmjit::kX86InstIdPminsw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pminuw):
      case OP_X(Pminuw): emit3i(asmjit::kX86InstIdPminuw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pminsd):
      case OP_X(Pminsd): emit3i(asmjit::kX86InstIdPminsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pminud):
      case OP_X(Pminud): emit3i(asmjit::kX86InstIdPminud, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pmaxsb):
      case OP_X(Pmaxsb): emit3i(asmjit::kX86InstIdPmaxsb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmaxub):
      case OP_X(Pmaxub): emit3i(asmjit::kX86InstIdPmaxub, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmaxsw):
      case OP_X(Pmaxsw): emit3i(asmjit::kX86InstIdPmaxsw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmaxuw):
      case OP_X(Pmaxuw): emit3i(asmjit::kX86InstIdPmaxuw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmaxsd):
      case OP_X(Pmaxsd): emit3i(asmjit::kX86InstIdPmaxsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmaxud):
      case OP_X(Pmaxud): emit3i(asmjit::kX86InstIdPmaxud, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Psllw):
      case OP_X(Psllw): emit3i(asmjit::kX86InstIdPsllw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psrlw):
      case OP_X(Psrlw): emit3i(asmjit::kX86InstIdPsrlw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psraw):
      case OP_X(Psraw): emit3i(asmjit::kX86InstIdPsraw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pslld):
      case OP_X(Pslld): emit3i(asmjit::kX86InstIdPslld, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psrld):
      case OP_X(Psrld): emit3i(asmjit::kX86InstIdPsrld, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psrad):
      case OP_X(Psrad): emit3i(asmjit::kX86InstIdPsrad, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psllq):
      case OP_X(Psllq): emit3i(asmjit::kX86InstIdPsllq, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psrlq):
      case OP_X(Psrlq): emit3i(asmjit::kX86InstIdPsrlq, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pmaddwd):
      case OP_X(Pmaddwd): emit3i(asmjit::kX86InstIdPmaddwd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pcmpeqb):
      case OP_X(Pcmpeqb): emit3i(asmjit::kX86InstIdPcmpeqb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pcmpeqw):
      case OP_X(Pcmpeqw): emit3i(asmjit::kX86InstIdPcmpeqw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pcmpeqd):
      case OP_X(Pcmpeqd): emit3i(asmjit::kX86InstIdPcmpeqd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pcmpgtb):
      case OP_X(Pcmpgtb): emit3i(asmjit::kX86InstIdPcmpgtb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pcmpgtw):
      case OP_X(Pcmpgtw): emit3i(asmjit::kX86InstIdPcmpgtw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pcmpgtd):
      case OP_X(Pcmpgtd): emit3i(asmjit::kX86InstIdPcmpgtd, asmOp[0], asmOp[1], asmOp[2]); break;

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

void IRToX86::emit3i(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2) {
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

void IRToX86::emit3f(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2) {
  if (o0.getId() != o1.getId())
    _c->emit(o1.isVar() ? asmjit::kX86InstIdMovaps : asmjit::kX86InstIdMovss, o0, o1);
  _c->emit(instId, o0, o2);
}

void IRToX86::emit3f(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2, int imm) {
  if (o0.getId() != o1.getId())
    _c->emit(o1.isVar() ? asmjit::kX86InstIdMovaps : asmjit::kX86InstIdMovss, o0, o1);
  _c->emit(instId, o0, o2, imm);
}

void IRToX86::emit3d(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2) {
  if (o0.getId() != o1.getId())
    _c->emit(o1.isVar() ? asmjit::kX86InstIdMovapd : asmjit::kX86InstIdMovsd, o0, o1);
  _c->emit(instId, o0, o2);
}

void IRToX86::emit3d(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2, int imm) {
  if (o0.getId() != o1.getId())
    _c->emit(o1.isVar() ? asmjit::kX86InstIdMovapd : asmjit::kX86InstIdMovsd, o0, o1);
  _c->emit(instId, o0, o2, imm);
}

X86GpVar IRToX86::varAsPtr(IRVar* irVar) {
  uint32_t id = irVar->getJitId();
  MPSL_ASSERT(id != kInvalidValue);

  return _c->getIntPtrById(id);
}

X86GpVar IRToX86::varAsI32(IRVar* irVar) {
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

X86XmmVar IRToX86::varAsXmm(IRVar* irVar) {
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
