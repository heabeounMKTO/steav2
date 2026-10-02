#ifndef STEAV_FRONTEND_SCANNER_H
#define STEAV_FRONTEND_SCANNER_H

#include "frontend/token.h"
#include "logging/steav_logger.h"
#include <vector>

namespace steav_frontend {

struct Scanner {
  Scanner(const char *source) : start(source), current(source), line(1) {}

  // whole file -> tokens, always ends with TOKEN_EOF
  SteavStatus scan_tokens(std::vector<Token> &tokens);
  Token scan_token();

private:
  const char *start;
  const char *current;
  int line;

  inline bool _is_at_end() const { return *current == '\0'; }
  inline char _advance() { return *current++; }
  inline char _peek() const { return *current; }
  inline char _peek_next() const { return _is_at_end() ? '\0' : current[1]; }
  inline bool _match(char expected) {
    if (_is_at_end() || *current != expected) return false;
    this->current++;
    return true;
  }
  inline Token _make_token(TokenType type) const {
    Token t;
    t.type = type;
    t.start = start;
    t.length = (int)(current - start);
    t.line = line;
    return t;
  }
  // error tokens point at a static message instead of the source
  inline Token _error_token(const char *message) const {
    Token t;
    t.type = TOKEN_ERROR;
    t.start = message;
    t.length = 0;
    while (message[t.length] != '\0') t.length++;
    t.line = line;
    return t;
  }

  void _skip_whitespace();
  Token _string();
  Token _number();
  Token _identifier();
  TokenType _identifier_type() const;
};

} // namespace steav_frontend
#endif // STEAV_FRONTEND_SCANNER_H
