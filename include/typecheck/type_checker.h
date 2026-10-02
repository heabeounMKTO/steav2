#ifndef STEAV_TYPECHECK_TYPE_CHECKER_H
#define STEAV_TYPECHECK_TYPE_CHECKER_H

#include "ast/ast.h"
#include "logging/steav_logger.h"
#include "typecheck/types.h"
#include <string>
#include <vector>

namespace steav_typecheck {

/* sampoan, tnak and rupamun share ONE namespace per module, so a call
 * like Sphere(...) resolves to exactly one of these at compile time.
 * top level akthe bindings and a native module's functions / constants
 * live in there too */
enum SymbolKind {
  SYM_STRUCT,
  SYM_CLASS,
  SYM_FUN,
  SYM_GLOBAL,
  SYM_NATIVE_FUN,
  SYM_NATIVE_CONST
};

struct Symbol {
  SymbolKind kind;
  std::string name;
  int decl = -1;   // index into Ast::decls
  int type = -1;   // SYM_GLOBAL / SYM_NATIVE_CONST
  int index = -1;  // SYM_GLOBAL: global slot, SYM_NATIVE_*: TypeInfo::natives
  int record = -1; // SYM_STRUCT / SYM_CLASS: TypeInfo::records
};

// what one module can see: its own names, bare imports, `jea` aliases
struct ModuleScope {
  std::vector<Symbol> symbols;
  std::vector<int> bare_imports; // module ids, `yok vector;`
  std::vector<steav_frontend::Token> bare_import_at; // the `yok`, for errors
  std::vector<std::string> alias_names;
  std::vector<int> alias_modules; // `yok vector jea vec;`
};

/* exact signature match only, NO automatic commutativity:
 * Vec3 * LekThom and LekThom * Vec3 are two separate overloads */
struct OperatorOverload {
  steav_frontend::TokenType op;
  int arity;    // 1 = unary -, 2 = binary
  int lhs = -1; // index into TypeInfo::types
  int rhs = -1; // binary only
  int decl;     // the rupamun, index into Ast::decls
};

/* a sampoan / tnak layout. sampoan fields are flattened into consecutive
 * slots (nested structs inline), tnak fields into the instance's slots */
struct FieldInfo {
  steav_frontend::Token name;
  int type;
  int offset; // in slots
  int width;
};

struct Record {
  int decl;
  bool is_class;
  int type = -1; // the non optional struct / class type
  std::vector<FieldInfo> fields;
  int slots = 0;  // sampoan: its width, tnak: slots per instance
  int init = -1;  // tnak: the init decl
  int state = 0;  // layout: 0 todo, 1 in progress (cycle check), 2 done
};

struct FunSig {
  std::vector<int> params; // types
  int ret = -1;
  int record = -1;     // method: the owner
  int param_slots = 0; // receiver + params
};

// a stdlib native function or constant, see stdlib/module.h
struct NativeRef {
  int module; // stdlib module index
  int index;  // function / constant index in that module
};

// how a variable / get resolved
enum RefKind {
  REF_NONE,
  REF_LOCAL,        // index = slot
  REF_GLOBAL,       // index = global slot
  REF_SELF_FIELD,   // bare field name inside a method, index = offset
  REF_STRUCT_FIELD, // sampoan.field, index = offset
  REF_CLASS_FIELD,  // tnak.field, index = offset
  REF_NATIVE_CONST, // index = TypeInfo::natives
  REF_STRUCT_INDEX, // v[i] on a same-typed sampoan, index = field count
  REF_MODULE        // a `jea` alias, only valid left of a '.'
};

// what the call site compiles to (see the table in README)
enum CallKind {
  CALL_NONE,             // not a call expr
  CALL_CONSTRUCT_STRUCT, // fields pushed in order ARE the struct
  CALL_CONSTRUCT_CLASS,  // OP_CONSTRUCT_INSTANCE, then init
  CALL_FUNCTION,         // OP_CALL
  CALL_METHOD,           // OP_CALL with the receiver in front
  CALL_METHOD_IMPLICIT,  // bare method name inside a method, receiver = nis
  CALL_NATIVE,           // OP_CALL_NATIVE
  CALL_CAST,             // OP_CONVERT, target = TypeKind
  CALL_ARRAY_LEN,        // array.len()
  CALL_ARRAY_PUSH        // array.push(x)
};

// everything the compiler needs, filled in by the checker
struct TypeInfo {
  std::vector<Type> types;
  std::vector<ModuleScope> scopes; // one per Ast::modules
  std::vector<OperatorOverload> overloads;
  std::vector<Record> records;
  std::vector<NativeRef> natives;
  int global_slots = 0;

  // one entry per Ast::decls
  std::vector<int> decl_record; // DECL_STRUCT / DECL_CLASS
  std::vector<FunSig> fun_sigs; // DECL_FUN
  std::vector<int> decl_type;   // DECL_VAR
  std::vector<int> decl_slot;   // DECL_VAR: local slot or global slot
  std::vector<char> decl_global;

