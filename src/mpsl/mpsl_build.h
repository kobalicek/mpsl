// [MPSL]
// Shader-Like Mathematical Expression JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_BUILD_H
#define _MPSL_BUILD_H

// ============================================================================
// [blend::Build - Configuration]
// ============================================================================

// External Config File
// --------------------
//
// Define in case your configuration is generated in an external file to be
// included.

#if defined(MPSL_CONFIG_FILE)
# include MPSL_CONFIG_FILE
#endif // MPSL_CONFIG_FILE

// MPSL Static Builds and Embedding
// --------------------------------------
//
// These definitions can be used to enable static library build. Embed is used
// when MPSL's source code is embedded directly in another project, it implies
// static build as well.
//
// #define MPSL_EMBED                // MPSL is embedded.
// #define MPSL_STATIC               // Define to enable static-library build.

// MPSL Build Modes
// ----------------
//
// These definitions control the build mode and tracing support. The build mode
// should be auto-detected at compile time, but it's possible to override it in
// case that the auto-detection fails.
//
// Tracing is a feature that is never compiled by default and it's only used to
// debug MPSL itself.
//
// #define MPSL_DEBUG                // Define to enable debug-mode.
// #define MPSL_RELEASE              // Define to enable release-mode.
// #define MPSL_TRACE                // Define to enable tracing.

// Detect MPSL_DEBUG and MPSL_RELEASE if not forced from outside.
#if !defined(MPSL_DEBUG) && !defined(MPSL_RELEASE)
# if !defined(NDEBUG)
#  define MPSL_DEBUG
# else
#  define MPSL_RELEASE
# endif
#endif

// MPSL_EMBED implies MPSL_STATIC.
#if defined(MPSL_EMBED) && !defined(MPSL_STATIC)
# define MPSL_STATIC
#endif

// ============================================================================
// [blend::Build - VERSION]
// ============================================================================

// [@VERSION{@]
#define MPSL_VERSION_MAJOR 1
#define MPSL_VERSION_MINOR 0
#define MPSL_VERSION_PATCH 0
#define MPSL_VERSION_STRING "1.0.0"
// [@VERSION}@]

// ============================================================================
// [blend::Build - WIN32]
// ============================================================================

// [@WIN32_CRT_NO_DEPRECATE{@]
#if defined(_MSC_VER) && defined(MPSL_EXPORTS)
# if !defined(_CRT_SECURE_NO_DEPRECATE)
#  define _CRT_SECURE_NO_DEPRECATE
# endif
# if !defined(_CRT_SECURE_NO_WARNINGS)
#  define _CRT_SECURE_NO_WARNINGS
# endif
#endif
// [@WIN32_CRT_NO_DEPRECATE}@]

// ============================================================================
// [blend::Build - OS]
// ============================================================================

// [@OS{@]
#if defined(_WIN32) || defined(_WINDOWS)
#define MPSL_OS_WINDOWS       (1)
#else
#define MPSL_OS_WINDOWS       (0)
#endif

#if defined(__APPLE__)
# include <TargetConditionals.h>
# define MPSL_OS_MAC          (TARGET_OS_MAC)
# define MPSL_OS_IOS          (TARGET_OS_IPHONE)
#else
# define MPSL_OS_MAC          (0)
# define MPSL_OS_IOS          (0)
#endif

#if defined(__ANDROID__)
# define MPSL_OS_ANDROID      (1)
#else
# define MPSL_OS_ANDROID      (0)
#endif

#if defined(__linux__) || defined(__ANDROID__)
# define MPSL_OS_LINUX        (1)
#else
# define MPSL_OS_LINUX        (0)
#endif

#if defined(__DragonFly__)
# define MPSL_OS_DRAGONFLYBSD (1)
#else
# define MPSL_OS_DRAGONFLYBSD (0)
#endif

#if defined(__FreeBSD__)
# define MPSL_OS_FREEBSD      (1)
#else
# define MPSL_OS_FREEBSD      (0)
#endif

#if defined(__NetBSD__)
# define MPSL_OS_NETBSD       (1)
#else
# define MPSL_OS_NETBSD       (0)
#endif

#if defined(__OpenBSD__)
# define MPSL_OS_OPENBSD      (1)
#else
# define MPSL_OS_OPENBSD      (0)
#endif

#if defined(__QNXNTO__)
# define MPSL_OS_QNX          (1)
#else
# define MPSL_OS_QNX          (0)
#endif

#if defined(__sun)
# define MPSL_OS_SOLARIS      (1)
#else
# define MPSL_OS_SOLARIS      (0)
#endif

