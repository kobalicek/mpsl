// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPIRPASS_P_H
#define _MPSL_MPIRPASS_P_H

// [Dependencies - MPSL]
#include "./mpir_p.h"
#include "./mplang_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

Error mpIRPass(IRBuilder* ir) noexcept;

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPIRPASS_P_H
