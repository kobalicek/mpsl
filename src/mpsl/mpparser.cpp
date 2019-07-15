// [MPSL]
// MathPresso's Shading Language with JIT Engine for C++.
//
// [License]
// Zlib - See LICENSE.md file in the package.

// [Export]
#define MPSL_EXPORTS

// [Dependencies - MPSL]
#include "./mpparser_p.h"

// [Api-Begin]
#include "./mpsl_apibegin.h"

namespace mpsl {

// ============================================================================
// [mpsl::Parser - Macros]
// ============================================================================

#define MPSL_PARSER_WARNING(TOKEN, ...)                                       \
  _errorReporter->onWarning(                                                  \
    static_cast<uint32_t>(TOKEN.position()), __VA_ARGS__)

#define MPSL_PARSER_ERROR(TOKEN, ...)                                         \
  return _errorReporter->onError(                                             \
    kErrorInvalidSyntax, static_cast<uint32_t>(TOKEN.position()), __VA_ARGS__)

// ============================================================================
// [mpsl::AstNestedScope]
// ============================================================================

//! \internal
//!
//! Nested scope used only by the parser and always allocated statically.
class AstNestedScope : public AstScope {
public:
  MPSL_NONCOPYABLE(AstNestedScope)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  MPSL_INLINE AstNestedScope(Parser* parser) noexcept
    : AstScope(parser->_ast, parser->_currentScope, AstScope::kTypeNested),
      _parser(parser) {
    _parser->_currentScope = this;
  }

