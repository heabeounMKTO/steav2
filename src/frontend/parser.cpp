#include "frontend/parser.h"
#include "logging/steav_diagnostics.h"
#include <cstring>

namespace steav_frontend {

using steav_ast::Ast;
using steav_ast::Decl;
using steav_ast::Expr;
using steav_ast::Stmt;
using steav_ast::TypeRef;

SteavStatus Parser::parse(Ast &ast) {
  // same rule as steavscript, the very first token
  if (require_bong && _peek().type != TOKEN_BONG_SLANH_OUN) {
    _error_at(_peek(), "na `bongSlanhOun`?!!! the file must start with `bongSlanhOun \"...\";`");
    this->panic_mode = false; // still report everything else in the file
  }
  while (!_is_at_end()) {
    int decl = _declaration(ast);
    if (decl >= 0) ast.modules[module].program.push_back(decl);
  }
  return had_error ? STEAV_LOGGING_PARSING_ERROR : STEAV_LOGGING_OK;
}

void Parser::_error_at(const Token &token, const char *message) {
  if (panic_mode) return;
  this->panic_mode = true;
  this->had_error = true;
  if (steav_capture_diagnostic(token.line, token.start, token.length, message)) return;
  char parse_err_str[512];
  if (token.type == TOKEN_EOF) {
    snprintf(parse_err_str, sizeof(parse_err_str),
             "[line %d] Error at end: %s", token.line, message);
  } else {
    snprintf(parse_err_str, sizeof(parse_err_str),
             "[line %d] Error at '%.*s': %s", token.line, token.length,
             token.start, message);
  }
  STEAV_LOGGING_LOG(parse_err_str, STEAV_LOGGING_PARSING_ERROR);
}

bool Parser::_consume(TokenType type, const char *message) {
  if (_check(type)) {
    _advance();
    return true;
  }
  _error_at_current(message);
  return false;
}

// skip to the next statement boundary so one typo = one error
void Parser::_synchronize() {
  this->panic_mode = false;
  while (!_is_at_end()) {
    if (_previous().type == TOKEN_SEMICOLON) return;
    switch (_peek().type) {
    case TOKEN_STRUCT:
    case TOKEN_CLASS:
    case TOKEN_FUN:
    case TOKEN_VAR:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_PRINT:
    case TOKEN_RETURN:
    case TOKEN_IMPORT:
    case TOKEN_BONG_SLANH_OUN:
      return;
    default:
      _advance();
    }
  }
}

// declaration → structDecl | classDecl | funDecl | varDecl | statement
int Parser::_declaration(Ast &ast) {
  int start = current;
  int decl;
  if (_match(TOKEN_BONG_SLANH_OUN)) {
    _bong_slanh_oun(ast);
    decl = -1;
  } else if (_match(TOKEN_STRUCT))
    decl = _struct_decl(ast);
  else if (_match(TOKEN_CLASS))
    decl = _class_decl(ast);
  else if (_match(TOKEN_FUN))
    decl = _fun_decl(ast);
  else if (_match(TOKEN_VAR))
    decl = _var_decl(ast);
  else
    decl = _wrap_stmt(ast, _statement(ast));

  if (panic_mode) {
    // an error on a token nobody consumed (e.g. a stray '}') would leave us
    // stuck on it forever, skip it
    if (current == start) _advance();
    _synchronize();
    return -1;
  }
  return decl;
}

int Parser::_wrap_stmt(Ast &ast, int stmt) {
  if (stmt < 0) return -1;
  Decl d;
  d.kind = steav_ast::DECL_STMT;
  d.name = ast.stmts[stmt].keyword;
  d.module = module;
  d.stmt = stmt;
  return ast.add_decl(d);
}

/* bongSlanhOun → "bongSlanhOun" expression ";"
 * ceremonial: parsed so it has to be well formed, then thrown away, the
 * expression is never checked or run */
void Parser::_bong_slanh_oun(Ast &ast) {
  if (_expression(ast) < 0) return;
  _consume(TOKEN_SEMICOLON, "mex ban tha sl ke hz ort dak ; jeng :<");
}

// structDecl → "sampoan" IDENTIFIER "{" typeBody "}"
int Parser::_struct_decl(Ast &ast) {
  if (!_consume(TOKEN_IDENTIFIER, "expected a sampoan name")) return -1;
  Decl d;
  d.kind = steav_ast::DECL_STRUCT;
  d.name = _previous();
  d.module = module;
  if (!_type_body(ast, d)) return -1;
  int idx = ast.add_decl(d);
  for (int m : ast.decls[idx].methods) ast.decls[m].owner = idx;
  return idx;
}

// classDecl → "tnak" IDENTIFIER "{" typeBody "}"
int Parser::_class_decl(Ast &ast) {
  if (!_consume(TOKEN_IDENTIFIER, "expected a tnak name")) return -1;
  Decl d;
  d.kind = steav_ast::DECL_CLASS;
  d.name = _previous();
  d.module = module;
  if (!_type_body(ast, d)) return -1;
  int idx = ast.add_decl(d);
  for (int m : ast.decls[idx].methods) ast.decls[m].owner = idx;
  return idx;
}

// typeBody → ( fieldDecl | funDecl )*  shared by sampoan + tnak
bool Parser::_type_body(Ast &ast, Decl &decl) {
  if (!_consume(TOKEN_LEFT_BRACE, "expected '{' before the type body"))
    return false;
  while (!_check(TOKEN_RIGHT_BRACE) && !_is_at_end()) {
    if (_match(TOKEN_VAR)) {
      int field = _field_decl(ast);
      if (field < 0) return false;
      decl.fields.push_back(field);
    } else if (_match(TOKEN_FUN)) {
      int method = _fun_decl(ast);
      if (method < 0) return false;
      decl.methods.push_back(method);
    } else {
      _error_at_current("expected 'akthe' or 'rupamun' in a type body");
      return false;
    }
  }
  return _consume(TOKEN_RIGHT_BRACE, "expected '}' after the type body");
}

// fieldDecl → "akthe" IDENTIFIER ":" type ";"
int Parser::_field_decl(Ast &ast) {
  if (!_consume(TOKEN_IDENTIFIER, "expected a field name")) return -1;
  Decl d;
  d.kind = steav_ast::DECL_VAR;
  d.name = _previous();
  d.module = module;
  if (!_consume(TOKEN_COLON, "fields need a type, expected ':'")) return -1;
  d.var_type = _type(ast);
  if (d.var_type < 0) return -1;
  if (!_consume(TOKEN_SEMICOLON, "expected ';' after the field")) return -1;
  return ast.add_decl(d);
}

// funDecl → "rupamun" function
int Parser::_fun_decl(Ast &ast) { return _function(ast); }

static bool is_overloadable(TokenType type) {
  switch (type) {
  case TOKEN_PLUS:
  case TOKEN_MINUS:
  case TOKEN_STAR:
  case TOKEN_SLASH:
  case TOKEN_EQUAL_EQUAL:
  case TOKEN_BANG_EQUAL:
  case TOKEN_LESS:
  case TOKEN_LESS_EQUAL:
  case TOKEN_GREATER:
  case TOKEN_GREATER_EQUAL:
    return true;
  default:
    return false;
  }
}

/* function → ( IDENTIFIER | operator | "init" ) "(" parameters? ")"
 *            ( "->" type )? ( block | ";" )
 * a `;` instead of a block = implemented in C++ (stdlib modules only)
 * one fork after `rupamun` (identifier, overloadable operator or init),
 * then every branch is the same params/arrow/block parsing.
 * no `->` means OrtMean, that's how init parses */
int Parser::_function(Ast &ast) {
  Decl d;
  d.kind = steav_ast::DECL_FUN;
  d.module = module;
  if (_match(TOKEN_IDENTIFIER)) {
    d.name = _previous();
    d.fun_name_kind = (d.name.length == 4 && memcmp(d.name.start, "init", 4) == 0)
                          ? steav_ast::FUN_INIT
                          : steav_ast::FUN_NAMED;
  } else if (is_overloadable(_peek().type)) {
    d.name = _advance();
    d.fun_name_kind = steav_ast::FUN_OPERATOR;
  } else {
    _error_at_current("expected a function name or an overloadable operator");
    return -1;
  }

  if (!_consume(TOKEN_LEFT_PAREN, "expected '(' after the function name"))
    return -1;
  if (!_check(TOKEN_RIGHT_PAREN)) {
    do {
      steav_ast::Param p;
      if (!_consume(TOKEN_IDENTIFIER, "expected a parameter name")) return -1;
      p.name = _previous();
      if (!_consume(TOKEN_COLON, "parameters need a type, expected ':'"))
        return -1;
      p.type = _type(ast);
      if (p.type < 0) return -1;
      d.params.push_back(p);
    } while (_match(TOKEN_COMMA));
  }
  if (!_consume(TOKEN_RIGHT_PAREN, "expected ')' after the parameters"))
    return -1;
  if (_match(TOKEN_ARROW)) {
    d.return_type = _type(ast);
    if (d.return_type < 0) return -1;
  }

  if (_match(TOKEN_SEMICOLON)) {
    d.is_native = true;
    return ast.add_decl(d);
  }
  if (!_consume(TOKEN_LEFT_BRACE, "expected '{' before the function body"))
    return -1;
  while (!_check(TOKEN_RIGHT_BRACE) && !_is_at_end()) {
    int decl = _declaration(ast);
    if (decl >= 0) d.body.push_back(decl);
  }
  if (!_consume(TOKEN_RIGHT_BRACE, "expected '}' after the function body"))
    return -1;
  return ast.add_decl(d);
}

// varDecl → "akthe" IDENTIFIER ( ":" type )? ( "=" expression )? ";"
int Parser::_var_decl(Ast &ast) {
  if (!_consume(TOKEN_IDENTIFIER, "expected a variable name")) return -1;
  Decl d;
  d.kind = steav_ast::DECL_VAR;
  d.name = _previous();
  d.module = module;
  if (_match(TOKEN_COLON)) {
    d.var_type = _type(ast);
    if (d.var_type < 0) return -1;
  }
  if (_match(TOKEN_EQUAL)) {
    d.initializer = _expression(ast);
    if (d.initializer < 0) return -1;
  }
  if (!_consume(TOKEN_SEMICOLON, "expected ';' after the variable")) return -1;
  return ast.add_decl(d);
}

/* statement → exprStmt | printStmt | block | ifStmt | whileStmt
 *           | forStmt | returnStmt | importStmt */
int Parser::_statement(Ast &ast) {
  if (_match(TOKEN_PRINT)) return _print_stmt(ast);
  if (_match(TOKEN_LEFT_BRACE)) return _block(ast);
  if (_match(TOKEN_IF)) return _if_stmt(ast);
  if (_match(TOKEN_WHILE)) return _while_stmt(ast);
  if (_match(TOKEN_FOR)) return _for_stmt(ast);
  if (_match(TOKEN_RETURN)) return _return_stmt(ast);
  if (_match(TOKEN_IMPORT)) return _import_stmt(ast);
  return _expr_stmt(ast);
}

// printStmt → "jongyeytha" expression ";"
int Parser::_print_stmt(Ast &ast) {
  Stmt s;
  s.kind = steav_ast::STMT_PRINT;
  s.keyword = _previous();
  s.expr = _expression(ast);
  if (s.expr < 0) return -1;
  if (!_consume(TOKEN_SEMICOLON, "expected ';' after the value")) return -1;
  return ast.add_stmt(s);
}

// block → "{" declaration* "}"
int Parser::_block(Ast &ast) {
  Stmt s;
  s.kind = steav_ast::STMT_BLOCK;
  s.keyword = _previous();
  while (!_check(TOKEN_RIGHT_BRACE) && !_is_at_end()) {
    int decl = _declaration(ast);
    if (decl >= 0) s.body.push_back(decl);
  }
  if (!_consume(TOKEN_RIGHT_BRACE, "expected '}' after the block")) return -1;
  return ast.add_stmt(s);
}

// ifStmt → "ber" "(" expression ")" statement ( "minjengte" statement )?
int Parser::_if_stmt(Ast &ast) {
  Stmt s;
  s.kind = steav_ast::STMT_IF;
  s.keyword = _previous();
  if (!_consume(TOKEN_LEFT_PAREN, "expected '(' after 'ber'")) return -1;
  s.expr = _expression(ast);
  if (s.expr < 0) return -1;
  if (!_consume(TOKEN_RIGHT_PAREN, "expected ')' after the condition"))
    return -1;
  s.then_branch = _wrap_stmt(ast, _statement(ast));
  if (s.then_branch < 0) return -1;
  if (_match(TOKEN_ELSE)) {
    s.else_branch = _wrap_stmt(ast, _statement(ast));
    if (s.else_branch < 0) return -1;
  }
  return ast.add_stmt(s);
}

// whileStmt → "nvpeldae" "(" expression ")" statement
int Parser::_while_stmt(Ast &ast) {
  Stmt s;
  s.kind = steav_ast::STMT_WHILE;
  s.keyword = _previous();
  if (!_consume(TOKEN_LEFT_PAREN, "expected '(' after 'nvpeldae'")) return -1;
  s.expr = _expression(ast);
  if (s.expr < 0) return -1;
  if (!_consume(TOKEN_RIGHT_PAREN, "expected ')' after the condition"))
    return -1;
  s.then_branch = _wrap_stmt(ast, _statement(ast));
  if (s.then_branch < 0) return -1;
  return ast.add_stmt(s);
}

/* forStmt → "somhab" "(" ( varDecl | exprStmt | ";" )
 *           expression? ";" expression? ")" statement */
int Parser::_for_stmt(Ast &ast) {
  Stmt s;
  s.kind = steav_ast::STMT_FOR;
  s.keyword = _previous();
  if (!_consume(TOKEN_LEFT_PAREN, "expected '(' after 'somhab'")) return -1;
  if (_match(TOKEN_SEMICOLON)) {
    // no initializer
  } else if (_match(TOKEN_VAR)) {
    s.initializer = _var_decl(ast);
    if (s.initializer < 0) return -1;
  } else {
    s.initializer = _wrap_stmt(ast, _expr_stmt(ast));
    if (s.initializer < 0) return -1;
  }
  if (!_check(TOKEN_SEMICOLON)) {
    s.expr = _expression(ast);
    if (s.expr < 0) return -1;
  }
  if (!_consume(TOKEN_SEMICOLON, "expected ';' after the loop condition"))
    return -1;
  if (!_check(TOKEN_RIGHT_PAREN)) {
    s.increment = _expression(ast);
    if (s.increment < 0) return -1;
  }
  if (!_consume(TOKEN_RIGHT_PAREN, "expected ')' after the somhab clauses"))
    return -1;
  s.then_branch = _wrap_stmt(ast, _statement(ast));
  if (s.then_branch < 0) return -1;
  return ast.add_stmt(s);
}

// returnStmt → "morvenh" expression? ";"
int Parser::_return_stmt(Ast &ast) {
  Stmt s;
  s.kind = steav_ast::STMT_RETURN;
  s.keyword = _previous();
  if (!_check(TOKEN_SEMICOLON)) {
    s.expr = _expression(ast);
    if (s.expr < 0) return -1;
  }
  if (!_consume(TOKEN_SEMICOLON, "expected ';' after the return value"))
    return -1;
  return ast.add_stmt(s);
}

// importStmt → "yok" ( IDENTIFIER | STRING ) ( "jea" IDENTIFIER )? ";"
int Parser::_import_stmt(Ast &ast) {
  Stmt s;
  s.kind = steav_ast::STMT_IMPORT;
  s.keyword = _previous();
  if (!_check(TOKEN_IDENTIFIER) && !_check(TOKEN_STRING)) {
    _error_at_current("expected a module name or \"path/to/file.sts\" after 'yok'");
    return -1;
  }
  s.name = _advance();
  if (_match(TOKEN_AS)) {
    if (!_consume(TOKEN_IDENTIFIER, "expected an alias after 'jea'"))
      return -1;
    s.alias = _previous();
    s.has_alias = true;
  }
  if (!_consume(TOKEN_SEMICOLON, "expected ';' after the import")) return -1;
  return ast.add_stmt(s);
}

// exprStmt → expression ";"
int Parser::_expr_stmt(Ast &ast) {
  Stmt s;
  s.kind = steav_ast::STMT_EXPR;
  s.keyword = _peek();
  s.expr = _expression(ast);
  if (s.expr < 0) return -1;
  if (!_consume(TOKEN_SEMICOLON, "expected ';' after the expression"))
    return -1;
  return ast.add_stmt(s);
}

// type → "[" type "]" "?"? | ( IDENTIFIER "." )? IDENTIFIER "?"?
int Parser::_type(Ast &ast) {
  TypeRef t;
  if (_match(TOKEN_LEFT_BRACKET)) {
    t.kind = steav_ast::TYPEREF_ARRAY;
    t.name = _previous();
    t.elem = _type(ast);
    if (t.elem < 0) return -1;
    if (!_consume(TOKEN_RIGHT_BRACKET, "expected ']' after the element type"))
      return -1;
  } else if (_match(TOKEN_IDENTIFIER)) {
    t.kind = steav_ast::TYPEREF_NAMED;
    t.name = _previous();
    if (_match(TOKEN_DOT)) {
      t.qualifier = t.name;
      t.has_qualifier = true;
      if (!_consume(TOKEN_IDENTIFIER, "expected a type name after '.'"))
        return -1;
      t.name = _previous();
    }
  } else {
    _error_at_current("expected a type");
    return -1;
  }
  t.optional = _match(TOKEN_QUESTION);
  return ast.add_type(t);
}

// expression → assignment
int Parser::_expression(Ast &ast) { return _assignment(ast); }

/* assignment → ( call "." IDENTIFIER | call "[" expression "]"
 *              | IDENTIFIER ) ( "=" | "+=" | "-=" | "*=" | "/=" ) assignment
 *              | logic_or
 * parse the lhs as a normal expression, then check it's a place.
 * compound ones stay one EXPR_ASSIGN (token = the op) so the target is
 * only evaluated once */
int Parser::_assignment(Ast &ast) {
  int target = _logic_or(ast);
  if (target < 0) return -1;
  if (_match(TOKEN_EQUAL) || _match(TOKEN_PLUS_EQUAL) ||
      _match(TOKEN_MINUS_EQUAL) || _match(TOKEN_STAR_EQUAL) ||
      _match(TOKEN_SLASH_EQUAL)) {
    Token equals = _previous();
    int value = _assignment(ast);
    if (value < 0) return -1;
    steav_ast::ExprKind k = ast.exprs[target].kind;
    if (k != steav_ast::EXPR_VARIABLE && k != steav_ast::EXPR_GET &&
        k != steav_ast::EXPR_INDEX) {
      _error_at(equals, "invalid assignment target");
      return -1;
    }
    Expr e;
    e.kind = steav_ast::EXPR_ASSIGN;
    e.token = equals;
    e.left = target;
    e.right = value;
    return ast.add_expr(e);
  }
  return target;
}

int Parser::_binary(Ast &ast, steav_ast::ExprKind kind, int left,
                    const Token &op, int right) {
  Expr e;
  e.kind = kind;
  e.token = op;
  e.left = left;
  e.right = right;
  return ast.add_expr(e);
}

// logic_or → logic_and ( "reu" logic_and )*
int Parser::_logic_or(Ast &ast) {
  int expr = _logic_and(ast);
  while (expr >= 0 && _match(TOKEN_OR)) {
    Token op = _previous();
    int right = _logic_and(ast);
    if (right < 0) return -1;
    expr = _binary(ast, steav_ast::EXPR_LOGICAL, expr, op, right);
  }
  return expr;
}

// logic_and → equality ( "ng" equality )*
int Parser::_logic_and(Ast &ast) {
  int expr = _equality(ast);
  while (expr >= 0 && _match(TOKEN_AND)) {
    Token op = _previous();
    int right = _equality(ast);
    if (right < 0) return -1;
    expr = _binary(ast, steav_ast::EXPR_LOGICAL, expr, op, right);
  }
  return expr;
}

// equality → comparison ( ( "!=" | "==" ) comparison )*
int Parser::_equality(Ast &ast) {
  int expr = _comparison(ast);
  while (expr >= 0 && (_match(TOKEN_BANG_EQUAL) || _match(TOKEN_EQUAL_EQUAL))) {
    Token op = _previous();
    int right = _comparison(ast);
    if (right < 0) return -1;
    expr = _binary(ast, steav_ast::EXPR_BINARY, expr, op, right);
  }
  return expr;
}

// comparison → term ( ( "<" | "<=" | ">" | ">=" ) term )*
int Parser::_comparison(Ast &ast) {
  int expr = _term(ast);
  while (expr >= 0 &&
         (_match(TOKEN_LESS) || _match(TOKEN_LESS_EQUAL) ||
          _match(TOKEN_GREATER) || _match(TOKEN_GREATER_EQUAL))) {
    Token op = _previous();
    int right = _term(ast);
    if (right < 0) return -1;
    expr = _binary(ast, steav_ast::EXPR_BINARY, expr, op, right);
  }
  return expr;
}

// term → factor ( ( "-" | "+" ) factor )*
int Parser::_term(Ast &ast) {
  int expr = _factor(ast);
  while (expr >= 0 && (_match(TOKEN_MINUS) || _match(TOKEN_PLUS))) {
    Token op = _previous();
    int right = _factor(ast);
    if (right < 0) return -1;
    expr = _binary(ast, steav_ast::EXPR_BINARY, expr, op, right);
  }
  return expr;
}

// factor → unary ( ( "/" | "*" ) unary )*
int Parser::_factor(Ast &ast) {
  int expr = _unary(ast);
  while (expr >= 0 && (_match(TOKEN_SLASH) || _match(TOKEN_STAR))) {
    Token op = _previous();
    int right = _unary(ast);
    if (right < 0) return -1;
    expr = _binary(ast, steav_ast::EXPR_BINARY, expr, op, right);
  }
  return expr;
}

// unary → ( "-" | "!" ) unary | call
int Parser::_unary(Ast &ast) {
  if (_match(TOKEN_MINUS) || _match(TOKEN_BANG)) {
    Expr e;
    e.kind = steav_ast::EXPR_UNARY;
    e.token = _previous();
    e.right = _unary(ast);
    if (e.right < 0) return -1;
    return ast.add_expr(e);
  }
  return _call(ast);
}

// call → primary ( "(" arguments? ")" | "." IDENTIFIER | "[" expression "]" )*
int Parser::_call(Ast &ast) {
  int expr = _primary(ast);
  while (expr >= 0) {
    if (_match(TOKEN_LEFT_PAREN)) {
      Expr e;
      e.kind = steav_ast::EXPR_CALL;
      e.token = _previous();
      e.left = expr;
      if (!_check(TOKEN_RIGHT_PAREN)) {
        do {
          int arg = _expression(ast);
          if (arg < 0) return -1;
          e.args.push_back(arg);
        } while (_match(TOKEN_COMMA));
      }
      if (!_consume(TOKEN_RIGHT_PAREN, "expected ')' after the arguments"))
        return -1;
      expr = ast.add_expr(e);
    } else if (_match(TOKEN_DOT)) {
      if (!_consume(TOKEN_IDENTIFIER, "expected a name after '.'")) return -1;
      Expr e;
      e.kind = steav_ast::EXPR_GET;
      e.token = _previous();
      e.left = expr;
      expr = ast.add_expr(e);
    } else if (_match(TOKEN_LEFT_BRACKET)) {
      Expr e;
      e.kind = steav_ast::EXPR_INDEX;
      e.token = _previous();
      e.left = expr;
      e.right = _expression(ast);
      if (e.right < 0) return -1;
      if (!_consume(TOKEN_RIGHT_BRACKET, "expected ']' after the index"))
        return -1;
      expr = ast.add_expr(e);
    } else {
      break;
    }
  }
  return expr;
}

/* primary → NUMBER | STRING | "ok" | "ort" | "sone" | "nis"
 *         | "(" expression ")" | IDENTIFIER | arrayLiteral
 * "(" expression ")" eats the ")" and returns, it does NOT recurse back
 * into primary (that was steavscript's stack overflow) */
int Parser::_primary(Ast &ast) {
  Expr e;
  if (_match(TOKEN_NUMBER) || _match(TOKEN_STRING) || _match(TOKEN_TRUE) ||
      _match(TOKEN_FALSE) || _match(TOKEN_NIL)) {
    e.kind = steav_ast::EXPR_LITERAL;
    e.token = _previous();
    return ast.add_expr(e);
  }
  if (_match(TOKEN_THIS)) {
    e.kind = steav_ast::EXPR_THIS;
    e.token = _previous();
    return ast.add_expr(e);
  }
  if (_match(TOKEN_IDENTIFIER)) {
    e.kind = steav_ast::EXPR_VARIABLE;
    e.token = _previous();
    return ast.add_expr(e);
  }
  if (_match(TOKEN_LEFT_PAREN)) {
    e.kind = steav_ast::EXPR_GROUPING;
    e.token = _previous();
    e.right = _expression(ast);
    if (e.right < 0) return -1;
    if (!_consume(TOKEN_RIGHT_PAREN, "expected ')' after the expression"))
      return -1;
    return ast.add_expr(e);
  }
  if (_match(TOKEN_LEFT_BRACKET)) return _array_literal(ast);
  _error_at_current("expected an expression");
  return -1;
}

// arrayLiteral → "[" ( expression ( "," expression )* )? "]"
int Parser::_array_literal(Ast &ast) {
  Expr e;
  e.kind = steav_ast::EXPR_ARRAY;
  e.token = _previous();
  if (!_check(TOKEN_RIGHT_BRACKET)) {
    do {
      int item = _expression(ast);
      if (item < 0) return -1;
      e.args.push_back(item);
    } while (_match(TOKEN_COMMA));
  }
  if (!_consume(TOKEN_RIGHT_BRACKET, "expected ']' after the array items"))
    return -1;
  return ast.add_expr(e);
}

} // namespace steav_frontend
