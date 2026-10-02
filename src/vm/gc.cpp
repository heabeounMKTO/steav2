#include "vm/gc.h"
#include "vm/vm.h"

namespace steav_vm {

void Heap::_maybe_collect(size_t incoming) {
  if (vm == nullptr) return;
#ifdef STEAV_DEBUG_STRESS_GC
  (void)incoming;
  collect(*vm);
#else
  if (bytes_allocated + incoming > next_gc) collect(*vm);
#endif
}

void Heap::collect(VM &vm) {
  _mark_roots(vm);
  _trace_references();
  _sweep();
  this->next_gc = bytes_allocated * STEAV_GC_GROW_FACTOR;
  if (next_gc < STEAV_GC_INITIAL_THRESHOLD)
    this->next_gc = STEAV_GC_INITIAL_THRESHOLD;
}

static void free_object(Obj *obj) {
  switch (obj->type) {
  case OBJ_STRING:
    delete (ObjString *)obj;
    break;
  case OBJ_FUNCTION:
    delete (ObjFunction *)obj;
    break;
  case OBJ_CLASS:
    delete (ObjClass *)obj;
    break;
  case OBJ_INSTANCE:
    delete (ObjInstance *)obj;
    break;
  case OBJ_ARRAY:
    delete (ObjArray *)obj;
    break;
  case OBJ_STRUCT_TYPE:
    delete (ObjStructType *)obj;
    break;
  }
}

void Heap::free_objects() {
  Obj *obj = objects;
  while (obj != nullptr) {
    Obj *next = obj->next;
    free_object(obj);
    obj = next;
  }
  this->objects = nullptr;
  this->bytes_allocated = 0;
}

/* roots = value stack + globals + call frames + every compiled function
 * (functions hold the strings / classes / struct types in their constants) */
void Heap::_mark_roots(VM &vm) {
  for (Value *slot = vm.stack; slot < vm.stack_top; slot++) _mark_value(*slot);
  for (const Value &v : vm.globals) _mark_value(v);
  for (int i = 0; i < vm.frame_count; i++) _mark_object(vm.frames[i].function);
  for (ObjFunction *f : vm.roots) _mark_object(f);
}

void Heap::_mark_value(const Value &value) {
  if (value.type == VAL_OBJ) _mark_object(value.obj);
}

void Heap::_mark_object(Obj *obj) {
  if (obj == nullptr || obj->is_marked) return;
  obj->is_marked = true;
  gray.push_back(obj);
}

void Heap::_trace_references() {
  while (!gray.empty()) {
    Obj *obj = gray.back();
    gray.pop_back();
    _blacken(obj);
  }
}

void Heap::_blacken(Obj *obj) {
  switch (obj->type) {
  case OBJ_STRING:
    break;
  case OBJ_FUNCTION:
    for (const Value &v : ((ObjFunction *)obj)->chunk.constants) _mark_value(v);
    break;
  case OBJ_CLASS:
    _mark_object(((ObjClass *)obj)->init);
    break;
  case OBJ_INSTANCE: {
    ObjInstance *instance = (ObjInstance *)obj;
    _mark_object(instance->klass);
    for (const Value &v : instance->fields) _mark_value(v);
    break;
  }
  case OBJ_ARRAY: {
    ObjArray *array = (ObjArray *)obj;
    _mark_object(array->elem_desc);
    for (const Value &v : array->items) _mark_value(v);
    break;
  }
  case OBJ_STRUCT_TYPE:
    for (const StructFieldDesc &f : ((ObjStructType *)obj)->fields)
      _mark_object(f.desc);
    break;
  }
}

void Heap::_sweep() {
  Obj **link = &objects;
  while (*link != nullptr) {
    Obj *obj = *link;
    if (obj->is_marked) {
      obj->is_marked = false;
      link = &obj->next;
    } else {
      *link = obj->next;
      this->bytes_allocated -= obj->size;
      free_object(obj);
    }
  }
}

} // namespace steav_vm
