#ifndef STEAV_VM_VALUE_H
#define STEAV_VM_VALUE_H

#include <cstdint>
#include <cstdio>

namespace steav_vm {

struct Obj; // vm/object.h

/* the type checker already proved everything, so the vm can skip most
 * tag checks, the tag is still here for printing / gc / debugging */
enum ValueType {
  VAL_NIL,            // sone
  VAL_BOOL,           // Boolean
  VAL_LEK_KUT,        // LekKut, Int
  VAL_LEK_THOM,       // LekThom, Double
  VAL_LEK_THOM_KLANG, // LekThomKlang, Long
  VAL_OBJ             // tnak instances, arrays, Ahsor, functions (gc'd)
};

/* one slot. a sampoan is several of these side by side on the stack,
 * never one Value */
struct Value {
  ValueType type;
  bool boolean = false;       // VAL_BOOL
  int32_t lek_kut = 0;        // VAL_LEK_KUT
  double lek_thom = 0.0;      // VAL_LEK_THOM
  int64_t lek_thom_klang = 0; // VAL_LEK_THOM_KLANG
  Obj *obj = nullptr;         // VAL_OBJ

  void print() const; // impl in value.cpp, needs object.h
};
static inline Value make_nil() {
  Value v;
  v.type = VAL_NIL;
  return v;
}
static inline Value make_bool(bool b) {
  Value v;
  v.type = VAL_BOOL;
  v.boolean = b;
  return v;
}
static inline Value make_lek_kut(int32_t n) {
  Value v;
  v.type = VAL_LEK_KUT;
  v.lek_kut = n;
  return v;
}
static inline Value make_lek_thom(double n) {
  Value v;
  v.type = VAL_LEK_THOM;
  v.lek_thom = n;
  return v;
}
static inline Value make_lek_thom_klang(int64_t n) {
  Value v;
  v.type = VAL_LEK_THOM_KLANG;
  v.lek_thom_klang = n;
  return v;
}
static inline Value make_obj(Obj *obj) {
  Value v;
  v.type = VAL_OBJ;
  v.obj = obj;
  return v;
}

} // namespace steav_vm
#endif // STEAV_VM_VALUE_H
