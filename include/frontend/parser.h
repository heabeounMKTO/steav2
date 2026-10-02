#ifndef STEAV_FRONTEND_PARSER_H
#define STEAV_FRONTEND_PARSER_H

#include "ast/ast.h"
#include "frontend/token.h"
#include "logging/steav_logger.h"
#include <vector>

namespace steav_frontend {

/* recursive descent, one function per grammar rule (see README).
 * every rule returns an index into the Ast, -1 on error.
 * the parser knows nothing about operator overloading, `a + b` is the
 * same EXPR_BINARY for numbers and Vec3s, the type checker sorts it out */
struct Parser {
  /* require_bong: the main script has to open with `bongSlanhOun ...;`
   * (like steavscript), stdlib modules don't */
  Parser(const std::vector<Token> &tokens, int module, bool require_bong)
      : tokens(tokens), current(0), module(module), require_bong(require_bong),
        had_error(false), panic_mode(false) {}

  // appends into ast.modules[module].program
  SteavStatus parse(steav_ast::Ast &ast);

private:
  const std::vector<Token> &tokens;
  int current;
  int module;
  bool require_bong;
  bool had_error;
  bool panic_mode;

  // declarations
  int _declaration(steav_ast::Ast &ast);
  int _struct_decl(steav_ast::Ast &ast);
  int _class_decl(steav_ast::Ast &ast);
  bool _type_body(steav_ast::Ast &ast, steav_ast::Decl &decl);
  int _field_decl(steav_ast::Ast &ast);
  int _fun_decl(steav_ast::Ast &ast);
  int _function(steav_ast::Ast &ast);
  int _var_decl(steav_ast::Ast &ast);
  int _statement(steav_ast::Ast &ast);
  void _bong_slanh_oun(steav_ast::Ast &ast);

  // statements (clox shaped, the README grammar leaves `statement` open)
  int _print_stmt(steav_ast::Ast &ast);
  int _block(steav_ast::Ast &ast);
  int _if_stmt(steav_ast::Ast &ast);
  int _while_stmt(steav_ast::Ast &ast);
  int _for_stmt(steav_ast::Ast &ast);
  int _return_stmt(steav_ast::Ast &ast);
  int _import_stmt(steav_ast::Ast &ast);
  int _expr_stmt(steav_ast::Ast &ast);
  int _wrap_stmt(steav_ast::Ast &ast, int stmt);

  // types
  int _type(steav_ast::Ast &ast);

  // expressions
  int _expression(steav_ast::Ast &ast);
  int _assignment(steav_ast::Ast &ast);
  int _logic_or(steav_ast::Ast &ast);
  int _logic_and(steav_ast::Ast &ast);
  int _equality(steav_ast::Ast &ast);
  int _comparison(steav_ast::Ast &ast);
  int _term(steav_ast::Ast &ast);
  int _factor(steav_ast::Ast &ast);
  int _unary(steav_ast::Ast &ast);
  int _call(steav_ast::Ast &ast);
  int _primary(steav_ast::Ast &ast);
  int _array_literal(steav_ast::Ast &ast);

  int _binary(steav_ast::Ast &ast, steav_ast::ExprKind kind, int left,
              const Token &op, int right);

  void _error_at(const Token &token, const char *message);
  inline void _error(const char *message) { _error_at(_previous(), message); }
  inline void _error_at_current(const char *message) {
    _error_at(_peek(), message);
  }
  void _synchronize();
  bool _consume(TokenType type, const char *message);

  inline const Token &_peek() const { return tokens[current]; }
  inline const Token &_previous() const { return tokens[current - 1]; }
  inline bool _is_at_end() const { return _peek().type == TOKEN_EOF; }
  inline bool _check(TokenType type) const {
    return !_is_at_end() && _peek().type == type;
  }
  inline bool _match(TokenType type) {
    if (!_check(type)) return false;
    _advance();
    return true;
  }
  inline const Token &_advance() {
    if (!_is_at_end())
      current++;
    return _previous();
  }
};

} // namespace steav_frontend
#endif // STEAV_FRONTEND_PARSER_H
