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
  IRInst* inst = block->getFirstChild();
  while (inst != nullptr) {
    IRInst* next = inst->getNext();

    // TODO: Just testing some concepts.
    const InstInfo& info = mpInstInfo[inst->getInstCode() & kInstCodeMask];
    if (!info.isStore() && !info.isCall() && !info.isRet()) {
      IRObject** opArray = inst->getOpArray();
      uint32_t opCount = inst->getOpCount();

      IRObject* dst = opArray[0];
      if (dst->isVar() && dst->_usageCount == 1) {
        block->remove(inst);
        ir->deleteObject(inst);
      }
    }

    inst = next;
  }
  return kErrorOk;
}

Error mpIRPass(IRBuilder* ir) noexcept {
  IRBlock* block = ir->_blockFirst;
  while (block != nullptr) {
    MPSL_PROPAGATE(mpIRPassBlock(ir, block));
    block = block->_nextBlock;
  }
  return kErrorOk;
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
