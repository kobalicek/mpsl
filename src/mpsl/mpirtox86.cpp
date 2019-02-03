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

using namespace asmjit;

// ============================================================================
// [mpsl::IRToX86 - Construction / Destruction]
// ============================================================================

IRToX86::IRToX86(ZoneAllocator* allocator, x86::Compiler* cc)
  : _allocator(allocator),
    _cc(cc),
    _functionBody(nullptr),
    _constPool(&cc->_codeZone) {

  _tmpXmm0 = _cc->newXmm("tmpXmm0");
  _tmpXmm1 = _cc->newXmm("tmpXmm1");

  const x86::Features& features = CpuInfo::host().features().as<x86::Features>();
  _enableSSE4_1 = features.hasSSE4_1();
}

IRToX86::~IRToX86() {}

// ============================================================================
// [mpsl::IRToX86 - Const Pool]
// ============================================================================

void IRToX86::prepareConstPool() {
  if (!_constLabel.isValid()) {
    _constLabel = _cc->newLabel();
    _constPtr = _cc->newIntPtr("constPool");

    BaseNode* prev = _cc->setCursor(_functionBody);
    _cc->lea(_constPtr, x86::ptr(_constLabel));
    if (prev != _functionBody) _cc->setCursor(prev);
  }
}

x86::Mem IRToX86::getConstantU64(uint64_t value) {
  prepareConstPool();

  size_t offset;
  if (_constPool.add(&value, sizeof(uint64_t), offset) != kErrorOk)
    return x86::Mem();

  return x86::ptr(_constPtr, static_cast<int>(offset));
}

x86::Mem IRToX86::getConstantU64AsPD(uint64_t value) {
  prepareConstPool();

  size_t offset;
  Data128 vec = Data128::fromI64(value, 0);
  if (_constPool.add(&vec, sizeof(Data128), offset) != kErrorOk)
    return x86::Mem();

  return x86::ptr(_constPtr, static_cast<int>(offset));
}

x86::Mem IRToX86::getConstantD64(double value) {
  DoubleBits bits;
  bits.d = value;
  return getConstantU64(bits.u);
}

x86::Mem IRToX86::getConstantD64AsPD(double value) {
  DoubleBits bits;
  bits.d = value;
  return getConstantU64AsPD(bits.u);
}

x86::Mem IRToX86::getConstantByValue(const Value& value, uint32_t width) {
  prepareConstPool();

  size_t offset;
  if (_constPool.add(&value, width, offset) != kErrorOk)
    return x86::Mem();

  return x86::ptr(_constPtr, static_cast<int>(offset));
}

// ============================================================================
// [mpsl::IRToX86 - Compile]
// ============================================================================

Error IRToX86::compileIRAsFunc(IRBuilder* ir) {
  uint32_t i;
  uint32_t numSlots = ir->numSlots();

  FuncSignatureBuilder proto;
  proto.setRetT<unsigned int>();

  for (i = 0; i < numSlots; i++) {
    _data[i] = _cc->newIntPtr("ptr%u", i);
    ir->dataPtr(i)->setJitId(_data[i].id());
    proto.addArgT<void*>();
  }

  _cc->addFunc(proto);
  _functionBody = _cc->cursor();

  for (i = 0; i < numSlots; i++) {
    _cc->setArg(i, _data[i]);
  }

  compileIRAsPart(ir);

  x86::Gp errCode = _cc->newInt32("err");
  _cc->xor_(errCode, errCode);
  _cc->ret(errCode);

  _cc->endFunc();

  if (_constLabel.isValid())
    _cc->embedConstPool(_constLabel, _constPool);

  return kErrorOk;
}

Error IRToX86::compileIRAsPart(IRBuilder* ir) {
  // Compile the entry block and all other reacheable blocks.
  MPSL_PROPAGATE(compileConsecutiveBlocks(ir->entryBlock()));

  // Compile all remaining, reacheable, and non-assembled blocks.
  for (IRBlock* block : ir->blocks())
    if (!block->isAssembled())
      compileConsecutiveBlocks(block);

  return kErrorOk;
}