  // one entry per Ast::exprs
  std::vector<int> expr_types;    // index into types
  std::vector<int> expr_coerce;   // T -> T? wrap into this type, -1 = none
  std::vector<int> expr_overload; // operator rupamun decl, -1 = builtin op
  std::vector<RefKind> ref_kinds;
  std::vector<int> ref_index;
  std::vector<char> narrowed; // read inside `ber (x != sone)`, skip the flag
  std::vector<int> none_side; // `x == sone`: 1 = sone on the right, 2 = left
  std::vector<int> str_side;  // Ahsor + T auto-concat: 1 = left needs
                               // stringifying, 2 = right needs it, 0 = neither
  std::vector<CallKind> call_kinds;
  std::vector<int> call_targets; // decl / record / native / TypeKind

  int intern(const Type &t);
  int width(int type) const; // in slots
  int base(int type);        // T? -> T
  int optional_of(int type); // T -> T?
  std::string type_name(int type, const steav_ast::Ast &ast) const;
};

struct TypeChecker {
  TypeChecker(const steav_ast::Ast &ast) : ast(ast) {}

  SteavStatus check(TypeInfo &info);

private:
  struct LocalVar {
    steav_frontend::Token name;
    int type;
    int slot;
    int depth;
  };
  /* `ber (x != sone)` / `nvpeldae (x != sone)`: x reads as T until it's
   * assigned (killed), then as T? again for the rest of the branch */
  struct Narrow {
    bool global;
    int index;
    int loop_depth; // loops open when the narrowing started
    bool killed;
  };

  const steav_ast::Ast &ast;
  TypeInfo *ti = nullptr;
  bool had_error = false;

  // what we're inside of right now
  int cur_module = 0;
  int cur_fun = -1;    // -1 = top level script
  int cur_record = -1; // method owner
  int scope_depth = 0;
  int loop_depth = 0;
  int slot_top = 0;
  std::vector<LocalVar> locals;
  std::vector<Narrow> narrows;

  int t_lek_kut, t_lek_thom, t_lek_thom_klang, t_ahsor, t_boolean,
      t_ort_mean, t_none;

  // passes
  void _declare_module(int module);
  void _import(int module, const steav_ast::Stmt &stmt);
  void _check_import_collisions(int module);
  void _layout_record(int record);
  void _declare_signatures(int decl);
  void _check_top_level(int module);
  void _check_fun_body(int decl);

  void _check_decl(int decl);
  void _check_stmt(int stmt);
  bool _returns(const std::vector<int> &decls) const;

  // expressions, `expected` is a hint for literals (-1 = none)
  int _check_expr(int expr, int expected);
  int _expect(int expr, int want); // check + coerce, -1 on error
  bool _coerce(int expr, int actual, int want);
  int _check_binary(int expr, int expected);
  int _check_unary(int expr, int expected);
  int _check_literal(int expr, int expected);
  int _check_variable(int expr);
  int _check_get(int expr);
  int _check_assign(int expr);
  int _check_compound(int expr, int target_type);
  int _struct_elem(int record) const;
  int _check_call(int expr);
  int _call_symbol(int expr, const Symbol *symbol);
  int _native_type(int kind) const;
  int _check_array(int expr, int expected);
  bool _check_args(int expr, const std::vector<int> &params);
  bool _assignable(int expr);
  bool _is_untyped_literal(int expr) const;
  bool _is_nil_literal(int expr) const;
  int _strip_grouping(int expr) const;
  int _overload_hint(steav_frontend::TokenType op, int known, bool known_is_lhs);

  // names
  int _resolve_type_ref(int module, int type_ref);
  const Symbol *_lookup(int module, const steav_frontend::Token &name) const;
  const Symbol *_lookup_own(int module, const char *name, int length) const;
  int _lookup_alias(int module, const steav_frontend::Token &name) const;
  int _find_local(const steav_frontend::Token &name) const;
  int _find_field(int record, const steav_frontend::Token &name) const;
  int _find_method(int record, const steav_frontend::Token &name) const;
  int _builtin_type(const steav_frontend::Token &name) const;
  int _find_overload(steav_frontend::TokenType op, int arity, int lhs,
                     int rhs) const;
  bool _declare_symbol(int module, const Symbol &symbol,
                       const steav_frontend::Token &at);
  void _declare_local(int decl, int type);
  bool _is_narrowed(RefKind kind, int index) const;
  Narrow *_find_narrow(RefKind kind, int index);
  bool _narrowing(int cond, Narrow &narrow, bool &when_true) const;

  void _begin_scope();
  void _end_scope();

  void _error(const steav_frontend::Token &at, const char *fmt, ...);
  std::string _name(int type) const { return ti->type_name(type, ast); }
};

} // namespace steav_typecheck
#endif // STEAV_TYPECHECK_TYPE_CHECKER_H
