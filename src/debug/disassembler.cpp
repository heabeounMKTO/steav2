#include "debug/disassembler.h"
#include <cstdio>

namespace steav_debug {

struct Disassembler {
  static inline int u16(const steav_vm::Chunk &chunk, int at) {
    return (chunk.code[at] << 8) | chunk.code[at + 1];
  }

  static int simple_instruction(const char *name, int offset) {
    printf("%s\n", name);
    return offset + 1;
  }

  // n u8 operands
  static int byte_instruction(const char *name, const steav_vm::Chunk &chunk,
                              int offset, int n) {
    printf("%-22s", name);
    for (int i = 1; i <= n; i++) printf(" %4d", chunk.code[offset + i]);
    printf("\n");
    return offset + 1 + n;
  }

  // u16 global slot, then width
  static int global_instruction(const char *name, const steav_vm::Chunk &chunk,
                                int offset) {
    printf("%-22s %4d %4d\n", name, u16(chunk, offset + 1), chunk.code[offset + 3]);
    return offset + 4;
  }

  // u16 constant, then n u8 operands
  static int constant_instruction(const char *name, const steav_vm::Chunk &chunk,
                                  int offset, int n) {
    int idx = u16(chunk, offset + 1);
    printf("%-22s %4d '", name, idx);
    chunk.constants[idx].print();
    printf("'");
    for (int i = 0; i < n; i++) printf(" %4d", chunk.code[offset + 3 + i]);
    printf("\n");
    return offset + 3 + n;
  }

