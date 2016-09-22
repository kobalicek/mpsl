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

// ============================================================================
// [CmdLine]
// ============================================================================

class CmdLine {
public:
  CmdLine(int argc, const char* const* argv)
    : argc(argc),
      argv(argv) {}

  bool hasKey(const char* key) const {
    for (int i = 0; i < argc; i++)
      if (::strcmp(argv[i], key) == 0)
        return true;
    return false;
  }

  int argc;
  const char* const* argv;
};

// ============================================================================
// [TestUtils]
// ============================================================================

struct TestUtils {
  static bool isVerbose(uint32_t options) {
    const uint32_t kVerboseMask =
      mpsl::kOptionVerbose  |
      mpsl::kOptionDebugAST |
      mpsl::kOptionDebugIR  |
      mpsl::kOptionDebugASM ;
    return (options & kVerboseMask) != 0;
  }

  static void printCode(const char* prefix, const char* body) {
    size_t prefixLen = ::strlen(prefix);

    size_t bodyStart = 0;
    size_t bodyLen = ::strlen(body);

    bool needsIndent = false;
    char indent[2] = " ";

    size_t i = 0;
    printf("%s", prefix);

    if (bodyLen && body[bodyLen - 1] == '\n')
      bodyLen--;

    for (;;) {
      if (i == bodyLen || body[i] == '\n') {
        if (needsIndent) {
          for (size_t j = 0; j < prefixLen; j++)
            printf(indent);
        }

        printf("%.*s\n", int(i - bodyStart), body + bodyStart);
        if (i == bodyLen) return;

        bodyStart = ++i;
        needsIndent = true;
      }
      else {
        i++;
      }
    }
  }
};

// ============================================================================
// [Helpers]
// ============================================================================

static mpsl::Value makeIVal(int x, int y = 0, int z = 0, int w = 0) {
  mpsl::Value v; v.i.set(x, y, z, w, 0, 0, 0, 0); return v;
}

static mpsl::Value makeFVal(float x, float y = 0.0f, float z = 0.0f, float w = 0.0f) {
  mpsl::Value v; v.f.set(x, y, z, w, 0, 0, 0, 0); return v;
}

static mpsl::Value makeDVal(double x, double y = 0.0, double z = 0.0, double w = 0.0) {
  mpsl::Value v; v.d.set(x, y, z, w); return v;
}

// ============================================================================
// [Test]
// ============================================================================

class Test {
public:
  struct Args {
    int ia, ib, ic;
    mpsl::Int2 i2a, i2b, i2c;
    mpsl::Int3 i3a, i3b, i3c;
    mpsl::Int4 i4a, i4b, i4c;

    float fa, fb, fc;
    mpsl::Float2 f2a, f2b, f2c;
    mpsl::Float3 f3a, f3b, f3c;
    mpsl::Float4 f4a, f4b, f4c;

    double da, db, dc;
    mpsl::Double2 d2a, d2b, d2c;
    mpsl::Double3 d3a, d3b, d3c;
    mpsl::Double4 d4a, d4b, d4c;

    mpsl::Value ret;
  };

  Test();

  void initLayout(mpsl::Layout& layout, uint32_t retType);
  void initArgs(Args& args);

  void printTest(const char* body, uint32_t options);
  void printPass(const char* body, uint32_t options);
  void printFail(const char* body, uint32_t options, const char* fmt, ...);

  bool basicTest(const char* body, uint32_t options, uint32_t retType, const mpsl::Value& retValue);
  bool failureTest(const char* body, uint32_t options);

  mpsl::Context _context;
  int a[4];
  int b[4];
  int c[4];
  bool _succeeded;
};

Test::Test()
  : _context(mpsl::Context::create()),
    _succeeded(true) {
  a[0] = 1; a[1] = 2; a[2] = 3; a[3] = 4;
  b[0] = 9; b[1] = 8; b[2] = 7; b[3] = 6;
  c[0] =-2; c[1] =-3; c[2] = 4; c[3] = 5;
}

