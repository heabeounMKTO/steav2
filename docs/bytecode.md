# Bytecode and the VM

```
source → tokens (Scanner) → AST (Parser) → type-checked AST (TypeChecker)
       → bytecode (Compiler) → VM
```

- **Scanner/parser** (`src/frontend/`): brace-delimited, recursive
  descent, conceptually close to steavscript's. See
  [lexical-structure.md](lexical-structure.md) and
  [grammar.md](grammar.md).
- **Type checker** (`src/typecheck/`): runs on the whole AST before
  compilation. See [types.md](types.md) and [operators.md](operators.md).
- **Compiler** (`src/compiler/`): AST + type info → a flat `Chunk` of
  bytecode, `clox`-style. Locals become stack slots addressed by index,
  never by name.
- **VM** (`src/vm/vm.cpp`): a stack-based bytecode loop with explicit
  `CallFrame`s (`include/vm/vm.h`). Value types live inline on the value
  stack; reference types (`tnak` instances, arrays, strings) are `Obj*`
  behind a mark-and-sweep GC (`include/vm/gc.h`, `src/vm/gc.cpp`).

## Values and slots

`Value` (`include/vm/value.h`) is one stack slot — a tagged union of
`VAL_NIL` / `VAL_BOOL` / `VAL_LEK_KUT` / `VAL_LEK_THOM` /
`VAL_LEK_THOM_KLANG` / `VAL_OBJ`. A `sampoan` is never one `Value` — it's
several slots side by side on the stack (nested structs flattened inline),
so almost every opcode that moves data carries an explicit width `w` in
slots, known statically by the compiler. `Obj` (`include/vm/object.h`)
covers everything heap-allocated: `ObjString`, `ObjFunction`, `ObjClass`,
`ObjInstance`, `ObjArray`, and `ObjStructType` (a struct's field layout,
kept around purely so `jongyeytha` can format `tnak`/array elements it
finds at runtime).

## Opcode reference

One VM instruction per line below, operand encoding per the comments in
`include/vm/chunk.h`: `u8` unless noted, `u16` big-endian, `w`/`n` = a
slot-width operand the compiler always knows statically.

