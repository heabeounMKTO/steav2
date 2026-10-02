#ifndef STEAV_DEBUG_DISASSEMBLER_H
#define STEAV_DEBUG_DISASSEMBLER_H

#include "vm/chunk.h"

namespace steav_debug {

void disassemble_chunk(const steav_vm::Chunk &chunk, const char *name);
int disassemble_instruction(const steav_vm::Chunk &chunk, int offset);

} // namespace steav_debug
#endif // STEAV_DEBUG_DISASSEMBLER_H
