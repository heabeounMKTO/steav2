# Type system

steav2 is statically typed with local inference. Function signatures
(parameter and return types) are always explicit; a missing `-> type` means
`OrtMean` (nil). Local `akthe` bindings infer their type from the
initializer:

```
akthe a = 5;              // inferred Lek
akthe b: LekThom? = sone; // explicit, since there's no initializer to infer from
```

The type checker runs on the whole AST before compilation — it verifies
function signatures, infers local types, resolves operator overloads, and
checks optional unwrapping, so the compiler can assume well-typed input and
the VM can skip runtime type-tag checks on most operations.

## Base types

| steav2 | meaning |
|---|---|
| `Lek` | Number — alias for `LekThom` |
| `LekKut` | Int |
| `LekThom` | Double |
| `LekThomKlang` | Long |
| `Ahsor` | String |
| `Boolean` | Boolean |
| `OrtMean` | Nil |

Number types never mix implicitly: `LekKut + LekThom` is a type error
("numbers never mix implicitly, convert one with `LekThom(x)` /
`LekKut(x)`"). A numeric literal takes its type from context — an
initializer, a typed parameter, an operator's other operand — and defaults
to `Lek` otherwise:

```
akthe i: LekKut = 5;  // literal pinned to LekKut by the annotation
i + 1;                 // the 1 becomes LekKut too, from i
```

Convert explicitly by calling the type as a function: `LekThom(i)`,
`LekKut(x)` (truncates toward zero, two's-complement wrap on overflow, not
UB), `LekThomKlang(i)`. This compiles to `OP_CONVERT` (see
[bytecode.md](bytecode.md#numeric-casts)).

## `sampoan` — value types

Declared with fields and methods together, never heap-allocated, never
touched by the GC:

```
sampoan Vec3 {
    akthe x: LekThom;
    akthe y: LekThom;
    akthe z: LekThom;

    rupamun length() -> LekThom {
        morvenh sqrt(x * x + y * y + z * z);
    }
}
```

- **No constructor needed** — free, implicit memberwise construction:
  `Vec3(1, 2, 3)` in field-declaration order, no named-argument reordering
  in v1.
- **Copied on assignment and pass.** A struct is N consecutive slots on the
  value stack (nested structs flattened inline, so a
  `HitRecord { p: Vec3, t: LekThom }` is 4 slots), and `akthe b = a;` copies
  those slots. `b.x = 5;` afterward never affects `a` — no copy-on-write,
  no refcounting. For the small, fixed-size structs a renderer lives on
  (`Vec3` is 3 slots), a plain copy is cheaper than sharing, and it keeps
  structs truly off the heap.
- **Field reads/writes are one slot access** at a statically known offset —
  no hashmap lookup.
- **A method receives a copy of its receiver.** It can read `nis` (or bare
  field names) but not assign into it — mutate a struct from outside:
  `v.x = 5;`.

## `tnak` — reference types

Heap-allocated, shared by reference, garbage collected (mark-and-sweep,
`clox`-style). For anything naturally shared or graph-like — `Material`,
`Mesh`, `Light`, `Scene`, `Camera` — and, as of v1, **arrays** (below),
since a growable shared collection needs the same heap/GC machinery a
user-defined reference type does.

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

Instances are constructed via an explicit `init` method — `rupamun
init(...)` is a reserved method name the compiler recognizes specially.
Calling `Sphere(Vec3(0, 0, 0), 1.0)` allocates the instance on the heap
(`OP_CONSTRUCT_INSTANCE`) and runs `init` with those arguments. `nis`
refers to the instance being constructed inside `init`, same as any other
method.

This is a deliberate split from `sampoan`: structs get free memberwise
construction (just list field values in order); classes require an
explicit `init` since instance creation is heap allocation plus arbitrary
setup logic (validation, defaults, whatever), not just a field copy.

Subclassing is **not** in v1 (`super` is reserved but unused).

## Arrays

`[T]` for an array of any type `T` — structs, other arrays, and eventually
`tnak` instances (`[Vec3]`, `[Sphere]`, `[[Lek]]`). Arrays are a built-in
reference type: heap-allocated and GC-managed like any `tnak`, not a value
type, since collections like a `Scene`'s triangle list need to be shared
rather than copied.

```
akthe verts: [Vec3] = [Vec3(0, 0, 0), Vec3(1, 0, 0), Vec3(0, 1, 0)];
akthe first = verts[0];
```

No generic type parameter syntax (`Array<T>`) in v1 — `[T]` is
special-cased grammar, not sugar over a general generics mechanism.

Two built-in methods: `xs.len() -> LekKut` and `xs.push(x)`. Indexes are
`LekKut` and bounds-checked at runtime — an out-of-bounds index is a
runtime error, not UB. An empty `[]` needs a type from context (`akthe xs:
[Vec3] = [];`). Elements are stored inline: an `[Vec3]` is one flat run of
3-slot elements, so `verts[1].x = 2` writes one slot.

## Optionals

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

- Assigning to `x` ends the narrowing for the rest of that branch, so `cur =
  cur.next;` in a `nvpeldae (cur != sone)` walk just works.
- It's an error inside a loop nested in the check, where the next iteration
  would read it as still checked.
- Using a `T?` field or method without the check first is a type error.
- A `T` converts into a `T?` implicitly (`expr_coerce` in the type checker,
  `OP_WRAP_SOME` in the compiler).

**Representation:** a non-struct optional is one slot — the value, or
`VAL_NIL` for `sone`. A struct optional is the struct's width plus one flag
slot in front (`OP_WRAP_SOME` / `OP_NIL_N` set that flag).

## Name resolution — structs, classes, and functions share one namespace

`sampoan`, `tnak`, and `rupamun` declarations all live in the same
top-level namespace — a `sampoan Sphere` and a `rupamun Sphere(...)` can't
coexist, same as any ordinary redeclaration error. This is what makes a
call site like `Sphere(...)` unambiguous: the type checker resolves the
name exactly once, at compile time, to one of three kinds of thing, and the
compiler emits different bytecode accordingly — there's no runtime "what
kind of thing is this" dispatch, since the compiler already knows by the
time it generates code.

| name resolves to | call site means | emitted opcode (roughly) |
|---|---|---|
| `sampoan` | struct construction, inline on the value stack, no heap alloc | nothing — arguments pushed in field order *are* the struct |
| `tnak` | heap allocation + run `init` | `OP_CONSTRUCT_INSTANCE <type>` |
| `rupamun` | ordinary function call | `OP_CALL` |

Argument matching is the same underlying mechanism (arity + per-argument
type-check against an expected list) in all three cases, just sourced
differently: struct construction matches field declaration order
positionally, class construction matches `init`'s parameter list, and a
plain function call matches its own declared parameters.
