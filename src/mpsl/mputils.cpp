// [MPSL]
// Shader-Like Mathematical Expression JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MPSL_EXPORTS

// [Dependencies - MPSL]
#include "./mputils_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

struct EnumString {
  const char name[16];
};

static const EnumString mpAstSymbolType[] = {
  { "<none>"   },
  { "typename" },
  { "operator" },
  { "variable" },
  { "function" }
};

// ============================================================================
// [mpsl::Utils]
// ============================================================================

StringBuilder& Utils::sformat(StringBuilder& sb, const char* fmt, ...) noexcept {
  va_list ap;
  va_start(ap, fmt);

  Utils::vformat(sb, fmt, ap);

  va_end(ap);
  return sb;
}

StringBuilder& Utils::vformat(StringBuilder& sb, const char* fmt, va_list ap) noexcept {
  const char kFormatChar = '%';

  const char* p = fmt;
  const char* mark = fmt;

  for (;;) {
    uint32_t c = static_cast<unsigned char>(*p);
    if (c == '\0')
      break;
    p++;

    // Don't continue if the character is not a formatting mark. In most cases
    // this branch should be taken as most of the string should be just a text.
    if (c != kFormatChar)
      continue;

    // NULL terminator after a formatting mark. It's safe to just break here.
    // The trailing mark will be appended to the string as well. However, this
    // is basically handling of an invalid `fmt` string.
    c = static_cast<unsigned char>(*p);
    if (c == '\0')
      break;

    // Handle a double-escaped formatting mark "%%", which produces "%".
    bool isEscape = (c == kFormatChar);
    sb.appendString(mark, (size_t)(p - mark) - 1 + isEscape);

    // Guess a new mark position, which is exactly two characters after the
    // initial mark when simple or a double-escaped mark formatting has been
    // used. MPSL formatting extensions will adjust the mark again.
    mark = ++p;
    if (isEscape)
      continue;

    if (c == '{') {
      // ----------------------------------------------------------------------
      // [MPSL Formatting Extensions - '%{...}']
      // ----------------------------------------------------------------------

      do {
        c = static_cast<unsigned char>(*p);
        // Invalid formatting, bail out.
        if (c == '\0') {
          mark -= 2;
          goto _Done;
        }
        p++;
      } while (c != '}');

      StringRef ext(mark, (size_t)(p - mark) - 1);
      mark = p;

      // Handle '%{StringRef}' passed as 'const StringRef*'.
      if (ext.eq("StringRef", 9)) {
        const StringRef* value = va_arg(ap, StringRef*);

        sb.appendString(value->getData(), value->getLength());
        continue;
      }

      // Handle '%{Type}' passed as 'unsigned int'.
      if (ext.eq("Type", 4)) {
        unsigned int type = va_arg(ap, unsigned int);

        Utils::formatType(sb, type);
        continue;
      }

      // Handle '%{Value}' passed as 'unsigned int, const Value*'.
      if (ext.eq("Value", 5)) {
        unsigned int type = va_arg(ap, unsigned int);
        const Value* value = va_arg(ap, const Value*);

        Utils::formatValue(sb, type, value);
        continue;
      }

      // Handle '%{SymbolType}' passed as 'unsigned int'.
      if (ext.eq("SymbolType", 10)) {
        unsigned int value = va_arg(ap, unsigned int);

        sb.appendString(mpAstSymbolType[value].name);
        continue;
      }

      // Unhandled, revert to '%'.
      mark = ext.getData() - 2;
    }
    else {
      // ----------------------------------------------------------------------
      // [C-Like Formatting Extensions]
      // ----------------------------------------------------------------------

      // Handle '%d' - C[int32_t], MPSL[int].
      if (c == 'd') {
        sb.appendInt(static_cast<int64_t>(va_arg(ap, int)), 10);
        continue;
      }

      // Handle '%u' - C[uint32_t], MPSL[uint].
      if (c == 'u') {
        sb.appendUInt(static_cast<uint64_t>(va_arg(ap, unsigned int)), 10);
        continue;
      }

      // Handle '%q' - C[int64_t] / MPSL[long].
      if (c == 'q') {
        sb.appendInt(va_arg(ap, int64_t), 10);
        continue;
      }

      // Handle '%s' - NULL terminated string.
      if (c == 's') {
        const char* s = va_arg(ap, const char*);
        if (s == nullptr) s = "<null>";

        sb.appendString(s);
        continue;
      }

      // Unhandled, revert to '%'.
      mark = p - 2;
    }
  }

_Done:
  if (mark != p)
    sb.appendString(mark, (size_t)(p - mark));

  return sb;
}

StringBuilder& Utils::formatType(StringBuilder& sb, uint32_t type) noexcept {
  uint32_t typeId = type & kTypeIdMask;
  const char* typeName = "<unknown>";

  switch (type & kTypeRW) {
    case kTypeRO: sb.appendString("const ", 6); break;
    case kTypeWO: sb.appendString("out ", 6); break;
  }

  if (typeId < kTypeCount)
    typeName = TypeInfo::get(typeId).name;

  sb.appendString(typeName);

  uint32_t nVec = (type & kTypeVecMask) >> kTypeVecShift;
  if (nVec > 0)
    sb.appendInt(nVec);

  if (type & kTypeRef)
    sb.appendString(" &", 2);

  return sb;
}

StringBuilder& Utils::formatValue(StringBuilder& sb, uint32_t type, const Value* value_) noexcept {
  uint32_t id = type & kTypeIdMask;
  uint32_t vec = type & kTypeVecMask;

  uint32_t i = 0;
  const uint8_t* value = reinterpret_cast<const uint8_t*>(value_);

  if (vec > 1)
    sb.appendChar('{');

  for (;;) {
    switch (id) {
      case kTypeVoid: {
        sb.appendString("(void)");
        break;
      }

      case kTypeBool: {
        uint32_t mask = reinterpret_cast<const uint32_t*>(value)[0];
        if (mask <= 1) {
          sb.appendString(mask == 0 ? "false" : "true");
        }
        else {
          sb.appendString("0x");
          sb.appendUInt(mask, 16, 8);
        }
        value += 4;
        break;
      }

      case kTypeQBool: {
        uint64_t mask = reinterpret_cast<const uint64_t*>(value)[0];
        if (mask <= 1) {
          sb.appendString(mask == 0 ? "false" : "true");
        }
        else {
          sb.appendString("0x");
          sb.appendUInt(mask, 16, 16);
        }
        value += 8;
        break;
      }

      case kTypeInt: {
        sb.appendInt(reinterpret_cast<const int32_t*>(value)[0]);
        value += 4;
        break;
      }

      case kTypeFloat:
      case kTypeDouble: {
        double dVal;

        if (id == kTypeFloat) {
          dVal = reinterpret_cast<const float*>(value)[0];
          value += 4;
        }
        else {
          dVal = reinterpret_cast<const double*>(value)[0];
          value += 8;
        }

        sb.appendFormat("%g", dVal);
        break;
      }

      case kTypePtr: {
        sb.appendString("__ptr");
        break;
      }
    }

    if (++i >= vec)
      break;

    sb.appendString(", ");
  }

  if (vec > 1)
    sb.appendChar('}');

  return sb;
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
