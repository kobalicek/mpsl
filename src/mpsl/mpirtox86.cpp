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

using asmjit::X86Inst;

static MPSL_INLINE bool mpIsGp(const asmjit::Operand& op) {
  return op.isReg() && static_cast<const asmjit::X86Reg&>(op).isGp();
}

// ============================================================================
// [mpsl::IRToX86 - Construction / Destruction]
// ============================================================================

IRToX86::IRToX86(ZoneHeap* heap, X86Compiler* cc)
  : _heap(heap),
    _cc(cc),
    _functionBody(nullptr),
    _constPool(&cc->_cbDataZone) {

  _tmpXmm0 = _cc->newXmm("tmpXmm0");
  _tmpXmm1 = _cc->newXmm("tmpXmm1");

  const asmjit::CpuInfo& cpu = asmjit::CpuInfo::getHost();
  _enableSSE4_1 = cpu.hasFeature(asmjit::CpuInfo::kX86FeatureSSE4_1);
}

IRToX86::~IRToX86() {}

// ============================================================================
// [mpsl::IRToX86 - Const Pool]
// ============================================================================

void IRToX86::prepareConstPool() {
  if (!_constLabel.isValid()) {
    _constLabel = _cc->newLabel();
    _constPtr = _cc->newIntPtr("constPool");
    _cc->setPriority(_constPtr, 2);

    asmjit::CBNode* prev = _cc->setCursor(_functionBody);
    _cc->lea(_constPtr, asmjit::x86::ptr(_constLabel));
    if (prev != _functionBody) _cc->setCursor(prev);
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
  asmjit::Data128 vec = asmjit::Data128::fromI64(value, 0);
  if (_constPool.add(&vec, sizeof(asmjit::Data128), offset) != asmjit::kErrorOk)
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

  asmjit::FuncSignatureX proto;
  proto.setRetT<unsigned int>();

  for (i = 0; i < numSlots; i++) {
    _data[i] = _cc->newIntPtr("ptr%u", i);
    ir->getDataPtr(i)->setJitId(_data[i].getId());
    proto.addArgT<void*>();
  }

  _cc->addFunc(proto);
  _functionBody = _cc->getCursor();

  for (i = 0; i < numSlots; i++) {
    _cc->setArg(i, _data[i]);
  }

  compileIRAsPart(ir);

  X86Gp errCode = _cc->newInt32("err");
  _cc->xor_(errCode, errCode);
  _cc->ret(errCode);

  _cc->endFunc();

  if (_constLabel.isValid())
    _cc->embedConstPool(_constLabel, _constPool);

  return kErrorOk;
}

Error IRToX86::compileIRAsPart(IRBuilder* ir) {
  // Compile the entry block and all other reacheable blocks.
  IRBlock* block = ir->getMainBlock();
  MPSL_PROPAGATE(compileConsecutiveBlocks(block));

  // Compile all remaining, reacheable, and non-assembled blocks.
  block = ir->_blockFirst;
  while (block) {
    if (!block->isAssembled())
      compileConsecutiveBlocks(block);
    block = block->_nextBlock;
  }

  return kErrorOk;
}

Error IRToX86::compileConsecutiveBlocks(IRBlock* block) {
  while (block) {
    const IRBody& body = block->getBody();
    IRBlock* next = nullptr;

    if (!body.isEmpty()) {
      IRInst* inst = body[body.getLength() - 1];
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
  IRBody& body = block->getBody();
  asmjit::Operand asmOp[3];

  for (size_t i = 0, len = body.getLength(); i < len; i++) {
    IRInst* inst = body[i];

    uint32_t opCount = inst->getOpCount();
    IRObject** irOpArray = inst->getOpArray();

    const InstInfo& info = mpInstInfo[inst->getInstCode() & kInstCodeMask];

    for (uint32_t opIndex = 0; opIndex < opCount; opIndex++) {
      IRObject* irOp = irOpArray[opIndex];

      switch (irOp->getObjectType()) {
        case IRObject::kTypeVar: {
          IRVar* var = static_cast<IRVar*>(irOp);
          if (var->getReg() == kIRRegGP)
            asmOp[opIndex] = varAsI32(var);
          if (var->getReg() == kIRRegSIMD)
            asmOp[opIndex] = varAsXmm(var);
          break;
        }

        case IRObject::kTypeMem: {
          IRMem* mem = static_cast<IRMem*>(irOp);
          IRVar* base = mem->getBase();
          IRVar* index = mem->getIndex();

          // TODO: index.
          asmOp[opIndex] = asmjit::x86::ptr(varAsPtr(base), mem->getOffset());
          break;
        }

        case IRObject::kTypeImm: {
          // TODO:
          IRImm* imm = static_cast<IRImm*>(irOp);

          if ((info.hasImm() && i == opCount - 1) || (info.isI32() && !info.isCvt()))
            asmOp[opIndex] = asmjit::imm(imm->getValue().i[0]);
          else
            asmOp[opIndex] = getConstantByValue(imm->getValue(), imm->getWidth());

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
          _cc->emit(X86Inst::kIdMov, asmOp[0], asmOp[1]);
        else
          _cc->emit(X86Inst::kIdMovd, asmOp[0], asmOp[1]);
        break;

      case OP_1(Fetch64):
      case OP_1(Store64):
        _cc->emit(X86Inst::kIdMovq, asmOp[0], asmOp[1]);
        break;

      case OP_1(Fetch96): {
        X86Mem mem = asmOp[1].as<X86Mem>();
        _cc->emit(X86Inst::kIdMovq, asmOp[0], mem);
        mem.addOffsetLo32(8);
        _cc->emit(X86Inst::kIdMovd, _tmpXmm0, mem);
        _cc->emit(X86Inst::kIdPunpcklqdq, asmOp[0], _tmpXmm0);
        break;
      }

      case OP_1(Store96): {
        X86Mem mem = asmOp[0].as<X86Mem>();
        _cc->emit(X86Inst::kIdMovq, mem, asmOp[1]);
        _cc->emit(X86Inst::kIdPshufd, _tmpXmm0, asmOp[1], X86Util::shuffle(1, 0, 3, 2));
        mem.addOffsetLo32(8);
        _cc->emit(X86Inst::kIdMovd, mem, _tmpXmm0);
        break;
      }

      case OP_1(Fetch128):
      case OP_1(Store128):
        _cc->emit(X86Inst::kIdMovups, asmOp[0], asmOp[1]);
        break;

      case OP_1(Mov32): emit2x(X86Inst::kIdMovd, asmOp[0], asmOp[1]); break;
      case OP_1(Mov64): emit2x(X86Inst::kIdMovq, asmOp[0], asmOp[1]); break;
      case OP_1(Mov128): emit2x(X86Inst::kIdMovaps, asmOp[0], asmOp[1]); break;

      case OP_1(Cvtitof): emit2x(X86Inst::kIdCvtsi2ss, asmOp[0], asmOp[1]); break;
      case OP_1(Cvtitod): emit2x(X86Inst::kIdCvtsi2sd, asmOp[0], asmOp[1]); break;

      case OP_1(Cvtftoi): emit2x(X86Inst::kIdCvttss2si, asmOp[0], asmOp[1]); break;
      case OP_1(Cvtftod): emit2x(X86Inst::kIdCvtss2sd, asmOp[0], asmOp[1]); break;

      case OP_1(Cvtdtoi): emit2x(X86Inst::kIdCvttsd2si, asmOp[0], asmOp[1]); break;
      case OP_1(Cvtdtof): emit2x(X86Inst::kIdCvtsd2ss, asmOp[0], asmOp[1]); break;

      case OP_1(Addf): emit3f(X86Inst::kIdAddss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Addf): emit3f(X86Inst::kIdAddps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Addd): emit3d(X86Inst::kIdAddsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Addd): emit3d(X86Inst::kIdAddpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Subf): emit3f(X86Inst::kIdSubss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Subf): emit3f(X86Inst::kIdSubps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Subd): emit3d(X86Inst::kIdSubsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Subd): emit3d(X86Inst::kIdSubpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Mulf): emit3f(X86Inst::kIdMulss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Mulf): emit3f(X86Inst::kIdMulps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Muld): emit3d(X86Inst::kIdMulsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Muld): emit3d(X86Inst::kIdMulpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Divf): emit3f(X86Inst::kIdDivss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Divf): emit3f(X86Inst::kIdDivps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Divd): emit3d(X86Inst::kIdDivsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Divd): emit3d(X86Inst::kIdDivpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Andi): emit3i(X86Inst::kIdAnd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Andi): emit3i(X86Inst::kIdPand, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Andf):
      case OP_X(Andf): emit3f(X86Inst::kIdAndps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Andd):
      case OP_X(Andd): emit3d(X86Inst::kIdAndpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Ori): emit3i(X86Inst::kIdOr, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Ori): emit3i(X86Inst::kIdPor, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Orf):
      case OP_X(Orf): emit3f(X86Inst::kIdOrps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Ord):
      case OP_X(Ord): emit3d(X86Inst::kIdOrpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Xori): emit3i(X86Inst::kIdXor, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Xori): emit3i(X86Inst::kIdPxor, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Xorf):
      case OP_X(Xorf): emit3f(X86Inst::kIdXorps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Xord):
      case OP_X(Xord): emit3d(X86Inst::kIdXorpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Minf): emit3f(X86Inst::kIdMinss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Minf): emit3f(X86Inst::kIdMinps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Mind): emit3d(X86Inst::kIdMinsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Mind): emit3d(X86Inst::kIdMinpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Maxf): emit3f(X86Inst::kIdMaxss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Maxf): emit3f(X86Inst::kIdMaxps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Maxd): emit3d(X86Inst::kIdMaxsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Maxd): emit3d(X86Inst::kIdMaxpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Sqrtf): emit2x(X86Inst::kIdSqrtss, asmOp[0], asmOp[1]); break;
      case OP_X(Sqrtf): emit2x(X86Inst::kIdSqrtps, asmOp[0], asmOp[1]); break;
      case OP_1(Sqrtd): emit2x(X86Inst::kIdSqrtsd, asmOp[0], asmOp[1]); break;
      case OP_X(Sqrtd): emit2x(X86Inst::kIdSqrtpd, asmOp[0], asmOp[1]); break;

      case OP_1(Cmpeqf): emit3f(X86Inst::kIdCmpss, asmOp[0], asmOp[1], asmOp[2], X86Inst::kCmpEQ); break;
      case OP_X(Cmpeqf): emit3f(X86Inst::kIdCmpps, asmOp[0], asmOp[1], asmOp[2], X86Inst::kCmpEQ); break;
      case OP_1(Cmpeqd): emit3d(X86Inst::kIdCmpsd, asmOp[0], asmOp[1], asmOp[2], X86Inst::kCmpEQ); break;
      case OP_X(Cmpeqd): emit3d(X86Inst::kIdCmppd, asmOp[0], asmOp[1], asmOp[2], X86Inst::kCmpEQ); break;

      case OP_1(Cmpnef): emit3f(X86Inst::kIdCmpss, asmOp[0], asmOp[1], asmOp[2], X86Inst::kCmpNEQ); break;
      case OP_X(Cmpnef): emit3f(X86Inst::kIdCmpps, asmOp[0], asmOp[1], asmOp[2], X86Inst::kCmpNEQ); break;
      case OP_1(Cmpned): emit3d(X86Inst::kIdCmpsd, asmOp[0], asmOp[1], asmOp[2], X86Inst::kCmpNEQ); break;
      case OP_X(Cmpned): emit3d(X86Inst::kIdCmppd, asmOp[0], asmOp[1], asmOp[2], X86Inst::kCmpNEQ); break;

      case OP_1(Cmpltf): emit3f(X86Inst::kIdCmpss, asmOp[0], asmOp[1], asmOp[2], X86Inst::kCmpLT); break;
      case OP_X(Cmpltf): emit3f(X86Inst::kIdCmpps, asmOp[0], asmOp[1], asmOp[2], X86Inst::kCmpLT); break;
      case OP_1(Cmpltd): emit3d(X86Inst::kIdCmpsd, asmOp[0], asmOp[1], asmOp[2], X86Inst::kCmpLT); break;
      case OP_X(Cmpltd): emit3d(X86Inst::kIdCmppd, asmOp[0], asmOp[1], asmOp[2], X86Inst::kCmpLT); break;

      case OP_1(Cmplef): emit3f(X86Inst::kIdCmpss, asmOp[0], asmOp[1], asmOp[2], X86Inst::kCmpLE); break;
      case OP_X(Cmplef): emit3f(X86Inst::kIdCmpps, asmOp[0], asmOp[1], asmOp[2], X86Inst::kCmpLE); break;
      case OP_1(Cmpled): emit3d(X86Inst::kIdCmpsd, asmOp[0], asmOp[1], asmOp[2], X86Inst::kCmpLE); break;
      case OP_X(Cmpled): emit3d(X86Inst::kIdCmppd, asmOp[0], asmOp[1], asmOp[2], X86Inst::kCmpLE); break;

      case OP_1(Cmpgtf): emit3f(X86Inst::kIdCmpss, asmOp[0], asmOp[2], asmOp[1], X86Inst::kCmpLE); break;
      case OP_X(Cmpgtf): emit3f(X86Inst::kIdCmpps, asmOp[0], asmOp[2], asmOp[1], X86Inst::kCmpLE); break;
      case OP_1(Cmpgtd): emit3d(X86Inst::kIdCmpsd, asmOp[0], asmOp[2], asmOp[1], X86Inst::kCmpLE); break;
      case OP_X(Cmpgtd): emit3d(X86Inst::kIdCmppd, asmOp[0], asmOp[2], asmOp[1], X86Inst::kCmpLE); break;

      case OP_1(Cmpgef): emit3f(X86Inst::kIdCmpss, asmOp[0], asmOp[2], asmOp[1], X86Inst::kCmpLT); break;
      case OP_X(Cmpgef): emit3f(X86Inst::kIdCmpps, asmOp[0], asmOp[2], asmOp[1], X86Inst::kCmpLT); break;
      case OP_1(Cmpged): emit3d(X86Inst::kIdCmpsd, asmOp[0], asmOp[2], asmOp[1], X86Inst::kCmpLT); break;
      case OP_X(Cmpged): emit3d(X86Inst::kIdCmppd, asmOp[0], asmOp[2], asmOp[1], X86Inst::kCmpLT); break;

      case OP_1(Pshufd):
      case OP_X(Pshufd): emit3d(X86Inst::kIdPshufd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pmovsxbw):
      case OP_X(Pmovsxbw): emit3i(X86Inst::kIdPmovsxbw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmovzxbw):
      case OP_X(Pmovzxbw): emit3i(X86Inst::kIdPmovzxbw, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pmovsxwd):
      case OP_X(Pmovsxwd): emit3i(X86Inst::kIdPmovsxwd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmovzxwd):
      case OP_X(Pmovzxwd): emit3i(X86Inst::kIdPmovzxwd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Packsswb):
      case OP_X(Packsswb): emit3i(X86Inst::kIdPacksswb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Packuswb):
      case OP_X(Packuswb): emit3i(X86Inst::kIdPackuswb, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Packssdw):
      case OP_X(Packssdw): emit3i(X86Inst::kIdPackssdw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Packusdw):
      case OP_X(Packusdw): emit3i(X86Inst::kIdPackusdw, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Paddb):
      case OP_X(Paddb): emit3i(X86Inst::kIdPaddb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Paddw):
      case OP_X(Paddw): emit3i(X86Inst::kIdPaddw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Paddd): emit3i(X86Inst::kIdAdd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Paddd): emit3i(X86Inst::kIdPaddd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Paddq):
      case OP_X(Paddq): emit3i(X86Inst::kIdPaddq, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Paddssb):
      case OP_X(Paddssb): emit3i(X86Inst::kIdPaddsb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Paddusb):
      case OP_X(Paddusb): emit3i(X86Inst::kIdPaddusb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Paddssw):
      case OP_X(Paddssw): emit3i(X86Inst::kIdPaddsw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Paddusw):
      case OP_X(Paddusw): emit3i(X86Inst::kIdPaddusw, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Psubb):
      case OP_X(Psubb): emit3i(X86Inst::kIdPsubb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psubw):
      case OP_X(Psubw): emit3i(X86Inst::kIdPsubw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psubd): emit3i(X86Inst::kIdSub, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Psubd): emit3i(X86Inst::kIdPsubd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psubq):
      case OP_X(Psubq): emit3i(X86Inst::kIdPsubq, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psubssb):
      case OP_X(Psubssb): emit3i(X86Inst::kIdPsubsb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psubusb):
      case OP_X(Psubusb): emit3i(X86Inst::kIdPsubusb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psubssw):
      case OP_X(Psubssw): emit3i(X86Inst::kIdPsubsw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psubusw):
      case OP_X(Psubusw): emit3i(X86Inst::kIdPsubusw, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pmulw):
      case OP_X(Pmulw): emit3i(X86Inst::kIdPmullw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmulhsw):
      case OP_X(Pmulhsw): emit3i(X86Inst::kIdPmulhw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmulhuw):
      case OP_X(Pmulhuw): emit3i(X86Inst::kIdPmulhuw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmuld): emit3i(X86Inst::kIdImul, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Pmuld): emit3i(X86Inst::kIdPmulld, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pminsb):
      case OP_X(Pminsb): emit3i(X86Inst::kIdPminsb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pminub):
      case OP_X(Pminub): emit3i(X86Inst::kIdPminub, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pminsw):
      case OP_X(Pminsw): emit3i(X86Inst::kIdPminsw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pminuw):
      case OP_X(Pminuw): emit3i(X86Inst::kIdPminuw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pminsd):
      case OP_X(Pminsd): emit3i(X86Inst::kIdPminsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pminud):
      case OP_X(Pminud): emit3i(X86Inst::kIdPminud, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pmaxsb):
      case OP_X(Pmaxsb): emit3i(X86Inst::kIdPmaxsb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmaxub):
      case OP_X(Pmaxub): emit3i(X86Inst::kIdPmaxub, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmaxsw):
      case OP_X(Pmaxsw): emit3i(X86Inst::kIdPmaxsw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmaxuw):
      case OP_X(Pmaxuw): emit3i(X86Inst::kIdPmaxuw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmaxsd):
      case OP_X(Pmaxsd): emit3i(X86Inst::kIdPmaxsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmaxud):
      case OP_X(Pmaxud): emit3i(X86Inst::kIdPmaxud, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Psllw):
      case OP_X(Psllw): emit3i(X86Inst::kIdPsllw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psrlw):
      case OP_X(Psrlw): emit3i(X86Inst::kIdPsrlw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psraw):
      case OP_X(Psraw): emit3i(X86Inst::kIdPsraw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pslld):
      case OP_X(Pslld): emit3i(X86Inst::kIdPslld, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psrld):
      case OP_X(Psrld): emit3i(X86Inst::kIdPsrld, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psrad):
      case OP_X(Psrad): emit3i(X86Inst::kIdPsrad, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psllq):
      case OP_X(Psllq): emit3i(X86Inst::kIdPsllq, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psrlq):
      case OP_X(Psrlq): emit3i(X86Inst::kIdPsrlq, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pmaddwd):
      case OP_X(Pmaddwd): emit3i(X86Inst::kIdPmaddwd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pcmpeqb):
      case OP_X(Pcmpeqb): emit3i(X86Inst::kIdPcmpeqb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pcmpeqw):
      case OP_X(Pcmpeqw): emit3i(X86Inst::kIdPcmpeqw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pcmpeqd):
      case OP_X(Pcmpeqd): emit3i(X86Inst::kIdPcmpeqd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pcmpgtb):
      case OP_X(Pcmpgtb): emit3i(X86Inst::kIdPcmpgtb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pcmpgtw):
      case OP_X(Pcmpgtw): emit3i(X86Inst::kIdPcmpgtw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pcmpgtd):
      case OP_X(Pcmpgtd): emit3i(X86Inst::kIdPcmpgtd, asmOp[0], asmOp[1], asmOp[2]); break;

      default:
        // TODO:
        MPSL_ASSERT(!"Implemented");
    }
#undef OP_Y
#undef OP_X
#undef OP_1
  }

  block->setAssembled();
  return kErrorOk;
}

void IRToX86::emit3i(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2) {
  // Intercept instructions that are disabled for the current target and
  // substitute them with a sequential code that is compatible. It's easier
  // to deal with it here than dealing with it in `compileBasicBlock()`.
  switch (instId) {
    case X86Inst::kIdPmulld: {
      if (_enableSSE4_1)
        break;

      if (o0.getId() != o1.getId())
        _cc->emit(X86Inst::kIdMovaps, o0, o1);

      _cc->emit(X86Inst::kIdPshufd, _tmpXmm0, o1, X86Util::shuffle(2, 3, 0, 1));
      _cc->emit(X86Inst::kIdPshufd, _tmpXmm1, o2, X86Util::shuffle(2, 3, 0, 1));

      _cc->emit(X86Inst::kIdPmuludq, _tmpXmm0, _tmpXmm1);
      _cc->emit(X86Inst::kIdPmuludq, o0, o2);
      _cc->emit(X86Inst::kIdShufps, o0, _tmpXmm0, X86Util::shuffle(0, 2, 0, 2));
      return;
    }

    case X86Inst::kIdPackusdw: {
      // TODO:
    }
  }

  // Instruction `instId` is emitted as is.
  if (o0.getId() != o1.getId()) {
    if (mpIsGp(o0) && mpIsGp(o1))
      _cc->emit(X86Inst::kIdMov, o0, o1);
    else
      _cc->emit(X86Inst::kIdMovaps, o0, o1);
  }
  _cc->emit(instId, o0, o2);
}

void IRToX86::emit3f(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2) {
  if (o0.getId() != o1.getId())
    _cc->emit(o1.isReg() ? X86Inst::kIdMovaps : X86Inst::kIdMovss, o0, o1);
  _cc->emit(instId, o0, o2);
}

void IRToX86::emit3f(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2, int imm) {
  if (o0.getId() != o1.getId())
    _cc->emit(o1.isReg() ? X86Inst::kIdMovaps : X86Inst::kIdMovss, o0, o1);
  _cc->emit(instId, o0, o2, imm);
}

void IRToX86::emit3d(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2) {
  if (o0.getId() != o1.getId())
    _cc->emit(o1.isReg() ? X86Inst::kIdMovapd : X86Inst::kIdMovsd, o0, o1);
  _cc->emit(instId, o0, o2);
}

void IRToX86::emit3d(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2, int imm) {
  if (o0.getId() != o1.getId())
    _cc->emit(o1.isReg() ? X86Inst::kIdMovapd : X86Inst::kIdMovsd, o0, o1);
  _cc->emit(instId, o0, o2, imm);
}

X86Gp IRToX86::varAsPtr(IRVar* irVar) {
  uint32_t id = irVar->getJitId();
  MPSL_ASSERT(id != kInvalidValue);

  return _cc->gpz(id);
}

X86Gp IRToX86::varAsI32(IRVar* irVar) {
  uint32_t id = irVar->getJitId();

  if (id == kInvalidValue) {
    X86Gp gp = _cc->newI32("%%%u", irVar->getId());
    irVar->setJitId(gp.getId());
    return gp;
  }
  else {
    return asmjit::x86::gpd(id);
  }
}

X86Xmm IRToX86::varAsXmm(IRVar* irVar) {
  uint32_t id = irVar->getJitId();

  if (id == kInvalidValue) {
    X86Xmm xmm = _cc->newXmm("%%%u", irVar->getId());
    irVar->setJitId(xmm.getId());
    return xmm;
  }
  else {
    return asmjit::x86::xmm(id);
  }
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
