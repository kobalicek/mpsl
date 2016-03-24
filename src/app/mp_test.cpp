// [MPSL-Test]
// Shader-Like Mathematical Expression JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Dependencies]
#include "../mpsl/mpsl.h"
#include "./mp_utils.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static mpsl::Value makeIVal(int x, int y = 0, int z = 0, int w = 0) {
  mpsl::Value v;
  v.i.x = x;
  v.i.y = y;
  v.i.z = z;
  v.i.w = w;
  return v;
}

static mpsl::Value makeFVal(float x, float y = 0.0f, float z = 0.0f, float w = 0.0f) {
  mpsl::Value v;
  v.f.x = x;
  v.f.y = y;
  v.f.z = z;
  v.f.w = w;
  return v;
}

static mpsl::Value makeDVal(double x, double y = 0.0, double z = 0.0, double w = 0.0) {
  mpsl::Value v;
  v.d.x = x;
  v.d.y = y;
  v.d.z = z;
  v.d.w = w;
  return v;
}

struct Test {
  Test()
    : _isolate(mpsl::Isolate::create()),
      _succeeded(true) {}

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

  bool basicTest(const char* body, uint32_t options, uint32_t retType, const mpsl::Value& retValue) {

    mpsl::LayoutTmp<1024> layout;
    layout.addMember("ia" , mpsl::kTypeInt      | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, ia));
    layout.addMember("ib" , mpsl::kTypeInt      | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, ib));
    layout.addMember("ic" , mpsl::kTypeInt      | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, ic));

    layout.addMember("i2a", mpsl::kTypeInt_2    | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, i2a));
    layout.addMember("i2b", mpsl::kTypeInt_2    | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, i2b));
    layout.addMember("i2c", mpsl::kTypeInt_2    | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, i2c));

    layout.addMember("i3a", mpsl::kTypeInt_3    | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, i3a));
    layout.addMember("i3b", mpsl::kTypeInt_3    | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, i3b));
    layout.addMember("i3c", mpsl::kTypeInt_3    | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, i3c));

    layout.addMember("i4a", mpsl::kTypeInt_4    | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, i4a));
    layout.addMember("i4b", mpsl::kTypeInt_4    | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, i4b));
    layout.addMember("i4c", mpsl::kTypeInt_4    | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, i4c));

    layout.addMember("fa" , mpsl::kTypeFloat    | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, fa));
    layout.addMember("fb" , mpsl::kTypeFloat    | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, fb));
    layout.addMember("fc" , mpsl::kTypeFloat    | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, fc));

    layout.addMember("f2a", mpsl::kTypeFloat_2  | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, f2a));
    layout.addMember("f2b", mpsl::kTypeFloat_2  | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, f2b));
    layout.addMember("f2c", mpsl::kTypeFloat_2  | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, f2c));

    layout.addMember("f3a", mpsl::kTypeFloat_3  | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, f3a));
    layout.addMember("f3b", mpsl::kTypeFloat_3  | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, f3b));
    layout.addMember("f3c", mpsl::kTypeFloat_3  | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, f3c));

    layout.addMember("f4a", mpsl::kTypeFloat_4  | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, f4a));
    layout.addMember("f4b", mpsl::kTypeFloat_4  | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, f4b));
    layout.addMember("f4c", mpsl::kTypeFloat_4  | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, f4c));

    layout.addMember("da" , mpsl::kTypeDouble   | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, da));
    layout.addMember("db" , mpsl::kTypeDouble   | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, db));
    layout.addMember("dc" , mpsl::kTypeDouble   | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, dc));

    layout.addMember("d2a", mpsl::kTypeDouble_2 | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, d2a));
    layout.addMember("d2b", mpsl::kTypeDouble_2 | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, d2b));
    layout.addMember("d2c", mpsl::kTypeDouble_2 | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, d2c));

    layout.addMember("d3a", mpsl::kTypeDouble_3 | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, d3a));
    layout.addMember("d3b", mpsl::kTypeDouble_3 | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, d3b));
    layout.addMember("d3c", mpsl::kTypeDouble_3 | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, d3c));

    layout.addMember("d4a", mpsl::kTypeDouble_4 | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, d4a));
    layout.addMember("d4b", mpsl::kTypeDouble_4 | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, d4b));
    layout.addMember("d4c", mpsl::kTypeDouble_4 | mpsl::kTypeRO, MPSL_OFFSET_OF(Args, d4c));

    layout.addMember("@ret", retType, MPSL_OFFSET_OF(Args, ret));

    Args args;
    args.i4a.set(1, 2, 3, 4);
    args.i4b.set(5, 6, 7, 8);
    args.i4c.set(-1, -1, -1, -1);
    args.i3a.set(args.i4a.x, args.i4a.y, args.i4a.z);
    args.i3b.set(args.i4b.x, args.i4b.y, args.i4b.z);
    args.i3c.set(args.i4c.x, args.i4c.y, args.i4c.z);
    args.i2a.set(args.i4a.x, args.i4a.y);
    args.i2b.set(args.i4b.x, args.i4b.y);
    args.i2c.set(args.i4c.x, args.i4c.y);
    args.ia = args.i4a.x;
    args.ib = args.i4b.x;
    args.ic = args.i4c.x;

    args.f4a.set(1.0f, 2.0f, 3.0f, 4.0f);
    args.f4b.set(5.0f, 6.0f, 7.0f, 8.0f);
    args.f4c.set(-1.0f, -1.0f, -1.0f, -1.0f);
    args.f3a.set(args.f4a.x, args.f4a.y, args.f4a.z);
    args.f3b.set(args.f4b.x, args.f4b.y, args.f4b.z);
    args.f3c.set(args.f4c.x, args.f4c.y, args.f4c.z);
    args.f2a.set(args.f4a.x, args.f4a.y);
    args.f2b.set(args.f4b.x, args.f4b.y);
    args.f2c.set(args.f4c.x, args.f4c.y);
    args.fa = args.f4a.x;
    args.fb = args.f4b.x;
    args.fc = args.f4c.x;

    args.d4a.set(1.0, 2.0, 3.0, 4.0);
    args.d4b.set(5.0, 6.0, 7.0, 8.0);
    args.d4c.set(-1.0, -1.0, -1.0, -1.0);
    args.d3a.set(args.d4a.x, args.d4a.y, args.d4a.z);
    args.d3b.set(args.d4b.x, args.d4b.y, args.d4b.z);
    args.d3c.set(args.d4c.x, args.d4c.y, args.d4c.z);
    args.d2a.set(args.d4a.x, args.d4a.y);
    args.d2b.set(args.d4b.x, args.d4b.y);
    args.d2c.set(args.d4c.x, args.d4c.y);
    args.da = args.d4a.x;
    args.db = args.d4b.x;
    args.dc = args.d4c.x;

    CustomLog log;

    mpsl::Program1<Args> program;
    mpsl::Error err = program.compile(_isolate, body, options, layout, &log);

    if (err != mpsl::kErrorOk) {
      printf("ERROR: Compilation failed.\n");
      return false;
    }

    err = program.run(&args);
    if (err != mpsl::kErrorOk) {
      printf("ERROR: Execution failed.\n");
      return false;
    }

    bool isOk = true;
    unsigned int i, n;

    switch (retType) {
      case mpsl::kTypeInt  : n = 1; goto checkInt;
      case mpsl::kTypeInt_2: n = 2; goto checkInt;
      case mpsl::kTypeInt_3: n = 3; goto checkInt;
      case mpsl::kTypeInt_4: n = 4; goto checkInt;
checkInt:
        for (i = 0; i < n; i++) {
          int x = args.ret.i[i];
          int y = retValue.i[i];

          if (x != y) {
            printf("ERROR: ic[%u] %d != Expected(%d)\n", i, x, y);
            isOk = false;
          }
        }
        break;

      case mpsl::kTypeFloat  : n = 1; goto checkFloat;
      case mpsl::kTypeFloat_2: n = 2; goto checkFloat;
      case mpsl::kTypeFloat_3: n = 3; goto checkFloat;
      case mpsl::kTypeFloat_4: n = 4; goto checkFloat;
checkFloat:
        for (i = 0; i < n; i++) {
          float x = args.ret.f[i];
          float y = retValue.f[i];

          if (x != y) {
            printf("ERROR: fc[%u] %g != Expected(%g)\n", i, x, y);
            isOk = false;
          }
        }
        break;

      case mpsl::kTypeDouble  : n = 1; goto checkDouble;
      case mpsl::kTypeDouble_2: n = 2; goto checkDouble;
      case mpsl::kTypeDouble_3: n = 3; goto checkDouble;
      case mpsl::kTypeDouble_4: n = 4; goto checkDouble;
checkDouble:
        for (i = 0; i < n; i++) {
          double x = args.ret.d[i];
          double y = retValue.d[i];

          if (x != y) {
            printf("ERROR: dc[%u] %g != Expected(%g)\n", i, x, y);
            isOk = false;
          }
        }
        break;
    }

    _succeeded &= isOk;
    return isOk;
  }

  mpsl::Isolate _isolate;
  bool _succeeded;
};

int main(int argc, char* argv[]) {
  uint32_t options = 
    mpsl::kOptionVerbose  |
    mpsl::kOptionDebugAST |
    mpsl::kOptionDebugIR  |
    mpsl::kOptionDebugASM ;

  Test test;
  test.basicTest("int main() { return ia * ib + ic; }", options, mpsl::kTypeInt, makeIVal(4));
  test.basicTest("float main() { return fa * fb + fc; }", options, mpsl::kTypeFloat, makeFVal(4.0f));
  test.basicTest("double main() { return da * db + dc; }", options, mpsl::kTypeDouble, makeDVal(4.0));

  return test._succeeded ? 0 : 1;
}
