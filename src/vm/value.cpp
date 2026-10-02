#include "vm/object.h"
#include "vm/value.h"
#include <cmath>
#include <cstdlib>

namespace steav_vm {

// shortest form that reads back as the same double, so 0.1 renders as 0.1
static std::string lek_thom_chars(double n) {
  char buf[32];
  // whole numbers as plain digits, the shortest %g of 10 is "1e+01"
  if (n == floor(n) && fabs(n) < 1e15) {
    snprintf(buf, sizeof(buf), "%.0f", n == 0 ? 0.0 : n); // no "-0"
    return buf;
  }
  for (int precision = 1; precision <= 17; precision++) {
    snprintf(buf, sizeof(buf), "%.*g", precision, n);
    if (strtod(buf, nullptr) == n) break;
  }
  return buf;
}

static void print_lek_thom(double n) { printf("%s", lek_thom_chars(n).c_str()); }

void Value::print() const {
  switch (type) {
  case VAL_NIL:
    printf("sone");
    break;
  case VAL_BOOL:
    printf(boolean ? "ok" : "ort");
    break;
  case VAL_LEK_KUT:
    printf("%d", lek_kut);
    break;
  case VAL_LEK_THOM:
    print_lek_thom(lek_thom);
    break;
  case VAL_LEK_THOM_KLANG:
    printf("%lld", (long long)lek_thom_klang);
    break;
  case VAL_OBJ:
    switch (obj->type) {
    case OBJ_STRING:
      printf("%s", ((ObjString *)obj)->chars.c_str());
      break;
    case OBJ_FUNCTION:
      printf("<rupamun %s>", ((ObjFunction *)obj)->name.c_str());
      break;
    case OBJ_CLASS:
      printf("<tnak %s>", ((ObjClass *)obj)->name.c_str());
      break;
    case OBJ_INSTANCE:
      printf("<%s instance>", ((ObjInstance *)obj)->klass->name.c_str());
      break;
    case OBJ_ARRAY: {
      ObjArray *array = (ObjArray *)obj;
      printf("[");
      for (size_t i = 0; i < array->items.size(); i += array->elem_width) {
        if (i > 0) printf(", ");
        print_slots(&array->items[i], array->elem_desc, array->elem_optional);
      }
      printf("]");
      break;
    }
    case OBJ_STRUCT_TYPE:
      printf("<sampoan %s>", ((ObjStructType *)obj)->name.c_str());
      break;
    }
    break;
  }
}

void print_slots(const Value *slots, ObjStructType *desc, bool optional) {
  if (optional) {
    if (slots[0].type == VAL_NIL) {
      printf("sone");
      return;
    }
    // one slot optionals are just the value, structs have a flag in front
    if (desc != nullptr) slots++;
  }
  if (desc == nullptr) {
    slots[0].print();
    return;
  }
  printf("%s(", desc->name.c_str());
  for (size_t i = 0; i < desc->fields.size(); i++) {
    const StructFieldDesc &f = desc->fields[i];
    if (i > 0) printf(", ");
    print_slots(slots, f.desc, f.optional);
    slots += f.width;
  }
  printf(")");
}

// same shape as Value::print(), appended to `out` instead of written to
// stdout, for Ahsor + T auto-concat
std::string to_string(const Value &v) {
  switch (v.type) {
  case VAL_NIL: return "sone";
  case VAL_BOOL: return v.boolean ? "ok" : "ort";
  case VAL_LEK_KUT: return std::to_string(v.lek_kut);
  case VAL_LEK_THOM: return lek_thom_chars(v.lek_thom);
  case VAL_LEK_THOM_KLANG: return std::to_string((long long)v.lek_thom_klang);
  case VAL_OBJ:
    switch (v.obj->type) {
    case OBJ_STRING: return ((ObjString *)v.obj)->chars;
    case OBJ_FUNCTION: return "<rupamun " + ((ObjFunction *)v.obj)->name + ">";
    case OBJ_CLASS: return "<tnak " + ((ObjClass *)v.obj)->name + ">";
    case OBJ_INSTANCE:
      return "<" + ((ObjInstance *)v.obj)->klass->name + " instance>";
    case OBJ_ARRAY: {
      ObjArray *array = (ObjArray *)v.obj;
      std::string out = "[";
      for (size_t i = 0; i < array->items.size(); i += array->elem_width) {
        if (i > 0) out += ", ";
        out += to_string_slots(&array->items[i], array->elem_desc,
                                array->elem_optional);
      }
      out += "]";
      return out;
    }
    case OBJ_STRUCT_TYPE: return "<sampoan " + ((ObjStructType *)v.obj)->name + ">";
    }
  }
  return ""; // unreachable, silences -Wreturn-type
}

std::string to_string_slots(const Value *slots, ObjStructType *desc, bool optional) {
  if (optional) {
    if (slots[0].type == VAL_NIL) return "sone";
    if (desc != nullptr) slots++;
  }
  if (desc == nullptr) return to_string(slots[0]);
  std::string out = desc->name + "(";
  for (size_t i = 0; i < desc->fields.size(); i++) {
    const StructFieldDesc &f = desc->fields[i];
    if (i > 0) out += ", ";
    out += to_string_slots(slots, f.desc, f.optional);
    slots += f.width;
  }
  out += ")";
  return out;
}

} // namespace steav_vm
