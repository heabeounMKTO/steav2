#ifndef STEAV_FRONTEND_TOKEN_H
#define STEAV_FRONTEND_TOKEN_H

namespace steav_frontend {

enum TokenType {
  // single char tokens (ti eh)
  TOKEN_LEFT_PAREN,    // (
  TOKEN_RIGHT_PAREN,   // )
  TOKEN_LEFT_BRACE,    // {
  TOKEN_RIGHT_BRACE,   // }
  TOKEN_LEFT_BRACKET,  // [  arrays
  TOKEN_RIGHT_BRACKET, // ]
  TOKEN_COMMA,         // ,
  TOKEN_DOT,           // .
  TOKEN_PLUS,          // +
  TOKEN_SEMICOLON,     // ;
  TOKEN_SLASH,         // /
  TOKEN_STAR,          // *
  TOKEN_COLON,         // :  type annotations
  TOKEN_QUESTION,      // ?  optionals

  // one or two char tokens
  TOKEN_MINUS,         // -
  TOKEN_ARROW,         // -> return types
  TOKEN_BANG,          // !
  TOKEN_BANG_EQUAL,    // !=
  TOKEN_EQUAL,         // =
  TOKEN_EQUAL_EQUAL,   // ==
  TOKEN_GREATER,       // >
  TOKEN_GREATER_EQUAL, // >=
  TOKEN_LESS,          // <
  TOKEN_LESS_EQUAL,    // <=
  TOKEN_PLUS_EQUAL,    // +=
  TOKEN_MINUS_EQUAL,   // -=
  TOKEN_STAR_EQUAL,    // *=
  TOKEN_SLASH_EQUAL,   // /=

  // literals
  TOKEN_IDENTIFIER,
  TOKEN_STRING,
  /* one generic number token, `5` stays untyped until the type
   * checker pins it down (LekKut / LekThom / LekThomKlang) */
  TOKEN_NUMBER,

  // keywords
  TOKEN_VAR,    // akthe
  TOKEN_IF,     // ber
  TOKEN_ELSE,   // minjengte
  TOKEN_FOR,    // somhab
  TOKEN_WHILE,  // nvpeldae
  TOKEN_FUN,    // rupamun
  TOKEN_IMPORT, // yok
  TOKEN_AS,     // jea
  TOKEN_RETURN, // morvenh
  TOKEN_STRUCT, // sampoan
  TOKEN_CLASS,  // tnak
  TOKEN_PRINT,  // jongyeytha
  TOKEN_AND,    // ng
  TOKEN_OR,     // reu
  TOKEN_TRUE,   // ok
  TOKEN_FALSE,  // ort
  TOKEN_NIL,    // sone
  TOKEN_THIS,   // nis
  TOKEN_SUPER,  // super (reserved)
  TOKEN_BONG_SLANH_OUN, // bongSlanhOun, every script starts with one

  TOKEN_ERROR,
  TOKEN_EOF
};

/* points straight into the source buffer so no copies */
struct Token {
  TokenType type;
  const char *start;
  int length;
  int line;
};

} // namespace steav_frontend
#endif // STEAV_FRONTEND_TOKEN_H
