MPSL
====

Shader-Like Mathematical Expression JIT Engine for C++.

  * [Official Repository (kobalicek/mpsl)](https://github.com/kobalicek/mpsl)
  * [Official Chat (Gitter)](https://gitter.im/kobalicek/mpsl)
  * [Zlib Licensed](http://www.opensource.org/licenses/zlib-license.php)

Disclaimer
----------

The library is not usable at the moment.

This is a WORK-IN-PROGRESS that is far from being complete. MPSL is a challenging and difficult project and I work on it mostly when I'm tired of other projects. Contact me if you found MPSL interesting and want to collaborate on its development - people joining this project are very welcome.

Project Status
--------------

What is implemented:
  * [x] MPSL APIs (public) for embedding
  * [x] AST definition and source code to AST conversion (parser)
  * [x] AST-based semantic code analysis (happens after parsing)
  * [x] AST-based optimizations (constant folding and dead code elimination)

What is a work-in-progress:
  * [ ] IR is not fully defined yet, it may change a bit, it's an initial idea
  * [ ] AST-To-IR translation is only basic for now (doesn't implement control-flow and many operators)
  * [ ] IR-based optimizations are not implemented yet
  * [ ] IR-To-ASM translation is very basic and buggy
  * [ ] IR is not in SSA form yet, this is to-be-researched subject atm

Introduction
------------

MPSL is a shader-like mathematical expression parser and JIT compiler for C++. It implements a minimal shader-like language that has types, scalars, vectors (up to 4 elements), flow control, and basic math functions that can operate on scalars and vectors. MPSL started as a sister project of [MathPresso](https://github.com/kobalicek/mathpresso) library, but was rewritten completely and then some parts of MPSL were backported back to MathPresso. MSPL has been developed to be a very lightweight library that can fully utilize CPU's SIMD capability in a safe way through MPSL programs that are just-in-time compiled at runtime.

Check out a working [mp_tutorial.cpp](./src/app/mp_tutorial.cpp) to see how MPSL APIs are designed and how MPSL engine is embedded and used within an application.

Language Types
--------------

MPSL is a statically typed language where variables have to be declared before they are used like in any other C-like language. Variables can be declared anywhere and can shadow other variables and/or function arguments. 

Available types:

  * `int` - 32-bit signed integer
  * `float` - 32-bit (single-precision) floating point
  * `double` - 64-bit (double-precision) floating point
  * `__bool32` - Internal 32-bit boolean type (implicitly castable to 32-bit types)
  * `__bool64` - Internal 64-bit boolean type (implicitly castable to 64-bit types)
  * vector types up to 4 elements, like `int2`, `float3`, and `double4`

Constants are parsed in the following way:

  * If the number doesn't have any of fraction and exponent parts and it's in range [-2147483648, 2147483647] it's parsed as a 32-bit integer, otherwise it's parsed as `double` by default
  * If the number contains "f" suffix it's parsed as `float`
  * If the number contains "d" suffix it's parsed as `double`

Language Constructs
-------------------

  * Variables have to be declared before they are used:
    * `[const] type var [= value];`

  * Typedef (aka type alias)
    * `typedef type newtype`

  * If-Else:
    * `if (cond) { taken-body } [else { not-taken-body }]`

  * Implicit and explicit casts:
    * `(type)expression` is an explicit cast
    * Integer and float can implicitly cast to double
    * `__bool32` and `__bool64` can implicitly cast to 32-bit or 64-bit type, respectively
    * Scalar is implicitly promoted to vector if used with a vector type

  * Loops:
    * `for (init; cond; iter) { body; }`
    * `do { body } while(cond);`
    * `while(cond) { body }`

  * Functions:
    * `ret-type func([arg-type arg-name [, ...]]) { body; }`
    * Functions can call other functions, but they can't recurse
    * Each MPSL program contains a `main()` entry-point

  * Arithmetic operators:
    * Negation `-(x)`
    * Addition `x + y`
    * Subtraction `x - y`
    * Multiplication `x * y`
    * Division `x / y`
    * Modulo `x % y`

  * Bitwise and shift operators and intrinsics:
    * Bitwise not `~(x)`
    * Bitwise and `x & y`
    * Bitwise or `x | y`
    * Bitwise xor `x ^ y`
    * Shift arithmetic right `x >> y`
    * Shift logical right `x >>> y`
    * Shift logical left `x << y`
    * Rotate right `ror(x, y)`
    * Rotate left `rol(x, y)`

  * Logical operators:
    * Logical not `!(x)`
    * Logical and `x && y`
    * Logical or `x || y`

  * Comparison operators:
    * Equal `x == y`
    * Not equal `x != y`
    * Greater `x > y`
    * Greater or equal `x >= y`
    * Lesser `x < y`
    * Lesser or equal `x <= y`

  * Assignment operators:
    * Pre-increment `++x`
    * Pre-decrement `--x`
    * Post-increment `x++`
    * Post-decrement `x--`
    * Assignment `x = y`
    * Add `x += y`
    * Subtract `x -= y`
    * Multiply `x *= y`
    * Divide `x /= y`
    * Modulo `x %= y`
    * Bitwise and `x &= y`
    * Bitwise or `x |= y`
    * Bitwise xor `x ^= y`
    * Shift arithmetic right `x >>= y`
    * Shift logical right `x >>>= y`
    * Shift logical left `x <<= y`

  * Built-in intrinsics for special number handling:
    * Check for NaN `isnan(x)`
    * Check for infinity `isinf(x)`
    * Check for finite number `isfinite(x)`
    * Get a sign bit `signbit(x)`
    * Copy sign `copysign(x, y)`

  * Built-in intrinsics for floating-point rounding:
    * Round to nearest integer `round(x)`
    * Round to even integer `roundeven(x)`
    * Round towards zero `trunc(x)`
    * Round down `floor(x)`
    * Round up `ceil(x)`

  * Built-in intrinsics that map easily to CPU instructions:
    * Absolute value `abs(x)`
    * Extract fraction `frac(x)`
    * Square root `sqrt(x)`
    * Minimum value `min(x, y)`
    * Maximum value `max(x, y)`

  * Other built-in functions:
    * Exponential `exp(x)`
    * Logarithm `log(x)`
    * Logarithm of base 2 `log2(x)`
    * Logarithm of base 10 `log10(x)`
    * Power `pow(x, y)`
    * Sine `sin(x)`
    * Cosine `cos(x)`
    * Tangent `tan(x)`
    * Arcsine `asin(x)`
    * Arccosine `acos(x)`
    * Arctangent `atan(x)` and `atan2(x, y)`

  * Built-in special constants:
    * Infinity `INF`
    * Not a Number `NaN`

  * Built-in math constants from C's math.h:
    * Euler's    `M_E`        = `2.71828182845904523536`
    * log2(e)    `M_LOG2E`    = `1.44269504088896340736`
    * log10(e)   `M_LOG10E`   = `0.434294481903251827651`
    * ln(2)      `M_LN2`      = `0.693147180559945309417`
    * ln(10)     `M_LN10`     = `2.30258509299404568402`
    * PI         `M_PI`       = `3.14159265358979323846`
    * PI/2       `M_PI_2`     = `1.57079632679489661923`
    * PI/4       `M_PI_4`     = `0.785398163397448309616`
    * 1/PI       `M_1_PI`     = `0.318309886183790671538`
    * 2/PI       `M_2_PI`     = `0.636619772367581343076`
    * 2/sqrt(pi) `M_2_SQRTPI` = `1.1283791670955125739`
    * sqrt(2)    `M_SQRT2`    = `1.4142135623730950488`
    * 1/sqrt(2)  `M_SQRT1_2`  = `0.707106781186547524401`

Embedding
---------

MPSL is written in C++ and provides C++ APIs for embedders. To use MPSL from your C++ code you must first include `mpsl/mpsl.h` to make all public APIs available within `mpsl` namespace. The following concepts are provided:

  * `mpsl::Isolate` - This is an expression's environment (in MathPresso library this used to be a `Context`). A program cannot be created without having an `Isolate`. `Isolate` also manages virtual memory that is used by shaders.
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

Below is a minimal code that puts all together and uses MPSL to create and execute a very simple shader:

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
  mpsl::Isolate isolate = mpsl::Isolate::create();

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
  mpsl::Error err = program.compile(isolate, body, mpsl::kNoOptions, layout);

  if (err == mpsl::kErrorOk) {
    Data data;
    data.a = 4.0;
    data.b = 16.0;
    data.c = 0.5f;

    err = program.run(&data);
    if (err == mpsl::kErrorOk)
      printf("Return=%g\n", data.result);
    else
      printf("Execution failed: ERROR 0x%0.8X\n", static_cast<unsigned int>(err));
  }
  else {
    printf("Compilation failed: 0x%0.8X\n", err);
  }

  // RAII - `Isolate` and `Program` are ref-counted and will
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