#if defined(__CYGWIN__)
# define MPSL_OS_CYGWIN       (1)
#else
# define MPSL_OS_CYGWIN       (0)
#endif

#define MPSL_OS_BSD ( \
        MPSL_OS_FREEBSD       || \
        MPSL_OS_DRAGONFLYBSD  || \
        MPSL_OS_NETBSD        || \
        MPSL_OS_OPENBSD       || \
        MPSL_OS_MAC)
#define MPSL_OS_POSIX         (!MPSL_OS_WINDOWS)
// [@OS}@]

// ============================================================================
// [blend::Build - ARCH]
// ============================================================================

// [@ARCH{@]
// \def MPSL_ARCH_ARM32
// True if the target architecture is a 32-bit ARM.
//
// \def MPSL_ARCH_ARM64
// True if the target architecture is a 64-bit ARM.
//
// \def MPSL_ARCH_X86
// True if the target architecture is a 32-bit X86/IA32
//
// \def MPSL_ARCH_X64
// True if the target architecture is a 64-bit X64/AMD64
//
// \def MPSL_ARCH_LE
// True if the target architecture is little endian.
//
// \def MPSL_ARCH_BE
// True if the target architecture is big endian.
//
// \def MPSL_ARCH_64BIT
// True if the target architecture is 64-bit.

#if (defined(_M_X64  ) || defined(__x86_64) || defined(__x86_64__) || \
     defined(_M_AMD64) || defined(__amd64 ) || defined(__amd64__ ))
# define MPSL_ARCH_X64 1
#else
# define MPSL_ARCH_X64 0
#endif

#if (defined(_M_IX86 ) || defined(__X86__ ) || defined(__i386  ) || \
     defined(__IA32__) || defined(__I86__ ) || defined(__i386__) || \
     defined(__i486__) || defined(__i586__) || defined(__i686__))
# define MPSL_ARCH_X86 (!MPSL_ARCH_X64)
#else
# define MPSL_ARCH_X86 0
#endif

#if defined(__aarch64__)
# define MPSL_ARCH_ARM64 1
#else
# define MPSL_ARCH_ARM64 0
#endif

#if (defined(_M_ARM  ) || defined(__arm    ) || defined(__thumb__ ) || \
     defined(_M_ARMT ) || defined(__arm__  ) || defined(__thumb2__))
# define MPSL_ARCH_ARM32 (!MPSL_ARCH_ARM64)
#else
# define MPSL_ARCH_ARM32 0
#endif

#define MPSL_ARCH_LE    (  \
        MPSL_ARCH_X86   || \
        MPSL_ARCH_X64   || \
        MPSL_ARCH_ARM32 || \
        MPSL_ARCH_ARM64 )
#define MPSL_ARCH_BE (!(MPSL_ARCH_LE))
#define MPSL_ARCH_64BIT (MPSL_ARCH_X64 || MPSL_ARCH_ARM64)
// [@ARCH}@]

// [@ARCH_UNALIGNED_RW{@]
// \def MPSL_ARCH_UNALIGNED_16
// True if the target architecture allows unaligned 16-bit reads and writes.
//
// \def MPSL_ARCH_UNALIGNED_32
// True if the target architecture allows unaligned 32-bit reads and writes.
//
// \def MPSL_ARCH_UNALIGNED_64
// True if the target architecture allows unaligned 64-bit reads and writes.

#define MPSL_ARCH_UNALIGNED_16 (MPSL_ARCH_X86 || MPSL_ARCH_X64)
#define MPSL_ARCH_UNALIGNED_32 (MPSL_ARCH_X86 || MPSL_ARCH_X64)
#define MPSL_ARCH_UNALIGNED_64 (MPSL_ARCH_X86 || MPSL_ARCH_X64)
// [@ARCH_UNALIGNED_RW}@]

// ============================================================================
// [blend::Build - CC]
// ============================================================================

// [@CC{@]
// \def MPSL_CC_CLANG
// True if the detected C++ compiler is CLANG (contains normalized CLANG version).
//
// \def MPSL_CC_CODEGEAR
// True if the detected C++ compiler is CODEGEAR or BORLAND (version not normalized).
//
// \def MPSL_CC_GCC
// True if the detected C++ compiler is GCC (contains normalized GCC version).
//
// \def MPSL_CC_MSC
// True if the detected C++ compiler is MSC (contains normalized MSC version).
//
// \def MPSL_CC_MINGW
// Defined to 32 or 64 in case this is a MINGW, otherwise 0.