Error IRToX86::compileConsecutiveBlocks(IRBlock* block) {
  while (block) {
    const IRBody& body = block->body();
    IRBlock* next = nullptr;

    if (!body.empty()) {
      IRInst* inst = body[body.size() - 1];
      if (inst->instCode() == kInstCodeJmp) {
        next = static_cast<IRBlock*>(inst->op(0));
        if (next->isAssembled()) next = nullptr;
      }
      else if (inst->instCode() == kInstCodeJnz) {
        next = static_cast<IRBlock*>(inst->op(1));
        if (next->isAssembled()) {
          next = static_cast<IRBlock*>(inst->op(2));
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
  IRBody& body = block->body();
  Operand asmOp[3];

  for (size_t i = 0, size = body.size(); i < size; i++) {
    IRInst* inst = body[i];

    uint32_t opCount = inst->opCount();
    IRObject** irOpArray = inst->operands();

    const InstInfo& info = mpInstInfo[inst->instCode() & kInstCodeMask];

    for (uint32_t opIndex = 0; opIndex < opCount; opIndex++) {
      IRObject* irOp = irOpArray[opIndex];

      switch (irOp->objectType()) {
        case IRObject::kTypeReg: {
          IRReg* var = static_cast<IRReg*>(irOp);
          if (var->reg() == IRReg::kKindGp ) asmOp[opIndex] = varAsI32(var);
          if (var->reg() == IRReg::kKindVec) asmOp[opIndex] = varAsXmm(var);
          break;
        }

        case IRObject::kTypeMem: {
          IRMem* mem = static_cast<IRMem*>(irOp);
          IRReg* base = mem->base();
          IRReg* index = mem->index();

          // TODO: index.
          asmOp[opIndex] = x86::ptr(varAsPtr(base), mem->offset());
          break;
        }

        case IRObject::kTypeImm: {
          // TODO:
          IRImm* immValue = static_cast<IRImm*>(irOp);

          if ((info.hasImm() && i == opCount - 1) || (info.isI32() && !info.isCvt()))
            asmOp[opIndex] = imm(immValue->value().i[0]);
          else
            asmOp[opIndex] = getConstantByValue(immValue->value(), immValue->width());

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
    switch (inst->instCode()) {
      case OP_1(Fetch32):
      case OP_1(Store32):
        if ((x86::Reg::isGp(asmOp[0]) && (asmOp[1].isMem() || x86::Reg::isGp(asmOp[1]))) ||
            (x86::Reg::isGp(asmOp[1]) && (asmOp[0].isMem())))
          _cc->emit(x86::Inst::kIdMov, asmOp[0], asmOp[1]);
        else
          _cc->emit(x86::Inst::kIdMovd, asmOp[0], asmOp[1]);
        break;

      case OP_1(Fetch64):
      case OP_1(Store64):
        _cc->emit(x86::Inst::kIdMovq, asmOp[0], asmOp[1]);
        break;

      case OP_1(Fetch96): {
        x86::Mem mem = asmOp[1].as<x86::Mem>();
        _cc->emit(x86::Inst::kIdMovq, asmOp[0], mem);
        mem.addOffsetLo32(8);
        _cc->emit(x86::Inst::kIdMovd, _tmpXmm0, mem);
        _cc->emit(x86::Inst::kIdPunpcklqdq, asmOp[0], _tmpXmm0);
        break;
      }

      case OP_1(Store96): {
        x86::Mem mem = asmOp[0].as<x86::Mem>();
        _cc->emit(x86::Inst::kIdMovq, mem, asmOp[1]);
        _cc->emit(x86::Inst::kIdPshufd, _tmpXmm0, asmOp[1], x86::Predicate::shuf(1, 0, 3, 2));
        mem.addOffsetLo32(8);
        _cc->emit(x86::Inst::kIdMovd, mem, _tmpXmm0);
        break;
      }

      case OP_1(Fetch128):
      case OP_1(Store128):
        _cc->emit(x86::Inst::kIdMovups, asmOp[0], asmOp[1]);
        break;

      case OP_1(Mov32): emit2x(x86::Inst::kIdMovd, asmOp[0], asmOp[1]); break;
      case OP_1(Mov64): emit2x(x86::Inst::kIdMovq, asmOp[0], asmOp[1]); break;
      case OP_1(Mov128): emit2x(x86::Inst::kIdMovaps, asmOp[0], asmOp[1]); break;

      case OP_1(Cvtitof): emit2x(x86::Inst::kIdCvtsi2ss, asmOp[0], asmOp[1]); break;
      case OP_1(Cvtitod): emit2x(x86::Inst::kIdCvtsi2sd, asmOp[0], asmOp[1]); break;

      case OP_1(Cvtftoi): emit2x(x86::Inst::kIdCvttss2si, asmOp[0], asmOp[1]); break;
      case OP_1(Cvtftod): emit2x(x86::Inst::kIdCvtss2sd, asmOp[0], asmOp[1]); break;

      case OP_1(Cvtdtoi): emit2x(x86::Inst::kIdCvttsd2si, asmOp[0], asmOp[1]); break;
      case OP_1(Cvtdtof): emit2x(x86::Inst::kIdCvtsd2ss, asmOp[0], asmOp[1]); break;

      case OP_1(Addf): emit3f(x86::Inst::kIdAddss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Addf): emit3f(x86::Inst::kIdAddps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Addd): emit3d(x86::Inst::kIdAddsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Addd): emit3d(x86::Inst::kIdAddpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Subf): emit3f(x86::Inst::kIdSubss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Subf): emit3f(x86::Inst::kIdSubps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Subd): emit3d(x86::Inst::kIdSubsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Subd): emit3d(x86::Inst::kIdSubpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Mulf): emit3f(x86::Inst::kIdMulss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Mulf): emit3f(x86::Inst::kIdMulps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Muld): emit3d(x86::Inst::kIdMulsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Muld): emit3d(x86::Inst::kIdMulpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Divf): emit3f(x86::Inst::kIdDivss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Divf): emit3f(x86::Inst::kIdDivps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Divd): emit3d(x86::Inst::kIdDivsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Divd): emit3d(x86::Inst::kIdDivpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Andi): emit3i(x86::Inst::kIdAnd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Andi): emit3i(x86::Inst::kIdPand, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Andf):
      case OP_X(Andf): emit3f(x86::Inst::kIdAndps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Andd):
      case OP_X(Andd): emit3d(x86::Inst::kIdAndpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Ori): emit3i(x86::Inst::kIdOr, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Ori): emit3i(x86::Inst::kIdPor, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Orf):
      case OP_X(Orf): emit3f(x86::Inst::kIdOrps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Ord):
      case OP_X(Ord): emit3d(x86::Inst::kIdOrpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Xori): emit3i(x86::Inst::kIdXor, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Xori): emit3i(x86::Inst::kIdPxor, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Xorf):
      case OP_X(Xorf): emit3f(x86::Inst::kIdXorps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Xord):
      case OP_X(Xord): emit3d(x86::Inst::kIdXorpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Minf): emit3f(x86::Inst::kIdMinss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Minf): emit3f(x86::Inst::kIdMinps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Mind): emit3d(x86::Inst::kIdMinsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Mind): emit3d(x86::Inst::kIdMinpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Maxf): emit3f(x86::Inst::kIdMaxss, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Maxf): emit3f(x86::Inst::kIdMaxps, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Maxd): emit3d(x86::Inst::kIdMaxsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Maxd): emit3d(x86::Inst::kIdMaxpd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Sqrtf): emit2x(x86::Inst::kIdSqrtss, asmOp[0], asmOp[1]); break;
      case OP_X(Sqrtf): emit2x(x86::Inst::kIdSqrtps, asmOp[0], asmOp[1]); break;
      case OP_1(Sqrtd): emit2x(x86::Inst::kIdSqrtsd, asmOp[0], asmOp[1]); break;
      case OP_X(Sqrtd): emit2x(x86::Inst::kIdSqrtpd, asmOp[0], asmOp[1]); break;

      case OP_1(Cmpeqf): emit3f(x86::Inst::kIdCmpss, asmOp[0], asmOp[1], asmOp[2], x86::Predicate::kCmpEQ); break;
      case OP_X(Cmpeqf): emit3f(x86::Inst::kIdCmpps, asmOp[0], asmOp[1], asmOp[2], x86::Predicate::kCmpEQ); break;
      case OP_1(Cmpeqd): emit3d(x86::Inst::kIdCmpsd, asmOp[0], asmOp[1], asmOp[2], x86::Predicate::kCmpEQ); break;
      case OP_X(Cmpeqd): emit3d(x86::Inst::kIdCmppd, asmOp[0], asmOp[1], asmOp[2], x86::Predicate::kCmpEQ); break;

      case OP_1(Cmpnef): emit3f(x86::Inst::kIdCmpss, asmOp[0], asmOp[1], asmOp[2], x86::Predicate::kCmpNEQ); break;
      case OP_X(Cmpnef): emit3f(x86::Inst::kIdCmpps, asmOp[0], asmOp[1], asmOp[2], x86::Predicate::kCmpNEQ); break;
      case OP_1(Cmpned): emit3d(x86::Inst::kIdCmpsd, asmOp[0], asmOp[1], asmOp[2], x86::Predicate::kCmpNEQ); break;
      case OP_X(Cmpned): emit3d(x86::Inst::kIdCmppd, asmOp[0], asmOp[1], asmOp[2], x86::Predicate::kCmpNEQ); break;

      case OP_1(Cmpltf): emit3f(x86::Inst::kIdCmpss, asmOp[0], asmOp[1], asmOp[2], x86::Predicate::kCmpLT); break;
      case OP_X(Cmpltf): emit3f(x86::Inst::kIdCmpps, asmOp[0], asmOp[1], asmOp[2], x86::Predicate::kCmpLT); break;
      case OP_1(Cmpltd): emit3d(x86::Inst::kIdCmpsd, asmOp[0], asmOp[1], asmOp[2], x86::Predicate::kCmpLT); break;
      case OP_X(Cmpltd): emit3d(x86::Inst::kIdCmppd, asmOp[0], asmOp[1], asmOp[2], x86::Predicate::kCmpLT); break;

      case OP_1(Cmplef): emit3f(x86::Inst::kIdCmpss, asmOp[0], asmOp[1], asmOp[2], x86::Predicate::kCmpLE); break;
      case OP_X(Cmplef): emit3f(x86::Inst::kIdCmpps, asmOp[0], asmOp[1], asmOp[2], x86::Predicate::kCmpLE); break;
      case OP_1(Cmpled): emit3d(x86::Inst::kIdCmpsd, asmOp[0], asmOp[1], asmOp[2], x86::Predicate::kCmpLE); break;
      case OP_X(Cmpled): emit3d(x86::Inst::kIdCmppd, asmOp[0], asmOp[1], asmOp[2], x86::Predicate::kCmpLE); break;

      case OP_1(Cmpgtf): emit3f(x86::Inst::kIdCmpss, asmOp[0], asmOp[2], asmOp[1], x86::Predicate::kCmpLE); break;
      case OP_X(Cmpgtf): emit3f(x86::Inst::kIdCmpps, asmOp[0], asmOp[2], asmOp[1], x86::Predicate::kCmpLE); break;
      case OP_1(Cmpgtd): emit3d(x86::Inst::kIdCmpsd, asmOp[0], asmOp[2], asmOp[1], x86::Predicate::kCmpLE); break;
      case OP_X(Cmpgtd): emit3d(x86::Inst::kIdCmppd, asmOp[0], asmOp[2], asmOp[1], x86::Predicate::kCmpLE); break;

      case OP_1(Cmpgef): emit3f(x86::Inst::kIdCmpss, asmOp[0], asmOp[2], asmOp[1], x86::Predicate::kCmpLT); break;
      case OP_X(Cmpgef): emit3f(x86::Inst::kIdCmpps, asmOp[0], asmOp[2], asmOp[1], x86::Predicate::kCmpLT); break;
      case OP_1(Cmpged): emit3d(x86::Inst::kIdCmpsd, asmOp[0], asmOp[2], asmOp[1], x86::Predicate::kCmpLT); break;
      case OP_X(Cmpged): emit3d(x86::Inst::kIdCmppd, asmOp[0], asmOp[2], asmOp[1], x86::Predicate::kCmpLT); break;

      case OP_1(Pshufd):
      case OP_X(Pshufd): emit3d(x86::Inst::kIdPshufd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pmovsxbw):
      case OP_X(Pmovsxbw): emit3i(x86::Inst::kIdPmovsxbw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmovzxbw):
      case OP_X(Pmovzxbw): emit3i(x86::Inst::kIdPmovzxbw, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pmovsxwd):
      case OP_X(Pmovsxwd): emit3i(x86::Inst::kIdPmovsxwd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmovzxwd):
      case OP_X(Pmovzxwd): emit3i(x86::Inst::kIdPmovzxwd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Packsswb):
      case OP_X(Packsswb): emit3i(x86::Inst::kIdPacksswb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Packuswb):
      case OP_X(Packuswb): emit3i(x86::Inst::kIdPackuswb, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Packssdw):
      case OP_X(Packssdw): emit3i(x86::Inst::kIdPackssdw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Packusdw):
      case OP_X(Packusdw): emit3i(x86::Inst::kIdPackusdw, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Paddb):
      case OP_X(Paddb): emit3i(x86::Inst::kIdPaddb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Paddw):
      case OP_X(Paddw): emit3i(x86::Inst::kIdPaddw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Paddd): emit3i(x86::Inst::kIdAdd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Paddd): emit3i(x86::Inst::kIdPaddd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Paddq):
      case OP_X(Paddq): emit3i(x86::Inst::kIdPaddq, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Paddssb):
      case OP_X(Paddssb): emit3i(x86::Inst::kIdPaddsb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Paddusb):
      case OP_X(Paddusb): emit3i(x86::Inst::kIdPaddusb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Paddssw):
      case OP_X(Paddssw): emit3i(x86::Inst::kIdPaddsw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Paddusw):
      case OP_X(Paddusw): emit3i(x86::Inst::kIdPaddusw, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Psubb):
      case OP_X(Psubb): emit3i(x86::Inst::kIdPsubb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psubw):
      case OP_X(Psubw): emit3i(x86::Inst::kIdPsubw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psubd): emit3i(x86::Inst::kIdSub, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Psubd): emit3i(x86::Inst::kIdPsubd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psubq):
      case OP_X(Psubq): emit3i(x86::Inst::kIdPsubq, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psubssb):
      case OP_X(Psubssb): emit3i(x86::Inst::kIdPsubsb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psubusb):
      case OP_X(Psubusb): emit3i(x86::Inst::kIdPsubusb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psubssw):
      case OP_X(Psubssw): emit3i(x86::Inst::kIdPsubsw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psubusw):
      case OP_X(Psubusw): emit3i(x86::Inst::kIdPsubusw, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pmulw):
      case OP_X(Pmulw): emit3i(x86::Inst::kIdPmullw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmulhsw):
      case OP_X(Pmulhsw): emit3i(x86::Inst::kIdPmulhw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmulhuw):
      case OP_X(Pmulhuw): emit3i(x86::Inst::kIdPmulhuw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmuld): emit3i(x86::Inst::kIdImul, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_X(Pmuld): emit3i(x86::Inst::kIdPmulld, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pminsb):
      case OP_X(Pminsb): emit3i(x86::Inst::kIdPminsb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pminub):
      case OP_X(Pminub): emit3i(x86::Inst::kIdPminub, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pminsw):
      case OP_X(Pminsw): emit3i(x86::Inst::kIdPminsw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pminuw):
      case OP_X(Pminuw): emit3i(x86::Inst::kIdPminuw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pminsd):
      case OP_X(Pminsd): emit3i(x86::Inst::kIdPminsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pminud):
      case OP_X(Pminud): emit3i(x86::Inst::kIdPminud, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pmaxsb):
      case OP_X(Pmaxsb): emit3i(x86::Inst::kIdPmaxsb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmaxub):
      case OP_X(Pmaxub): emit3i(x86::Inst::kIdPmaxub, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmaxsw):
      case OP_X(Pmaxsw): emit3i(x86::Inst::kIdPmaxsw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmaxuw):
      case OP_X(Pmaxuw): emit3i(x86::Inst::kIdPmaxuw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmaxsd):
      case OP_X(Pmaxsd): emit3i(x86::Inst::kIdPmaxsd, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pmaxud):
      case OP_X(Pmaxud): emit3i(x86::Inst::kIdPmaxud, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Psllw):
      case OP_X(Psllw): emit3i(x86::Inst::kIdPsllw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psrlw):
      case OP_X(Psrlw): emit3i(x86::Inst::kIdPsrlw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psraw):
      case OP_X(Psraw): emit3i(x86::Inst::kIdPsraw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pslld):
      case OP_X(Pslld): emit3i(x86::Inst::kIdPslld, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psrld):
      case OP_X(Psrld): emit3i(x86::Inst::kIdPsrld, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psrad):
      case OP_X(Psrad): emit3i(x86::Inst::kIdPsrad, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psllq):
      case OP_X(Psllq): emit3i(x86::Inst::kIdPsllq, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Psrlq):
      case OP_X(Psrlq): emit3i(x86::Inst::kIdPsrlq, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pmaddwd):
      case OP_X(Pmaddwd): emit3i(x86::Inst::kIdPmaddwd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pcmpeqb):
      case OP_X(Pcmpeqb): emit3i(x86::Inst::kIdPcmpeqb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pcmpeqw):
      case OP_X(Pcmpeqw): emit3i(x86::Inst::kIdPcmpeqw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pcmpeqd):
      case OP_X(Pcmpeqd): emit3i(x86::Inst::kIdPcmpeqd, asmOp[0], asmOp[1], asmOp[2]); break;

      case OP_1(Pcmpgtb):
      case OP_X(Pcmpgtb): emit3i(x86::Inst::kIdPcmpgtb, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pcmpgtw):
      case OP_X(Pcmpgtw): emit3i(x86::Inst::kIdPcmpgtw, asmOp[0], asmOp[1], asmOp[2]); break;
      case OP_1(Pcmpgtd):
      case OP_X(Pcmpgtd): emit3i(x86::Inst::kIdPcmpgtd, asmOp[0], asmOp[1], asmOp[2]); break;

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
    case x86::Inst::kIdPmulld: {
      if (_enableSSE4_1)
        break;

      if (o0.id() != o1.id())
        _cc->emit(x86::Inst::kIdMovaps, o0, o1);

      _cc->emit(x86::Inst::kIdPshufd, _tmpXmm0, o1, x86::Predicate::shuf(2, 3, 0, 1));
      _cc->emit(x86::Inst::kIdPshufd, _tmpXmm1, o2, x86::Predicate::shuf(2, 3, 0, 1));

      _cc->emit(x86::Inst::kIdPmuludq, _tmpXmm0, _tmpXmm1);
      _cc->emit(x86::Inst::kIdPmuludq, o0, o2);
      _cc->emit(x86::Inst::kIdShufps, o0, _tmpXmm0, x86::Predicate::shuf(0, 2, 0, 2));
      return;
    }

    case x86::Inst::kIdPackusdw: {
      // TODO:
    }
  }

  // Instruction `instId` is emitted as is.
  if (o0.id() != o1.id()) {
    if (x86::Reg::isGp(o0) && x86::Reg::isGp(o1))
      _cc->emit(x86::Inst::kIdMov, o0, o1);
    else
      _cc->emit(x86::Inst::kIdMovaps, o0, o1);
  }
  _cc->emit(instId, o0, o2);
}

void IRToX86::emit3f(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2) {
  if (o0.id() != o1.id())
    _cc->emit(o1.isReg() ? x86::Inst::kIdMovaps : x86::Inst::kIdMovss, o0, o1);
  _cc->emit(instId, o0, o2);
}

void IRToX86::emit3f(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2, int imm) {
  if (o0.id() != o1.id())
    _cc->emit(o1.isReg() ? x86::Inst::kIdMovaps : x86::Inst::kIdMovss, o0, o1);
  _cc->emit(instId, o0, o2, imm);
}

void IRToX86::emit3d(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2) {
  if (o0.id() != o1.id())
    _cc->emit(o1.isReg() ? x86::Inst::kIdMovapd : x86::Inst::kIdMovsd, o0, o1);
  _cc->emit(instId, o0, o2);
}

void IRToX86::emit3d(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2, int imm) {
  if (o0.id() != o1.id())
    _cc->emit(o1.isReg() ? x86::Inst::kIdMovapd : x86::Inst::kIdMovsd, o0, o1);
  _cc->emit(instId, o0, o2, imm);
}

x86::Gp IRToX86::varAsPtr(IRReg* irVar) {
  uint32_t id = irVar->jitId();
  MPSL_ASSERT(id != kInvalidRegId);

  return _cc->gpz(id);
}

x86::Gp IRToX86::varAsI32(IRReg* irVar) {
  uint32_t id = irVar->jitId();

  if (id == kInvalidRegId) {
    x86::Gp gp = _cc->newI32("%%%u", irVar->id());
    irVar->setJitId(gp.id());
    return gp;
  }
  else {
    return x86::gpd(id);
  }
}

x86::Xmm IRToX86::varAsXmm(IRReg* irVar) {
  uint32_t id = irVar->jitId();

  if (id == kInvalidRegId) {
    x86::Xmm xmm = _cc->newXmm("%%%u", irVar->id());
    irVar->setJitId(xmm.id());
    return xmm;
  }
  else {
    return x86::xmm(id);
  }
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
