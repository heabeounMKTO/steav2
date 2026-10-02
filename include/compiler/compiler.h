#ifndef STEAV_COMPILER_COMPILER_H
#define STEAV_COMPILER_COMPILER_H

#include "ast/ast.h"
#include "logging/steav_logger.h"
#include "typecheck/type_checker.h"
#include "vm/gc.h"
#include "vm/object.h"
#include <vector>

namespace steav_compiler {

/* ast -> flat bytecode, clox style. assumes the input is well typed
 * (the checker already ran) so no type errors in here. the checker also
 * already gave every local its slot, and every expression its width */
struct Compiler {
  Compiler(const steav_ast::Ast &ast, const steav_typecheck::TypeInfo &info,
           steav_vm::Heap &heap)
      : ast(ast), info(info), heap(heap), function(nullptr), cur_fun(-1),
        cur_record(-1), had_error(false) {}

  // `functions` gets every ObjFunction made, the vm keeps them as gc roots
  SteavStatus compile(steav_vm::ObjFunction *&script,
                      std::vector<steav_vm::ObjFunction *> &functions);

private:
  /* where an assignable / readable thing lives. P_FIELD and P_INDEX have
   * already pushed their instance / array (+ index) when this comes back.
   * `dynamic` = v[i] on top of that: i is pushed last, n is the field count */
  enum PlaceKind { P_LOCAL, P_GLOBAL, P_FIELD, P_INDEX };
  struct Place {
    PlaceKind kind;
    int base = 0; // local / global slot
    int offset = 0;
    bool dynamic = false;
    int count = 0;
  };

  const steav_ast::Ast &ast;
  const steav_typecheck::TypeInfo &info;
  steav_vm::Heap &heap;
  steav_vm::ObjFunction *function; // the one being compiled right now
  int cur_fun;                     // decl being compiled, -1 = script
  int cur_record;                  // method owner, -1 = none
  bool had_error;

  std::vector<steav_vm::ObjFunction *> fun_objs;       // per decl
  std::vector<steav_vm::ObjClass *> class_objs;        // per record
  std::vector<steav_vm::ObjStructType *> struct_types; // per record, lazy

  void _function(int decl);
  void _bind_native(int decl, steav_vm::ObjFunction *fn);
  void _decl(int decl);
  void _stmt(int stmt);
  void _expr(int expr);
  void _expr_coerced(int expr); // + the T -> T? wrap the checker asked for
  void _read(int expr);
  void _assign(int expr);
  void _call(int expr);
  void _binary(int expr);
  void _to_string(int operand_expr, int line); // Ahsor + T auto-concat
  bool _place(int expr, Place &place); // false = a temporary, emits nothing
  void _emit_get(const Place &place, int w, int line);
  void _emit_set(const Place &place, int w, int line);
  void _emit_binary_op(steav_frontend::TokenType op, int overload, int line);
  void _pop_locals(const std::vector<int> &decls, int line);
  void _pop(int slots, int line);

  int _width(int type) const { return info.width(type); }
  bool _is_struct(int type) const {
    return type >= 0 && info.types[type].kind == steav_typecheck::TYPE_STRUCT;
  }
  steav_vm::ObjStructType *_struct_type(int record);
  int _constant(const steav_vm::Value &value, int line);
  void _number(int expr);
  void _string(int expr);

  int _emit_jump(uint8_t op, int line);
  void _patch_jump(int offset);
  void _emit_loop(int loop_start, int line);
  void _error(int line, const char *message);

  inline steav_vm::Chunk &_chunk() { return function->chunk; }
  inline void _emit_byte(uint8_t byte, int line) {
    _chunk().write(byte, line);
  }
  inline void _emit_bytes(uint8_t a, uint8_t b, int line) {
    _emit_byte(a, line);
    _emit_byte(b, line);
  }
  inline void _emit_u16(int value, int line) {
    _emit_byte((uint8_t)((value >> 8) & 0xff), line);
    _emit_byte((uint8_t)(value & 0xff), line);
  }
};

} // namespace steav_compiler
#endif // STEAV_COMPILER_COMPILER_H
