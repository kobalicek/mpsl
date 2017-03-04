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

using asmjit::Label;
using asmjit::Operand;

using asmjit::X86Compiler;
using asmjit::X86Reg;
using asmjit::X86Gp;
using asmjit::X86Mm;
using asmjit::X86Xmm;
using asmjit::X86Mem;

using asmjit::kConstScopeLocal;
using asmjit::kConstScopeGlobal;

// ============================================================================
// [mpsl::IRToX86]
// ============================================================================

class IRToX86 {
public:
  MPSL_NONCOPYABLE(IRToX86)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  IRToX86(ZoneHeap* heap, X86Compiler* cc);
  ~IRToX86();

  // --------------------------------------------------------------------------
  // [Const Pool]
  // --------------------------------------------------------------------------

  void prepareConstPool();

  X86Mem getConstantU64(uint64_t value);
  X86Mem getConstantU64AsPD(uint64_t value);
  X86Mem getConstantD64(double value);
  X86Mem getConstantD64AsPD(double value);
  X86Mem getConstantByValue(const Value& value, uint32_t width);

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

  X86Gp varAsPtr(IRReg* irVar);
  X86Gp varAsI32(IRReg* irVar);
  X86Xmm varAsXmm(IRReg* irVar);

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  ZoneHeap* _heap;
  X86Compiler* _cc;

  X86Gp _data[Globals::kMaxArgumentsCount];
  X86Gp _ret;

  asmjit::CBNode* _functionBody;
  asmjit::ConstPool _constPool;
  Label _constLabel;
  X86Gp _constPtr;

  X86Xmm _tmpXmm0;
  X86Xmm _tmpXmm1;

  bool _enableSSE4_1;
};

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPIRTOX86_P_H
