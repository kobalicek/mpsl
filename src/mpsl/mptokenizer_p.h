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
    position = 0;
    length = 0;
    value = 0.0;
    token = kTokenInvalid;
  }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE uint32_t setData(size_t position, size_t length, uint32_t hValOrNType, uint32_t token) noexcept {
    this->position = position;
    this->length = length;
    this->hVal = hValOrNType;
    this->token = token;

    return token;
  }

  MPSL_INLINE uint32_t getPosAsUInt() const noexcept {
    MPSL_ASSERT(position < ~static_cast<uint32_t>(0));
    return static_cast<uint32_t>(position);
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Token position from the beginning of the input.
  size_t position;
  //! Token string length.
  size_t length;

  union {
    //! Token hash (only if the token is symbol or keyword).
    uint32_t hVal;
    //! Number type if the token is `kTokenNumber`.
    uint32_t nType;
  };

  //! Token type.
  uint32_t token;

  //! Token value (if the token is a number).
  double value;
};

// ============================================================================
// [mpsl::Tokenizer]
// ============================================================================

struct Tokenizer {
  MPSL_NO_COPY(Tokenizer)

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
    _p = _start + token->position + token->length;
    _token = *token;
  }

  //! Consume a token got by using peek().
  MPSL_INLINE void consume() noexcept {
    _token.token = kTokenInvalid;
  }

  //! Consume a token got by using peek() and call `peek()`.
  //!
  //! NOTE: Can be called only immediately after peek().
  MPSL_INLINE uint32_t consumeAndPeek(Token* token) noexcept {
    consume();
    return peek(token);
  }

  //! Consume a token got by using peek() and call `next()`.
  //!
  //! NOTE: Can be called only immediately after peek().
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
