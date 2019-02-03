// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPTOKENIZER_P_H
#define _MPSL_MPTOKENIZER_P_H

// [Dependencies - MPSL]
#include "./mpsl_p.h"
#include "./mpstrtod_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [mpsl::Token]
// ============================================================================

//! \internal
//!
//! Token type.
enum TokenType {
  kTokenInvalid = 0,  // <invalid>

  kTokenSymbol,       // <symbol>
  kTokenNumber,       // <number>

  kTokenBreak,        // break
  kTokenConst,        // const
  kTokenContinue,     // continue
  kTokenDo,           // do
  kTokenElse,         // else
  kTokenFor,          // for
  kTokenIf,           // if
  kTokenReturn,       // return
  kTokenTypeDef,      // typedef
  kTokenVoid,         // void
  kTokenWhile,        // while

  kTokenReserved,     // reserved keyword

  kTokenDot = 36,     // .
  kTokenComma,        // ,
  kTokenSemicolon,    // ;

  kTokenQMark,        // ?
  kTokenColon,        // :

  kTokenLCurl,        // {
  kTokenRCurl,        // }

  kTokenLBracket,     // [
  kTokenRBracket,     // ]

  kTokenLParen,       // (
  kTokenRParen,       // )

  kTokenAdd,          // +
  kTokenSub,          // -
  kTokenMul,          // *
  kTokenDiv,          // /
  kTokenMod,          // %
  kTokenNot,          // !

  kTokenAnd,          // &
  kTokenOr,           // |
  kTokenXor,          // ^
  kTokenBitNeg,       // ~

  kTokenAssign,       // =
  kTokenLt,           // <
  kTokenGt,           // >

  kTokenPlusPlus,     // ++
  kTokenMinusMinus,   // --

  kTokenEq,           // ==
  kTokenNe,           // !=
  kTokenLe,           // <=
  kTokenGe,           // >=

  kTokenLogAnd,       // &&
  kTokenLogOr,        // ||

  kTokenSll,          // <<
  kTokenSrl,          // >>>
  kTokenSra,          // >>

  kTokenAssignAdd,    // +=
  kTokenAssignSub,    // -=
  kTokenAssignMul,    // *=
  kTokenAssignDiv,    // /=
  kTokenAssignMod,    // %=

  kTokenAssignAnd,    // &=
  kTokenAssignOr,     // |=
  kTokenAssignXor,    // ^=
  kTokenAssignSll,    // <<=
  kTokenAssignSrl,    // >>>=
  kTokenAssignSra,    // >>=

  kTokenEnd           // <end>
};

// ============================================================================
// [mpsl::Token]
// ============================================================================

//! \internal
//!
//! Token.
struct Token {
  // --------------------------------------------------------------------------
  // [Reset]
  // --------------------------------------------------------------------------

  MPSL_INLINE void reset() noexcept {
    _position = 0;
    _size = 0;
    _value = 0.0;
    _tokenType = kTokenInvalid;
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE uint32_t setData(size_t position, size_t size, uint32_t hashCodeOrNumberType, uint32_t tokenType) noexcept {
    _position = position;
    _size = size;
    _hashCode = hashCodeOrNumberType;
    _tokenType = tokenType;

    return tokenType;
  }

  MPSL_INLINE uint32_t tokenType() const noexcept { return _tokenType; }
  MPSL_INLINE uint32_t hashCode() const noexcept { return _hashCode; }
  MPSL_INLINE uint32_t numberType() const noexcept { return _numberType; }

  MPSL_INLINE size_t position() const noexcept { return _position; }
  MPSL_INLINE uint32_t positionAsUInt() const noexcept {
    MPSL_ASSERT(_position < ~static_cast<uint32_t>(0));
    return static_cast<uint32_t>(_position);
  }

  MPSL_INLINE size_t size() const noexcept { return _size; }
  MPSL_INLINE double value() const noexcept { return _value; }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint32_t _tokenType;                   //!< Token type.
  union {
    uint32_t _hashCode;                  //!< Token hash (only if the token is symbol or keyword).
    uint32_t _numberType;                //!< Number type if the token is `kTokenNumber`.
  };
  size_t _position;                      //!< Token position from the beginning of the input.
  size_t _size;                          //!< Token string size.
  double _value;                         //!< Token value (if the token is a number).
};

// ============================================================================
// [mpsl::Tokenizer]
// ============================================================================

struct Tokenizer {
  MPSL_NONCOPYABLE(Tokenizer)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE Tokenizer(const char* s, size_t sLen) noexcept
    : _p(s),
      _start(s),
      _end(s + sLen),
      _strtod() {
    _token.reset();
  }

  // --------------------------------------------------------------------------
  // [Ops]
  // --------------------------------------------------------------------------

  //! Get the current token.
  uint32_t peek(Token* token) noexcept;
  //! Get the current token and advance.
  uint32_t next(Token* token) noexcept;

  //! Set the token that will be returned by `next()` and `peek()` functions.
  MPSL_INLINE void set(Token* token) noexcept {
    // We have to update also _p in case that multiple tokens were put back.
    _p = _start + token->position() + token->size();
    _token = *token;
  }

  //! Consume a token got by using peek().
  MPSL_INLINE void consume() noexcept {
    _token._tokenType = kTokenInvalid;
  }

  //! Consume a token got by using peek() and call `peek()`.
  //!
  //! \note Can be called only immediately after peek().
  MPSL_INLINE uint32_t consumeAndPeek(Token* token) noexcept {
    consume();
    return peek(token);
  }

  //! Consume a token got by using peek() and call `next()`.
  //!
  //! \note Can be called only immediately after peek().
  MPSL_INLINE uint32_t consumeAndNext(Token* token) noexcept {
    consume();
    return next(token);
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  const char* _p;
  const char* _start;
  const char* _end;

  StrToD _strtod;
  Token _token;
};

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPTOKENIZER_P_H
