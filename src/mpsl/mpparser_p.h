// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Guard]
#ifndef _MPSL_MPPARSER_P_H
#define _MPSL_MPPARSER_P_H

// [Dependencies - MPSL]
#include "./mpast_p.h"
#include "./mptokenizer_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [Forward Declaration]
// ============================================================================

class AstBuilder;
class AstBlock;
class AstNode;
class AstProgram;
class AstVar;

// ============================================================================
// [mpsl::Parser]
// ============================================================================

class Parser {
public:
  MPSL_NONCOPYABLE(Parser)

  enum Flags {
    kNoFlags = 0x00,
    kEnableNewSymbols = 0x01,
    kEnableNestedBlock = 0x02
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE Parser(AstBuilder* ast, ErrorReporter* errorReporter, const char* body, size_t len) noexcept
    : _ast(ast),
      _errorReporter(errorReporter),
      _currentScope(ast->getGlobalScope()),
      _tokenizer(body, len) {}
  MPSL_INLINE ~Parser() noexcept {}

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstScope* getCurrentScope() const noexcept { return _currentScope; }

  // --------------------------------------------------------------------------
  // [Parse]
  // --------------------------------------------------------------------------

  Error parseProgram(AstProgram* block) noexcept;
  Error parseFunction(AstProgram* block) noexcept;

  Error parseStatement(AstBlock* block, uint32_t flags) noexcept;
  Error parseBlockOrStatement(AstBlock* block) noexcept;

  Error parseTypedef(AstBlock* block) noexcept;
  Error parseVarDecl(AstBlock* block) noexcept;
  Error parseIfElse(AstBlock* block) noexcept;

  Error parseFor(AstBlock* block) noexcept;
  Error parseWhile(AstBlock* block) noexcept;
  Error parseDoWhile(AstBlock* block) noexcept;

  Error parseBreak(AstBlock* block) noexcept;
  Error parseContinue(AstBlock* block) noexcept;
  Error parseReturn(AstBlock* block) noexcept;

  Error parseExpression(AstNode** pNodeOut) noexcept;
  Error parseVariable(AstVar** pNodeOut) noexcept;
  Error parseCall(AstCall** pNodeOut) noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  AstBuilder* _ast;
  ErrorReporter* _errorReporter;

  AstScope* _currentScope;
  Tokenizer _tokenizer;
};

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"

// [Guard]
#endif // _MPSL_MPPARSER_P_H
