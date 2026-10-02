# steav2 language reference

This directory is the language specification for `.sts` — the reference for
what a construct means and why, as opposed to the top-level `README.md`,
which is the project pitch (why the rewrite happened, build instructions,
editor setup). Everything here is kept in sync with the actual scanner,
parser, type checker, compiler and VM in `include/` / `src/` — where a page
gives a grammar rule, an opcode layout, or a stdlib signature, it's
transcribed from the source, not from memory.

- **[lexical-structure.md](lexical-structure.md)** — tokens, comments,
  literals, the keyword table, and the `bongSlanhOun` opening statement
  every script needs
- **[grammar.md](grammar.md)** — the full recursive-descent grammar, one
  rule per parser function
- **[types.md](types.md)** — base types, `sampoan` (value types) vs. `tnak`
  (reference types), arrays, optionals, inference and casts
- **[operators.md](operators.md)** — overloadable operators, resolution
  rules, and the built-in `Ahsor + T` auto-stringify concat
- **[modules.md](modules.md)** — the `yok`-importable stdlib overview,
  `yok "path/to/file.sts"` file imports, `math` and `random`
- **[vector.md](vector.md)** — `Vec2`/`Vec3`/`Vec4` (+ `Vec2i`/`Vec3i`/
  `Vec4i`) in full: every operator, method, and free function, with
  worked examples
- **[bytecode.md](bytecode.md)** — the compilation pipeline and the VM's
  opcode reference
- **[cli.md](cli.md)** — the `steav` CLI, exit codes, and the CMake build
  options that shape a build

## Status

v1 is feature-complete end to end (scanner → parser → type checker →
compiler → VM), with `sampoan`s, operator overloads, `tnak`s, arrays,
optionals, a mark-and-sweep GC, and the `vector` / `math` / `random`
modules. See the top-level `TODO.md` for open work and `../README.md`'s
"Planned, not in v1" section for deliberately deferred features
(inheritance, if-let binding, generics).
