# Operators

## Overloadable operators

`+ - * / == != < <= > >=` (binary), plus unary `-` for negation. steav2
doesn't assign inherent meaning to any of these for a user type —
`<`/`<=`/`>`/`>=` in particular have no built-in semantics for a `sampoan`
(no assumed lexicographic or length-based ordering); the author defines
whatever `rupamun <(a: Vec3, b: Vec3) -> Boolean { ... }` should mean, same
as any other operator:

```
rupamun +(a: Vec3, b: Vec3) -> Vec3 {
    morvenh Vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}
```

Operators are real, overloadable functions — `Vec3 + Vec3`, `Color +
Color`, `Ray * LekThom`, all use the same mechanism (`OP_CALL`), no
special-casing in the VM.

## Resolution order

`TypeChecker::_check_binary` (`src/typecheck/type_checker.cpp`) tries, in
order:

1. **A user overload.** `_find_overload(op, arity, lhs_type, rhs_type)` —
   exact signature match only.
2. **A built-in op on matching primitive types** — arithmetic on two equal
   numeric types, comparison on two equal numeric types, equality on
   numbers/`Boolean`/`Ahsor`, and `Ahsor + Ahsor` (concatenation).
3. **The `Ahsor + T` / `T + Ahsor` auto-stringify concat** (below) — only
   for `+`, only when exactly one side is a non-optional `Ahsor`.
4. Otherwise, a compile error: `no operator '<op>' for <A> and <B>`, with a
   hint about explicit numeric casts if both sides are numeric but unequal
   types.

**No automatic commutativity.** A mixed-type operator like `Vec3 * LekThom`
and `LekThom * Vec3` are two distinct overloads that must each be declared
explicitly (see `vector`'s `rupamun *(a: Vec3, s: LekThom)` and `rupamun
*(s: LekThom, a: Vec3)` in [modules.md](modules.md)) — overload resolution
never tries swapped operand order as a fallback.

## `Ahsor + T` auto-stringify concat

`jongyeytha` already knows how to render any value (numbers, `Boolean`,
`sone`, structs, arrays, instances — see
[bytecode.md](bytecode.md#printing)). `+` reuses that same formatting to
let you build a string out of one inline, without an explicit `str(x)`
call:

```
akthe n = 5;
jongyeytha "n = " + n;   // "n = 5"
jongyeytha n + " is n";  // "5 is n"

akthe v = Vec3(1, 2, 3);
jongyeytha "v = " + v;   // "v = Vec3(1, 2, 3)"
```

This is a **built-in fallback**, not a user-declarable overload set — it
only kicks in once steps 1–2 above have failed to resolve `+`, so an
explicit `rupamun +(a: Ahsor, b: Vec3) -> Ahsor` (if one existed) would
still win. Rules:

- Exactly one operand must resolve to a non-optional `Ahsor`; the other may
  be any type (number, `Boolean`, `sone`, a struct, an array, a class
  instance) — a plain `Ahsor + Ahsor` is handled by the built-in
  concatenation above, not this fallback.
- Both operand orders work: `"label: " + x` and `x + "label"`.
- An `Ahsor?` (optional string) doesn't trigger it — only `Ahsor` itself.
- The formatting matches `jongyeytha` exactly: `LekThom` prints the
  shortest round-tripping form (`0.1`, not `0.1000000000000001`), `sone`
  for nil, `ok`/`ort` for booleans, `Name(field, field, ...)` for structs,
  `[a, b, c]` for arrays.

Mechanically: the type checker marks which side needs conversion
(`TypeInfo::str_side`), and the compiler emits `OP_TO_STRING` (or
`OP_TO_STRING_STRUCT` for a struct operand) on that operand right after
it's compiled, before the `OP_ADD` that follows — see
[bytecode.md](bytecode.md#printing).

## Number/string caveats

- Numbers never mix implicitly with each other (`LekKut + LekThom` is a
  type error) — but a number *does* auto-concat with a string via the
  fallback above, since that's a formatting operation, not arithmetic.
- **Compound assignment doesn't get the fallback.** `_check_compound`
  (`s += n;`) has its own resolution — user overload, then an exact
  same-type builtin — and doesn't fall through to the `Ahsor + T`
  auto-concat the way `_check_binary` does. `s = s + n;` auto-concats;
  `s += n;` (for `s: Ahsor`, `n` anything but `Ahsor`) is currently a type
  error. Worth aligning if this trips someone up.
