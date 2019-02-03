// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MPSL_EXPORTS

// [Dependencies - MPSL]
#include "./mpirpass_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

static Error mpIRPassBlock(IRBuilder* ir, IRBlock* block) noexcept {
  IRBody& body = block->body();
  size_t i = body.size();

  while (i != 0) {
    IRInst* inst = body[--i];
    if (inst) {
      // TODO: Just testing some concepts.
      const InstInfo& info = mpInstInfo[inst->instCode() & kInstCodeMask];
      if (!info.isStore() && !info.isCall() && !info.isRet()) {
        IRObject** opArray = inst->operands();
        uint32_t opCount = inst->opCount();

        IRObject* dst = opArray[0];
        if (dst->isReg() && dst->refCount() == 1) {
          block->neuterAt(i);
          ir->deleteInst(inst);
        }
      }
    }
  }

  block->fixupAfterNeutering();
  return kErrorOk;
}

Error mpIRPass(IRBuilder* ir) noexcept {
  for (IRBlock* block : ir->blocks())
    MPSL_PROPAGATE(mpIRPassBlock(ir, block));
  return kErrorOk;
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
