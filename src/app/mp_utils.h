// [MPSL-Test]
// Shader-Like Mathematical Expression JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef _TEST_MP_UTILS_H
#define _TEST_MP_UTILS_H

#include "../mpsl/mpsl.h"

struct CustomLog : public mpsl::OutputLog {
  virtual void log(const Info& info) MPSL_NOEXCEPT {
    const mpsl::StringRef& msg = info.getMessage();

    switch (info.getType()) {
      case mpsl::OutputLog::kMessageError:
        printf("ERROR [Line:%u] [Column:%u]\n%s\n", info.getLine(), info.getColumn(), msg.getData());
        break;
      case mpsl::OutputLog::kMessageWarning:
        printf("WARNING [Line:%u] [Column:%u]\n%s\n", info.getLine(), info.getColumn(), msg.getData());
        break;
      case mpsl::OutputLog::kMessageAstInitial:
        printf("[AST-Initial]\n%s\n", msg.getData());
        break;
      case mpsl::OutputLog::kMessageAstFinal:
        printf("[AST-Final]\n%s\n", msg.getData());
        break;
      case mpsl::OutputLog::kMessageIRInitial:
        printf("[IR-Initial]\n%s\n", msg.getData());
        break;
      case mpsl::OutputLog::kMessageIRFinal:
        printf("[IR-Final]\n%s\n", msg.getData());
        break;
      case mpsl::OutputLog::kMessageAsm:
        printf("[MachineCode]\n%s\n", msg.getData());
        break;
    }
  }
};

#endif // _TEST_MP_UTILS_H
