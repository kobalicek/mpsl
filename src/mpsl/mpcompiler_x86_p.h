// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPCOMPILER_X86_P_H
#define _MPSL_MPCOMPILER_X86_P_H

// [Dependencies - MPSL]
#include "./mpast_p.h"
#include "./mphash_p.h"
#include "./mpir_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

using asmjit::Label;
using asmjit::Operand;
using asmjit::Var;

using asmjit::X86Compiler;
using asmjit::X86Var;
using asmjit::X86GpVar;
using asmjit::X86MmVar;
using asmjit::X86XmmVar;
using asmjit::X86Mem;
using asmjit::X86Util;

using asmjit::kConstScopeLocal;
using asmjit::kConstScopeGlobal;

// ============================================================================
// [mpsl::JitCompiler]
// ============================================================================

struct JitCompiler {
  MPSL_NO_COPY(JitCompiler)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_NOAPI JitCompiler(Allocator* allocator, X86Compiler* c);
  MPSL_NOAPI ~JitCompiler();

  // --------------------------------------------------------------------------
  // [Const Pool]
  // --------------------------------------------------------------------------

  MPSL_NOAPI void prepareConstPool();

  MPSL_NOAPI X86Mem getConstantU64(uint64_t value);
  MPSL_NOAPI X86Mem getConstantU64AsPD(uint64_t value);
  MPSL_NOAPI X86Mem getConstantD64(double value);
  MPSL_NOAPI X86Mem getConstantD64AsPD(double value);
  MPSL_NOAPI X86Mem getConstantByValue(const Value& value, uint32_t width);

  // --------------------------------------------------------------------------
  // [Compile]
  // --------------------------------------------------------------------------

  MPSL_NOAPI Error compileIRAsFunc(IRBuilder* ir);
  MPSL_NOAPI Error compileIRAsPart(IRBuilder* ir);
  MPSL_NOAPI Error compileConsecutiveBlocks(IRBlock* block);
  MPSL_NOAPI Error compileBasicBlock(IRBlock* block, IRBlock* next);

  MPSL_INLINE void emit2x(uint32_t instId, const Operand& o0, const Operand& o1) { _c->emit(instId, o0, o1); }
  MPSL_NOAPI void emit3i(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2);
  MPSL_NOAPI void emit3f(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2);
  MPSL_NOAPI void emit3f(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2, int imm);
  MPSL_NOAPI void emit3d(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2);
  MPSL_NOAPI void emit3d(uint32_t instId, const Operand& o0, const Operand& o1, const Operand& o2, int imm);

  MPSL_NOAPI X86GpVar varAsPtr(IRVar* irVar);
  MPSL_NOAPI X86GpVar varAsI32(IRVar* irVar);
  MPSL_NOAPI X86XmmVar varAsXmm(IRVar* irVar);

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  Allocator* _allocator;
  X86Compiler* _c;

  X86GpVar _data[Globals::kMaxArgumentsCount];
  X86GpVar _ret;

  asmjit::HLNode* _functionBody;
  asmjit::ConstPool _constPool;
  Label _constLabel;
  X86GpVar _constPtr;

  X86XmmVar _tmpXmm0;
  X86XmmVar _tmpXmm1;

  bool _enableSSE4_1;
};

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPCOMPILER_X86_P_H
