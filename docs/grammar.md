# Grammar

Recursive descent, one function per rule — `include/frontend/parser.h` /
`src/frontend/parser.cpp`, same shape as steavscript's (and jlox's):
precedence climbing for expressions, no separate parser-generator step.

```
declaration    → structDecl | classDecl | funDecl | varDecl | statement

structDecl     → "sampoan" IDENTIFIER "{" typeBody "}"
classDecl      → "tnak" IDENTIFIER "{" typeBody "}"
typeBody       → ( fieldDecl | funDecl )*
fieldDecl      → "akthe" IDENTIFIER ":" type ";"

funDecl        → "rupamun" function
function       → ( IDENTIFIER | operator | "init" ) "(" parameters? ")" ( "->" type )?
                 ( block | ";" )   // ";" = C++ body, stdlib modules only
operator       → "+" | "-" | "*" | "/" | "==" | "!=" | "<" | "<=" | ">" | ">="
parameters     → parameter ( "," parameter )*
parameter      → IDENTIFIER ":" type

varDecl        → "akthe" IDENTIFIER ( ":" type )? ( "=" expression )? ";"

type           → "[" type "]" "?"?
               | ( IDENTIFIER "." )? IDENTIFIER "?"?

expression     → assignment
assignment     → ( call "." IDENTIFIER | call "[" expression "]" | IDENTIFIER )
                 ( "=" | "+=" | "-=" | "*=" | "/=" ) assignment | logic_or
logic_or       → logic_and ( "reu" logic_and )*
logic_and      → equality ( "ng" equality )*
equality       → comparison ( ( "!=" | "==" ) comparison )*
comparison     → term ( ( "<" | "<=" | ">" | ">=" ) term )*
term           → factor ( ( "-" | "+" ) factor )*
factor         → unary ( ( "/" | "*" ) unary )*
unary          → ( "-" | "!" ) unary | call
call           → primary ( "(" arguments? ")" | "." IDENTIFIER | "[" expression "]" )*
primary        → NUMBER | STRING | "ok" | "ort" | "sone"
               | "(" expression ")" | IDENTIFIER | "nis" | arrayLiteral
arrayLiteral   → "[" ( expression ( "," expression )* )? "]"

statement      → exprStmt | printStmt | block | ifStmt | whileStmt
               | forStmt | returnStmt | importStmt
printStmt      → "jongyeytha" expression ";"
block          → "{" declaration* "}"
ifStmt         → "ber" "(" expression ")" statement ( "minjengte" statement )?
whileStmt      → "nvpeldae" "(" expression ")" statement
forStmt        → "somhab" "(" ( varDecl | exprStmt | ";" ) expression? ";"
                 expression? ")" statement
returnStmt     → "morvenh" expression? ";"
importStmt     → "yok" ( IDENTIFIER | STRING ) ( "jea" IDENTIFIER )? ";"
exprStmt       → expression ";"
```

`rupamun`, `sampoan` and `tnak` only go at the top level — no nested
functions or closures in v1. Top-level `akthe`s are globals, addressed by
index like locals.

## Structural notes

- **`structDecl` and `classDecl` share a body-parsing rule** (`typeBody`) —
  same loop, only the wrapping AST node and triggering keyword differ.
- **Operator overloads reuse the ordinary `function` rule.** Parsing
  `rupamun +(a: Vec3, b: Vec3) -> Vec3 { ... }` is one small fork right
  after consuming `rupamun` (peek: is the next token an identifier, an
  overloadable operator, or `init`?) — every branch converges immediately
  after into identical parameter/arrow/block parsing.
- **The parser does not know about operator overloading at all.** `a + b`
  builds the same `Binary` AST node whether `a`/`b` are numbers or
  `Vec3`s — resolving `+` to a user-defined overload (or a built-in, or the
  auto-stringify concat, see [operators.md](operators.md)) is entirely the
  type checker's job, at a later pass.
- **`call` gained a third postfix suffix**, `"[" expression "]"`, for array
  indexing (`verts[0]`) — same postfix chain as function-call-parens and
  dot-access, just one more alternative.
- **`type` is its own recursive rule**, separate from the expression
  grammar, since a type never appears where an expression could (`:`
  always introduces a type). Recursion handles nested arrays (`[[Lek]]`).
- This grammar also fixes steavscript's known "parenthesized expressions
  overflow the stack" bug — `primary`'s `"(" expression ")"` case actually
  consumes the closing paren and returns, instead of recursing back into
  itself.

## Compound assignment

`assignment` parses its left-hand side as an ordinary expression first
(through `logic_or`), then checks it's a valid place (`EXPR_VARIABLE`,
`EXPR_GET`, or `EXPR_INDEX`) once it sees `=`/`+=`/`-=`/`*=`/`/=`. A
compound assignment stays a single `EXPR_ASSIGN` node (the token carries
which operator) so the target is only evaluated once — `xs[i()] += 1`
calls `i()` once, not twice.

## Pipeline

```
source → tokens (Scanner) → AST (Parser) → type-checked AST (TypeChecker)
       → bytecode (Compiler) → VM
```

See [bytecode.md](bytecode.md) for the compiler/VM half of this pipeline.
