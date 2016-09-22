// [MPSL-Test]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Dependencies]
#include "./mpsl.h"
#include "./mp_utils.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

struct Args {
  mpsl::Int4 bg, fg;
  mpsl::Int4 alpha;
  mpsl::Int4 result;
};

int main(int argc, char* argv[]) {
  mpsl::Context context = mpsl::Context::create();

  mpsl::LayoutTmp<> args;
  args.addMember("bg"   , mpsl::kTypeInt4 | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, bg));
  args.addMember("fg"   , mpsl::kTypeInt4 | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, fg));
  args.addMember("alpha", mpsl::kTypeInt4 | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, alpha));
  args.addMember("@ret" , mpsl::kTypeInt4 | mpsl::kTypeWO, MPSL_OFFSET_OF(Args, result));

  const char body[] =
    "int4 main() {\n"
    "  const int inv = 0x01000100;\n"
    "  int4 x = pmulw(bg, psubw(inv, alpha));\n"
    "  int4 y = pmulw(fg, alpha);\n"
    "  return psrlw(paddw(x, y), 8);\n"
    "}";

  printf("[Program]\n%s\n", body);
  uint32_t options = 
    mpsl::kOptionVerbose  |
    mpsl::kOptionDebugAST |
    mpsl::kOptionDebugIR  |
    mpsl::kOptionDebugASM ;
  TestLog log;

  mpsl::Program1<> program;
  mpsl::Error err = program.compile(context, body, options, args, &log);

  if (err) {
    printf("Compilation failed: ERROR: 0x%08X\n", static_cast<unsigned int>(err));
  }
  else {
    Args args;
    args.bg.set(0x00200030, 0x00400050, 0x00600070, 0x008000FF);
    args.fg.set(0x00000000);
    args.alpha.set(0x00800080);

    err = program.run(&args);
    if (err)
      printf("Execution failed: ERROR %08X\n", static_cast<unsigned int>(err));
    else
      printf("Return=%08X %08X %08X %08X\n", args.result[0], args.result[1], args.result[2], args.result[3]);
  }

  return 0;
}
