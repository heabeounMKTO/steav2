# Modules

Same `yok` import syntax as steavscript:

```
yok vector;          // Vec3, dot, cross, ... in scope unqualified
yok vector jea vec;  // vec.Vec3, vec.dot, ... instead
```

Imports aren't transitive, and an unqualified import that clashes with a
name the importing module declares is an error (import it with `jea`
instead). Operator overloads are always in scope, however a module was
imported.

Every stdlib module is native C++ (`src/stdlib/*.cpp`): it declares its
steav-side signatures as body-less `rupamun name(...) -> T;` (the type
checker binds names and types from these) and provides the matching C++
implementation the compiler wires each call to directly — a call to one is
a single `OP_CALL` that runs the C++ function, without pushing a bytecode
frame.

## Your own files

`yok` also takes a quoted path instead of a module name:

```
yok "helpers.sts";           // in scope unqualified
yok "lib/helpers.sts" jea h; // h.whatever(...) instead
```

Mechanically this is the exact same machinery as a stdlib module — every
imported file is just another entry in `Ast::modules`, with its own
`ModuleScope` (own namespace, unqualified only where `yok`ed bare), parsed
and type-checked/compiled through the identical per-module passes the
main script's own declarations go through (`TypeChecker::check`,
`Compiler::compile` both just loop `ast.module_order`, blind to whether a
given module is native, stdlib, or a `yok`ed file). The only things that
are actually new:

- **Parsing**: `import_stmt` accepts a `STRING` in addition to an
  `IDENTIFIER`; `Parser::parse(ast)` for module `!= 0` already skips the
  `bongSlanhOun` requirement (`require_bong = module == 0`), so this
  needed no changes for imported files.
- **Resolution**: `Ast::resolve_import()` normalizes a `yok "..."` path
  against the *importing module's own* `dir` (not the main script's) —
  `include/ast/ast.h`'s `path_join`/`path_normalize`. The normalized path
  becomes that `Module`'s `name`, so `Ast::find_module()` (name-based
  lookup, unchanged) dedupes correctly regardless of which importer's
  relative spelling reached the file first, and the existing DFS
  cycle-detection in `order_modules` (`src/vm/vm.cpp`) catches an import
  cycle through file imports the same way it already did for two stdlib
  modules importing each other.
- **Loading**: `load_imports` reads the file itself (plain `<fstream>`,
  only reachable through the CLI's real filesystem — see
  [bytecode.md](bytecode.md) for why the VM/checker otherwise never touch
  disk) and owns the text in `Ast::file_sources` (a `std::deque`, not a
  `std::vector`: tokens point straight into it, and unlike a vector, a
  deque never invalidates existing elements' addresses when it grows).

`VM::interpret` / `check_source` take an optional `path` — the main
script's own file, if it has one, anchoring its top-level `yok "..."`s
the same way. Without one (an inline snippet, or the WASM embedding,
which has no filesystem to begin with) relative imports resolve against
the current working directory instead, which for the WASM build under
`-sFILESYSTEM=0` just means `yok "..."` fails cleanly with the same
"couldn't open" compile error as a genuinely missing file, rather than
being a distinct unsupported-feature error path.

## `vector`

`Vec2`, `Vec3`, `Vec4` (ported from keo's `math/vec.h`) plus their
`Vec2i`/`Vec3i`/`Vec4i` `LekKut` counterparts — operators, methods, free
functions (`dot`, `cross`, `lerp`, `rotate`, `reflect`, `refract`, the
random-sampling helpers, ...), and casts between the two. Big enough to
get its own page: **[vector.md](vector.md)**.

`random_unit_vector` / `random_in_unit_disk` / `random_on_hemisphere` are
declared there (that's where `Vec3` is) but pull from `random`'s shared
RNG state, below. `Color` is deliberately not part of `vector` — see
vector.md's note on why.

## `math`

Native, `src/stdlib/math.cpp`. Every function is `LekThom` in, `LekThom`
out:

```
sqrt(x)  abs(x)  floor(x)  ceil(x)  min(a, b)  max(a, b)  pow(a, b)
sin(x)  cos(x)  tan(x)  clamp(x, lo, hi)  rad2deg(x)  deg2rad(x)
```

Constants (same values as keo, just without the `M_` prefix):

```
INFINITY  PI  TAU  PI_2  PI_4  SQRT2  SQRT1_2  SQRT3  SQRT1_3
INV_PI  E  LOG2E  LOG10E  LN2  LN10  NEAR_ZERO
```

## `random`

Native, `src/stdlib/random.cpp`. One PCG32 RNG shared for the whole
process (keo's algorithm) — default seed `98737613`, stream `1`, so an
unseeded script produces the same numbers every run until it calls `seed`:

```
seed(s: LekKut) -> OrtMean;
seed_stream(s: LekKut, seq: LekKut) -> OrtMean;  // keo's PCG32(seed, seq)
uniform() -> LekThom;
interval(min: LekThom, max: LekThom) -> LekThom;
```

The `Vec3` samplers (`random_unit_vector`, `random_in_unit_disk`,
`random_on_hemisphere`) read this same shared RNG but are declared under
`vector`, since they return `Vec3`.