#define MPSL_CC_CLANG 0
#define MPSL_CC_CODEGEAR 0
#define MPSL_CC_GCC 0
#define MPSL_CC_MSC 0

#if defined(__CODEGEARC__)
# undef  MPSL_CC_CODEGEAR
# define MPSL_CC_CODEGEAR (__CODEGEARC__)
#elif defined(__BORLANDC__)
# undef  MPSL_CC_CODEGEAR
# define MPSL_CC_CODEGEAR (__BORLANDC__)
#elif defined(__clang__) && defined(__clang_minor__)
# undef  MPSL_CC_CLANG
# define MPSL_CC_CLANG (__clang_major__ * 10000000 + __clang_minor__ * 100000 + __clang_patchlevel__)
#elif defined(__GNUC__) && defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)
# undef  MPSL_CC_GCC
# define MPSL_CC_GCC (__GNUC__ * 10000000 + __GNUC_MINOR__ * 100000 + __GNUC_PATCHLEVEL__)
#elif defined(_MSC_VER) && defined(_MSC_FULL_VER)
# undef  MPSL_CC_MSC
# if _MSC_VER == _MSC_FULL_VER / 10000
#  define MPSL_CC_MSC (_MSC_VER * 100000 + (_MSC_FULL_VER % 10000))
# else
#  define MPSL_CC_MSC (_MSC_VER * 100000 + (_MSC_FULL_VER % 100000))
# endif
#else
# error "[mpsl] Unable to detect the C/C++ compiler."
#endif

#if MPSL_CC_GCC && defined(__GXX_EXPERIMENTAL_CXX0X__)
# define MPSL_CC_GCC_CXX0X 1
#else
# define MPSL_CC_GCC_CXX0X 0
#endif

#if defined(__MINGW64__)
# define MPSL_CC_MINGW 64
#elif defined(__MINGW32__)
# define MPSL_CC_MINGW 32
#else
# define MPSL_CC_MINGW 0
#endif

#define MPSL_CC_CODEGEAR_EQ(x, y, z) (MPSL_CC_CODEGEAR == (x << 8) + y)
#define MPSL_CC_CODEGEAR_GE(x, y, z) (MPSL_CC_CODEGEAR >= (x << 8) + y)

#define MPSL_CC_CLANG_EQ(x, y, z) (MPSL_CC_CLANG == x * 10000000 + y * 100000 + z)
#define MPSL_CC_CLANG_GE(x, y, z) (MPSL_CC_CLANG >= x * 10000000 + y * 100000 + z)

#define MPSL_CC_GCC_EQ(x, y, z) (MPSL_CC_GCC == x * 10000000 + y * 100000 + z)
#define MPSL_CC_GCC_GE(x, y, z) (MPSL_CC_GCC >= x * 10000000 + y * 100000 + z)

#define MPSL_CC_MSC_EQ(x, y, z) (MPSL_CC_MSC == x * 10000000 + y * 100000 + z)
#define MPSL_CC_MSC_GE(x, y, z) (MPSL_CC_MSC >= x * 10000000 + y * 100000 + z)
// [@CC}@]

// [@CC_FEATURES{@]
// \def MPSL_CC_HAS_NATIVE_CHAR
// True if the C++ compiler treats char as a native type.
//
// \def MPSL_CC_HAS_NATIVE_WCHAR_T
// True if the C++ compiler treats wchar_t as a native type.
//
// \def MPSL_CC_HAS_NATIVE_CHAR16_T
// True if the C++ compiler treats char16_t as a native type.
//
// \def MPSL_CC_HAS_NATIVE_CHAR32_T
// True if the C++ compiler treats char32_t as a native type.
//
// \def MPSL_CC_HAS_OVERRIDE
// True if the C++ compiler supports override keyword.
//
// \def MPSL_CC_HAS_NOEXCEPT
// True if the C++ compiler supports noexcept keyword.

#if MPSL_CC_CLANG
# define MPSL_CC_HAS_ATTRIBUTE               (1)
# define MPSL_CC_HAS_BUILTIN                 (1)
# define MPSL_CC_HAS_DECLSPEC                (0)

