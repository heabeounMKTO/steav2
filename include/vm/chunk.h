#ifndef STEAV_VM_CHUNK_H
#define STEAV_VM_CHUNK_H

#include "vm/value.h"
#include <cstdint>
#include <vector>

namespace steav_vm {

/* a sampoan is N consecutive slots, so everything that moves data carries
 * a width `w` in slots. the compiler always knows it statically.
 * operands: u8 unless noted, u16 are big endian */
enum OpCode {
  OP_CONSTANT, // u16 idx
  OP_NIL,
  OP_NIL_N, // n, sone for a width n optional
  OP_TRUE,
  OP_FALSE,
  OP_POPN, // n
  OP_DUP,  // n, copy the top n slots (compound assignment re-reads its place)

  // locals are stack slots by index, no name lookups
  OP_GET_LOCAL,  // slot w
  OP_SET_LOCAL,  // slot w, leaves the value
  OP_GET_GLOBAL, // u16 idx, w
  OP_SET_GLOBAL, // u16 idx, w, leaves the value

  OP_STRUCT_FIELD, // off w total: struct on the stack -> just one field
  OP_GET_FIELD,    // off w: instance -> field slots
  OP_SET_FIELD,    // off w: instance value[w] -> value[w]
  OP_GET_INDEX,    // off w: array idx -> slots inside the element
  OP_SET_INDEX,    // off w: array idx value[w] -> value[w]

  /* v[i] on a same-typed sampoan: one slot, `i` is on the stack and gets
   * bounds checked against n (the field count) */
  OP_GET_LOCAL_AT,  // slot n: i -> value
  OP_SET_LOCAL_AT,  // slot n: i value -> value
  OP_GET_GLOBAL_AT, // u16 idx, n
  OP_SET_GLOBAL_AT, // u16 idx, n
  OP_GET_FIELD_AT,  // off n: instance i -> value
  OP_SET_FIELD_AT,  // off n: instance i value -> value
  OP_GET_INDEX_AT,  // off n: array idx i -> value
  OP_SET_INDEX_AT,  // off n: array idx i value -> value
  OP_STRUCT_AT,     // n: struct[n] i -> value, a temporary struct

  /* builtin ops on primitives only, user overloads compile to OP_CALL */
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_NEGATE,
  OP_NOT,
  OP_EQUAL,
  OP_GREATER,
  OP_GREATER_EQUAL,
  OP_LESS,
  OP_LESS_EQUAL,
  OP_CONVERT, // ValueType, LekThom(i) / LekKut(x) / LekThomKlang(i)

  OP_IS_NONE,   // w: optional[w] -> Boolean
  OP_WRAP_SOME, // w: struct[w] -> ok, struct[w]

  OP_JUMP,          // u16 forward
  OP_JUMP_IF_FALSE, // u16 forward, leaves the condition
  OP_LOOP,          // u16 backward

  OP_CALL,               // u16 fn constant, arg slots. rupamun + overloads
  OP_CALL_NATIVE,        // module, fn
  OP_CONSTRUCT_INSTANCE, // u16 class constant, arg slots. tnak + init
  OP_ARRAY,              // u16 count, elem w, u16 struct type (0xffff none), opt
  OP_ARRAY_LEN,
  OP_ARRAY_PUSH, // w: array value[w] -> sone

  OP_PRINT,        // jongyeytha, one slot
  OP_PRINT_STRUCT, // u16 struct type constant, opt

  /* Ahsor + T / T + Ahsor auto-concat: stringify T (jongyeytha's own
   * formatting) before the OP_ADD that follows */
  OP_TO_STRING,        // one slot -> Ahsor
  OP_TO_STRING_STRUCT, // u16 struct type constant, opt: struct[w] -> Ahsor

  OP_RETURN // w
};

#define STEAV_NO_STRUCT_TYPE 0xffff

struct Chunk {
  std::vector<uint8_t> code;
  std::vector<int> lines; // one per byte in code
  std::vector<Value> constants;

  inline void write(uint8_t byte, int line) {
    code.push_back(byte);
    lines.push_back(line);
  }

  inline int add_constant(const Value &value) {
    constants.push_back(value);
    return (int)constants.size() - 1;
  }
};

} // namespace steav_vm
#endif // STEAV_VM_CHUNK_H