| opcode | operands | effect |
|---|---|---|
| `OP_CONSTANT` | u16 idx | push `constants[idx]` |
| `OP_NIL` | | push one `sone` |
| `OP_NIL_N` | n | push `n` `sone`s (a width-`n` optional's empty case) |
| `OP_TRUE` / `OP_FALSE` | | push `ok` / `ort` |
| `OP_POPN` | n | drop the top `n` slots |
| `OP_DUP` | n | copy the top `n` slots (compound assignment re-reads its place) |
| `OP_GET_LOCAL` / `OP_SET_LOCAL` | slot, w | read/write `w` slots at a frame-relative local slot |
| `OP_GET_GLOBAL` / `OP_SET_GLOBAL` | u16 idx, w | same, for a global |
| `OP_STRUCT_FIELD` | off, w, total | struct on the stack → just one field (drops the rest) |
| `OP_GET_FIELD` / `OP_SET_FIELD` | off, w | instance → field slots / instance value[w] → value[w] |
| `OP_GET_INDEX` / `OP_SET_INDEX` | off, w | array idx → element slots, bounds-checked |
| `OP_GET_LOCAL_AT` / `OP_SET_LOCAL_AT` | slot, n | `v[i]` on a same-typed struct: `i` on the stack, bounds-checked against `n` fields |
| `OP_GET_GLOBAL_AT` / `OP_SET_GLOBAL_AT` | u16 idx, n | same, global |
| `OP_GET_FIELD_AT` / `OP_SET_FIELD_AT` | off, n | same, through an instance field |
| `OP_GET_INDEX_AT` / `OP_SET_INDEX_AT` | off, n | same, through an array element |
| `OP_STRUCT_AT` | n | `struct[n]` `i` → value, for a temporary struct not in a place |
| `OP_ADD` | | pop 2, push sum — `Ahsor + Ahsor` allocates a concatenated `ObjString`, otherwise numeric add |
| `OP_SUBTRACT` / `OP_MULTIPLY` / `OP_DIVIDE` | | numeric only, both operands proven equal-typed by the checker |
| `OP_NEGATE` / `OP_NOT` | | unary `-` / `!` |
| `OP_EQUAL` / `OP_GREATER` / `OP_GREATER_EQUAL` / `OP_LESS` / `OP_LESS_EQUAL` | | comparisons, push `Boolean` |
| `OP_CONVERT` | ValueType | numeric cast — `LekThom(i)` / `LekKut(x)` / `LekThomKlang(i)` |
| `OP_IS_NONE` | w | `optional[w]` → `Boolean` (the `x == sone` / `!= sone` check) |
| `OP_WRAP_SOME` | w | `struct[w]` → flag `ok` + `struct[w]` (`T` → `T?` implicit coercion) |
| `OP_JUMP` / `OP_JUMP_IF_FALSE` | u16 forward | unconditional / conditional jump (the latter leaves the condition on the stack) |
| `OP_LOOP` | u16 backward | jump back, for `nvpeldae`/`somhab` |
| `OP_CALL` | u16 fn constant, arg slots | `rupamun`, overload resolution, and stdlib natives all end up here |
| `OP_CALL_NATIVE` | module, fn | a stdlib native by module/function index |
| `OP_CONSTRUCT_INSTANCE` | u16 class constant, arg slots | `tnak` heap allocation + run `init` |
| `OP_ARRAY` | u16 count, elem w, u16 struct type (`0xffff` = none), opt | build an `ObjArray` from `count` elements already on the stack |
| `OP_ARRAY_LEN` | | `xs.len()` |
| `OP_ARRAY_PUSH` | w | `xs.push(value[w])` → `sone` |
| `OP_PRINT` | | `jongyeytha`, one slot → stdout + `\n` |
| `OP_PRINT_STRUCT` | u16 struct type constant, opt | same, for a struct operand (needs the layout to format field names) |
| `OP_TO_STRING` | | one slot → `Ahsor`, same formatting as `OP_PRINT` — see [operators.md](operators.md#ahsor--t-auto-stringify-concat) |
| `OP_TO_STRING_STRUCT` | u16 struct type constant, opt | same, for a struct operand |
| `OP_RETURN` | w | pop the current frame, leaving `w` result slots where the call was |

### Printing

`jongyeytha` compiles to `OP_PRINT` for a single-slot value or
`OP_PRINT_STRUCT` for a struct, carrying the struct's `ObjStructType`
constant so the VM knows field names and nesting at print time
(`Compiler::_stmt`, `STMT_PRINT` case, `src/compiler/compiler.cpp`). The
actual formatting lives in `Value::print()` / `print_slots()`
(`src/vm/value.cpp`): numbers print the shortest decimal that reads back
as the same double (`0.1`, never `0.1000000000000001` or `1e-01`),
booleans as `ok`/`ort`, `sone` for nil, `Name(field, field, ...)` for
structs, `[a, b, c]` for arrays (recursing into `ObjArray::elem_desc` for
struct elements), and `<rupamun name>` / `<tnak name>` / `<name instance>`
/ `<sampoan name>` for the reference-type cases that aren't meant to be
read as data.

`to_string()` / `to_string_slots()` in the same file are string-building
twins of exactly that formatting — `OP_TO_STRING` / `OP_TO_STRING_STRUCT`
call them to materialize an `Ahsor` instead of writing to stdout, which is
what the `Ahsor + T` auto-concat operator fallback runs on its non-string
operand before the `OP_ADD` that follows.

### Numeric casts

`LekThom(i)`, `LekKut(x)`, `LekThomKlang(i)` compile to `OP_CONVERT` with
the target `ValueType` as its operand (`CALL_CAST` in the type checker).
`LekKut(x)` truncates toward zero; out-of-range doubles wrap via
`(uint32_t)`/`(uint64_t)` casts (two's-complement, not UB) rather than
saturating or trapping.

## GC discipline

`Heap::allocate<T>()` (`include/vm/gc.h`) **collects before allocating**,
not after — `_maybe_collect` runs first, so whatever the caller is
building the new object *from* has to still be reachable (a GC root, i.e.
still on the VM value stack or in a `CallFrame`) at the point `allocate` is
called, or already fully copied out into plain C++ data that doesn't need
tracing. This is why:

- `OP_ADD`'s string concatenation reads both operand `ObjString`s' `chars`
  *while they're still on the stack*, and only pops them after
  `heap.allocate<ObjString>()` returns.
- `OP_PRINT_STRUCT` formats directly off `stack_top - w` and only
  decrements `stack_top` afterward — nothing is allocated during printing,
  so this is really just "don't pop before you're done reading", not a GC
  requirement, but it's the same discipline.
- `OP_TO_STRING` / `OP_TO_STRING_STRUCT` build the full `std::string`
  first (`to_string()` / `to_string_slots()`, no allocation happens during
  that walk), *then* pop, *then* `heap.allocate<ObjString>()` — by the
  time an allocation (and thus a possible collection) can happen, the
  source value's data has already been fully copied into a plain
  `std::string` that doesn't reference the VM heap at all, so it doesn't
  matter whether the source `Obj` survives the collection.

## Disassembler

`src/debug/disassembler.cpp` decodes a `Chunk` back into this same table,
one instruction per line, driven by `STEAV_DEBUG_TRACE` (per-instruction,
with the live stack printed alongside) or `STEAV_DEBUG_PRINT` (whole
chunks, right after compiling) — see [cli.md](cli.md#cmake-build-options).
