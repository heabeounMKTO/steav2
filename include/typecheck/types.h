#ifndef STEAV_TYPECHECK_TYPES_H
#define STEAV_TYPECHECK_TYPES_H

namespace steav_typecheck {

/* `Lek` is just another name for LekThom, there's no TYPE_LEK */
enum TypeKind {
  TYPE_LEK_KUT,        // LekKut, Int
  TYPE_LEK_THOM,       // LekThom (and Lek), Double
  TYPE_LEK_THOM_KLANG, // LekThomKlang, Long
  TYPE_AHSOR,          // Ahsor, String
  TYPE_BOOLEAN,        // Boolean
  TYPE_ORT_MEAN,       // OrtMean, Nil
  TYPE_STRUCT,         // sampoan, value type
  TYPE_CLASS,          // tnak, reference type
  TYPE_ARRAY,          // [T], built in reference type
  TYPE_NONE            // a bare `sone` with nothing around it to say T?
};

/* types are interned in TypeInfo::types, so two types are the same type
 * iff they have the same index */
struct Type {
  TypeKind kind;
  int decl = -1;         // TYPE_STRUCT / TYPE_CLASS: index into Ast::decls
  int elem = -1;         // TYPE_ARRAY: index into TypeInfo::types
  bool optional = false; // T?
};

static inline Type make_type(TypeKind kind, bool optional = false) {
  Type t;
  t.kind = kind;
  t.optional = optional;
  return t;
}
static inline Type make_struct_type(int decl, bool optional = false) {
  Type t;
  t.kind = TYPE_STRUCT;
  t.decl = decl;
  t.optional = optional;
  return t;
}
static inline Type make_class_type(int decl, bool optional = false) {
  Type t;
  t.kind = TYPE_CLASS;
  t.decl = decl;
  t.optional = optional;
  return t;
}
static inline Type make_array_type(int elem, bool optional = false) {
  Type t;
  t.kind = TYPE_ARRAY;
  t.elem = elem;
  t.optional = optional;
  return t;
}

static inline bool types_equal(const Type &a, const Type &b) {
  return a.kind == b.kind && a.decl == b.decl && a.elem == b.elem &&
         a.optional == b.optional;
}

static inline bool is_number_kind(TypeKind kind) {
  return kind == TYPE_LEK_KUT || kind == TYPE_LEK_THOM ||
         kind == TYPE_LEK_THOM_KLANG;
}

// tnak + arrays live on the heap behind the gc, sampoan never does
static inline bool is_reference_type(const Type &t) {
  return t.kind == TYPE_CLASS || t.kind == TYPE_ARRAY;
}

} // namespace steav_typecheck
#endif // STEAV_TYPECHECK_TYPES_H
