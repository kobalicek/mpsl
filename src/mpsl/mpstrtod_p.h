// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPSTRTOD_P_H
#define _MPSL_MPSTRTOD_P_H

// [Dependencies]
#include "./mpsl_p.h"

#if defined(_WIN32)
  #define MPSL_STRTOD_MSLOCALE
  #include <locale.h>
  #include <stdlib.h>
#else
  #define MPSL_STRTOD_XLOCALE
  #include <locale.h>
  #include <stdlib.h>
  // xlocale.h is not available on Linux anymore, it uses <locale.h>.
  #if defined(__APPLE__    ) || \
      defined(__bsdi__     ) || \
      defined(__DragonFly__) || \
      defined(__FreeBSD__  ) || \
      defined(__NetBSD__   ) || \
      defined(__OpenBSD__  )
    #include <xlocale.h>
  #endif
#endif

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [mpsl::StrToD]
// ============================================================================

struct StrToD {
  MPSL_NONCOPYABLE(StrToD)

#if defined(MPSL_STRTOD_MSLOCALE)
  MPSL_INLINE StrToD() noexcept {
    handle = _create_locale(LC_ALL, "C");
  }
  MPSL_INLINE ~StrToD() noexcept {
    _free_locale(handle);
  }

  MPSL_INLINE bool isOk() const noexcept {
    return handle != nullptr;
  }
  MPSL_INLINE double conv(const char* s, char** end) const noexcept {
    return _strtod_l(s, end, handle);
  }

  _locale_t handle;
#elif defined(MPSL_STRTOD_XLOCALE)
  MPSL_INLINE StrToD() noexcept {
    handle = newlocale(LC_ALL_MASK, "C", nullptr);
  }
  MPSL_INLINE ~StrToD() noexcept {
    freelocale(handle);
  }

  MPSL_INLINE bool isOk() const noexcept {
    return handle != nullptr;
  }
  MPSL_INLINE double conv(const char* s, char** end) const noexcept {
    return strtod_l(s, end, handle);
  }

  locale_t handle;
#else
  // Time bomb!
  MPSL_INLINE bool isOk() const noexcept {
    return true;
  }
  MPSL_INLINE double conv(const char* s, char** end) const noexcept {
    return strtod(s, end);
  }
#endif
};

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPSTRTOD_P_H