# define MPSL_CC_HAS_ALIGNAS                 (__has_extension(__cxx_alignas__))
# define MPSL_CC_HAS_ALIGNOF                 (__has_extension(__cxx_alignof__))
# define MPSL_CC_HAS_ASSUME                  (0)
# define MPSL_CC_HAS_ATTRIBUTE_ALIGNED       (__has_attribute(__aligned__))
# define MPSL_CC_HAS_ATTRIBUTE_ALWAYS_INLINE (__has_attribute(__always_inline__))
# define MPSL_CC_HAS_ATTRIBUTE_NOINLINE      (__has_attribute(__noinline__))
# define MPSL_CC_HAS_ATTRIBUTE_NORETURN      (__has_attribute(__noreturn__))
# define MPSL_CC_HAS_BUILTIN_ASSUME          (__has_builtin(__builtin_assume))
# define MPSL_CC_HAS_BUILTIN_EXPECT          (__has_builtin(__builtin_expect))
# define MPSL_CC_HAS_BUILTIN_UNREACHABLE     (__has_builtin(__builtin_unreachable))
# define MPSL_CC_HAS_CONSTEXPR               (__has_extension(__cxx_constexpr__))
# define MPSL_CC_HAS_DECLTYPE                (__has_extension(__cxx_decltype__))
# define MPSL_CC_HAS_DEFAULT_FUNCTION        (__has_extension(__cxx_defaulted_functions__))
# define MPSL_CC_HAS_DELETE_FUNCTION         (__has_extension(__cxx_deleted_functions__))
# define MPSL_CC_HAS_FINAL                   (__has_extension(__cxx_override_control__))
# define MPSL_CC_HAS_INITIALIZER_LIST        (__has_extension(__cxx_generalized_initializers__))
# define MPSL_CC_HAS_LAMBDA                  (__has_extension(__cxx_lambdas__))
# define MPSL_CC_HAS_NATIVE_CHAR             (1)
# define MPSL_CC_HAS_NATIVE_CHAR16_T         (__has_extension(__cxx_unicode_literals__))
# define MPSL_CC_HAS_NATIVE_CHAR32_T         (__has_extension(__cxx_unicode_literals__))
# define MPSL_CC_HAS_NATIVE_WCHAR_T          (1)
# define MPSL_CC_HAS_NOEXCEPT                (__has_extension(__cxx_noexcept__))
# define MPSL_CC_HAS_NULLPTR                 (__has_extension(__cxx_nullptr__))
# define MPSL_CC_HAS_OVERRIDE                (__has_extension(__cxx_override_control__))
# define MPSL_CC_HAS_RVALUE                  (__has_extension(__cxx_rvalue_references__))
# define MPSL_CC_HAS_STATIC_ASSERT           (__has_extension(__cxx_static_assert__))
#endif

#if MPSL_CC_CODEGEAR
# define MPSL_CC_HAS_ATTRIBUTE               (0)
# define MPSL_CC_HAS_BUILTIN                 (0)
# define MPSL_CC_HAS_DECLSPEC                (1)

# define MPSL_CC_HAS_ALIGNAS                 (0)
# define MPSL_CC_HAS_ALIGNOF                 (0)
# define MPSL_CC_HAS_ASSUME                  (0)
# define MPSL_CC_HAS_CONSTEXPR               (0)
# define MPSL_CC_HAS_DECLSPEC_ALIGN          (MPSL_CC_CODEGEAR >= 0x0610)
# define MPSL_CC_HAS_DECLSPEC_FORCEINLINE    (0)
# define MPSL_CC_HAS_DECLSPEC_NOINLINE       (0)
# define MPSL_CC_HAS_DECLSPEC_NORETURN       (MPSL_CC_CODEGEAR >= 0x0610)
# define MPSL_CC_HAS_DECLTYPE                (MPSL_CC_CODEGEAR >= 0x0610)
# define MPSL_CC_HAS_DEFAULT_FUNCTION        (0)
# define MPSL_CC_HAS_DELETE_FUNCTION         (0)
# define MPSL_CC_HAS_FINAL                   (0)
# define MPSL_CC_HAS_INITIALIZER_LIST        (0)
# define MPSL_CC_HAS_LAMBDA                  (0)
# define MPSL_CC_HAS_NATIVE_CHAR             (1)
# define MPSL_CC_HAS_NATIVE_CHAR16_T         (0)
# define MPSL_CC_HAS_NATIVE_CHAR32_T         (0)
# define MPSL_CC_HAS_NATIVE_WCHAR_T          (1)
# define MPSL_CC_HAS_NOEXCEPT                (0)
# define MPSL_CC_HAS_NULLPTR                 (0)
# define MPSL_CC_HAS_OVERRIDE                (0)
# define MPSL_CC_HAS_RVALUE                  (MPSL_CC_CODEGEAR >= 0x0610)
# define MPSL_CC_HAS_STATIC_ASSERT           (MPSL_CC_CODEGEAR >= 0x0610)
#endif