  // 16 bit operand, sign = 1 forward (jumps), -1 backward (loop)
  static int jump_instruction(const char *name, int sign,
                              const steav_vm::Chunk &chunk, int offset) {
    int jump = u16(chunk, offset + 1);
    printf("%-22s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
  }
};

void disassemble_chunk(const steav_vm::Chunk &chunk, const char *name) {
  printf("== %s ==\n", name);
  for (int offset = 0; offset < (int)chunk.code.size();) {
    offset = disassemble_instruction(chunk, offset);
  }
}

// public dispatcher
int disassemble_instruction(const steav_vm::Chunk &chunk, int offset) {
  printf("%04d ", offset);
  if (offset > 0 && chunk.lines[offset] == chunk.lines[offset - 1])
    printf("   | ");
  else
    printf("%4d ", chunk.lines[offset]);

  using D = Disassembler;
  uint8_t instruction = chunk.code[offset];
  switch (instruction) {
  case steav_vm::OP_CONSTANT: return D::constant_instruction("OP_CONSTANT", chunk, offset, 0);
  case steav_vm::OP_NIL: return D::simple_instruction("OP_NIL", offset);
  case steav_vm::OP_NIL_N: return D::byte_instruction("OP_NIL_N", chunk, offset, 1);
  case steav_vm::OP_TRUE: return D::simple_instruction("OP_TRUE", offset);
  case steav_vm::OP_FALSE: return D::simple_instruction("OP_FALSE", offset);
  case steav_vm::OP_POPN: return D::byte_instruction("OP_POPN", chunk, offset, 1);
  case steav_vm::OP_GET_LOCAL: return D::byte_instruction("OP_GET_LOCAL", chunk, offset, 2);
  case steav_vm::OP_SET_LOCAL: return D::byte_instruction("OP_SET_LOCAL", chunk, offset, 2);
  case steav_vm::OP_GET_GLOBAL: return D::global_instruction("OP_GET_GLOBAL", chunk, offset);
  case steav_vm::OP_SET_GLOBAL: return D::global_instruction("OP_SET_GLOBAL", chunk, offset);
  case steav_vm::OP_STRUCT_FIELD:
    return D::byte_instruction("OP_STRUCT_FIELD", chunk, offset, 3);
  case steav_vm::OP_GET_FIELD: return D::byte_instruction("OP_GET_FIELD", chunk, offset, 2);
  case steav_vm::OP_SET_FIELD: return D::byte_instruction("OP_SET_FIELD", chunk, offset, 2);
  case steav_vm::OP_GET_INDEX: return D::byte_instruction("OP_GET_INDEX", chunk, offset, 2);
  case steav_vm::OP_SET_INDEX: return D::byte_instruction("OP_SET_INDEX", chunk, offset, 2);
  case steav_vm::OP_DUP: return D::byte_instruction("OP_DUP", chunk, offset, 1);
  case steav_vm::OP_GET_LOCAL_AT: return D::byte_instruction("OP_GET_LOCAL_AT", chunk, offset, 2);
  case steav_vm::OP_SET_LOCAL_AT: return D::byte_instruction("OP_SET_LOCAL_AT", chunk, offset, 2);
  case steav_vm::OP_GET_GLOBAL_AT: return D::global_instruction("OP_GET_GLOBAL_AT", chunk, offset);
  case steav_vm::OP_SET_GLOBAL_AT: return D::global_instruction("OP_SET_GLOBAL_AT", chunk, offset);
  case steav_vm::OP_GET_FIELD_AT: return D::byte_instruction("OP_GET_FIELD_AT", chunk, offset, 2);
  case steav_vm::OP_SET_FIELD_AT: return D::byte_instruction("OP_SET_FIELD_AT", chunk, offset, 2);
  case steav_vm::OP_GET_INDEX_AT: return D::byte_instruction("OP_GET_INDEX_AT", chunk, offset, 2);
  case steav_vm::OP_SET_INDEX_AT: return D::byte_instruction("OP_SET_INDEX_AT", chunk, offset, 2);
  case steav_vm::OP_STRUCT_AT: return D::byte_instruction("OP_STRUCT_AT", chunk, offset, 1);
  case steav_vm::OP_ADD: return D::simple_instruction("OP_ADD", offset);
  case steav_vm::OP_SUBTRACT: return D::simple_instruction("OP_SUBTRACT", offset);
  case steav_vm::OP_MULTIPLY: return D::simple_instruction("OP_MULTIPLY", offset);
  case steav_vm::OP_DIVIDE: return D::simple_instruction("OP_DIVIDE", offset);
  case steav_vm::OP_NEGATE: return D::simple_instruction("OP_NEGATE", offset);
  case steav_vm::OP_NOT: return D::simple_instruction("OP_NOT", offset);
  case steav_vm::OP_EQUAL: return D::simple_instruction("OP_EQUAL", offset);
  case steav_vm::OP_GREATER: return D::simple_instruction("OP_GREATER", offset);
  case steav_vm::OP_GREATER_EQUAL: return D::simple_instruction("OP_GREATER_EQUAL", offset);
  case steav_vm::OP_LESS: return D::simple_instruction("OP_LESS", offset);
  case steav_vm::OP_LESS_EQUAL: return D::simple_instruction("OP_LESS_EQUAL", offset);
  case steav_vm::OP_CONVERT: return D::byte_instruction("OP_CONVERT", chunk, offset, 1);
  case steav_vm::OP_IS_NONE: return D::byte_instruction("OP_IS_NONE", chunk, offset, 1);
  case steav_vm::OP_WRAP_SOME: return D::byte_instruction("OP_WRAP_SOME", chunk, offset, 1);
  case steav_vm::OP_JUMP: return D::jump_instruction("OP_JUMP", 1, chunk, offset);
  case steav_vm::OP_JUMP_IF_FALSE:
    return D::jump_instruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
  case steav_vm::OP_LOOP: return D::jump_instruction("OP_LOOP", -1, chunk, offset);
  case steav_vm::OP_CALL: return D::constant_instruction("OP_CALL", chunk, offset, 1);
  case steav_vm::OP_CALL_NATIVE:
    return D::byte_instruction("OP_CALL_NATIVE", chunk, offset, 2);
  case steav_vm::OP_CONSTRUCT_INSTANCE:
    return D::constant_instruction("OP_CONSTRUCT_INSTANCE", chunk, offset, 1);
  case steav_vm::OP_ARRAY:
    // u16 count, w, u16 struct type, opt
    printf("%-22s %4d %4d %4d %4d\n", "OP_ARRAY", D::u16(chunk, offset + 1),
           chunk.code[offset + 3], D::u16(chunk, offset + 4), chunk.code[offset + 6]);
    return offset + 7;
  case steav_vm::OP_ARRAY_LEN: return D::simple_instruction("OP_ARRAY_LEN", offset);
  case steav_vm::OP_ARRAY_PUSH: return D::byte_instruction("OP_ARRAY_PUSH", chunk, offset, 1);
  case steav_vm::OP_PRINT: return D::simple_instruction("OP_PRINT", offset);
  case steav_vm::OP_PRINT_STRUCT:
    return D::constant_instruction("OP_PRINT_STRUCT", chunk, offset, 1);
  case steav_vm::OP_TO_STRING: return D::simple_instruction("OP_TO_STRING", offset);
  case steav_vm::OP_TO_STRING_STRUCT:
    return D::constant_instruction("OP_TO_STRING_STRUCT", chunk, offset, 1);
  case steav_vm::OP_RETURN: return D::byte_instruction("OP_RETURN", chunk, offset, 1);
  default:
    printf("unknown opcode %d\n", instruction);
    return offset + 1;
  }
}

} // namespace steav_debug