void Test::initLayout(mpsl::Layout& layout, uint32_t retType) {
  layout.addMember("ia" , mpsl::kTypeInt     | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, ia));
  layout.addMember("ib" , mpsl::kTypeInt     | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, ib));
  layout.addMember("ic" , mpsl::kTypeInt     | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, ic));

  layout.addMember("i2a", mpsl::kTypeInt2    | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, i2a));
  layout.addMember("i2b", mpsl::kTypeInt2    | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, i2b));
  layout.addMember("i2c", mpsl::kTypeInt2    | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, i2c));

  layout.addMember("i3a", mpsl::kTypeInt3    | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, i3a));
  layout.addMember("i3b", mpsl::kTypeInt3    | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, i3b));
  layout.addMember("i3c", mpsl::kTypeInt3    | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, i3c));

  layout.addMember("i4a", mpsl::kTypeInt4    | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, i4a));
  layout.addMember("i4b", mpsl::kTypeInt4    | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, i4b));
  layout.addMember("i4c", mpsl::kTypeInt4    | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, i4c));

  layout.addMember("fa" , mpsl::kTypeFloat   | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, fa));
  layout.addMember("fb" , mpsl::kTypeFloat   | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, fb));
  layout.addMember("fc" , mpsl::kTypeFloat   | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, fc));

  layout.addMember("f2a", mpsl::kTypeFloat2  | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, f2a));
  layout.addMember("f2b", mpsl::kTypeFloat2  | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, f2b));
  layout.addMember("f2c", mpsl::kTypeFloat2  | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, f2c));

  layout.addMember("f3a", mpsl::kTypeFloat3  | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, f3a));
  layout.addMember("f3b", mpsl::kTypeFloat3  | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, f3b));
  layout.addMember("f3c", mpsl::kTypeFloat3  | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, f3c));

  layout.addMember("f4a", mpsl::kTypeFloat4  | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, f4a));
  layout.addMember("f4b", mpsl::kTypeFloat4  | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, f4b));
  layout.addMember("f4c", mpsl::kTypeFloat4  | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, f4c));

  layout.addMember("da" , mpsl::kTypeDouble  | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, da));
  layout.addMember("db" , mpsl::kTypeDouble  | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, db));
  layout.addMember("dc" , mpsl::kTypeDouble  | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, dc));

  layout.addMember("d2a", mpsl::kTypeDouble2 | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, d2a));
  layout.addMember("d2b", mpsl::kTypeDouble2 | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, d2b));
  layout.addMember("d2c", mpsl::kTypeDouble2 | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, d2c));

  layout.addMember("d3a", mpsl::kTypeDouble3 | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, d3a));
  layout.addMember("d3b", mpsl::kTypeDouble3 | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, d3b));
  layout.addMember("d3c", mpsl::kTypeDouble3 | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, d3c));

  layout.addMember("d4a", mpsl::kTypeDouble4 | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, d4a));
  layout.addMember("d4b", mpsl::kTypeDouble4 | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, d4b));
  layout.addMember("d4c", mpsl::kTypeDouble4 | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, d4c));

  layout.addMember("@ret", retType, MPSL_OFFSET_OF(Args, ret));
}

void Test::initArgs(Args& args) {
  args.ia = a[0];
  args.ib = b[0];
  args.ic = c[0];
  args.i2a.set(a[0], a[1]);
  args.i2b.set(b[0], b[1]);
  args.i2c.set(c[0], c[1]);
  args.i3a.set(a[0], a[1], a[2]);
  args.i3b.set(b[0], b[1], b[2]);
  args.i3c.set(c[0], c[1], c[2]);
  args.i4a.set(a[0], a[1], a[2], a[3]);
  args.i4b.set(b[0], b[1], b[2], b[3]);
  args.i4c.set(c[0], c[1], c[2], c[3]);

  args.fa = float(a[0]);
  args.fb = float(b[0]);
  args.fc = float(c[0]);
  args.f2a.set(float(a[0]), float(a[1]));
  args.f2b.set(float(b[0]), float(b[1]));
  args.f2c.set(float(c[0]), float(c[1]));
  args.f3a.set(float(a[0]), float(a[1]), float(a[2]));
  args.f3b.set(float(b[0]), float(b[1]), float(b[2]));
  args.f3c.set(float(c[0]), float(c[1]), float(c[2]));
  args.f4a.set(float(a[0]), float(a[1]), float(a[2]), float(a[3]));
  args.f4b.set(float(b[0]), float(b[1]), float(b[2]), float(b[3]));
  args.f4c.set(float(c[0]), float(c[1]), float(c[2]), float(c[3]));

  args.da = double(a[0]);
  args.db = double(b[0]);
  args.dc = double(c[0]);
  args.d2a.set(double(a[0]), double(a[1]));
  args.d2b.set(double(b[0]), double(b[1]));
  args.d2c.set(double(c[0]), double(c[1]));
  args.d3a.set(double(a[0]), double(a[1]), double(a[2]));
  args.d3b.set(double(b[0]), double(b[1]), double(b[2]));
  args.d3c.set(double(c[0]), double(c[1]), double(c[2]));
  args.d4a.set(double(a[0]), double(a[1]), double(a[2]), double(a[3]));
  args.d4b.set(double(b[0]), double(b[1]), double(b[2]), double(b[3]));
  args.d4c.set(double(c[0]), double(c[1]), double(c[2]), double(c[3]));
}

