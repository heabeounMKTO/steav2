#ifndef STEAV_VM_GC_H
#define STEAV_VM_GC_H

#include "vm/object.h"
#include <cstddef>
#include <vector>
#define STEAV_GC_INITIAL_THRESHOLD (1024 * 1024)
#define STEAV_GC_GROW_FACTOR 2

namespace steav_vm {

struct VM; // vm/vm.h

// owns every Obj, the compiler allocates through this too (strings, functions)
struct Heap {
  Obj *objects = nullptr;
  size_t bytes_allocated = 0;
  size_t next_gc = STEAV_GC_INITIAL_THRESHOLD;
  /* nullptr while compiling (nothing gets collected then), set once the
   * vm starts running */
  VM *vm = nullptr;
  std::vector<Obj *> gray;

  /* collects BEFORE allocating, so whatever the caller is building from
   * has to still be reachable (on the stack) at this point */
  template <typename T> inline T *allocate(ObjType type) {
    _maybe_collect(sizeof(T));
    T *obj = new T();
    obj->type = type;
    obj->size = sizeof(T);
    obj->next = objects;
    this->objects = obj;
    this->bytes_allocated += sizeof(T);
    return obj;
  }

  // arrays / strings growing after the fact
  inline void charge(Obj *obj, size_t bytes) {
    _maybe_collect(bytes);
    obj->size += bytes;
    this->bytes_allocated += bytes;
  }

  void collect(VM &vm); // mark + sweep, impl in gc.cpp
  void free_objects();

private:
  void _maybe_collect(size_t incoming);
  void _mark_roots(VM &vm);
  void _mark_value(const Value &value);
  void _mark_object(Obj *obj);
  void _trace_references();
  void _blacken(Obj *obj);
  void _sweep();
};

} // namespace steav_vm
#endif // STEAV_VM_GC_H
