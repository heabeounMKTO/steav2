#include "frontend/scanner.h"
#include "logging/steav_diagnostics.h"
#include <cstring>

namespace steav_frontend {

SteavStatus Scanner::scan_tokens(std::vector<Token> &tokens) {
  SteavStatus status = STEAV_LOGGING_OK;
  while (true) {
    Token t = scan_token();
    if (t.type == TOKEN_ERROR) {
      status = STEAV_LOGGING_PARSING_ERROR;
      if (steav_capture_diagnostic(t.line, current - 1, 1, std::string(t.start, t.length)))
        continue;
      char scan_err_str[512];
      snprintf(scan_err_str, sizeof(scan_err_str), "[line %d] Error: %.*s",
               t.line, t.length, t.start);
      STEAV_LOGGING_LOG(scan_err_str, STEAV_LOGGING_PARSING_ERROR);
      status = STEAV_LOGGING_PARSING_ERROR;
      continue;
    }
    tokens.push_back(t);
    if (t.type == TOKEN_EOF) return status;
  }
}

static inline bool is_digit(char c) { return c >= '0' && c <= '9'; }
static inline bool is_alpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

Token Scanner::scan_token() {
  _skip_whitespace();
  this->start = this->current;
  if (_is_at_end()) return _make_token(TOKEN_EOF);

  char c = _advance();
  if (is_alpha(c)) return _identifier();
  if (is_digit(c)) return _number();

  switch (c) {
  case '(': return _make_token(TOKEN_LEFT_PAREN);
  case ')': return _make_token(TOKEN_RIGHT_PAREN);
  case '{': return _make_token(TOKEN_LEFT_BRACE);
  case '}': return _make_token(TOKEN_RIGHT_BRACE);
  case '[': return _make_token(TOKEN_LEFT_BRACKET);
  case ']': return _make_token(TOKEN_RIGHT_BRACKET);
  case ',': return _make_token(TOKEN_COMMA);
  case '.': return _make_token(TOKEN_DOT);
  case '+': return _make_token(_match('=') ? TOKEN_PLUS_EQUAL : TOKEN_PLUS);
  case ';': return _make_token(TOKEN_SEMICOLON);
  case '/': return _make_token(_match('=') ? TOKEN_SLASH_EQUAL : TOKEN_SLASH);
  case '*': return _make_token(_match('=') ? TOKEN_STAR_EQUAL : TOKEN_STAR);
  case ':': return _make_token(TOKEN_COLON);
  case '?': return _make_token(TOKEN_QUESTION);
  case '-':
    if (_match('>')) return _make_token(TOKEN_ARROW);
    return _make_token(_match('=') ? TOKEN_MINUS_EQUAL : TOKEN_MINUS);
  case '!': return _make_token(_match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
  case '=': return _make_token(_match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
  case '<': return _make_token(_match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
  case '>':
    return _make_token(_match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
  case '"': return _string();
  }
  return _error_token("unexpected character");
}

void Scanner::_skip_whitespace() {
  while (true) {
    char c = _peek();
    switch (c) {
    case ' ':
    case '\r':
    case '\t':
      _advance();
      break;
    case '\n':
      this->line++;
      _advance();
      break;
    case '/':
      if (_peek_next() != '/') return;
      while (_peek() != '\n' && !_is_at_end()) _advance();
      break;
    default:
      return;
    }
  }
}

// the token keeps its quotes, the compiler strips them
Token Scanner::_string() {
  while (_peek() != '"' && !_is_at_end()) {
    if (_peek() == '\n') this->line++;
    _advance();
  }
  if (_is_at_end()) return _error_token("unterminated string");
  _advance();
  return _make_token(TOKEN_STRING);
}

/* one generic NUMBER, the type checker looks at the lexeme (is there a
 * '.') and the context to pin it to LekKut / LekThom / LekThomKlang */
Token Scanner::_number() {
  while (is_digit(_peek())) _advance();
  if (_peek() == '.' && is_digit(_peek_next())) {
    _advance();
    while (is_digit(_peek())) _advance();
  }
  return _make_token(TOKEN_NUMBER);
}

Token Scanner::_identifier() {
  while (is_alpha(_peek()) || is_digit(_peek())) _advance();
  return _make_token(_identifier_type());
}

struct Keyword {
  const char *name;
  TokenType type;
};

static const Keyword KEYWORDS[] = {
    {"akthe", TOKEN_VAR},        {"ber", TOKEN_IF},
    {"minjengte", TOKEN_ELSE},   {"somhab", TOKEN_FOR},
    {"nvpeldae", TOKEN_WHILE},   {"rupamun", TOKEN_FUN},
    {"yok", TOKEN_IMPORT},       {"jea", TOKEN_AS},
    {"morvenh", TOKEN_RETURN},   {"sampoan", TOKEN_STRUCT},
    {"tnak", TOKEN_CLASS},       {"jongyeytha", TOKEN_PRINT},
    {"ng", TOKEN_AND},           {"reu", TOKEN_OR},
    {"ok", TOKEN_TRUE},          {"ort", TOKEN_FALSE},
    {"sone", TOKEN_NIL},         {"nis", TOKEN_THIS},
    {"super", TOKEN_SUPER},       {"bongSlanhOun", TOKEN_BONG_SLANH_OUN},
};

TokenType Scanner::_identifier_type() const {
  int length = (int)(current - start);
  for (const Keyword &k : KEYWORDS) {
    if ((int)strlen(k.name) == length && memcmp(k.name, start, length) == 0)
      return k.type;
  }
  return TOKEN_IDENTIFIER;
}

} // namespace steav_frontend