void Test::printTest(const char* body, uint32_t options) {
  if (TestUtils::isVerbose(options))
    TestUtils::printCode("[TEST] ", body);
}

void Test::printPass(const char* body, uint32_t options) {
  if (!TestUtils::isVerbose(options)) {
    TestUtils::printCode("[PASS] ", body);
  }
}

void Test::printFail(const char* body, uint32_t options, const char* fmt, ...) {
  if (TestUtils::isVerbose(options)) {
    printf("[FAIL] ");

    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
  }
  else {
    TestUtils::printCode("[FAIL] ", body);
  }
}

bool Test::basicTest(const char* body, uint32_t options, uint32_t retType, const mpsl::Value& retValue) {
  mpsl::LayoutTmp<1024> layout;
  Args args;

  initLayout(layout, retType);
  initArgs(args);
  printTest(body, options);

  TestLog log;
  mpsl::Program1<Args> program;
  mpsl::Error err = program.compile(_context, body, options, layout, &log);

  if (err != mpsl::kErrorOk) {
    printFail(body, options, "COMPILATION ERROR 0x%08X.\n", static_cast<unsigned int>(err));
    return false;
  }

  err = program.run(&args);
  if (err != mpsl::kErrorOk) {
    printFail(body, options, "EXECUTION ERROR 0x%08X.\n", static_cast<unsigned int>(err));
    return false;
  }

  bool isOk = true;
  unsigned int i, n;

  switch (retType) {
    case mpsl::kTypeInt : n = 1; goto checkInt;
    case mpsl::kTypeInt2: n = 2; goto checkInt;
    case mpsl::kTypeInt3: n = 3; goto checkInt;
    case mpsl::kTypeInt4: n = 4; goto checkInt;
checkInt:
      for (i = 0; i < n; i++) {
        int x = args.ret.i[i];
        int y = retValue.i[i];

        if (x != y) {
          printf("[FAIL] ic[%u] %d != Expected(%d)\n", i, x, y);
          isOk = false;
        }
      }
      break;

    case mpsl::kTypeFloat : n = 1; goto checkFloat;
    case mpsl::kTypeFloat2: n = 2; goto checkFloat;
    case mpsl::kTypeFloat3: n = 3; goto checkFloat;
    case mpsl::kTypeFloat4: n = 4; goto checkFloat;
checkFloat:
      for (i = 0; i < n; i++) {
        float x = args.ret.f[i];
        float y = retValue.f[i];

        if (x != y) {
          printf("[FAIL] fc[%u] %g != Expected(%g)\n", i, x, y);
          isOk = false;
        }
      }
      break;

    case mpsl::kTypeDouble : n = 1; goto checkDouble;
    case mpsl::kTypeDouble2: n = 2; goto checkDouble;
    case mpsl::kTypeDouble3: n = 3; goto checkDouble;
    case mpsl::kTypeDouble4: n = 4; goto checkDouble;
checkDouble:
      for (i = 0; i < n; i++) {
        double x = args.ret.d[i];
        double y = retValue.d[i];

        if (x != y) {
          printf("[FAIL] dc[%u] %g != Expected(%g)\n", i, x, y);
          isOk = false;
        }
      }
      break;
  }

  if (isOk)
    printPass(body, options);
  else
    _succeeded = false;
  return isOk;
}

bool Test::failureTest(const char* body, uint32_t options) {

  return true;
}

// ============================================================================
// [Main]
// ============================================================================

