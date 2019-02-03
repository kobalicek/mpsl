// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPIRTOX86_P_H
#define _MPSL_MPIRTOX86_P_H

// [Dependencies - MPSL]
#include "./mpast_p.h"
#include "./mphash_p.h"
#include "./mpir_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

namespace x86 = asmjit::x86;

using asmjit::BaseNode;
using asmjit::ConstPool;
using asmjit::Label;
using asmjit::Operand;

// ============================================================================
// [mpsl::IRToX86]
// ============================================================================

class IRToX86 {
public:
  MPSL_NONCOPYABLE(IRToX86)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  IRToX86(ZoneAllocator* allocator, x86::Compiler* cc);
  ~IRToX86();

  // --------------------------------------------------------------------------
  // [Const Pool]
  // --------------------------------------------------------------------------

  void prepareConstPool();

  x86::Mem getConstantU64(uint64_t value);
  x86::Mem getConstantU64AsPD(uint64_t value);
  x86::Mem getConstantD64(double value);
  x86::Mem getConstantD64AsPD(double value);
  x86::Mem getConstantByValue(const Value& value, uint32_t width);

  // --------------------------------------------------------------------------
  // [Compile]
  // --------------------------------------------------------------------------

  Error compileIRAsFunc(IRBuilder* ir);
  Error compileIRAsPart(IRBuilder* ir);
  Error compileConsecutiveBlocks(IRBlock* block);
  Error compileBasicBlock(IRBlock* block, IRBlock* next);

  MPSL_INLINE void emit2x(uint32_t instId, const Operand& o0, const Operand& o1) { _cc->emit(instId, o0, o1); }
  void emit3i(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2);
  void emit3f(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2);
  void emit3f(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2, int imm);
  void emit3d(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2);
  void emit3d(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2, int imm);

  x86::Gp varAsPtr(IRReg* irVar);
  x86::Gp varAsI32(IRReg* irVar);
  x86::Xmm varAsXmm(IRReg* irVar);

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  ZoneAllocator* _allocator;
  x86::Compiler* _cc;

  x86::Gp _data[Globals::kMaxArgumentsCount];
  x86::Gp _ret;

  BaseNode* _functionBody;
  ConstPool _constPool;
  Label _constLabel;
  x86::Gp _constPtr;

  x86::Xmm _tmpXmm0;
  x86::Xmm _tmpXmm1;

  bool _enableSSE4_1;
};

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPIRTOX86_P_H
