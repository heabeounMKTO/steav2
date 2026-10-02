# steav2 (`.sts`)
```
 ___| |_ ___  __ ___   __\_   \\_   \
/ __| __/ _ \/ _` \ \ / / / /\/ / /\/
\__ \ ||  __/ (_| |\ V /\/ /_/\/ /_  
|___/\__\___|\__,_| \_/\____/\____/  
```

A strongly-typed scripting language, implemented as a bytecode VM in C++,
built to be embedded in a ray tracing renderer. Spiritual successor to
steavscript — same `.sts` extension, same Khmer-transliterated keywords,
everything under the hood redesigned.

Where steavscript was a Lox-faithful tree-walking interpreter, steav2 is
compiled to bytecode and run on a stack-based VM, `clox`-style, with real
static typing on top. The keyword vocabulary is the throughline between the
two — most of steavscript's keyword table carries over unchanged.

## Why a rewrite

steavscript proved the front end (scanner/parser/tree-walk evaluator) end to
end, plus a working stdlib (`vector`, `random`, `math`) ported from and
verified against keo's own math code. But a tree-walker re-traverses the AST
and does name-based environment lookups on every evaluation — fine for a
toy, not what you want running millions of times per frame inside a
renderer. steav2 keeps the parts of that design that worked (the stdlib
shape, the keyword vocabulary, the `yok` import syntax) and replaces the
execution model with a compiled bytecode VM, plus adds real static typing.

## Goals

- **Fast enough to live in the hot path.** Materials, shaders, and camera
  logic run per-ray, per-sample. A bytecode VM with indexed local variable
  access (no hashmap lookups) is the baseline; value types that never touch
  the heap or the GC are the other half of that story.
- **Strongly typed, locally inferred.** Function signatures are always
  explicit; local variables infer from their initializer. Type errors are
  caught before the script runs, not mid-render.
- **Same steav-flavored syntax, new semantics underneath.** Struct/class
  split for value vs. reference types, real operator overloading — all
  expressed with the same keyword vocabulary and brace-delimited blocks
  steavscript already used.
- **A real scripting surface for the renderer.** Materials (scatter
  functions), whole scenes (object placement, transforms), shaders/post
  processing, and camera behavior are all meant to be `.sts` scripts, not
  hardcoded in the engine.

## Keywords

| steav2 | meaning |
|---|---|
| `bongSlanhOun` | required opening statement (see below) |
| `akthe` | local binding (`var`/`let`) |
| `ber` | `if` |
| `minjengte` | `else` |
| `somhab` | `for` |
| `nvpeldae` | `while` |
| `rupamun` | `func` |
| `yok` | `import` |
| `jea` | `as` (in `yok vector jea vec;`) |
| `morvenh` | `return` |
| `sampoan` | `struct` (new — value types) |
| `tnak` | `class` (v1 — reference types) |
| `jongyeytha` | `print` |
| `ng` | `and` |
| `reu` | `or` |
| `ok` | `true` |
| `ort` | `false` |
| `sone` | `nil` |
| `nis` | `this` (v1 — refers to the instance inside a `tnak` method) |
| `super` | `super` (reserved) |

Every script has to start with `bongSlanhOun <expression>;`, same as
steavscript: if the first token isn't `bongSlanhOun`, nothing runs
(``na `bongSlanhOun`?!!!``, exit 65). It's ceremonial, the expression is
never checked or evaluated, and more `bongSlanhOun`s later in the file are
ignored. Only your script needs it, not the built-in modules.

Punctuation is unchanged from Swift/C-family convention where steavscript
didn't already have an opinion — `{ }` for blocks, `->` for return types,
`:` for type annotations. These are symbols, not keywords, so they aren't
translated.

## Types

Base types get Khmer names same as the keywords:

| steav2 | meaning |
|---|---|
| `Lek` | Number |
| `LekKut` | Int |
| `LekThom` | Double |
| `Ahsor` | String |
| `Boolean` | Boolean (untranslated) |
| `OrtMean` | Nil |
| `LekThomKlang` | Long |

With `yok vector;` you also get `Vec2`, `Vec3`, `Vec4` (see below) — these
keep their English names since they're stdlib-defined `sampoan`s, not
language primitives with reserved keywords.

## Type system

Two kinds of user-defined types, split by how they're meant to be used:

### `sampoan` — value types

Small, copied on assignment/pass, never heap-allocated, never touched by
the garbage collector. Declared with fields and methods together:

```
sampoan Vec3 {
    akthe x: LekThom;
    akthe y: LekThom;
    akthe z: LekThom;

    rupamun length() -> LekThom {
        morvenh sqrt(x * x + y * y + z * z);
    }
}

rupamun +(a: Vec3, b: Vec3) -> Vec3 {
    morvenh Vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}
```

Operators are real, overloadable functions — `Vec3 + Vec3`, and later
`Color + Color` or `Ray * LekThom`, all use the same mechanism, no
special-casing in the VM.

**Overloadable operators:** `+ - * / == != < <= > >=` (binary), plus unary
`-` for negation. steav2 doesn't assign inherent meaning to any of these —
`<`/`<=`/`>`/`>=` in particular have no built-in semantics for a struct (no
assumed lexicographic or length-based ordering); the author defines
whatever `rupamun <(a: Vec3, b: Vec3) -> Boolean { ... }` should mean, same
as any other operator.

Overload resolution is exact-signature-match only, with **no automatic
commutativity**: a mixed-type operator like `Vec3 * LekThom` and
`LekThom * Vec3` are two distinct overloads that must each be declared
explicitly — the type checker never tries swapped operand order as a
fallback. More typing per mixed-type pair, but overload resolution stays a
straightforward exact match with no implicit rules to reason about.

Assignment and passing copy: a struct is N consecutive slots on the value
stack (nested structs flattened inline, so a `HitRecord { p: Vec3, t: LekThom }`
is 4 slots), and `akthe b = a;` copies those slots. `akthe b = a; b.x = 5;`
never affects `a`. There's no copy-on-write or refcount: for the small,
fixed-size structs a renderer lives on (`Vec3` is 3 slots) a plain copy is
cheaper than sharing, and it keeps structs truly off the heap. Field reads
and writes compile to one slot access at a statically known offset.

A `sampoan` method gets a copy of its receiver, so it can read `nis` (or its
fields by bare name) but not assign into it. Mutate a struct from outside:
`v.x = 5;`.

### `tnak` — reference types

Heap-allocated, shared by reference, garbage collected (mark-and-sweep,
`clox`-style). For anything that's naturally shared or graph-like:
`Material`, `Mesh`, `Light`, `Scene`, `Camera` — and, as of v1, **arrays**
(see below), since a growable, shared collection needs the same heap/GC
machinery a user-defined reference type does. Building `tnak` properly now,
rather than special-casing a one-off reference type just for arrays, means
that machinery only gets built once.

```
tnak Sphere {
    akthe center: Vec3;
    akthe radius: LekThom;

    rupamun init(center: Vec3, radius: LekThom) {
        nis.center = center;
        nis.radius = radius;
    }
}
```

`tnak` instances are constructed via an explicit `init` method — `rupamun
init(...)` is a reserved method name the compiler recognizes specially;
calling `Sphere(Vec3(0, 0, 0), 1.0)` allocates the instance on the heap and
runs `init` with those arguments. `nis` (steavscript's reserved word for
`this`, unused until now) refers to the instance being constructed inside
`init`, same as any other method.

This is a deliberate split from `sampoan`: structs get free, implicit
memberwise construction (no method needed, just list the field values in
order); classes require an explicit `init` since instance creation is
heap allocation plus arbitrary setup logic (validation, defaults,
whatever), not just a field copy.

### Arrays

`[T]` for an array of any type `T` — structs, other arrays, and eventually
`tnak` instances (`[Vec3]`, `[Sphere]`, `[[Lek]]`). Arrays are a built-in
reference type: heap-allocated and GC-managed like any `tnak`, not a value
type, since collections like a `Scene`'s triangle list need to be shared
rather than copied.

```
akthe verts: [Vec3] = [Vec3(0, 0, 0), Vec3(1, 0, 0), Vec3(0, 1, 0)];
akthe first = verts[0];
```

No generic type parameter syntax (`Array<T>`) for v1 — `[T]` is
special-cased grammar, not sugar over a general generics mechanism, since
generics aren't otherwise part of the v1 language.

Arrays have two built-in methods, `xs.len() -> LekKut` and `xs.push(x)`.
Indexes are `LekKut` and bounds-checked at runtime. An empty `[]` needs a
type from context (`akthe xs: [Vec3] = [];`). Elements are stored inline
too: an `[Vec3]` is one flat run of 3-slot elements, so `verts[1].x = 2`
writes one slot.

Arrays compose with `tnak` fields the same as any other type — this is the
actual point of building them as a reference type, so something like a
`Scene` can hold a shared, growable list of objects:

```
tnak Scene {
    akthe objects: [Sphere];

    rupamun init(objects: [Sphere]) {
        nis.objects = objects;
    }
}
```

### Optionals

`T?` for anything that may or may not produce a value — the canonical case
being ray/scene intersection:

```
rupamun hit(ray: Ray, tMin: LekThom, tMax: LekThom) -> HitRecord? {
    // ...
}
```

No combined "check + bind" (if-let) form yet. Instead, comparing a variable
against `sone` narrows it: inside `ber (x != sone)` (and the body of
`nvpeldae (x != sone)`, and the `minjengte` of `ber (x == sone)`) `x` reads
as plain `T`:

```
akthe hitResult = scene.hit(ray, 0.001, INFINITY);
ber (hitResult != sone) {
    jongyeytha hitResult.t; // HitRecord, not HitRecord?
}
```

Assigning to `x` ends the narrowing for the rest of that branch, so
`cur = cur.next;` in a `nvpeldae (cur != sone)` walk just works. It's an
error inside a loop nested in the check, where the next time around would
read it as still checked. Using a `T?` field or method without the check is
a type error. A `T` goes into a `T?` implicitly.

An if-let-style form is a natural later addition on top of this — nothing
about the plan below forecloses it.

### Inference

Function signatures (parameter and return types) are always explicit; a
missing `-> type` means `OrtMean`. `akthe` bindings infer their type from
the initializer:

```
akthe a = 5;             // inferred Lek
akthe b: LekThom? = sone; // explicit, since there's no initializer to infer from
```

`Lek` is another name for `LekThom`. Number types never mix implicitly
(`LekKut + LekThom` is a type error). A literal takes its type from context
(`akthe i: LekKut = 5;`, `i + 1`, a `LekKut` parameter) and is a `Lek`
otherwise. Convert explicitly by calling the type: `LekThom(i)`,
`LekKut(x)` (truncates toward zero), `LekThomKlang(i)`.

## Modules

Same `yok` import syntax as steavscript:

```
yok vector;          // Vec3, dot, cross, ... in scope unqualified
yok vector jea vec;  // vec.Vec3, vec.dot, ... instead
```

Imports aren't transitive, and an unqualified import that clashes with a
name the importing module declares is an error (import it with `jea`
instead). Operator overloads are always in scope, however a module was
imported.

### Your own files

`yok` also takes a quoted path instead of a module name, to split a
program across `.sts` files:

```
yok "helpers.sts";           // in scope unqualified
yok "lib/helpers.sts" jea h; // h.whatever(...) instead
```

The path resolves relative to the *importing file's own directory*, not
the main script's — so `lib/helpers.sts` importing `"deep.sts"` itself
means `lib/deep.sts`, regardless of where the main script lives. The same
file always resolves to the same module no matter which importer's
relative spelling reached it first, so diamond imports and import cycles
(including a file importing itself, transitively or directly) are caught
at compile time rather than silently re-parsing or looping. An imported
file doesn't start with `bongSlanhOun` — only the main script does.

### `vector`

Ships `Vec2`, `Vec3`, and `Vec4`, ported from keo's `math/vec.h`, written
in C++ and compiled into the binary (`src/stdlib/vector.cpp`). They're
still ordinary `sampoan`s to the language (`v.x`, printing, stored inline
on the value stack, never on the heap), but every operation on them is a
C++ function: the module declares each one as a body-less
`rupamun dot(a: Vec3, b: Vec3) -> LekThom;` and the compiler binds it to
the C++ with the same signature. A call to one is a single `OP_CALL`
that runs the C++ directly, without pushing a bytecode frame. Same
formulas as keo, `LekThom` (double) instead of `float`.

- operators: `+ - * /` componentwise, `v + s`, `v - s`, `v * s`, `v / s`,
  `s * v`, unary `-`, `==`, `!=`
- methods: `length()`, `length_sq()`, `normalized()`, `min(o)`, `max(o)`,
  `abs()`, plus `Vec3.near_zero()` and `Vec4.xyz()`
- free functions: `dot`, `cross`, `lerp`, `rotate`, `reflect`, `refract`,
  `schlick_approx`, `zero()`, `one()`, `up()`, `right()`, `forward()`,
  `splat(s)` (keo's `explicit Vec3(float s)`),
  `extend(v: Vec3, w) -> Vec4`, and keo's `random_unit_vector()`,
  `random_in_unit_disk()`, `random_on_hemisphere(n)`
- `Vec2i` / `Vec3i` / `Vec4i`: `LekKut`-component counterparts (same field
  names, no operators/methods of their own), cast to/from with
  `to_lekkut(v) -> Vec3i` / `to_lekthom(v) -> Vec3` (and the usual `2`/`4`
  suffixes) — `to_lekkut*` truncates toward zero and clamps at the 32 bit
  boundary, same as the scalar `LekKut(x)` cast

steav2 has no overloading by name, so the Vec3 version of a free function
gets keo's plain name and the Vec2 / Vec4 versions get a suffix (`dot2`,
`dot4`, `cross2`, `lerp4`, `zero2`, `one4`, ...). keo's `min` / `max` /
`abs` are methods so they don't clash with `math`'s scalar ones. keo's
mutating `normalize()`, `+=`, and `v[i]` aren't there yet (no compound
assignment or struct indexing in v1): write `v = v.normalized();`.

**Not part of `vector`:** `Color` is not a generic linear-algebra type, so
it isn't shipped here — it lives in the ray tracing implementation itself,
where it can grow renderer-specific behavior (gamma correction, clamping,
`toRGB8()`) that a generic vector module shouldn't have to carry.

### `random`, `math`

Native (C++) modules. `math`: `sqrt abs floor ceil min max pow sin cos tan
clamp rad2deg deg2rad`, all over `LekThom`, plus keo's constants (`PI`,
`TAU`, `INFINITY`, `NEAR_ZERO`, ...). `random`: keo's PCG32, same seed and
same stream (`uniform()`, `interval(min, max)`, `seed(s)`,
`seed_stream(s, seq)`).

## Name resolution — structs, classes, and functions share one namespace

`sampoan`, `tnak`, and `rupamun` declarations all live in the same
top-level namespace — a `sampoan Sphere` and a `rupamun Sphere(...)` can't
coexist, same as any ordinary redeclaration error. This is what makes a
call site like `Sphere(...)` unambiguous: the type checker resolves the
name exactly once, at compile time, to one of three kinds of thing, and
the compiler emits different bytecode accordingly — there's no runtime
"what kind of thing is this" dispatch, since steav2 is statically typed
and the compiler already knows by the time it generates code.

| name resolves to | call site means | emitted opcode (roughly) |
|---|---|---|
| `sampoan` | struct construction, inline on the value stack, no heap alloc | nothing: the arguments pushed in field order *are* the struct |
| `tnak` | heap allocation + run `init` | `OP_CONSTRUCT_INSTANCE <type>` |
| `rupamun` | ordinary function call | `OP_CALL` |

Argument matching is the same underlying mechanism (arity + per-argument
type-check against an expected list) in all three cases, just sourced
differently: struct construction matches the struct's field declaration
order positionally (`Vec3(1, 2, 3)` → `x=1, y=2, z=3`, no named-argument
reordering in v1), class construction matches `init`'s parameter list, and
a plain function call matches its own declared parameters.

## Grammar

Recursive descent, same shape as steavscript's (and jlox's) — precedence
climbing for expressions, one function per grammar rule. Most of it is
unchanged; what's new mostly slots in as extra rules or extra postfix
suffixes rather than requiring a different parsing strategy.

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

`rupamun`, `sampoan` and `tnak` only go at the top level (no nested
functions or closures in v1). Top-level `akthe`s are globals, addressed by
index like locals.

A few structural notes:

- **`structDecl` and `classDecl` share a body-parsing rule** (`typeBody`) —
  same loop, only the wrapping AST node and triggering keyword differ.
- **Operator overloads reuse the ordinary `function` rule.** Parsing
  `rupamun +(a: Vec3, b: Vec3) -> Vec3 { ... }` is one small fork right
  after consuming `rupamun` (peek: is the next token an identifier, an
  overloadable operator, or `init`?) — every branch converges immediately
  after into identical parameter/arrow/block parsing.
- **The parser does not know about operator overloading at all.** `a + b`
  builds the same `Binary` AST node whether `a`/`b` are numbers or
  `Vec3`s — resolving `+` to a user-defined overload (or rejecting it) is
  entirely the type checker's job, at a later pass.
- **`call` gained a third postfix suffix**, `"[" expression "]"`, for array
  indexing (`verts[0]`) — same postfix chain as function-call-parens and
  dot-access, just one more alternative.
- **`type` is its own recursive rule**, separate from the expression
  grammar, since a type never appears where an expression could (`:`
  always introduces a type). Recursion handles nested arrays (`[[Lek]]`).
- This grammar is also the fix for steavscript's known "parenthesized
  expressions overflow the stack" bug — `primary`'s `"(" expression ")"`
  case needs to actually consume the closing paren and return, not
  recurse back into itself.

## Architecture

```
source → tokens → AST → type-check pass → compile to bytecode → VM
```

- **Scanner/parser**: brace-delimited, recursive descent — conceptually
  close to steavscript's, adjusted for the new grammar (`sampoan`, operator
  overload declarations, optionals, type annotations). New punctuation over
  steavscript: `[` `]` (arrays), `:` (type annotations), `?` (optionals),
  `->` (return types). Numeric literals stay a single generic token, same
  as steavscript's `NUMBER` — the scanner doesn't distinguish `LekKut` vs.
  `LekThom` vs. `LekThomKlang` lexically; a literal like `5` is untyped
  until the type checker pins it down from context, the same way integer
  literals are polymorphic in Rust or Swift until inference resolves them.
- **Type checker**: runs on the AST before compilation. Verifies function
  signatures, infers local types, resolves operator overloads, checks
  optional unwrapping — so the compiler can assume well-typed input and
  the VM can skip runtime type tag-checking on most operations.
- **Compiler**: AST → flat bytecode instruction array, `clox`-style. Locals
  become stack slots addressed by index, not name lookups.
- **VM**: a stack-based bytecode loop with explicit `CallFrame`s. Value
  types live inline on the value stack; reference types (`tnak` instances,
  arrays) are `Obj*` behind a mark-and-sweep GC.

## Status

v1 works end to end: scanner, parser, type checker, compiler and VM, with
`sampoan`s, operator overloads, `tnak`s, arrays, optionals, a mark-and-sweep
GC and the `vector` / `math` / `random` modules. `tests/` has a golden-file
test per feature, the README's own examples and a small ray tracer
(`tests/raytrace/sphere.sts`). See `TODO.md` for what's left.

## Planned, not in v1

- Inheritance (`tnak` itself is in v1; subclassing is not)
- if-let-style optional binding
- Generics (`[T]` arrays are special-cased grammar, not general generics)
- User-facing GC tuning / introspection
- Anything beyond `vector`/`random`/`math` in the stdlib

## Building

See [`BUILD.md`](BUILD.md) for platform-by-platform instructions (macOS,
Linux, Windows/MSVC, Windows/MinGW, WASM via Emscripten). Quick start:

```sh
cmake -S . -B build
cmake --build build
./build/steav path/to/script.sts
./build/steav --dump path/to/script.sts   # print the bytecode, don't run it
./build/steav --version
```

| option                 | default | what                                  |
|------------------------|---------|---------------------------------------|
| `BUILD_STEAV_EXAMPLES` | ON      | build the `steav` CLI                 |
| `BUILD_STEAV_LSP`      | ON      | build `steav2-lsp`                    |
| `BUILD_STEAV_WASM`     | OFF     | build `steav-wasm.js`/`.wasm` (needs Emscripten) |
| `BUILD_STEAV_TESTS`    | OFF     | register `tests/` with ctest          |
| `STEAV_DEBUG_TRACE`    | OFF     | print disassembly per executed op     |
| `STEAV_DEBUG_PRINT`    | OFF     | disassemble chunks after compiling    |
| `STEAV_ASAN`           | OFF     | build with AddressSanitizer + UBSan   |
| `STEAV_DEBUG_STRESS_GC`| OFF     | collect garbage on every allocation   |

The build type defaults to `Debug`; pass `-DCMAKE_BUILD_TYPE=Release` for an
optimized build. Warnings (`-Wall -Wextra`) are on for everything.

Tests are `.sts` files with a matching `.expected` stdout, or a first line
`// expect: <message>` for ones that must fail:

```sh
cmake -S . -B build -DBUILD_STEAV_TESTS=ON && cmake --build build
ctest --test-dir build        # or: tests/run.sh build/steav
```

To install somewhere, `cmake --install build --prefix <dir>`.

## Editor support

`steav2-lsp` is a language server for `.sts` files:

- live errors from the real scanner, parser and type checker (the same
  checks `steav` runs, so the editor and the CLI never disagree)
- hover docs for keywords (`somhab` → `for`), base types, modules and
  their members (`vec.dot`, `v.length`, `PI`)
- completion for keywords, types, imported names, names declared in the
  file, and module members after `vec.` (it follows `jea` aliases)

It also ships a Vim syntax file for highlighting, an ftplugin so `gc`
comments lines with `//`, and a Lua snippet that registers the `.sts`
filetype and starts the server.

```sh
./install.sh            # or: make install
```

This builds a Release build and puts `steav` and `steav2-lsp` in
`~/.local/bin` (make sure that's on your `PATH`), then copies
`editor/nvim/` into `~/.config/lvim` (or `~/.config/nvim` if there's no
LunarVim). `--prefix DIR`, `--vim-dir DIR` and `--no-editor` change that.
Then add this once to `config.lua` (LunarVim) or `init.lua` (Neovim):

```lua
require("steav2")
```

steavscript uses `.sts` too: if your config still has its
`sts = "steavscript"` / `sts-lsp` block, remove it, the last one to claim
`.sts` wins. Open a `.sts` file and `:LspInfo` should list `steav2-lsp`.
After changing the server or the Vim files, run `./install.sh` again.

## Project layout

One folder per stage of the architecture above, `include/` + `src/` each:

- `frontend/`: tokens, scanner, recursive descent parser (one function per grammar rule)
- `ast/`: index-based AST (header only)
- `typecheck/`: types, the shared symbol namespace, operator overloads, the checker
- `compiler/`: AST + type info → bytecode, locals as stack slots
- `vm/`: `Value`, `Chunk`, heap objects (`tnak` instances, arrays, strings, functions), the mark-and-sweep `Heap`, and the `VM` with `CallFrame`s
- `debug/`: disassembler
- `stdlib/`: built-in modules for `yok` (`math`, `random`, `vector`), all C++ in `src/stdlib/`
- `include/math/`: `pcg32.h` and `constants.h` (from keo2)
- `include/logging/`: logger header (from keo2)
- `examples/steav.cpp`: the CLI, `examples/sts/`: scripts to play with —
  `basics.sts` (variables, control flow, loops, functions) is the place to
  start, `tour.sts` goes on to `sampoan`/`tnak`/optionals/`vector`,
  `mistakes.sts` shows what the type checker catches, `raytracer.sts` is a
  small ray tracer
- `lsp/`: `steav2-lsp` (plus a tiny dependency free JSON reader / writer)
- `editor/nvim/`: syntax, ftplugin, filetype + LSP setup, `install.sh` installs them
- `tests/`: golden-file tests, `tests/run.sh` runs them