int main(int argc, char* argv[]) {
  CmdLine cmd(argc, argv);
  uint32_t options = 0;

  if (cmd.hasKey("--verbose")) options |= mpsl::kOptionVerbose;
  if (cmd.hasKey("--ast"    )) options |= mpsl::kOptionDebugAST;
  if (cmd.hasKey("--ir"     )) options |= mpsl::kOptionDebugIR;
  if (cmd.hasKey("--asm"    )) options |= mpsl::kOptionDebugASM;

  Test test;

  // Variables are initialized to these:
  //   a[0] = 1; a[1] = 2; a[2] = 3; a[3] = 4;
  //   b[0] = 9; b[1] = 8; b[2] = 7; b[3] = 6;
  //   c[0] =-2; c[1] =-3; c[2] =-4; c[3] =-5;

  // Test MPSL basics.
  test.basicTest("int     main() { return ia + ib; }", options, mpsl::kTypeInt    , makeIVal(10   ));
  test.basicTest("float   main() { return fa + fb; }", options, mpsl::kTypeFloat  , makeFVal(10.0f));
  test.basicTest("double  main() { return da + db; }", options, mpsl::kTypeDouble , makeDVal(10.0 ));

  test.basicTest("int2    main() { return i2a + i2b; }", options, mpsl::kTypeInt2   , makeIVal(10   , 10   ));
  test.basicTest("float2  main() { return f2a + f2b; }", options, mpsl::kTypeFloat2 , makeFVal(10.0f, 10.0f));
  test.basicTest("double2 main() { return d2a + d2b; }", options, mpsl::kTypeDouble2, makeDVal(10.0 , 10.0 ));

  test.basicTest("int3    main() { return i3a + i3b; }", options, mpsl::kTypeInt3   , makeIVal(10   , 10   , 10   ));
  test.basicTest("float3  main() { return f3a + f3b; }", options, mpsl::kTypeFloat3 , makeFVal(10.0f, 10.0f, 10.0f));
  test.basicTest("double3 main() { return d3a + d3b; }", options, mpsl::kTypeDouble3, makeDVal(10.0 , 10.0 , 10.0 ));

  test.basicTest("int4    main() { return i4a + i4b; }", options, mpsl::kTypeInt4   , makeIVal(10   , 10   , 10   , 10   ));
  test.basicTest("float4  main() { return f4a + f4b; }", options, mpsl::kTypeFloat4 , makeFVal(10.0f, 10.0f, 10.0f, 10.0f));
  test.basicTest("double4 main() { return d4a + d4b; }", options, mpsl::kTypeDouble4, makeDVal(10.0 , 10.0 , 10.0 , 10.0 ));

  test.basicTest("int     main() { return (ia + ib) * ic - ia; }", options, mpsl::kTypeInt    , makeIVal(-21   ));
  test.basicTest("float   main() { return (fa + fb) * fc - fa; }", options, mpsl::kTypeFloat  , makeFVal(-21.0f));
  test.basicTest("double  main() { return (da + db) * dc - da; }", options, mpsl::kTypeDouble , makeDVal(-21.0 ));

  test.basicTest("int2    main() { return (i2a + i2b) * i2c - i2a; }", options, mpsl::kTypeInt2   , makeIVal(-21   , -32   ));
  test.basicTest("float2  main() { return (f2a + f2b) * f2c - f2a; }", options, mpsl::kTypeFloat2 , makeFVal(-21.0f, -32.0f));
  test.basicTest("double2 main() { return (d2a + d2b) * d2c - d2a; }", options, mpsl::kTypeDouble2, makeDVal(-21.0 , -32.0 ));

  test.basicTest("int3    main() { return (i3a + i3b) * i3c - i3a; }", options, mpsl::kTypeInt3   , makeIVal(-21   , -32   , 37   ));
  test.basicTest("float3  main() { return (f3a + f3b) * f3c - f3a; }", options, mpsl::kTypeFloat3 , makeFVal(-21.0f, -32.0f, 37.0f));
  test.basicTest("double3 main() { return (d3a + d3b) * d3c - d3a; }", options, mpsl::kTypeDouble3, makeDVal(-21.0 , -32.0 , 37.0 ));

  test.basicTest("int4    main() { return (i4a + i4b) * i4c - i4a; }", options, mpsl::kTypeInt4   , makeIVal(-21   , -32   , 37   , 46   ));
  test.basicTest("float4  main() { return (f4a + f4b) * f4c - f4a; }", options, mpsl::kTypeFloat4 , makeFVal(-21.0f, -32.0f, 37.0f, 46.0f));
  test.basicTest("double4 main() { return (d4a + d4b) * d4c - d4a; }", options, mpsl::kTypeDouble4, makeDVal(-21.0 , -32.0 , 37.0 , 46.0 ));

  // Test creating and calling functions inside the shader.
  test.basicTest("int dummy(int a, int b) { return a + b; }\n"
                 "int main() { return dummy(1, 2); }\n",
                 options, mpsl::kTypeInt, makeIVal(3));

  test.basicTest("int dummy(int a, int b) { return a + b; }\n"
                 "int main() { return dummy(ia, ib); }\n",
                 options, mpsl::kTypeInt, makeIVal(10));

  test.basicTest("int xFunc(int a, int b) { return a + b; }\n"
                 "int yFunc(int a, int b) { return xFunc(a, b); }\n"
                 "int main() { return yFunc(ia, ib); }\n",
                 options, mpsl::kTypeInt, makeIVal(10));

  test.basicTest("int xFunc(int a, int b) { return a + b; }\n"
                 "int yFunc(int a, int b) { return xFunc(a, b); }\n"
                 "int main() { return xFunc(ia, ib) + yFunc(ia, ic) + yFunc(ib, ic); }\n",
                 options, mpsl::kTypeInt, makeIVal(16));

  return test._succeeded ? 0 : 1;
}
