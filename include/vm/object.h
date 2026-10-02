#ifndef STEAV_VM_OBJECT_H
#define STEAV_VM_OBJECT_H

#include "vm/chunk.h"
#include "vm/value.h"
#include <string>
#include <vector>

namespace steav_vm {

/* everything behind an Obj* is heap allocated and owned by the gc
 * (mark and sweep, clox style), sampoan values never end up here */
enum ObjType {
  OBJ_STRING,      // Ahsor
  OBJ_FUNCTION,    // rupamun
  OBJ_CLASS,       // tnak declaration
  OBJ_INSTANCE,    // tnak instance
  OBJ_ARRAY,       // [T]
  OBJ_STRUCT_TYPE  // sampoan layout, only for printing
};

struct Obj {
  ObjType type;
  bool is_marked = false;
  size_t size = 0;     // what bytes_allocated got charged for this one
  Obj *next = nullptr; // every object ever allocated, for the sweep
};

struct ObjString : Obj {
  std::string chars;
};

/* a rupamun implemented in C++: reads its params from `args` (flattened
 * slots, receiver first), writes its return value's slots into `out` */
typedef void (*NativeSlotsFn)(const Value *args, Value *out);

struct ObjFunction : Obj {
  int param_slots = 0; // receiver + params, in slots
  NativeSlotsFn native = nullptr; // set = no chunk, OP_CALL runs this instead
  int ret_slots = 1;
  Chunk chunk;
  std::string name; // "script" for the top level
};

struct ObjClass : Obj {
  std::string name;
  int field_slots = 0;         // every field flattened
  ObjFunction *init = nullptr; // rupamun init(...), nullptr = none
};

struct ObjInstance : Obj {
  ObjClass *klass = nullptr;
  std::vector<Value> fields; // flattened, declaration order
};

/* a sampoan never lives on the heap, this is just its layout so
 * jongyeytha can print `Vec3(1, 2, 3)` */
struct ObjStructType;
struct StructFieldDesc {
  std::string name;
  int width;                   // slots, including the optional flag
  ObjStructType *desc;         // nested sampoan, nullptr = one slot
  bool optional;               // S? has a flag slot in front
};
struct ObjStructType : Obj {
  std::string name;
  std::vector<StructFieldDesc> fields;
};

struct ObjArray : Obj {
  int elem_width = 1;                  // slots per element
  ObjStructType *elem_desc = nullptr;  // sampoan elements, for printing
  bool elem_optional = false;
  std::vector<Value> items;            // flattened, elem_width per element
};

// print the slots starting at `slots`, laid out as `desc`
void print_slots(const Value *slots, ObjStructType *desc, bool optional);

// same formatting as print() / print_slots(), built into a string instead of
// written to stdout: what OP_TO_STRING / OP_TO_STRING_STRUCT hand to OP_ADD
// for an Ahsor + T auto-concat
std::string to_string(const Value &v);
std::string to_string_slots(const Value *slots, ObjStructType *desc, bool optional);

} // namespace steav_vm
#endif // STEAV_VM_OBJECT_H