#if MPSL_CC_GCC
# define MPSL_CC_HAS_ATTRIBUTE               (1)
# define MPSL_CC_HAS_BUILTIN                 (1)
# define MPSL_CC_HAS_DECLSPEC                (0)

# define MPSL_CC_HAS_ALIGNAS                 (MPSL_CC_GCC_GE(4, 8, 0) && MPSL_CC_GCC_CXX0X)
# define MPSL_CC_HAS_ALIGNOF                 (MPSL_CC_GCC_GE(4, 8, 0) && MPSL_CC_GCC_CXX0X)
# define MPSL_CC_HAS_ASSUME                  (0)
# define MPSL_CC_HAS_ATTRIBUTE_ALIGNED       (MPSL_CC_GCC_GE(2, 7, 0))
# define MPSL_CC_HAS_ATTRIBUTE_ALWAYS_INLINE (MPSL_CC_GCC_GE(4, 4, 0) && !MPSL_CC_MINGW)
# define MPSL_CC_HAS_ATTRIBUTE_NOINLINE      (MPSL_CC_GCC_GE(3, 4, 0) && !MPSL_CC_MINGW)
# define MPSL_CC_HAS_ATTRIBUTE_NORETURN      (MPSL_CC_GCC_GE(2, 5, 0))
# define MPSL_CC_HAS_BUILTIN_ASSUME          (0)
# define MPSL_CC_HAS_BUILTIN_EXPECT          (1)
# define MPSL_CC_HAS_BUILTIN_UNREACHABLE     (MPSL_CC_GCC_GE(4, 5, 0) && MPSL_CC_GCC_CXX0X)
# define MPSL_CC_HAS_CONSTEXPR               (MPSL_CC_GCC_GE(4, 6, 0) && MPSL_CC_GCC_CXX0X)
# define MPSL_CC_HAS_DECLTYPE                (MPSL_CC_GCC_GE(4, 3, 0) && MPSL_CC_GCC_CXX0X)
# define MPSL_CC_HAS_DEFAULT_FUNCTION        (MPSL_CC_GCC_GE(4, 4, 0) && MPSL_CC_GCC_CXX0X)
# define MPSL_CC_HAS_DELETE_FUNCTION         (MPSL_CC_GCC_GE(4, 4, 0) && MPSL_CC_GCC_CXX0X)
# define MPSL_CC_HAS_FINAL                   (MPSL_CC_GCC_GE(4, 7, 0) && MPSL_CC_GCC_CXX0X)
# define MPSL_CC_HAS_INITIALIZER_LIST        (MPSL_CC_GCC_GE(4, 4, 0) && MPSL_CC_GCC_CXX0X)
# define MPSL_CC_HAS_LAMBDA                  (MPSL_CC_GCC_GE(4, 5, 0) && MPSL_CC_GCC_CXX0X)
# define MPSL_CC_HAS_NATIVE_CHAR             (1)
# define MPSL_CC_HAS_NATIVE_CHAR16_T         (MPSL_CC_GCC_GE(4, 5, 0) && MPSL_CC_GCC_CXX0X)
# define MPSL_CC_HAS_NATIVE_CHAR32_T         (MPSL_CC_GCC_GE(4, 5, 0) && MPSL_CC_GCC_CXX0X)
# define MPSL_CC_HAS_NATIVE_WCHAR_T          (1)
# define MPSL_CC_HAS_NOEXCEPT                (MPSL_CC_GCC_GE(4, 6, 0) && MPSL_CC_GCC_CXX0X)
# define MPSL_CC_HAS_NULLPTR                 (MPSL_CC_GCC_GE(4, 6, 0) && MPSL_CC_GCC_CXX0X)
# define MPSL_CC_HAS_OVERRIDE                (MPSL_CC_GCC_GE(4, 7, 0) && MPSL_CC_GCC_CXX0X)
# define MPSL_CC_HAS_RVALUE                  (MPSL_CC_GCC_GE(4, 3, 0) && MPSL_CC_GCC_CXX0X)
# define MPSL_CC_HAS_STATIC_ASSERT           (MPSL_CC_GCC_GE(4, 3, 0) && MPSL_CC_GCC_CXX0X)
#endif

#if MPSL_CC_MSC
# define MPSL_CC_HAS_ATTRIBUTE               (0)
# define MPSL_CC_HAS_BUILTIN                 (0)
# define MPSL_CC_HAS_DECLSPEC                (1)

