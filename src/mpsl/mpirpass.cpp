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
  IRBody& body = block->getBody();
  for (size_t i = 0, len = body.getLength(); i < len; i++) {
    IRInst* inst = body[i];
    if (inst) {
      // TODO: Just testing some concepts.
      const InstInfo& info = mpInstInfo[inst->getInstCode() & kInstCodeMask];
      if (!info.isStore() && !info.isCall() && !info.isRet()) {
        IRObject** opArray = inst->getOpArray();
        uint32_t opCount = inst->getOpCount();

        IRObject* dst = opArray[0];
        if (dst->isVar() && dst->_usageCount == 1) {
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
  IRBlock* block = ir->_blockFirst;
  while (block) {
    MPSL_PROPAGATE(mpIRPassBlock(ir, block));
    block = block->_nextBlock;
  }
  return kErrorOk;
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