  MPSL_INLINE ~AstNestedScope() noexcept {
    AstScope* p = parent();
    MPSL_ASSERT(p != nullptr);

    _parser->_currentScope = p;
    p->_symbols.mergeToInvisibleSlot(this->_symbols);
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  Parser* _parser;
};

// ============================================================================
// [mpsl::Parser - Parse]
// ============================================================================

Error Parser::parseProgram(AstProgram* block) noexcept {
  Token token;
  uint32_t uToken;

  for (;;) {
    uToken = _tokenizer.peek(&token);

    // Parse a typedef declaration.
    if (uToken == kTokenTypeDef) {
      MPSL_PROPAGATE(parseTypedef(block));
    }
    // Parse a const expression.
    else if (uToken == kTokenConst) {
      MPSL_PROPAGATE(parseVarDecl(block));
    }
    // Parse a function declaration.
    else if (uToken == kTokenVoid || uToken == kTokenSymbol) {
      MPSL_PROPAGATE(parseFunction(block));
    }
    // End of input.
    else if (uToken == kTokenEnd) {
      break;
    }
    else {
      MPSL_PARSER_ERROR(token, "Unexpected token.");
    }
  }

  return kErrorOk;
}

// Parse "<type> <function>([<type> <var>[, ...]]) <block>"
Error Parser::parseFunction(AstProgram* block) noexcept {
  Token token;
  uint32_t uToken;
  StringRef str;

  AstScope* globalScope = _ast->globalScope();
  AstScope* localScope;

  MPSL_PROPAGATE(block->willAdd());
  MPSL_NULLCHECK(localScope = _ast->newScope(globalScope, AstScope::kTypeLocal));

  // Create `AstFunction` and essential sub-nodes.
  AstFunction* func = _ast->newNode<AstFunction>();
  MPSL_NULLCHECK_(func, { _ast->deleteScope(localScope); });

  block->appendNode(func);

  AstBlock* args = _ast->newNode<AstBlock>();
  MPSL_NULLCHECK(args);
  func->setArgs(args);

  AstBlock* body = _ast->newNode<AstBlock>();
  MPSL_NULLCHECK(body);
  func->setBody(body);

  // Parse the function return type.
  uToken = _tokenizer.next(&token);
  func->setPosition(token.positionAsUInt());

  if (uToken == kTokenSymbol) {
    str.set(_tokenizer._start + token.position(), token.size());
    AstSymbol* retType = localScope->resolveSymbol(str, token.hashCode());

    if (retType == nullptr || !retType->isTypeName())
      MPSL_PARSER_ERROR(token, "Expected type-name.");
    func->setRet(retType);
  }
  else if (uToken != kTokenVoid) {
    MPSL_PARSER_ERROR(token, "Expected type-name.");
  }

  // Parse the function name.
  if (_tokenizer.next(&token) != kTokenSymbol)
    MPSL_PARSER_ERROR(token, "Expected a function name.");

  str.set(_tokenizer._start + token.position(), token.size());
  AstSymbol* funcSym = localScope->resolveSymbol(str, token.hashCode());

  if (funcSym)
    MPSL_PARSER_ERROR(token, "Attempt to redefine '%s'.", funcSym->name());

  funcSym = _ast->newSymbol(str, token.hashCode(), AstSymbol::kTypeFunction, globalScope->scopeType());
  MPSL_NULLCHECK(funcSym);

  // Function name has to be put into the parent scope, otherwise the symbol
  // will not be visible.
  globalScope->putSymbol(funcSym);

  // Link.
  func->setFunc(funcSym);
  funcSym->setNode(func);

  // Parse the function arguments list.
  if (_tokenizer.next(&token) != kTokenLParen)
    MPSL_PARSER_ERROR(token, "Expected '(' token after a function name.");

  args->setPosition(token.positionAsUInt());
  uToken = _tokenizer.next(&token);

  if (uToken != kTokenRParen) {
    for (;;) {
      // Parse the argument type.
      if (uToken != kTokenSymbol)
        MPSL_PARSER_ERROR(token, "Expected an argument type.");

      // Resolve the argument type.
      str.set(_tokenizer._start + token.position(), token.size());
      AstSymbol* argType = localScope->resolveSymbol(str, token.hashCode());

      if (argType == nullptr || !argType->isTypeName())
        MPSL_PARSER_ERROR(token, "Expected an argument type.");

      // Parse the argument name.
      if (_tokenizer.next(&token) != kTokenSymbol)
        MPSL_PARSER_ERROR(token, "Expected an argument name.");

      // Resolve the argument name.
      str.set(_tokenizer._start + token.position(), token.size());
      AstScope* argScope;
      AstSymbol* argSym = localScope->resolveSymbol(str, token.hashCode(), &argScope);

      if (argSym) {
        if (!argSym->isVariable())
          MPSL_PARSER_ERROR(token, "Can't use %{SymbolType} '%s' as an argument name.", argSym->symbolType(), argSym->name());

        if (argScope == localScope)
          MPSL_PARSER_ERROR(token, "Can't redeclare argument '%s'.", argSym->name());
      }

      argSym = _ast->newSymbol(str, token.hashCode(), AstSymbol::kTypeVariable, AstScope::kTypeLocal);
      MPSL_NULLCHECK(argSym);

      argSym->setDeclared();
      argSym->setTypeInfo(argType->typeInfo());
      localScope->putSymbol(argSym);

      // Create/Append the argument node.
      MPSL_PROPAGATE(args->willAdd());
      AstVarDecl* argDecl = _ast->newNode<AstVarDecl>();
      MPSL_NULLCHECK(argDecl);

      argDecl->setPosition(token.positionAsUInt());
      argDecl->setSymbol(argSym);
      argDecl->setTypeInfo(argSym->typeInfo());

      argSym->setNode(argDecl);
      args->appendNode(argDecl);

      // Parse ')' or ',' tokens.
      uToken = _tokenizer.next(&token);
      if (uToken == kTokenRParen)
        break;

      if (uToken != kTokenComma)
        MPSL_PARSER_ERROR(token, "Expected ',' or ')' tokens.");

      // Parse the symbol, again...
      uToken = _tokenizer.next(&token);
    }
  }

  // Parse the function body.
  if (_tokenizer.peek(&token) != kTokenLCurl)
    MPSL_PARSER_ERROR(token, "Expected a function body starting with '{'.");

  _currentScope = localScope;
  Error err = parseBlockOrStatement(body);
  _currentScope = globalScope;

  // Make the function declared so it can be used after now.
  funcSym->setDeclared();

  return err;
}

// Parse <statement>; or { [<statement>; ...] }
Error Parser::parseStatement(AstBlock* block, uint32_t flags) noexcept {
  Token token;
  uint32_t uToken = _tokenizer.peek(&token);

  // Parse the typedef declaration.
  if (uToken == kTokenTypeDef) {
    if (!(flags & kEnableNewSymbols))
      MPSL_PARSER_ERROR(token, "Cannot declare a new typedef here.");
    return parseTypedef(block);
  }

  // Parse the const expression.
  if (uToken == kTokenConst) {
    if (!(flags & kEnableNewSymbols))
      MPSL_PARSER_ERROR(token, "Cannot declare a new variable here.");
    return parseVarDecl(block);
  }

  // Parse the if-else branch.
  if (uToken == kTokenIf)
    return parseIfElse(block);

  // Parse the for loop.
  if (uToken == kTokenFor)
    return parseFor(block);

  // Parse the while loop.
  if (uToken == kTokenWhile)
    return parseWhile(block);

  // Parse the do-while loop.
  if (uToken == kTokenDo)
    return parseDoWhile(block);

  // Parse the break.
  if (uToken == kTokenBreak)
    return parseBreak(block);

  // Parse the continue.
  if (uToken == kTokenContinue)
    return parseContinue(block);

  // Parse the return.
  if (uToken == kTokenReturn)
    return parseReturn(block);

  // Parse the ';' token.
  if (uToken == kTokenSemicolon) {
    _tokenizer.consume();
    return kErrorOk;
  }

  // Parse a nested block.
  if (uToken == kTokenLCurl) {
    AstBlock* nested;

    if (!(flags & kEnableNestedBlock))
      MPSL_PARSER_ERROR(token, "Cannot declare a new block-scope here.");

    MPSL_PROPAGATE(block->willAdd());
    MPSL_NULLCHECK(nested = _ast->newNode<AstBlock>());
    block->appendNode(nested);

    AstNestedScope tmpScope(this);
    return parseBlockOrStatement(nested);
  }

  // Parse a variable declaration.
  if (uToken == kTokenSymbol) {
    StringRef str(_tokenizer._start + token.position(), token.size());
    AstSymbol* sym = _currentScope->resolveSymbol(str, token.hashCode());

    if (sym && sym->isTypeName()) {
      if (!(flags & kEnableNewSymbols))
        MPSL_PARSER_ERROR(token, "Cannot declare a new variable here.");
      return parseVarDecl(block);
    }
  }

  // Parse an expression.
  AstNode* expression;

  MPSL_PROPAGATE(block->willAdd());
  MPSL_PROPAGATE(parseExpression(&expression));
  block->appendNode(expression);

  if (_tokenizer.next(&token) != kTokenSemicolon)
    MPSL_PARSER_ERROR(token, "Expected ';' after an expression.");

  return kErrorOk;
}

// Parse <block|statement>;.
Error Parser::parseBlockOrStatement(AstBlock* block) noexcept {
  Token token;
  uint32_t uToken = _tokenizer.peek(&token);

  if (uToken == kTokenLCurl) {
    // Parse <block>, consume '{' token.
    block->setPosition(token.positionAsUInt());
    _tokenizer.consume();

    for (;;) {
      uToken = _tokenizer.peek(&token);

      // Parse the end of the block '}'.
      if (uToken == kTokenRCurl) {
        // Consume the '}' token.
        _tokenizer.consume();
        return kErrorOk;
      }
      else {
        MPSL_PROPAGATE(parseStatement(block, kEnableNewSymbols | kEnableNestedBlock));
      }
    }
  }
  else {
    return parseStatement(block, kNoFlags);
  }
}

// Parse "typedef <type> <synonym>".
Error Parser::parseTypedef(AstBlock* block) noexcept {
  Token token;
  StringRef str;

  // Parse the 'typedef' statement.
  _tokenizer.next(&token);
  MPSL_ASSERT(token.tokenType() == kTokenTypeDef);

  // Parse the type-name.
  if (_tokenizer.next(&token) != kTokenSymbol)
    MPSL_PARSER_ERROR(token, "Expected type-name after 'typedef' declaration.");

  AstScope* scope = _currentScope;

  // Resolve the type-name.
  str.set(_tokenizer._start + token.position(), token.size());
  AstSymbol* typeSym = scope->resolveSymbol(str, token.hashCode());

  if (typeSym == nullptr || !typeSym->isTypeName())
    MPSL_PARSER_ERROR(token, "Unresolved type-name after 'typedef' declaration.");

  // Parse the synonym.
  if (_tokenizer.next(&token) != kTokenSymbol)
    MPSL_PARSER_ERROR(token, "Expected a new type-name after the type-name.");

  // Create the synonym.
  str.set(_tokenizer._start + token.position(), token.size());
  AstSymbol* synonym = scope->resolveSymbol(str, token.hashCode());

  if (synonym)
    MPSL_PARSER_ERROR(token, "Attempt to redefine '%s'.", synonym->name());

  synonym = _ast->newSymbol(str, token.hashCode(), AstSymbol::kTypeTypeName, scope->scopeType());
  MPSL_NULLCHECK(synonym);

  synonym->setDeclared();
  synonym->setTypeInfo(typeSym->typeInfo());
  scope->putSymbol(synonym);

  // Parse the ';' token.
  if (_tokenizer.next(&token) != kTokenSemicolon)
    MPSL_PARSER_ERROR(token, "Expected ';' after 'typedef' declaration.");

  return kErrorOk;
}

// Parse "[const] <type> <var> = <expression>[, <var> = <expression>, ...];".
Error Parser::parseVarDecl(AstBlock* block) noexcept {
  Token token;
  uint32_t uToken = _tokenizer.next(&token);
  StringRef str;

  bool isFirst = true;
  bool isConst = uToken == kTokenConst;
  uint32_t position = token.positionAsUInt();

  // Parse the optional 'const' keyword and type-name.
  if (isConst)
    uToken = _tokenizer.next(&token);

  // Parse the type-name.
  if (uToken != kTokenSymbol)
    MPSL_PARSER_ERROR(token, "Expected type-name.");

  // Resolve the type-name.
  str.set(_tokenizer._start + token.position(), token.size());

  AstScope* scope = _currentScope;
  AstSymbol* typeSym = scope->resolveSymbol(str, token.hashCode());

  if (typeSym == nullptr || !typeSym->isTypeName())
    MPSL_PARSER_ERROR(token, "Expected type-name.");

  for (;;) {
    // Parse the variable name.
    if (_tokenizer.next(&token) != kTokenSymbol)
      MPSL_PARSER_ERROR(token, isFirst
        ? "Expected a variable name after type-name."
        : "Expected a variable name after colon ','.");

    MPSL_PROPAGATE(block->willAdd());

    if (!isFirst)
      position = token.positionAsUInt();

    // Resolve the variable name.
    AstSymbol* vSym;
    AstScope* vScope;

    str.set(_tokenizer._start + token.position(), token.size());
    if ((vSym = scope->resolveSymbol(str, token.hashCode(), &vScope)) != nullptr) {
      if (!vSym->isVariable() || scope == vScope)
        MPSL_PARSER_ERROR(token, "Attempt to redefine %{SymbolType} '%s'.", vSym->symbolType(), vSym->name());

      if (vSym->node()) {
        uint32_t line, column;
        _errorReporter->getLineAndColumn(vSym->node()->position(), line, column);
        MPSL_PARSER_WARNING(token, "Variable '%s' shadows a variable declared at [%d:%d].", vSym->name(), line, column);
      }
      else {
        MPSL_PARSER_WARNING(token, "Variable '%s' shadows a variable of the same name.", vSym->name());
      }
    }

    vSym = _ast->newSymbol(str, token.hashCode(), AstSymbol::kTypeVariable, scope->scopeType());
    MPSL_NULLCHECK(vSym);

    uint32_t typeInfo = typeSym->typeInfo();

    if (isConst)
      typeInfo |= kTypeRead;
    else
      typeInfo |= kTypeRead | kTypeWrite;

    vSym->setTypeInfo(typeInfo);
    scope->putSymbol(vSym);

    AstVarDecl* decl = _ast->newNode<AstVarDecl>();
    MPSL_NULLCHECK_(decl, { _ast->deleteSymbol(vSym); });

    decl->setPosition(position);
    decl->setSymbol(vSym);
    vSym->setNode(decl);

    // Parse possible assignment '='.
    uToken = _tokenizer.next(&token);
    bool isAssigned = (uToken == kTokenAssign);

    if (isAssigned) {
      AstNode* expression;
      MPSL_PROPAGATE_(parseExpression(&expression), { _ast->deleteNode(decl); });
      decl->setChild(expression);
      uToken = _tokenizer.next(&token);
    }

    // Make the symbol declared so it can be referenced after now.
    vSym->setDeclared();

    // Parse the ',' or ';' tokens.
    if (uToken == kTokenComma || uToken == kTokenSemicolon) {
      if (isConst && !isAssigned)
        MPSL_PARSER_ERROR(token, "Unassigned constant '%s'.", vSym->name());

      block->appendNode(decl);

      // Token ';' terminates the declaration.
      if (uToken == kTokenSemicolon)
        break;
    }
    else {
      _ast->deleteSymbol(vSym);
      MPSL_PARSER_ERROR(token, "Unexpected token.");
    }

    isFirst = false;
  }

  return kErrorOk;
}

// Parse: "if (<condition>) <block|statement> [else <block|statement>]".
Error Parser::parseIfElse(AstBlock* block) noexcept {
  Token token;

  // Parse the 'if' statement.
  _tokenizer.next(&token);
  MPSL_ASSERT(token.tokenType() == kTokenIf);
  uint32_t position = token.positionAsUInt();

  // Parse the '(' token.
  if (_tokenizer.next(&token) != kTokenLParen)
    MPSL_PARSER_ERROR(token, "Expected '(' after 'if' keyword.");

  AstBranch* branch;

  MPSL_PROPAGATE(block->willAdd());
  MPSL_NULLCHECK(branch = _ast->newNode<AstBranch>());

  // block->appendNode(branch);
  branch->setPosition(position);
  position = token.positionAsUInt();

  //bool isLast = false;
  AstBranch* first = branch;
  for (;;) {
    // Parse the condition inside '(' and ')'.
    AstNode* condition;
    MPSL_PROPAGATE_(parseExpression(&condition), { _ast->deleteNode(first); });
    branch->setCondition(condition);

    if (_tokenizer.next(&token) != kTokenRParen) {
      _ast->deleteNode(first);
      MPSL_PARSER_ERROR(token, "Expected ')' after the end of condition.");
    }

    // Parse then branch.
    AstBlock* thenBody = _ast->newNode<AstBlock>();
    MPSL_NULLCHECK_(thenBody, { _ast->deleteNode(first); });
    branch->setThenBody(thenBody);
    MPSL_PROPAGATE_(parseBlockOrStatement(thenBody), { _ast->deleteNode(first); });

    // Parse else branch.
    if (_tokenizer.peek(&token) == kTokenElse) {
      position = token.positionAsUInt();
      if (_tokenizer.consumeAndPeek(&token) == kTokenIf) {
        // Parse "else if ...".
        MPSL_NULLCHECK_(branch = _ast->newNode<AstBranch>(), { _ast->deleteNode(first); });
        branch->setElseBody(branch);

        // Parse the '(' token.
        if (_tokenizer.consumeAndNext(&token) != kTokenLParen) {
          _ast->deleteNode(first);
          MPSL_PARSER_ERROR(token, "Expected '(' after 'else if' keyword.");
        }

        // And continue parsing the condition inside '(' and ')'.
        continue;
      }
      else {
        // Parse "else <block|statement>".
        AstBlock* elseBody = _ast->newNode<AstBlock>();
        MPSL_NULLCHECK_(elseBody, { _ast->deleteNode(first); });
        branch->setElseBody(elseBody);
        MPSL_PROPAGATE_(parseBlockOrStatement(elseBody), { _ast->deleteNode(first); });

        block->appendNode(first);
        return kErrorOk;
      }
    }
    else {
      block->appendNode(first);
      return kErrorOk;
    }
  }
}

// Parse: "for (<forInit>; <condition>; <iter>) <block|statement>".
Error Parser::parseFor(AstBlock* block) noexcept {
  Token token;

  // Parse the 'for' statement.
  _tokenizer.next(&token);
  MPSL_ASSERT(token.tokenType() == kTokenFor);

  // Create/Append the 'AstLoop' and essential sub-nodes.
  AstNestedScope tmpScope(this);
  AstLoop* for_;

  MPSL_PROPAGATE(block->willAdd());
  MPSL_NULLCHECK(for_ = _ast->newNode<AstLoop>(AstNode::kTypeFor));

  for_->setPosition(token.positionAsUInt());
  block->appendNode(for_);

  AstBlock* forInit = _ast->newNode<AstBlock>();
  MPSL_NULLCHECK(forInit);
  for_->setForInit(forInit);

  AstBlock* iter = _ast->newNode<AstBlock>();
  MPSL_NULLCHECK(iter);
  for_->setForIter(iter);

  AstBlock* body = _ast->newNode<AstBlock>();
  MPSL_NULLCHECK(body);
  for_->setBody(body);

  // Parse the '(' token.
  if (_tokenizer.next(&token) != kTokenLParen)
    MPSL_PARSER_ERROR(token, "Expected '(' after the 'for' statement.");

  // Parse the <forInit> section.
  bool hasVarDecl = false;

  uint32_t uToken = _tokenizer.peek(&token);
  forInit->setPosition(token.positionAsUInt());

  if (uToken == kTokenSymbol) {
    StringRef str(_tokenizer._start + token.position(), token.size());
    AstSymbol* sym = tmpScope.resolveSymbol(str, token.hashCode());

    if (sym && sym->isTypeName()) {
      MPSL_PROPAGATE(parseVarDecl(forInit));
      hasVarDecl = true;
    }
  }

  if (!hasVarDecl) {
    if (_tokenizer.peek(&token) == kTokenSemicolon) {
      _tokenizer.consume();
    }
    else {
      AstNode* expression;

      MPSL_PROPAGATE(forInit->willAdd());
      MPSL_PROPAGATE(parseExpression(&expression));
      forInit->appendNode(expression);

      if (_tokenizer.next(&token) != kTokenSemicolon)
        MPSL_PARSER_ERROR(token, "Expected ';' after the 'for' initializer.");
    }
  }

  // Parse the <condition> section.
  if (_tokenizer.peek(&token) != kTokenSemicolon) {
    AstNode* condition;
    MPSL_PROPAGATE(parseExpression(&condition));
    for_->setCondition(condition);
  }

  // Parse the ';' token after <condition>.
  if (_tokenizer.next(&token) != kTokenSemicolon)
    MPSL_PARSER_ERROR(token, "Expected ';' after the 'for' condition.");

  // Parse the <iter> section including ')' token.
  uToken = _tokenizer.peek(&token);
  iter->setPosition(token.positionAsUInt());

  if (uToken != kTokenRParen) {
    for (;;) {
      AstNode* expression;

      MPSL_PROPAGATE(iter->willAdd());
      MPSL_PROPAGATE(parseExpression(&expression));
      iter->appendNode(expression);

      // Parse the ')' token - end of the <iter> section.
      uint32_t uToken = _tokenizer.next(&token);
      if (uToken == kTokenRParen)
        break;

      // Parse the ',' token - <iter> separator.
      if (uToken != kTokenColon)
        MPSL_PARSER_ERROR(token, "Expected ',' or ')' after iterator.");
    }
  }

  // Parse the body.
  return parseBlockOrStatement(body);
}

// Parse: "while (<condition>) <block|statement>".
Error Parser::parseWhile(AstBlock* block) noexcept {
  Token token;

  // Parse the 'while' statement.
  _tokenizer.next(&token);
  MPSL_ASSERT(token.tokenType() == kTokenWhile);

  // Create/Append the 'AstLoop' and essential sub-nodes.
  AstNestedScope tmpScope(this);
  AstLoop* while_;

  MPSL_PROPAGATE(block->willAdd());
  MPSL_NULLCHECK(while_ = _ast->newNode<AstLoop>(AstNode::kTypeWhile));

  while_->setPosition(token.positionAsUInt());
  block->appendNode(while_);

  AstBlock* body = _ast->newNode<AstBlock>();
  MPSL_NULLCHECK(body);
  while_->setBody(body);

  // Parse the '(' token.
  if (_tokenizer.next(&token) != kTokenLParen)
    MPSL_PARSER_ERROR(token, "Expected '(' after the 'while' statement.");

  // Parse the <condition> section.
  AstNode* condition;
  MPSL_PROPAGATE(parseExpression(&condition));
  while_->setCondition(condition);

  // Parse the ')' token.
  if (_tokenizer.next(&token) != kTokenRParen)
    MPSL_PARSER_ERROR(token, "Expected ')' after the 'while' condition.");

  // Parse the <block|statement>.
  MPSL_PROPAGATE(parseBlockOrStatement(body));

  return kErrorOk;
}

// Parse: "do <block|statement> while (<condition>)".
Error Parser::parseDoWhile(AstBlock* block) noexcept {
  Token token;

  // Parse the 'do' statement.
  _tokenizer.next(&token);
  MPSL_ASSERT(token.tokenType() == kTokenDo);

  // Create/Append the 'AstLoop' and essential sub-nodes.
  AstNestedScope tmpScope(this);
  AstLoop* doWhile;

  MPSL_PROPAGATE(block->willAdd());
  MPSL_NULLCHECK(doWhile = _ast->newNode<AstLoop>(AstNode::kTypeDoWhile));

  doWhile->setPosition(token.positionAsUInt());
  block->appendNode(doWhile);

  AstBlock* body = _ast->newNode<AstBlock>();
  MPSL_NULLCHECK(body);
  doWhile->setBody(body);

  // Parse the body.
  MPSL_PROPAGATE(parseBlockOrStatement(doWhile->body()));

  // Parse the 'while' statement.
  if (_tokenizer.next(&token) != kTokenWhile)
    MPSL_PARSER_ERROR(token, "Expected 'while' keyword after the 'do-while' body.");

  // Parse the '(' token.
  if (_tokenizer.next(&token) != kTokenLParen)
    MPSL_PARSER_ERROR(token, "Expected '(' after the 'while' statement.");

  // Parse the <condition>.
  AstNode* condition;
  MPSL_PROPAGATE(parseExpression(&condition));
  doWhile->setCondition(condition);

  // Parse the ')' token.
  if (_tokenizer.next(&token) != kTokenRParen)
    MPSL_PARSER_ERROR(token, "Expected ')' after the 'do-while' condition.");

  // Parse the ';' token.
  if (_tokenizer.next(&token) != kTokenSemicolon)
    MPSL_PARSER_ERROR(token, "Expected ';' after the 'do-while' block.");

  return kErrorOk;
}

// Parse: "break".
Error Parser::parseBreak(AstBlock* block) noexcept {
  Token token;

  // Parse the 'break' keyword.
  _tokenizer.next(&token);
  MPSL_ASSERT(token.tokenType() == kTokenBreak);
  uint32_t position = token.positionAsUInt();

  // Parse the return expression or ';'.
  if (_tokenizer.next(&token) != kTokenSemicolon)
    MPSL_PARSER_ERROR(token, "Expected ';' after 'break' keyword.");

  AstBreak* node;
  MPSL_PROPAGATE(block->willAdd());
  MPSL_NULLCHECK(node = _ast->newNode<AstBreak>());

  node->setPosition(position);
  block->appendNode(node);

  return kErrorOk;
}

// Parse: "continue".
Error Parser::parseContinue(AstBlock* block) noexcept {
  Token token;

  // Parse the 'continue' keyword.
  _tokenizer.next(&token);
  MPSL_ASSERT(token.tokenType() == kTokenContinue);
  uint32_t position = token.positionAsUInt();

  // Parse the return expression or ';'.
  if (_tokenizer.next(&token) != kTokenSemicolon)
    MPSL_PARSER_ERROR(token, "Expected ';' after 'continue' keyword.");

  AstContinue* node;
  MPSL_PROPAGATE(block->willAdd());
  MPSL_NULLCHECK(node = _ast->newNode<AstContinue>());

  node->setPosition(position);
  block->appendNode(node);

  return kErrorOk;
}

// Parse: "return <statement>".
Error Parser::parseReturn(AstBlock* block) noexcept {
  Token token;

  // Parse the 'return' keyword.
  _tokenizer.next(&token);
  MPSL_ASSERT(token.tokenType() == kTokenReturn);

  AstReturn* ret;
  MPSL_PROPAGATE(block->willAdd());
  MPSL_NULLCHECK(ret = _ast->newNode<AstReturn>());

  ret->setPosition(token.positionAsUInt());
  block->appendNode(ret);

  // Parse the return expression or ';'.
  if (_tokenizer.peek(&token) != kTokenSemicolon) {
    AstNode* expression;
    MPSL_PROPAGATE(parseExpression(&expression));
    ret->setChild(expression);

    if (_tokenizer.next(&token) != kTokenSemicolon)
      MPSL_PARSER_ERROR(token, "Expected ';' after 'return' keyword.");
    return kErrorOk;
  }
  else {
    _tokenizer.consume();
    return kErrorOk;
  }
}

Error Parser::parseExpression(AstNode** pNode) noexcept {
  AstScope* scope = _currentScope;

  // It's important that the given expression is parsed in a way that it can be
  // correctly evaluated. The `parseExpression()` function can handle expressions
  // that contain unary and binary operators combined with terminals (variables,
  // constants or function calls).
  //
  // The most expression parsers usually use stack to handle operator precedence,
  // but MPSL uses AstNode parent->child hierarchy instead. When operator with a
  // higher precedence is found it traverses down in the hierarchy and when
  // operator with the same/lower precedence is found the hierarchy is traversed
  // back.
  //
  //                               AST Examples
  //
  // +-----------------+-----------------+-----------------+-----------------+
  // |                 |                 |                 |                 |
  // |   "a + b + c"   |   "a * b + c"   |   "a + b * c"   |   "a * b * c"   |
  // |                 |                 |                 |                 |
  // |       (+)       |       (+)       |       (+)       |       (*)       |
  // |      /   \      |      /   \      |      /   \      |      /   \      |
  // |   (+)     (c)   |   (*)     (c)   |   (a)     (*)   |   (*)     (c)   |
  // |   / \           |   / \           |           / \   |   / \           |
  // | (a) (b)         | (a) (b)         |         (b) (c) | (a) (b)         |
  // |                 |                 |                 |                 |
  // +-----------------+-----------------+-----------------+-----------------+
  // |                 |                 |                 |                 |
  // | "a + b + c + d" | "a + b * c + d" | "a * b + c * d" | "a * b * c + d" |
  // |                 |                 |                 |                 |
  // |       (+)       |       (+)       |       (+)       |       (+)       |
  // |      /   \      |      /   \      |      /   \      |      /   \      |
  // |   (+)     (d)   |    (+)   (d)    |   (*)     (*)   |   (*)     (d)   |
  // |    | \          |   /   \         |   / \     / \   |    | \          |
  // |   (+) (c)       |(a)     (*)      | (a) (b) (c) (d) |   (*) (c)       |
  // |   / \           |        / \      |                 |   / \           |
  // | (a) (b)         |      (b) (c)    |                 | (a) (b)         |
  // |                 |                 |                 |                 |
  // +-----------------+-----------------+-----------------+-----------------+

  Token token;
  uint32_t op;

  // Current binary operator node. Initial nullptr value means that the parsing
  // just started and there is no binary operator yet. Once the first binary
  // operator has been parsed `oNode` will be set accordingly.
  AstBinaryOp* oNode = nullptr;

  // Currently parsed node.
  AstNode* tNode = nullptr;

  for (;;) {
    // Last unary node. It's an optimization to prevent recursion in case that
    // we found two or more unary expressions after each other. For example the
    // expression "~!~-~1" contains only unary operators that will be parsed by
    // a single `parseExpression()` call.
    AstUnary* unary = nullptr;

_Repeat1:
    switch (_tokenizer.next(&token)) {
      // Parse a variable, a constant, or a function-call. This section is
      // repeated when a right-to-left unary has been parsed.

      // Parse a symbol (variable, object.member, or function call).
      case kTokenSymbol: {
        StringRef str(_tokenizer._start + token.position(), token.size());
        AstSymbol* sym = scope->resolveSymbol(str, token.hashCode());

        if (sym == nullptr)
          MPSL_PARSER_ERROR(token, "Unresolved symbol.");

        if (sym->isTypeName())
          MPSL_PARSER_ERROR(token, "Unexpected type-name '%s'.", sym->name());

        AstNode* zNode;
        if (sym->isVariable()) {
          if (!sym->isDeclared())
            MPSL_PARSER_ERROR(token, "Can't use variable '%s' that is being declared.", sym->name());

          zNode = _ast->newNode<AstVar>();
          MPSL_NULLCHECK(zNode);

          zNode->setPosition(token.positionAsUInt());
          zNode->setTypeInfo(sym->typeInfo());
          static_cast<AstVar*>(zNode)->setSymbol(sym);
        }
        else {
          // Will be parsed by `parseCall()` again.
          _tokenizer.set(&token);
          MPSL_PROPAGATE(parseCall(reinterpret_cast<AstCall**>(&zNode)));
        }

        if (unary == nullptr)
          tNode = zNode;
        else
          unary->setChild(zNode);
        break;
      }

      // Parse a number.
      case kTokenNumber: {
        AstImm* zNode = _ast->newNode<AstImm>();
        MPSL_NULLCHECK(zNode);

        zNode->setPosition(token.positionAsUInt());
        zNode->setTypeInfo(token.numberType() | kTypeRead);

        switch (token.numberType()) {
          case kTypeInt   : zNode->_value.i[0] = static_cast<int>(token.value()); break;
          case kTypeFloat : zNode->_value.f[0] = static_cast<float>(token.value()); break;
          case kTypeDouble: zNode->_value.d[0] = token.value(); break;

          default:
            MPSL_ASSERT(!"Reached");
        }

        if (unary == nullptr)
          tNode = zNode;
        else
          unary->setChild(zNode);
        break;
      }

      // Parse expression terminators - ',' or ':', or ';' or ')'.
      case kTokenComma:
      case kTokenColon:
      case kTokenSemicolon:
      case kTokenRParen: {
        MPSL_PARSER_ERROR(token, "Expected an expression.");
      }

      // Parse '(cast)' operator or '(...)' sub-expression.
      case kTokenLParen: {
        uint32_t position = token.positionAsUInt();

        // Parse 'cast' operator.
        if (_tokenizer.peek(&token) == kTokenSymbol) {
          StringRef str(_tokenizer._start + token.position(), token.size());
          AstSymbol* sym = scope->resolveSymbol(str, token.hashCode());

          if (sym && sym->isTypeName()) {
            if (_tokenizer.consumeAndNext(&token) != kTokenRParen)
              MPSL_PARSER_ERROR(token, "Expected ')' token.");

            AstUnaryOp* zNode = _ast->newNode<AstUnaryOp>(kOpCast, sym->typeInfo());
            MPSL_NULLCHECK(zNode);
            zNode->setPosition(position);

            if (unary == nullptr)
              tNode = zNode;
            else
              unary->setChild(zNode);

            // The 'cast' operator is an unary node.
            unary = zNode;
            goto _Repeat1;
          }
        }

        // Parse a nested expression if it's not a 'cast' operator.
        {
          AstNode* zNode;
          MPSL_PROPAGATE(parseExpression(&zNode));

          if (_tokenizer.next(&token) != kTokenRParen)
            MPSL_PARSER_ERROR(token, "Expected ')' token.");

          if (unary == nullptr)
            tNode = zNode;
          else
            unary->setChild(zNode);
        }

        break;
      }

      // Parse a right-to-left associative unary operator ('++', '--', '+', '-', "~", "!").
      case kTokenPlusPlus    : op = kOpPreInc; goto _Unary;
      case kTokenMinusMinus  : op = kOpPreDec; goto _Unary;
      case kTokenAdd         : op = kOpNone  ; goto _Unary;
      case kTokenSub         : op = kOpNeg   ; goto _Unary;
      case kTokenBitNeg      : op = kOpBitNeg; goto _Unary;
      case kTokenNot         : op = kOpNot   ; goto _Unary;
_Unary: {
        // Parse the unary operator.
        AstUnaryOp* opNode = _ast->newNode<AstUnaryOp>(op);
        MPSL_NULLCHECK(opNode);
        opNode->setPosition(token.positionAsUInt());

        if (unary == nullptr)
          tNode = opNode;
        else
          unary->setChild(opNode);

        unary = opNode;
        goto _Repeat1;
      }

      case kTokenEnd: {
        MPSL_PARSER_ERROR(token, "Unexpected end of the program.");
      }

      default: {
        MPSL_PARSER_ERROR(token, "Unexpected token.");
      }
    }

_Repeat2:
    switch (_tokenizer.next(&token)) {
      // Parse the expression terminators - ',' or ':', or ';' or ')'.
      case kTokenComma:
      case kTokenColon:
      case kTokenSemicolon:
      case kTokenRParen: {
        _tokenizer.set(&token);

        if (oNode) {
          oNode->setRight(tNode);
          // Iterate to the top-most node.
          while (oNode->hasParent())
            oNode = static_cast<AstBinaryOp*>(oNode->parent());
          tNode = oNode;
        }

        *pNode = tNode;
        return kErrorOk;
      }

      // Parse a suffix increment/decrement operator.
      case kTokenPlusPlus    : op = kOpPostInc; goto _UnaryPostfixOp;
      case kTokenMinusMinus  : op = kOpPostDec; goto _UnaryPostfixOp;
_UnaryPostfixOp: {
        // Fail if the current node is not a variable.
        AstNode* aNode = unary ? unary->child() : tNode;
        if (aNode == nullptr || (aNode->nodeType() != AstNode::kTypeVar && aNode->nodeType() != AstNode::kTypeVarMemb))
          MPSL_PARSER_ERROR(token, "Unexpected postfix operator.");

        AstUnaryOp* zNode = _ast->newNode<AstUnaryOp>(op);
        MPSL_NULLCHECK(zNode);
        zNode->setPosition(token.positionAsUInt());

        if (unary == nullptr) {
          zNode->setChild(aNode);
          tNode = zNode;
        }
        else {
          unary->setChild(zNode);
          zNode->setChild(aNode);
        }

        unary = zNode;
        goto _Repeat2;
      }

      // Parse '.' object's accessor.
      case kTokenDot: {
        // Fail if the current node is not a variable.
        uint32_t position = token.positionAsUInt();
        AstNode* aNode = unary ? unary->child() : tNode;

        if (aNode == nullptr || (aNode->nodeType() != AstNode::kTypeVar && aNode->nodeType() != AstNode::kTypeVarMemb))
          MPSL_PARSER_ERROR(token, "Unexpected member access.");

        if (_tokenizer.next(&token) != kTokenSymbol)
          MPSL_PARSER_ERROR(token, "Unexpected token after member access.");

        AstVarMemb* zNode = _ast->newNode<AstVarMemb>();
        MPSL_NULLCHECK(zNode);

        // TODO: Memory leak, it should be allocated as a part of `AstVarMemb`.
        char* zMemb = _ast->newString(_tokenizer._start + token.position(), token.size());
        MPSL_NULLCHECK_(zMemb, { _ast->deleteNode(zNode); });

        zNode->setPosition(position);
        zNode->setField(zMemb, token.size());

        if (unary == nullptr) {
          zNode->setChild(aNode);
          tNode = zNode;
        }
        else {
          unary->setChild(zNode);
          zNode->setChild(aNode);
        }

        unary = zNode;
        goto _Repeat2;
      }

      // Parse a binary operator.
      case kTokenEq       : op = kOpCmpEq    ; goto _Binary;
      case kTokenNe       : op = kOpCmpNe    ; goto _Binary;
      case kTokenGt       : op = kOpCmpGt    ; goto _Binary;
      case kTokenGe       : op = kOpCmpGe    ; goto _Binary;
      case kTokenLt       : op = kOpCmpLt    ; goto _Binary;
      case kTokenLe       : op = kOpCmpLe    ; goto _Binary;
      case kTokenLogAnd   : op = kOpLogAnd   ; goto _Binary;
      case kTokenLogOr    : op = kOpLogOr    ; goto _Binary;
      case kTokenAdd      : op = kOpAdd      ; goto _Binary;
      case kTokenSub      : op = kOpSub      ; goto _Binary;
      case kTokenMul      : op = kOpMul      ; goto _Binary;
      case kTokenDiv      : op = kOpDiv      ; goto _Binary;
      case kTokenMod      : op = kOpMod      ; goto _Binary;
      case kTokenAnd      : op = kOpAnd      ; goto _Binary;
      case kTokenOr       : op = kOpOr       ; goto _Binary;
      case kTokenXor      : op = kOpXor      ; goto _Binary;
      case kTokenBitNeg   : op = kOpBitNeg   ; goto _Binary;
      case kTokenSll      : op = kOpSll      ; goto _Binary;
      case kTokenSrl      : op = kOpSrl      ; goto _Binary;
      case kTokenSra      : op = kOpSra      ; goto _Binary;
      case kTokenAssign   : op = kOpAssign   ; goto _Binary;
      case kTokenAssignAdd: op = kOpAssignAdd; goto _Binary;
      case kTokenAssignSub: op = kOpAssignSub; goto _Binary;
      case kTokenAssignMul: op = kOpAssignMul; goto _Binary;
      case kTokenAssignDiv: op = kOpAssignDiv; goto _Binary;
      case kTokenAssignMod: op = kOpAssignMod; goto _Binary;
      case kTokenAssignAnd: op = kOpAssignAnd; goto _Binary;
      case kTokenAssignOr : op = kOpAssignOr ; goto _Binary;
      case kTokenAssignXor: op = kOpAssignXor; goto _Binary;
      case kTokenAssignSll: op = kOpAssignSll; goto _Binary;
      case kTokenAssignSrl: op = kOpAssignSrl; goto _Binary;
      case kTokenAssignSra: op = kOpAssignSra; goto _Binary;
_Binary: {
        AstBinaryOp* zNode = _ast->newNode<AstBinaryOp>(op);
        MPSL_NULLCHECK(zNode);
        zNode->setPosition(token.positionAsUInt());

        if (oNode == nullptr) {
          // oNode <------+
          //              |
          // +------------+------------+ First operand - oNode becomes the newly
          // |         (zNode)         | created zNode; tNode is assigned to the
          // |        /       \        | left side of zNode and will be referred
          // |     (tNode)  (nullptr)     | as (...) by the next operation.
          // +-------------------------+
          zNode->setLeft(tNode);
          oNode = zNode;
          break;
        }

        uint32_t oPrec = OpInfo::get(oNode->opType()).precedence();
        uint32_t zPrec = OpInfo::get(op).precedence();

        if (oPrec > zPrec) {
          // oNode <----------+
          //                  |
          // +----------------+--------+ The current operator (zPrec) has a
          // |     (oNode)    |        | higher precedence than the previous one
          // |    /       \   |        | (oPrec), so the zNode will be assigned
          // | (...)       (zNode)     | to the right side of oNode and it will
          // |            /       \    | function as a stack-like structure. We
          // |         (tNode)  (nullptr) | have to advance back at some point.
          // +-------------------------+
          oNode->setRight(zNode);
          zNode->setLeft(tNode);
          oNode = zNode;
          break;
        }
        else {
          oNode->setRight(tNode);

          // Advance to the top-most oNode that has less or equal precedence
          // than zPrec.
          while (oNode->hasParent()) {
            // Terminate conditions:
            //   1. oNode has higher precedence than zNode.
            //   2. oNode has equal precedence and right-to-left associativity.
            if (OpInfo::get(oNode->opType()).rightAssociate(zPrec))
              break;
            oNode = static_cast<AstBinaryOp*>(oNode->parent());
          }

          // oNode <------+
          //              |
          // +------------+------------+
          // |         (zNode)         | Simple case - oNode becomes the left
          // |        /       \        | node in the created zNode and zNode
          // |     (oNode)  (nullptr)     | becomes oNode for the next operator.
          // |    /       \            |
          // | (...)    (tNode)        | oNode will become a top-level node.
          // +-------------------------+
          if (!oNode->hasParent() && !OpInfo::get(oNode->opType()).rightAssociate(zPrec)) {
            zNode->setLeft(oNode);
          }
          // oNode <----------+
          //                  |
          // +----------------+--------+
          // |     (oNode)    |        |
          // |    /       \   |        | Complex case - inject node in place
          // | (...)       (zNode)     | of oNode.right (because of higher
          // |            /       \    | precedence or RTL associativity).
          // |        (tNode)   (nullptr) |
          // +-------------------------+
          else {
            AstNode* pNode = oNode->unlinkRight();
            oNode->setRight(zNode);
            zNode->setLeft(pNode);
          }

          oNode = zNode;
          break;
        }
      }

      default: {
        MPSL_PARSER_ERROR(token, "Unexpected token.");
      }
    }
  }
}

// Parse "variable" or "(variable)". The parentheses count is not limited.
Error Parser::parseVariable(AstVar** pNodeOut) noexcept {
  Token token;
  uint32_t uToken;

  uint32_t nParen = 0;
  AstSymbol* sym = nullptr;

  for (;;) {
    uToken = _tokenizer.next(&token);

    // Parse the variable.
    if (uToken == kTokenSymbol) {
      StringRef str(_tokenizer._start + token.position(), token.size());
      sym = _currentScope->resolveSymbol(str, token.hashCode());

      if (sym == nullptr || !sym->isVariable())
        MPSL_PARSER_ERROR(token, "Expected a variable.");
      break;
    }
    // Parse the '(' token.
    else if (uToken == kTokenLParen) {
      nParen++;
    }
    // Syntax error.
    else {
      MPSL_PARSER_ERROR(token, "Expected a variable.");
    }
  }

  uint32_t position = token.positionAsUInt();
  while (nParen > 0) {
    uToken = _tokenizer.next(&token);
    if (uToken != kTokenRParen) {
      MPSL_PARSER_ERROR(token, "Expected a variable.");
    }
    nParen--;
  }

  AstVar* node = _ast->newNode<AstVar>();
  MPSL_NULLCHECK(node);

  node->setPosition(position);
  node->setSymbol(sym);
  node->setTypeInfo(sym->typeInfo());

  *pNodeOut = node;
  return kErrorOk;
}

// Parse "function([arg1 [, arg2, ...] ])".
Error Parser::parseCall(AstCall** pNodeOut) noexcept {
  Token token;
  uint32_t uToken;

  uToken = _tokenizer.next(&token);
  MPSL_ASSERT(uToken == kTokenSymbol);
  uint32_t position = token.positionAsUInt();

  StringRef str(_tokenizer._start + token.position(), token.size());
  AstSymbol* sym = _currentScope->resolveSymbol(str, token.hashCode());

  if (sym == nullptr)
    MPSL_PARSER_ERROR(token, "Unresolved symbol.");

  if (!sym->isIntrinsic() && !sym->isFunction())
    MPSL_PARSER_ERROR(token, "Expected a function name.");

  uToken = _tokenizer.next(&token);
  if (uToken != kTokenLParen)
    MPSL_PARSER_ERROR(token, "Expected '(' after a function name.");

  AstCall* callNode = _ast->newNode<AstCall>();
  MPSL_NULLCHECK(callNode);

  callNode->setSymbol(sym);
  callNode->setPosition(position);

  uToken = _tokenizer.peek(&token);
  if (uToken != kTokenRParen) {
    for (;;) {
      // Parse the argument expression.
      AstNode* expression;
      Error err;

      if ((err = callNode->willAdd()) != kErrorOk || (err = parseExpression(&expression)) != kErrorOk) {
        _ast->deleteNode(callNode);
        return err;
      }

      callNode->appendNode(expression);

      // Parse ')' or ',' tokens.
      uToken = _tokenizer.peek(&token);
      if (uToken == kTokenRParen)
        break;

      if (uToken != kTokenComma) {
        _ast->deleteNode(callNode);
        MPSL_PARSER_ERROR(token, "Expected either ',' or ')'.");
      }

      _tokenizer.consume();
    }
  }

  _tokenizer.consume();
  *pNodeOut = callNode;

  return kErrorOk;
}

} // mpsl namespace

// [Api-End]
#include "./mpsl_apiend.h"
