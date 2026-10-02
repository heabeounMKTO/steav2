# Lexical structure

## The opening statement

Every script must start with:

```
bongSlanhOun <expression>;
```

If the first token in the file isn't `bongSlanhOun`, nothing runs — the
scanner/parser reject the file outright (`` na `bongSlanhOun`?!!! ``, exit
65). The expression itself is ceremonial: it's parsed but never checked or
evaluated. Additional `bongSlanhOun`s later in the file are ignored. Only a
top-level script needs one — the built-in stdlib modules (`vector`, `math`,
`random`) don't.

## Comments

Line comments only, `//` to end of line — no block comments:

```
// this line is ignored
akthe x = 5; // so is this part
```

## Literals

- **Numbers** — one generic token (`TOKEN_NUMBER`) for both integers and
  decimals; `5` and `5.0` are scanned identically. The scanner doesn't
  distinguish `LekKut` / `LekThom` / `LekThomKlang` lexically — a numeric
  literal is untyped until the type checker pins it down from context (see
  [types.md](types.md#base-types)), the same way integer literals are
  polymorphic in Rust or Swift until inference resolves them.
- **Strings** — double-quoted, run to the next unescaped `"`. There is no
  escape-sequence processing (no `\n`, `\"`, etc.) — the scanner just scans
  to the closing quote, so a literal newline inside the quotes is a
  multi-line string:
  ```
  akthe multi = "two
  lines";
  ```
  An unterminated string is a scan error.
- **Booleans** — `ok` (true) / `ort` (false), not `true`/`false`.
- **Nil** — `sone`.

## Punctuation

Unchanged from Swift/C-family convention: `{ }` for blocks, `->` for return
types, `:` for type annotations, `[` `]` for arrays, `?` for optionals.
These are symbols, not keywords, so they're never transliterated.

Compound assignment (`+= -= *= /=`) is supported; there is no `++`/`--`.

## Keywords

Every keyword is a Khmer transliteration, carried over from steavscript. The
scanner recognizes these as reserved identifiers (see
`include/frontend/token.h`):

| steav2 | meaning |
|---|---|
| `bongSlanhOun` | required opening statement |
| `akthe` | local/global binding (`var`/`let`) |
| `ber` | `if` |
| `minjengte` | `else` |
| `somhab` | `for` |
| `nvpeldae` | `while` |
| `rupamun` | `func` |
| `yok` | `import` |
| `jea` | `as` (`yok vector jea vec;`) |
| `morvenh` | `return` |
| `sampoan` | `struct` — value types |
| `tnak` | `class` — reference types |
| `jongyeytha` | `print` |
| `ng` | `and` |
| `reu` | `or` |
| `ok` | `true` |
| `ort` | `false` |
| `sone` | `nil` |
| `nis` | `this` (inside a `tnak` method) |
| `super` | reserved, not yet used |

## Types as identifiers

Base types aren't keywords — they're ordinary identifiers the type checker
resolves specially (see [types.md](types.md)):

| steav2 | meaning |
|---|---|
| `Lek` | Number (alias for `LekThom`) |
| `LekKut` | Int |
| `LekThom` | Double |
| `Ahsor` | String |
| `Boolean` | Boolean (untranslated) |
| `OrtMean` | Nil |
| `LekThomKlang` | Long |

`Vec2` / `Vec3` / `Vec4` (from `yok vector;`) also keep their English names,
since they're stdlib-defined `sampoan`s, not language primitives with
reserved keywords.