# define MPSL_CC_HAS_ALIGNAS                 (MPSL_CC_MSC_GE(19, 0, 0))
# define MPSL_CC_HAS_ALIGNOF                 (MPSL_CC_MSC_GE(19, 0, 0))
# define MPSL_CC_HAS_ASSUME                  (1)
# define MPSL_CC_HAS_CONSTEXPR               (MPSL_CC_MSC_GE(19, 0, 0))
# define MPSL_CC_HAS_DECLSPEC_ALIGN          (1)
# define MPSL_CC_HAS_DECLSPEC_FORCEINLINE    (1)
# define MPSL_CC_HAS_DECLSPEC_NOINLINE       (1)
# define MPSL_CC_HAS_DECLSPEC_NORETURN       (1)
# define MPSL_CC_HAS_DECLTYPE                (MPSL_CC_MSC_GE(16, 0, 0))
# define MPSL_CC_HAS_DEFAULT_FUNCTION        (MPSL_CC_MSC_GE(18, 0, 0))
# define MPSL_CC_HAS_DELETE_FUNCTION         (MPSL_CC_MSC_GE(18, 0, 0))
# define MPSL_CC_HAS_FINAL                   (MPSL_CC_MSC_GE(14, 0, 0))
# define MPSL_CC_HAS_INITIALIZER_LIST        (MPSL_CC_MSC_GE(18, 0, 0))
# define MPSL_CC_HAS_LAMBDA                  (MPSL_CC_MSC_GE(16, 0, 0))
# define MPSL_CC_HAS_NATIVE_CHAR             (1)
# define MPSL_CC_HAS_NATIVE_CHAR16_T         (MPSL_CC_MSC_GE(19, 0, 0))
# define MPSL_CC_HAS_NATIVE_CHAR32_T         (MPSL_CC_MSC_GE(19, 0, 0))
# if defined(_NATIVE_WCHAR_T_DEFINED)
#  define MPSL_CC_HAS_NATIVE_WCHAR_T         (1)
# else
#  define MPSL_CC_HAS_NATIVE_WCHAR_T         (0)
# endif
# define MPSL_CC_HAS_NOEXCEPT                (MPSL_CC_MSC_GE(19, 0, 0))
# define MPSL_CC_HAS_NULLPTR                 (MPSL_CC_MSC_GE(16, 0, 0))
# define MPSL_CC_HAS_OVERRIDE                (MPSL_CC_MSC_GE(14, 0, 0))
# define MPSL_CC_HAS_RVALUE                  (MPSL_CC_MSC_GE(16, 0, 0))
# define MPSL_CC_HAS_STATIC_ASSERT           (MPSL_CC_MSC_GE(16, 0, 0))
#endif

#if !MPSL_CC_HAS_ATTRIBUTE
# define MPSL_CC_HAS_ATTRIBUTE_ALIGNED       (0)
# define MPSL_CC_HAS_ATTRIBUTE_ALWAYS_INLINE (0)
# define MPSL_CC_HAS_ATTRIBUTE_NOINLINE      (0)
# define MPSL_CC_HAS_ATTRIBUTE_NORETURN      (0)
#endif

#if !MPSL_CC_HAS_BUILTIN
# define MPSL_CC_HAS_BUILTIN_ASSUME          (0)
# define MPSL_CC_HAS_BUILTIN_EXPECT          (0)
# define MPSL_CC_HAS_BUILTIN_UNREACHABLE     (0)
#endif

#if !MPSL_CC_HAS_DECLSPEC
# define MPSL_CC_HAS_DECLSPEC_ALIGN          (0)
# define MPSL_CC_HAS_DECLSPEC_FORCEINLINE    (0)
# define MPSL_CC_HAS_DECLSPEC_NOINLINE       (0)
# define MPSL_CC_HAS_DECLSPEC_NORETURN       (0)
#endif
// [@CC_FEATURES}@]

// [@CC_API{@]
// \def MPSL_API
// The decorated function is mpsl API and should be exported.
#if !defined(MPSL_API)
# if defined(MPSL_STATIC)
#  define MPSL_API
# elif MPSL_OS_WINDOWS
#  if (MPSL_CC_GCC || MPSL_CC_CLANG) && !MPSL_CC_MINGW
#   if defined(MPSL_EXPORTS)
#    define MPSL_API __attribute__((__dllexport__))
#   else
#    define MPSL_API __attribute__((__dllimport__))
#   endif
#  else
#   if defined(MPSL_EXPORTS)
#    define MPSL_API __declspec(dllexport)
#   else
#    define MPSL_API __declspec(dllimport)
#   endif
#  endif
# else
#  if MPSL_CC_CLANG || MPSL_CC_GCC_GE(4, 0, 0)
#   define MPSL_API __attribute__((__visibility__("default")))
#  endif
# endif
#endif
// [@CC_API}@]

