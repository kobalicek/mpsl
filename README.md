MPSL
====

MathPresso's Shading Language with JIT Engine for C++.

  * [Official Repository (kobalicek/mpsl)](https://github.com/kobalicek/mpsl)
  * [Official Blog (asmbits)] (https://asmbits.blogspot.com/ncr)
  * [Official Chat (gitter)](https://gitter.im/kobalicek/mpsl)
  * [Permissive ZLIB license](./LICENSE.md)

Important
---------

MPSL requires asmjit:next branch unless it's already merged with asmjit:master.


Disclaimer
----------

This is a WORK-IN-PROGRESS that is far from being complete. MPSL is a challenging and difficult project and I work on it mostly when I'm tired of other projects. Contact me if you found MPSL interesting and want to collaborate on its development - people joining this project are very welcome.


Project Status
--------------

What is implemented:
  * [x] MPSL APIs (public) for embedding
  * [x] AST definition and source code to AST conversion (parser)
  * [x] AST-based semantic code analysis (happens after parsing)
  * [x] AST-based optimizations (constant folding and dead code elimination)
  * [x] IR concept and initial support for AST to IR mapping

What is a work-in-progress:
  * [ ] AST-To-IR translation is only basic for now (doesn't implement control-flow and many operators)
  * [ ] IR-based optimizations are not implemented yet
  * [ ] IR-To-ASM translation is very basic and buggy
  * [ ] IR is not in SSA form yet, this is to-be-researched subject atm


Introduction
------------

MPSL is a lightweight shader-like programming language written in C++. Its name is based on a sister project called [MathPresso](https://github.com/kobalicek/mathpresso), which provided the basic idea and some building blocks. MPSL has been designed to be a safe programming language that can access CPU's SIMD capabilities through a shader-like programs compiled at runtime. The language is statically typed and allows to use up to 256-bit wide variables that map directly to CPU's SIMD registers (SSE, AVX, NEON, ...).

MPSL has been designed to be lightweight and embeddable - it doesn't depend on huge libraries like LLVM, it only uses a very lightweight library called [AsmJit](https://github.com/kobalicek/asmjit) as a JIT backend. It implements its own abstract syntax tree (AST) and intermediate representation (IR) of the input program, and then uses an IRToAsm translator to convert IR to machine code.

Check out a working [mp_tutorial.cpp](./src/app/mp_tutorial.cpp) to see how MPSL APIs are designed and how MPSL engine is embedded and used within an application.


MPSL Types
----------

MPSL is a statically typed language where variables have to be declared before they are used like in any other C-like language. Variables can be declared anywhere and can shadow other variables and/or function arguments.

Available types:

  * `int` - 32-bit signed integer
  * `float` - 32-bit (single-precision) floating point
  * `double` - 64-bit (double-precision) floating point
  * `bool` - 32-bit boolean type (implicitly castable to any type)
  * `__qbool` - 64-bit boolean type (implicitly castable to any type)
  * All types can form a vector up to 4 elements, like `int2`, `float3`, and `double4`
  * 32-bit types can additionally form a vector of 8 elements, like `bool8`, `int8`, and `float8`

Constants are parsed in the following way:

  * If the number doesn't have any of fraction and exponent parts and it's in range [-2147483648, 2147483647] it's parsed as a 32-bit integer, otherwise it's parsed as `double`
  * If the number contains "f" suffix it's parsed as `float`
  * If the number contains "d" suffix it's parsed as `double`
  * TODO: MSPL should allow to customize which floating type is preferred (if `float` or `double`).


MPSL Features
-------------

  * Variables have to be declared before they are used:
    * `[const] type var [= value];`
    * Variables declared as `const` have to be assigned immediately and cannot be changed
    * Expressions like `int x = x;` are not allowed (unlike C), unless `x` shadows another `x` from outer scope
    * TODO: Reading a variable that was not assigned yet is undefined, should be defined

  * Typedef (aka type alias)
    * `typedef type newtype`

  * If-Else:
    * `if (cond) { taken-body; } [else { not-taken-body; }]`

  * Implicit and explicit casts:
    * `(type)expression` is an explicit cast
    * Integer and float can implicitly cast to double
    * Boolean types can implicitly or explicitly cast to any 32-bit or 64-bit type, converting `true` value to `1` and `false` value to `0`
    * Scalar is implicitly promoted to vector if used with a vector type

  * Loops:
    * `for (init; cond; iter) { body; }`
    * `do { body } while(cond);`
    * `while(cond) { body; }`

  * Functions:
    * `ret-type func([arg-type arg-name [, ...]]) { body; }`
    * Functions can call other functions, but they can't recurse
    * Each MPSL program contains a `main() { ... }` entry-point

  * Arithmetic operators:
    * `-(x)` - negate
    * `x + y` - add
    * `x - y` - subtract
    * `x * y` - multiply
    * `x / y` - divide
    * `x % y` - modulo

  * Bitwise and shift operators and intrinsics:
    * `~(x)` - bitwise NOT
    * `x & y` - bitwise AND
    * `x | y` - bitwise OR
    * `x ^ y` - bitwise XOR
    * `x >> y` - shift arithmetic right
    * `x >>> y` - shift logical right
    * `x << y` - shift logical left
    * `ror(x, y)` - rotate right
    * `rol(x, y)` - rotate left
    * `lzcnt(x)` - count leading zeros
    * `popcnt(x)` - population count (count of bits set to `1`)

  * Logical operators:
    * `!(x)` - logical NOT
    * `x && y` - logical AND
    * `x || y` - logical OR

  * Comparison operators:
    * `x == y` - check if equal
    * `x != y` - check if not equal
    * `x > y` - check if greater than
    * `x >= y` - check if greater than or equal
    * `x < y` - check if lesser than
    * `x <= y` - check if lesser than or equal

  * Assignment operators:
    * `++x` - pre-increment
    * `--x` - pre-decrement
    * `x++` - post-increment
    * `x--` - post-decrement
    * `x = y` - assignment
    * `x += y` - add with assignment
    * `x -= y` - subtract with assignment
    * `x *= y` - multiply with assignment
    * `x /= y` - divide with assignment
    * `x %= y` - modulo with assignment
    * `x &= y` - bitwise AND with assignment
    * `x |= y` - bitwise OR with assignment
    * `x ^= y` - bitwise XOR with assignment
    * `x >>= y` - shift arithmetic right with assignment
    * `x >>>= y` - shift logical right with assignment
    * `x <<= y` - shift logical left with assignment

  * Built-in intrinsics for special number handling:
    * `isnan(x)` - check for NaN
    * `isinf(x)` - check for infinity
    * `isfinite(x)` - check for finite number
    * `signbit(x)` - get a sign bit
    * `copysign(x, y)` - copy sign

  * Built-in intrinsics for floating-point rounding:
    * `round(x)` - round to nearest integer
    * `roundeven(x)` - round to even integer
    * `trunc(x)` - round towards zero (truncate)
    * `floor(x)` - round down (floor)
    * `ceil(x)` - round up (ceil)

  * Built-in intrinsics that map easily to CPU instructions:
    * `abs(x)` - absolute value
    * `frac(x)` - extract fraction
    * `sqrt(x)` - square root
    * `min(x, y)` - minimum value
    * `max(x, y)` - maximum value

  * Other built-in intrinsics:
    * `exp(x)` - exponential
    * `log(x)` - logarithm of base E
    * `log2(x)` - logarithm of base 2
    * `log10(x)` - logarithm of base 10
    * `pow(x, y)` - power
    * `sin(x)` - sine
    * `cos(x)` - cosine
    * `tan(x)` - tangent
    * `asin(x)` - arcsine
    * `acos(x)` - arccosine
    * `atan(x)` and `atan2(x, y)` - arctangent

  * Built-in DSP intrinsics (`int` and `int2..8` only):
    * `vabsb(x)` - absolute value of packed bytes
    * `vabsw(x)` - absolute value of packed words
    * `vabsd(x)` - absolute value of packed dwords
    * `vaddb(x, y)` - add packed bytes
    * `vaddw(x, y)` - add packed words
    * `vaddd(x, y)` - add packed dwords
    * `vaddq(x, y)` - add packed qwords
    * `vaddssb(x, y)` - add packed bytes with signed saturation
    * `vaddusb(x, y)` - add packed bytes with unsigned saturation
    * `vaddssw(x, y)` - add packed words with signed saturation
    * `vaddusw(x, y)` - add packed words with unsigned saturation
    * `vsubb(x, y)` - subtract packed bytes
    * `vsubw(x, y)` - subtract packed words
    * `vsubd(x, y)` - subtract packed dwords
    * `vsubq(x, y)` - subtract packed qwords
    * `vsubssb(x, y)` - subtract packed bytes with signed saturation
    * `vsubusb(x, y)` - subtract packed bytes with unsigned saturation
    * `vsubssw(x, y)` - Subtract packed words with signed saturation
    * `vsubusw(x, y)` - subtract packed words with unsigned saturation
    * `vmulw(x, y)` - multiply packed words (signed or unsigned)
    * `vmulhsw(x, y)` - multiply packed words and store high word of a signed result
    * `vmulhuw(x, y)` - multiply packed words and store high word of an unsigned result
    * `vmuld(x, y)` - multiply packed words (signed or unsigned)
    * `vminsb(x, y)` - minimum of packed bytes (signed)
    * `vminub(x, y)` - minimum of packed bytes (unsigned)
    * `vminsw(x, y)` - minimum of packed words (signed)
    * `vminuw(x, y)` - minimum of packed words (unsigned)
    * `vminsd(x, y)` - minimum of packed dwords (signed)
    * `vminud(x, y)` - minimum of packed dwords (unsigned)
    * `vmaxsb(x, y)` - maximum of packed bytes (signed)
    * `vmaxub(x, y)` - maximum of packed bytes (unsigned)
    * `vmaxsw(x, y)` - maximum of packed words (signed)
    * `vmaxuw(x, y)` - maximum of packed words (unsigned)
    * `vmaxsd(x, y)` - maximum of packed dwords (signed)
    * `vmaxud(x, y)` - maximum of packed dwords (unsigned)
    * `vsllw(x, y)` - shift left logical of packed words by scalar `y`
    * `vsrlw(x, y)` - shift right logical of packed words by scalar `y`
    * `vsraw(x, y)` - shift right arithmetic of packed words by scalar `y`
    * `vslld(x, y)` - shift left logical of packed dwords by scalar `y`
    * `vsrld(x, y)` - shift right logical of packed dwords by scalar `y`
    * `vsrad(x, y)` - shift right arithmetic of packed dwords by scalar `y`
    * `vsllq(x, y)` - shift left logical of packed qwords by scalar `y`
    * `vsrlq(x, y)` - shift right logical of packed qwords by scalar `y`
    * `vcmpeqb(x, y)` - compare packed bytes (signed) if equal
    * `vcmpeqw(x, y)` - compare packed words (signed) if equal
    * `vcmpeqd(x, y)` - compare packed dwords (signed) if equal
    * `vcmpgtb(x, y)` - compare packed bytes (signed) if greater than
    * `vcmpgtw(x, y)` - compare packed words (signed) if greater than
    * `vcmpgtd(x, y)` - compare packed dwords (signed) if greater than
    
  * Built-in special constants:
    * `INF` - infinity
    * `NaN` - not a number

  * Built-in math constants from C's math.h:
    * `M_E        = 2.71828182845904523536 ` - Euler's number
    * `M_LOG2E    = 1.44269504088896340736 ` - log2(e)
    * `M_LOG10E   = 0.434294481903251827651` - log10(e)
    * `M_LN2      = 0.693147180559945309417` - ln(2)
    * `M_LN10     = 2.30258509299404568402 ` - ln(10)
    * `M_PI       = 3.14159265358979323846 ` - PI
    * `M_PI_2     = 1.57079632679489661923 ` - PI/2
    * `M_PI_4     = 0.785398163397448309616` - PI/4
    * `M_1_PI     = 0.318309886183790671538` - 1/PI
    * `M_2_PI     = 0.636619772367581343076` - 2/PI
    * `M_2_SQRTPI = 1.1283791670955125739  ` - 2/sqrt(pi)
    * `M_SQRT2    = 1.4142135623730950488  ` - sqrt(2)
    * `M_SQRT1_2  = 0.707106781186547524401` - 1/sqrt(2)


MPSL Program
------------

A typical MPSL program has an entry-point called `main()` and uses input and output variables provided by the embedder. If `a` and `b` variables of type `float4` are provided then we can write a simple shader that returns their sum:

```c++
// Shader's entry-point - can have return value (if embedded defines it), but has no arguments.
float4 main() {
  // In case the embedder provides two arguments `a` and `b` of `float4` type.
  return a + b;
}
```

Where `a`, `b`, and a hidden return variable are provided by the embedder (including their types). This means that the same MPSL program is able to use different data layouts and different number of data arguments passed to the compiled shader.


MPSL C++ API 
------------

MPSL is written in C++ and provides C++ APIs for embedders. Here is a summary of MPSL's design choices:

  * MPSL is written in C++ and exposes a simple C++ interface for embedders. It's possible to wrap it in a pure C interface, but it's not planned to be part of a MPSL project at the moment.
  * MPSL uses error codes, not exceptions, and guarantees that every failure is propagated to the embedder as an error code. MPSL never aborts on out-of-memory condition, never throws, and clean ups all resources in case of error properly.
  * MPSL has its own pooled memory allocator, which uses the OS allocator to allocate larger blocks of memory, which are then split into smaller chunks and pools. It's very fast and prevents memory fragmentation.
  * MPSL allows embedder to specify a data-layout of his own structures, which means that embedders generally don't have to change their data structures to use MPSL.

To use MPSL from your C++ code you must first include `mpsl/mpsl.h` to make all public APIs available within `mpsl` namespace. The following concepts are provided:

  * `mpsl::Context` - This is an expression's environment. A program cannot be created without having a `Context` associated. `Context` also manages virtual memory that is used by shaders.
  * `mpsl::Program[1-4]<>` - Program represents a compiled MPSL shader. The `[1-4]` is a number of data arguments passed to the shader. Here the data argument doesn't represent variables used in a shader, it represents number of "pointers" passed to the shader, where each pointer can contain variables the shader has access to.
  * `mpsl::Layout` - A layout of data arguments passed to the shader. Each data argument (or sometimes called slot) has its own `Layout` definition.
  * `mpsl::LayoutTmp<N>` - A layout that uses `N` bytes of stack before it allocates dynamic memory. Since `Layout` instances are short-lived it's beneficial to allocate them on stack.
  * `mpsl::OutputLog` - An interface that can be used to catch messages produced by MPSL parser, analyzer, optimizer, and JIT compiler.
  * `mpsl::StringRef` - A string reference, which may be used to specify string and its length. You can pass non-NULL terminated strings to all MPSL APIs.

  * `mpsl::Int[2-4]` - `int` vectors, which map to MPSL's `int[2-4]` types.
  * `mpsl::Float[2-4]` - `float` vectors, which map to MPSL's `float[2-4]` types.
  * `mpsl::Double[2-4]` - `double` vectors, which map to MPSL's `double[2-4]` types.

  * `mpsl::Error` - An error type (typedef to `uint32_t`) that is returned from MPSL APIs to report the result status.

Check out [mpsl.h](./src/mpsl/mpsl.h) for more details about class member functions and public enumerations that can be used by embedders.

Below is a minimal code that creates a simple data layout and compiles a very simple shader:

```c++
#include <mpsl/mpsl.h>

// Data structure that will be passed to the program.
struct Data {
  double a, b;
  float c;
  double result;
};

int main(int argc, char* argv[]) {
  // Create the shader environment.
  mpsl::Context context = mpsl::Context::create();

  // Create the `Data` layout and register all variables that the shader has
  // access to. Special variables like `@ret` have always `@` prefix to prevent
  // a collision with valid identifiers.
  mpsl::LayoutTmp<> layout;
  layout.addMember("a"   , mpsl::kTypeDouble  | mpsl::kTypeRO, MPSL_OFFSET_OF(Data, a));
  layout.addMember("b"   , mpsl::kTypeDouble  | mpsl::kTypeRO, MPSL_OFFSET_OF(Data, b));
  layout.addMember("c"   , mpsl::kTypeFloat   | mpsl::kTypeRO, MPSL_OFFSET_OF(Data, c));
  layout.addMember("@ret", mpsl::kTypeDouble  | mpsl::kTypeWO, MPSL_OFFSET_OF(Data, result));

  // This is our shader-program that will be compiled by MPSL.
  const char body[] = "double main() { return sqrt(a * b) * c; }";

  // Create the program object and try to compile it. The `Program1<Data>` means
  // that the program accepts one data argument of type `Data`. You can just use
  // `Program1<>` if you want the argument untyped (void).
  mpsl::Program1<Data> program;
  mpsl::Error err = program.compile(context, body, mpsl::kNoOptions, layout);

  if (err) {
    printf("Compilation failed: ERROR 0x%08X\n", static_cast<unsigned int>(err));
  }
  else {
    Data data;
    data.a = 4.0;
    data.b = 16.0;
    data.c = 0.5f;

    err = program.run(&data);
    if (err)
      printf("Execution failed: ERROR 0x%08X\n", static_cast<unsigned int>(err));
    else
      printf("Return=%g\n", data.result);
  }

  // RAII - `Context` and `Program` are ref-counted and will
  // be automatically destroyed when they go out of scope.
  return 0;
}
```

More documentation will come in the future.

Dependencies
------------

  * AsmJit - 1.0 or later.

Support
-------

Please consider a donation if you use the project and would like to keep it active in the future.

  * [Donate by PayPal](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=QDRM6SRNG7378&lc=EN;&item_name=mpsl&currency_code=EUR)

Authors & Maintainers
---------------------

  * Petr Kobalicek <kobalicek.petr@gmail.com>
