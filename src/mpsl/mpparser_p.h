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

  MPSL_NOAPI Error parseProgram(AstProgram* block) noexcept;
  MPSL_NOAPI Error parseFunction(AstProgram* block) noexcept;

  MPSL_NOAPI Error parseStatement(AstBlock* block, uint32_t flags) noexcept;
  MPSL_NOAPI Error parseBlockOrStatement(AstBlock* block) noexcept;

  MPSL_NOAPI Error parseTypedef(AstBlock* block) noexcept;
  MPSL_NOAPI Error parseVarDecl(AstBlock* block) noexcept;
  MPSL_NOAPI Error parseIfElse(AstBlock* block) noexcept;

  MPSL_NOAPI Error parseFor(AstBlock* block) noexcept;
  MPSL_NOAPI Error parseWhile(AstBlock* block) noexcept;
  MPSL_NOAPI Error parseDoWhile(AstBlock* block) noexcept;

  MPSL_NOAPI Error parseBreak(AstBlock* block) noexcept;
  MPSL_NOAPI Error parseContinue(AstBlock* block) noexcept;
  MPSL_NOAPI Error parseReturn(AstBlock* block) noexcept;

  MPSL_NOAPI Error parseExpression(AstNode** pNodeOut) noexcept;
  MPSL_NOAPI Error parseVariable(AstVar** pNodeOut) noexcept;
  MPSL_NOAPI Error parseCall(AstCall** pNodeOut) noexcept;

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
