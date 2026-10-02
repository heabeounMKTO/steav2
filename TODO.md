# TODO

## Done
- [x] Project skeleton (CMake, same layout + style as keo2)
- [x] Scanner: keywords incl. `sampoan`, `[ ] : ? ->`, one generic `NUMBER`
- [x] Parser: every grammar rule, `function` fork (identifier / operator / `init`), error recovery
- [x] Type checker
  - [x] shared namespace for `sampoan` / `tnak` / `rupamun` / top-level `akthe`, redeclaration errors
  - [x] base types, `Lek` = `LekThom`, strict number typing, casts via `LekThom(x)` etc.
  - [x] explicit signatures, `akthe` inference, literals pinned from context
  - [x] operator overloads, exact match, no commutativity
  - [x] call sites → struct construction / class construction / function / method / native
  - [x] optionals `T?`, compared against `sone`, narrowing in `ber` / `nvpeldae`
  - [x] arrays `[T]`, `.len()`, `.push(x)`
  - [x] "missing morvenh" check
- [x] Compiler: locals + globals as slots, slot widths for inline structs
- [x] VM: `CallFrame`s, `sampoan` values inline on the value stack (plain copies)
- [x] GC: mark-and-sweep for `tnak` instances, arrays, strings, `STEAV_DEBUG_STRESS_GC`
- [x] Modules: `yok` / `jea`, `vector` as real `.sts` (ported from keo's vec.h), `random` + `math`
- [x] `yok "path/to/file.sts";`: splitting a program across files, resolved relative to the importing file, cycle-checked
- [x] Golden-file tests (`tests/`), incl. a small ray tracer
- [x] `vector` in C++ (keo's vec.h), bound to body-less `rupamun ...;` declarations
- [x] Compound assignment (`+= -= *= /=`) and struct indexing (`v[i]`)
- [x] `steav2-lsp` (diagnostics, hover, completion) + Vim syntax / ftplugin + `install.sh`

## Up next
- [ ] Hook steav2 into keo: an embedding API (load a script, call a `rupamun` by name with Values, read back structs)
- [ ] LSP: go to definition, hover on user-declared names / inferred types (needs the checker's TypeInfo, not just tokens)
- [ ] if-let-style binding (`ber akthe h = scene.hit(...)`), narrowing through `ng`
- [ ] Typed fast-path opcodes (`OP_ADD_THOM` ...) instead of switching on the tag, then profile
- [ ] `math`: `exp`, `log`, `atan2`, `acos`, ... as keo needs them
- [ ] Better errors: one-line messages instead of the logger box for compile errors, source excerpts
- [ ] Drop the trailing `OP_NIL; OP_RETURN` after functions that always return
- [ ] Get `STEAV_ASAN` running on macOS (ASan hangs at startup in the dev environment, UBSan + stress GC pass)