// [@CC_NOAPI{@]
// \def MPSL_NOAPI
// The decorated function is considered private and is not exported.
#define MPSL_NOAPI
// [@CC_NOAPI}@]

// [@CC_VIRTAPI{@]
// \def MPSL_VIRTAPI
// The decorated class has a virtual table and is part of mpsl API.
//
// This is basically a workaround. When using MSVC and marking class as DLL
// export everything gets exported, which is unwanted in most projects. MSVC
// automatically exports typeinfo and vtable if at least one symbol of the
// class is exported. However, GCC has some strange behavior that even if
// one or more symbol is exported it doesn't export typeinfo unless the
// class itself is decorated with "visibility(default)" (i.e. mpsl_API).
#if (MPSL_CC_GCC || MPSL_CC_CLANG) && !MPSL_OS_WINDOWS
# define MPSL_VIRTAPI MPSL_API
#else
# define MPSL_VIRTAPI
#endif
// [@CC_VIRTAPI}@]

// [@CC_INLINE{@]
// \def MPSL_INLINE
// Always inline the decorated function.
#if MPSL_CC_HAS_ATTRIBUTE_ALWAYS_INLINE && MPSL_CC_CLANG
# define MPSL_INLINE inline __attribute__((__always_inline__, __visibility__("hidden")))
#elif MPSL_CC_HAS_ATTRIBUTE_ALWAYS_INLINE
# define MPSL_INLINE inline __attribute__((__always_inline__))
#elif MPSL_CC_HAS_DECLSPEC_FORCEINLINE
# define MPSL_INLINE __forceinline
#else
# define MPSL_INLINE inline
#endif
// [@CC_INLINE}@]

// [@CC_NOINLINE{@]
// \def MPSL_NOINLINE
// Never inline the decorated function.
#if MPSL_CC_HAS_ATTRIBUTE_NOINLINE
# define MPSL_NOINLINE __attribute__((__noinline__))
#elif MPSL_CC_HAS_DECLSPEC_NOINLINE
# define MPSL_NOINLINE __declspec(noinline)
#else
# define MPSL_NOINLINE
#endif
// [@CC_NOINLINE}@]

// [@CC_NORETURN{@]
// \def MPSL_NORETURN
// The decorated function never returns (exit, assertion failure, etc...).
#if MPSL_CC_HAS_ATTRIBUTE_NORETURN
# define MPSL_NORETURN __attribute__((__noreturn__))
#elif MPSL_CC_HAS_DECLSPEC_NORETURN
# define MPSL_NORETURN __declspec(noreturn)
#else
# define MPSL_NORETURN
#endif
// [@CC_NORETURN}@]

// [@CC_CDECL{@]
// \def MPSL_CDECL
// Standard C function calling convention decorator (__cdecl).
#if MPSL_ARCH_X86
# if MPSL_CC_HAS_ATTRIBUTE
#  define MPSL_CDECL __attribute__((__cdecl__))
# else
#  define MPSL_CDECL __cdecl
# endif
#else
# define MPSL_CDECL
#endif
// [@CC_CDECL}@]

// [@CC_NOEXCEPT{@]
// \def MPSL_NOEXCEPT
// The decorated function never throws an exception (noexcept).
#if MPSL_CC_HAS_NOEXCEPT
# define MPSL_NOEXCEPT noexcept
#else
# define MPSL_NOEXCEPT
#endif
// [@CC_NOEXCEPT}@]

// [@CC_MACRO{@]
// \def MPSL_MACRO_BEGIN
// Begin of a macro.
//
// \def MPSL_MACRO_END
// End of a macro.
#if MPSL_CC_GCC || MPSL_CC_CLANG
# define MPSL_MACRO_BEGIN ({
# define MPSL_MACRO_END })
#else
# define MPSL_MACRO_BEGIN do {
# define MPSL_MACRO_END } while (0)
#endif
// [@CC_MACRO}@]

// [@CC_ASSUME{@]
// \def MPSL_ASSUME(exp)
// Assume that the expression exp is always true.
#if MPSL_CC_HAS_ASSUME
# define MPSL_ASSUME(exp) __assume(exp)
#elif MPSL_CC_HAS_BUILTIN_ASSUME
# define MPSL_ASSUME(exp) __builtin_assume(exp)
#elif MPSL_CC_HAS_BUILTIN_UNREACHABLE
# define MPSL_ASSUME(exp) do { if (!(exp)) __builtin_unreachable(); } while (0)
#else
# define MPSL_ASSUME(exp) ((void)0)
#endif
// [@CC_ASSUME}@]

// [@CC_EXPECT{@]
// \def MPSL_LIKELY(exp)
// Expression exp is likely to be true.
//
// \def MPSL_UNLIKELY(exp)
// Expression exp is likely to be false.
#if MPSL_HAS_BUILTIN_EXPECT
# define MPSL_LIKELY(exp) __builtin_expect(!!(exp), 1)
# define MPSL_UNLIKELY(exp) __builtin_expect(!!(exp), 0)
#else
# define MPSL_LIKELY(exp) exp
# define MPSL_UNLIKELY(exp) exp
#endif
// [@CC_EXPECT}@]

// [@CC_FALLTHROUGH{@]
// \def MPSL_FALLTHROUGH
// The code falls through annotation (switch / case).
#if MPSL_CC_CLANG && __cplusplus >= 201103L
# define MPSL_FALLTHROUGH [[clang::fallthrough]]
#else
# define MPSL_FALLTHROUGH (void)0
#endif
// [@CC_FALLTHROUGH}@]

// [@CC_ALIGN{@]
// \def MPSL_ALIGN_TYPE
// \def MPSL_ALIGN_VAR
#if MPSL_CC_HAS_DECLSPEC_ALIGN
# define MPSL_ALIGN_TYPE(type, nbytes) __declspec(align(nbytes)) type
# define MPSL_ALIGN_VAR(type, name, nbytes) __declspec(align(nbytes)) type name
#elif MPSL_CC_HAS_ATTRIBUTE_ALIGNED
# define MPSL_ALIGN_TYPE(type, nbytes) __attribute__((__aligned__(nbytes))) type
# define MPSL_ALIGN_VAR(type, name, nbytes) type __attribute__((__aligned__(nbytes))) name
#else
# define MPSL_ALIGN_TYPE(type, nbytes) type
# define MPSL_ALIGN_VAR(type, name, nbytes) type name
#endif
// [@CC_ALIGN}@]

// [@CC_OFFSET_OF{@]
// \def MPSL_OFFSET_OF(x, y).
// Get the offset of a member y of a struct x at compile-time.
#define MPSL_OFFSET_OF(x, y) ((int)(intptr_t)((const char*)&((const x*)0x1)->y) - 1)
// [@CC_OFFSET_OF}@]

// [@CC_ARRAY_SIZE{@]
// \def MPSL_ARRAY_SIZE(x)
// Get the array size of x at compile-time.
#define MPSL_ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
// [@CC_ARRAY_SIZE}@]

// ============================================================================
// [blend::Build - STDTYPES]
// ============================================================================

// [@STDTYPES{@]
#if defined(__MINGW32__) || defined(__MINGW64__)
# include <sys/types.h>
#endif
#if defined(_MSC_VER) && (_MSC_VER < 1600)
# include <limits.h>
# if !defined(MPSL_SUPPRESS_STD_TYPES)
#  if (_MSC_VER < 1300)
typedef signed char      int8_t;
typedef signed short     int16_t;
typedef signed int       int32_t;
typedef signed __int64   int64_t;
typedef unsigned char    uint8_t;
typedef unsigned short   uint16_t;
typedef unsigned int     uint32_t;
typedef unsigned __int64 uint64_t;
#  else
typedef __int8           int8_t;
typedef __int16          int16_t;
typedef __int32          int32_t;
typedef __int64          int64_t;
typedef unsigned __int8  uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
#  endif
# endif
# define MPSL_INT64_C(x) (x##i64)
# define MPSL_UINT64_C(x) (x##ui64)
#else
# include <stdint.h>
# include <limits.h>
# define MPSL_INT64_C(x) (x##ll)
# define MPSL_UINT64_C(x) (x##ull)
#endif
// [@STDTYPES}@]

// ============================================================================
// [mpsl::Build - MPSL-Specific]
// ============================================================================

#define MPSL_NO_COPY(...) \
private: \
  MPSL_INLINE __VA_ARGS__(const __VA_ARGS__& other) MPSL_NOEXCEPT; \
  MPSL_INLINE __VA_ARGS__& operator=(const __VA_ARGS__& other) MPSL_NOEXCEPT; \
public:

// ============================================================================
// [blend::Build - Dependencies]
// ============================================================================

#include <new>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// [Guard]
#endif // _MPSL_BUILD_H
