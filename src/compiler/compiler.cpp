#include "compiler/compiler.h"
#include "logging/steav_diagnostics.h"
#include "stdlib/module.h"
#include <cstdlib>
#include <cstring>

#ifdef STEAV_DEBUG_PRINT
#include "debug/disassembler.h"
#endif

namespace steav_compiler {

using namespace steav_vm;
using steav_ast::Decl;
using steav_ast::Expr;
using steav_ast::Stmt;
using steav_typecheck::Record;
using steav_typecheck::Type;

SteavStatus Compiler::compile(ObjFunction *&script,
                              std::vector<ObjFunction *> &functions) {
  fun_objs.assign(ast.decls.size(), nullptr);
  class_objs.assign(info.records.size(), nullptr);
  struct_types.assign(info.records.size(), nullptr);

  /* every rupamun / method / tnak exists up front, so calls can point at
   * functions that haven't been compiled yet (recursion, later decls) */
  for (int m : ast.module_order) {
    for (int d : ast.modules[m].program) {
      const Decl &decl = ast.decls[d];
      std::vector<int> funs;
      if (decl.kind == steav_ast::DECL_FUN) funs.push_back(d);
      if (decl.kind == steav_ast::DECL_STRUCT || decl.kind == steav_ast::DECL_CLASS)
        funs = decl.methods;
      for (int f : funs) {
        ObjFunction *fn = heap.allocate<ObjFunction>(OBJ_FUNCTION);
        fn->param_slots = info.fun_sigs[f].param_slots;
        fn->ret_slots = _width(info.fun_sigs[f].ret);
        if (ast.decls[f].is_native) _bind_native(f, fn);
        fn->name = std::string(ast.decls[f].name.start, ast.decls[f].name.length);
        if (decl.kind != steav_ast::DECL_FUN)
          fn->name = std::string(decl.name.start, decl.name.length) + "." + fn->name;
        fun_objs[f] = fn;
        functions.push_back(fn);
      }
    }
  }
  for (int r = 0; r < (int)info.records.size(); r++) {
    const Record &rec = info.records[r];
    if (!rec.is_class) continue;
    ObjClass *klass = heap.allocate<ObjClass>(OBJ_CLASS);
    const Decl &decl = ast.decls[rec.decl];
    klass->name = std::string(decl.name.start, decl.name.length);
    klass->field_slots = rec.slots;
    klass->init = rec.init >= 0 ? fun_objs[rec.init] : nullptr;
    class_objs[r] = klass;
  }

  for (int f = 0; f < (int)fun_objs.size(); f++) {
    if (fun_objs[f] != nullptr && !ast.decls[f].is_native) _function(f);
  }

  // the script: every module's top level code, dependencies first
  this->function = heap.allocate<ObjFunction>(OBJ_FUNCTION);
  function->name = "script";
  functions.push_back(function);
  this->cur_record = -1;
  for (int m : ast.module_order) {
    for (int d : ast.modules[m].program) {
      steav_ast::DeclKind k = ast.decls[d].kind;
      if (k == steav_ast::DECL_VAR || k == steav_ast::DECL_STMT) _decl(d);
    }
  }
  _emit_bytes(OP_RETURN, 0, 0);

#ifdef STEAV_DEBUG_PRINT
  if (!had_error) {
    for (ObjFunction *fn : functions)
      steav_debug::disassemble_chunk(fn->chunk, fn->name.c_str());
  }
#endif
  script = function;
  return had_error ? STEAV_LOGGING_COMPILE_ERROR : STEAV_LOGGING_OK;
}

void Compiler::_function(int d) {
  const Decl &decl = ast.decls[d];
  this->function = fun_objs[d];
  this->cur_fun = d;
  this->cur_record = info.fun_sigs[d].record;
  for (int b : decl.body) _decl(b);

  int line = decl.name.line;
  if (decl.fun_name_kind == steav_ast::FUN_INIT) {
    // init hands back nis, OP_CONSTRUCT_INSTANCE leaves it on the stack
    _emit_bytes(OP_GET_LOCAL, 0, line);
    _emit_byte(1, line);
    _emit_bytes(OP_RETURN, 1, line);
  } else {
    // OrtMean rupamuns fall off the end, the checker made sure the rest can't
    _emit_byte(OP_NIL, line);
    _emit_bytes(OP_RETURN, 1, line);
  }
  this->cur_fun = -1;
  this->cur_record = -1;
}

/* ------------------------------------------------------ decls + statements */

void Compiler::_decl(int d) {
  const Decl &decl = ast.decls[d];
  int line = decl.name.line;
  switch (decl.kind) {
  case steav_ast::DECL_VAR: {
    int w = _width(info.decl_type[d]);
    if (decl.initializer >= 0) {
      _expr_coerced(decl.initializer);
    } else if (w == 1) {
      _emit_byte(OP_NIL, line);
    } else {
      _emit_bytes(OP_NIL_N, (uint8_t)w, line);
    }
    // a local just stays where it is, that's its slot
    if (info.decl_global[d]) {
      _emit_byte(OP_SET_GLOBAL, line);
      _emit_u16(info.decl_slot[d], line);
      _emit_byte((uint8_t)w, line);
      _pop(w, line);
    }
    break;
  }
  case steav_ast::DECL_STMT:
    _stmt(decl.stmt);
    break;
  default:
    break; // rupamun / sampoan / tnak were compiled up front
  }
}

void Compiler::_pop(int slots, int line) {
  while (slots > 0) {
    int n = slots > 255 ? 255 : slots;
    _emit_bytes(OP_POPN, (uint8_t)n, line);
    slots -= n;
  }
}

// end of a scope, drop the locals declared directly in it
void Compiler::_pop_locals(const std::vector<int> &decls, int line) {
  int slots = 0;
  for (int d : decls) {
    if (ast.decls[d].kind == steav_ast::DECL_VAR) slots += _width(info.decl_type[d]);
  }
  _pop(slots, line);
}

void Compiler::_stmt(int si) {
  const Stmt &s = ast.stmts[si];
  int line = s.keyword.line;
  switch (s.kind) {
  case steav_ast::STMT_EXPR:
    _expr_coerced(s.expr);
    _pop(_width(info.expr_types[s.expr]), line);
    break;
  case steav_ast::STMT_PRINT: {
    int t = info.expr_types[s.expr];
    _expr(s.expr);
    if (_is_struct(t)) {
      const Type &ty = info.types[t];
      _emit_byte(OP_PRINT_STRUCT, line);
      _emit_u16(_constant(make_obj(_struct_type(info.decl_record[ty.decl])), line),
                line);
      _emit_byte(ty.optional ? 1 : 0, line);
    } else {
      _emit_byte(OP_PRINT, line);
    }
    break;
  }
  case steav_ast::STMT_BLOCK:
    for (int d : s.body) _decl(d);
    _pop_locals(s.body, line);
    break;
  case steav_ast::STMT_IF: {
    _expr(s.expr);
    int then_jump = _emit_jump(OP_JUMP_IF_FALSE, line);
    _pop(1, line);
    _decl(s.then_branch);
    int else_jump = _emit_jump(OP_JUMP, line);
    _patch_jump(then_jump);
    _pop(1, line);
    if (s.else_branch >= 0) _decl(s.else_branch);
    _patch_jump(else_jump);
    break;
  }
  case steav_ast::STMT_WHILE: {
    int loop_start = (int)_chunk().code.size();
    _expr(s.expr);
    int exit_jump = _emit_jump(OP_JUMP_IF_FALSE, line);
    _pop(1, line);
    _decl(s.then_branch);
    _emit_loop(loop_start, line);
    _patch_jump(exit_jump);
    _pop(1, line);
    break;
  }
  case steav_ast::STMT_FOR: {
    if (s.initializer >= 0) _decl(s.initializer);
    int loop_start = (int)_chunk().code.size();
    int exit_jump = -1;
    if (s.expr >= 0) {
      _expr(s.expr);
      exit_jump = _emit_jump(OP_JUMP_IF_FALSE, line);
      _pop(1, line);
    }
    _decl(s.then_branch);
    if (s.increment >= 0) {
      _expr_coerced(s.increment);
      _pop(_width(info.expr_types[s.increment]), line);
    }
    _emit_loop(loop_start, line);
    if (exit_jump >= 0) {
      _patch_jump(exit_jump);
      _pop(1, line);
    }
    if (s.initializer >= 0) _pop_locals(std::vector<int>(1, s.initializer), line);
    break;
  }
  case steav_ast::STMT_RETURN: {
    if (ast.decls[cur_fun].fun_name_kind == steav_ast::FUN_INIT) {
      _emit_bytes(OP_GET_LOCAL, 0, line);
      _emit_byte(1, line);
      _emit_bytes(OP_RETURN, 1, line);
    } else if (s.expr >= 0) {
      _expr_coerced(s.expr);
      _emit_bytes(OP_RETURN, (uint8_t)_width(info.fun_sigs[cur_fun].ret), line);
    } else {
      _emit_byte(OP_NIL, line);
      _emit_bytes(OP_RETURN, 1, line);
    }
    break;
  }
  case steav_ast::STMT_IMPORT:
    break; // resolved statically, nothing runs
  }
}

/* ------------------------------------------------------------- expressions */

void Compiler::_expr_coerced(int expr) {
  _expr(expr);
  int to = info.expr_coerce[expr];
  if (to >= 0 && _is_struct(to)) {
    int line = ast.exprs[expr].token.line;
    _emit_bytes(OP_WRAP_SOME, (uint8_t)(_width(to) - 1), line);
  }
}

void Compiler::_expr(int expr) {
  const Expr &x = ast.exprs[expr];
  int line = x.token.line;
  switch (x.kind) {
  case steav_ast::EXPR_LITERAL:
    switch (x.token.type) {
    case steav_frontend::TOKEN_TRUE:
      _emit_byte(OP_TRUE, line);
      break;
    case steav_frontend::TOKEN_FALSE:
      _emit_byte(OP_FALSE, line);
      break;
    case steav_frontend::TOKEN_NIL: {
      int w = _width(info.expr_types[expr]);
      if (w == 1)
        _emit_byte(OP_NIL, line);
      else
        _emit_bytes(OP_NIL_N, (uint8_t)w, line);
      break;
    }
    case steav_frontend::TOKEN_NUMBER:
      _number(expr);
      break;
    case steav_frontend::TOKEN_STRING:
      _string(expr);
      break;
    default:
      break;
    }
    break;
  case steav_ast::EXPR_GROUPING:
    _expr(x.right);
    break;
  case steav_ast::EXPR_VARIABLE:
  case steav_ast::EXPR_THIS:
  case steav_ast::EXPR_GET:
  case steav_ast::EXPR_INDEX:
    _read(expr);
    break;
  case steav_ast::EXPR_ASSIGN:
    _assign(expr);
    break;
  case steav_ast::EXPR_UNARY: {
    int ov = info.expr_overload[expr];
    _expr(x.right);
    if (ov >= 0) {
      _emit_byte(OP_CALL, line);
      _emit_u16(_constant(make_obj(fun_objs[ov]), line), line);
      _emit_byte((uint8_t)info.fun_sigs[ov].param_slots, line);
    } else {
      _emit_byte(x.token.type == steav_frontend::TOKEN_BANG ? OP_NOT : OP_NEGATE, line);
    }
    break;
  }
  case steav_ast::EXPR_BINARY:
    _binary(expr);
    break;
  case steav_ast::EXPR_LOGICAL:
    _expr(x.left);
    if (x.token.type == steav_frontend::TOKEN_AND) {
      int end_jump = _emit_jump(OP_JUMP_IF_FALSE, line);
      _pop(1, line);
      _expr(x.right);
      _patch_jump(end_jump);
    } else {
      int else_jump = _emit_jump(OP_JUMP_IF_FALSE, line);
      int end_jump = _emit_jump(OP_JUMP, line);
      _patch_jump(else_jump);
      _pop(1, line);
      _expr(x.right);
      _patch_jump(end_jump);
    }
    break;
  case steav_ast::EXPR_CALL:
    _call(expr);
    break;
  case steav_ast::EXPR_ARRAY: {
    const Type &at = info.types[info.expr_types[expr]];
    int elem = at.elem;
    for (int item : x.args) _expr_coerced(item);
    _emit_byte(OP_ARRAY, line);
    _emit_u16((int)x.args.size(), line);
    _emit_byte((uint8_t)_width(elem), line);
    if (_is_struct(elem)) {
      _emit_u16(_constant(make_obj(_struct_type(
                              info.decl_record[info.types[elem].decl])),
                          line),
                line);
    } else {
      _emit_u16(STEAV_NO_STRUCT_TYPE, line);
    }
    _emit_byte(info.types[elem].optional ? 1 : 0, line);
    break;
  }
  }
}

void Compiler::_binary(int expr) {
  const Expr &x = ast.exprs[expr];
  int line = x.token.line;
  steav_frontend::TokenType op = x.token.type;

  if (info.none_side[expr] != 0) {
    int other = info.none_side[expr] == 1 ? x.left : x.right;
    _expr(other);
    _emit_bytes(OP_IS_NONE, (uint8_t)_width(info.expr_types[other]), line);
    if (op == steav_frontend::TOKEN_BANG_EQUAL) _emit_byte(OP_NOT, line);
    return;
  }

  _expr(x.left);
  if (info.str_side[expr] == 1) _to_string(x.left, line);
  _expr(x.right);
  if (info.str_side[expr] == 2) _to_string(x.right, line);
  _emit_binary_op(op, info.expr_overload[expr], line);
}

// convert the value an operand of Ahsor + T / T + Ahsor just pushed into an
// Ahsor, in place, the same formatting jongyeytha itself uses
void Compiler::_to_string(int operand_expr, int line) {
  int t = info.expr_types[operand_expr];
  if (_is_struct(t)) {
    const Type &ty = info.types[t];
    _emit_byte(OP_TO_STRING_STRUCT, line);
    _emit_u16(_constant(make_obj(_struct_type(info.decl_record[ty.decl])), line),
              line);
    _emit_byte(ty.optional ? 1 : 0, line);
  } else {
    _emit_byte(OP_TO_STRING, line);
  }
}

void Compiler::_emit_binary_op(steav_frontend::TokenType op, int ov, int line) {
  if (ov >= 0) {
    _emit_byte(OP_CALL, line);
    _emit_u16(_constant(make_obj(fun_objs[ov]), line), line);
    _emit_byte((uint8_t)info.fun_sigs[ov].param_slots, line);
    return;
  }
  switch (op) {
  case steav_frontend::TOKEN_PLUS: _emit_byte(OP_ADD, line); break;
  case steav_frontend::TOKEN_MINUS: _emit_byte(OP_SUBTRACT, line); break;
  case steav_frontend::TOKEN_STAR: _emit_byte(OP_MULTIPLY, line); break;
  case steav_frontend::TOKEN_SLASH: _emit_byte(OP_DIVIDE, line); break;
  case steav_frontend::TOKEN_EQUAL_EQUAL: _emit_byte(OP_EQUAL, line); break;
  case steav_frontend::TOKEN_BANG_EQUAL: _emit_bytes(OP_EQUAL, OP_NOT, line); break;
  case steav_frontend::TOKEN_LESS: _emit_byte(OP_LESS, line); break;
  case steav_frontend::TOKEN_LESS_EQUAL: _emit_byte(OP_LESS_EQUAL, line); break;
  case steav_frontend::TOKEN_GREATER: _emit_byte(OP_GREATER, line); break;
  case steav_frontend::TOKEN_GREATER_EQUAL: _emit_byte(OP_GREATER_EQUAL, line); break;
  default: break;
  }
}

static ValueType value_type_of(int kind) {
  switch (kind) {
  case steav_typecheck::TYPE_LEK_KUT: return VAL_LEK_KUT;
  case steav_typecheck::TYPE_LEK_THOM_KLANG: return VAL_LEK_THOM_KLANG;
  default: return VAL_LEK_THOM;
  }
}

void Compiler::_call(int expr) {
  const Expr &x = ast.exprs[expr];
  int line = x.token.line;
  int target = info.call_targets[expr];

  switch (info.call_kinds[expr]) {
  case steav_typecheck::CALL_CONSTRUCT_STRUCT:
    // the fields pushed in declaration order ARE the struct, nothing else
    for (int arg : x.args) _expr_coerced(arg);
    break;
  case steav_typecheck::CALL_CONSTRUCT_CLASS: {
    int slots = 0;
    for (int arg : x.args) {
      _expr_coerced(arg);
      int t = info.expr_coerce[arg] >= 0 ? info.expr_coerce[arg] : info.expr_types[arg];
      slots += _width(t);
    }
    _emit_byte(OP_CONSTRUCT_INSTANCE, line);
    _emit_u16(_constant(make_obj(class_objs[target]), line), line);
    _emit_byte((uint8_t)slots, line);
    break;
  }
  case steav_typecheck::CALL_FUNCTION:
  case steav_typecheck::CALL_METHOD:
  case steav_typecheck::CALL_METHOD_IMPLICIT: {
    steav_typecheck::CallKind kind = info.call_kinds[expr];
    if (kind == steav_typecheck::CALL_METHOD) {
      _expr(ast.exprs[x.left].left); // the receiver goes in front
    } else if (kind == steav_typecheck::CALL_METHOD_IMPLICIT) {
      _emit_bytes(OP_GET_LOCAL, 0, line);
      _emit_byte((uint8_t)_width(info.records[cur_record].type), line);
    }
    for (int arg : x.args) _expr_coerced(arg);
    _emit_byte(OP_CALL, line);
    _emit_u16(_constant(make_obj(fun_objs[target]), line), line);
    _emit_byte((uint8_t)info.fun_sigs[target].param_slots, line);
    break;
  }
  case steav_typecheck::CALL_NATIVE: {
    for (int arg : x.args) _expr_coerced(arg);
    const steav_typecheck::NativeRef &ref = info.natives[target];
    _emit_byte(OP_CALL_NATIVE, line);
    _emit_bytes((uint8_t)ref.module, (uint8_t)ref.index, line);
    break;
  }
  case steav_typecheck::CALL_CAST:
    _expr(x.args[0]);
    _emit_bytes(OP_CONVERT, (uint8_t)value_type_of(target), line);
    break;
  case steav_typecheck::CALL_ARRAY_LEN:
    _expr(ast.exprs[x.left].left);
    _emit_byte(OP_ARRAY_LEN, line);
    break;
  case steav_typecheck::CALL_ARRAY_PUSH: {
    _expr(ast.exprs[x.left].left);
    _expr_coerced(x.args[0]);
    int t = info.expr_coerce[x.args[0]] >= 0 ? info.expr_coerce[x.args[0]]
                                             : info.expr_types[x.args[0]];
    _emit_bytes(OP_ARRAY_PUSH, (uint8_t)_width(t), line);
    break;
  }
  case steav_typecheck::CALL_NONE:
    break;
  }
}

/* resolves an lvalue-ish expression to a slot range, pushing the instance
 * or array (+ index) it lives in first when it's on the heap.
 * `s.center.x` -> one P_FIELD with center's offset + x's offset */
bool Compiler::_place(int expr, Place &place) {
  const Expr &x = ast.exprs[expr];
  int line = x.token.line;
  steav_typecheck::RefKind ref = info.ref_kinds[expr];

  switch (x.kind) {
  case steav_ast::EXPR_GROUPING:
    return _place(x.right, place);
  case steav_ast::EXPR_THIS:
    place.kind = P_LOCAL;
    place.base = 0;
    place.offset = 0;
    return true;
  case steav_ast::EXPR_VARIABLE:
  case steav_ast::EXPR_GET:
    switch (ref) {
    case steav_typecheck::REF_LOCAL:
    case steav_typecheck::REF_GLOBAL:
      place.kind = ref == steav_typecheck::REF_LOCAL ? P_LOCAL : P_GLOBAL;
      place.base = info.ref_index[expr];
      // a narrowed S? reads past its flag slot
      place.offset = (info.narrowed[expr] && _is_struct(info.expr_types[expr])) ? 1 : 0;
      return true;
    case steav_typecheck::REF_SELF_FIELD:
      if (info.records[cur_record].is_class) {
        _emit_bytes(OP_GET_LOCAL, 0, line);
        _emit_byte(1, line);
        place.kind = P_FIELD;
      } else {
        place.kind = P_LOCAL;
        place.base = 0;
      }
      place.offset = info.ref_index[expr];
      return true;
    case steav_typecheck::REF_CLASS_FIELD:
      _expr(x.left);
      place.kind = P_FIELD;
      place.offset = info.ref_index[expr];
      return true;
    case steav_typecheck::REF_STRUCT_FIELD:
      if (!_place(x.left, place)) return false;
      place.offset += info.ref_index[expr];
      return true;
    default:
      return false;
    }
  case steav_ast::EXPR_INDEX:
    if (ref == steav_typecheck::REF_STRUCT_INDEX) {
      // v[i]: wherever v is, plus a runtime field index
      if (!_place(x.left, place)) return false;
      _expr(x.right);
      place.dynamic = true;
      place.count = info.ref_index[expr];
      return true;
    }
    _expr(x.left);
    _expr(x.right);
    place.kind = P_INDEX;
    place.offset = 0;
    return true;
  default:
    return false;
  }
}

void Compiler::_emit_get(const Place &p, int w, int line) {
  if (p.dynamic) {
    switch (p.kind) {
    case P_LOCAL:
      _emit_bytes(OP_GET_LOCAL_AT, (uint8_t)(p.base + p.offset), line);
      break;
    case P_GLOBAL:
      _emit_byte(OP_GET_GLOBAL_AT, line);
      _emit_u16(p.base + p.offset, line);
      break;
    case P_FIELD:
      _emit_bytes(OP_GET_FIELD_AT, (uint8_t)p.offset, line);
      break;
    case P_INDEX:
      _emit_bytes(OP_GET_INDEX_AT, (uint8_t)p.offset, line);
      break;
    }
    _emit_byte((uint8_t)p.count, line);
    return;
  }
  switch (p.kind) {
  case P_LOCAL:
    _emit_bytes(OP_GET_LOCAL, (uint8_t)(p.base + p.offset), line);
    break;
  case P_GLOBAL:
    _emit_byte(OP_GET_GLOBAL, line);
    _emit_u16(p.base + p.offset, line);
    break;
  case P_FIELD:
    _emit_bytes(OP_GET_FIELD, (uint8_t)p.offset, line);
    break;
  case P_INDEX:
    _emit_bytes(OP_GET_INDEX, (uint8_t)p.offset, line);
    break;
  }
  _emit_byte((uint8_t)w, line);
}

void Compiler::_emit_set(const Place &p, int w, int line) {
  if (p.dynamic) {
    switch (p.kind) {
    case P_LOCAL:
      _emit_bytes(OP_SET_LOCAL_AT, (uint8_t)(p.base + p.offset), line);
      break;
    case P_GLOBAL:
      _emit_byte(OP_SET_GLOBAL_AT, line);
      _emit_u16(p.base + p.offset, line);
      break;
    case P_FIELD:
      _emit_bytes(OP_SET_FIELD_AT, (uint8_t)p.offset, line);
      break;
    case P_INDEX:
      _emit_bytes(OP_SET_INDEX_AT, (uint8_t)p.offset, line);
      break;
    }
    _emit_byte((uint8_t)p.count, line);
    return;
  }
  switch (p.kind) {
  case P_LOCAL:
    _emit_bytes(OP_SET_LOCAL, (uint8_t)(p.base + p.offset), line);
    break;
  case P_GLOBAL:
    _emit_byte(OP_SET_GLOBAL, line);
    _emit_u16(p.base + p.offset, line);
    break;
  case P_FIELD:
    _emit_bytes(OP_SET_FIELD, (uint8_t)p.offset, line);
    break;
  case P_INDEX:
    _emit_bytes(OP_SET_INDEX, (uint8_t)p.offset, line);
    break;
  }
  _emit_byte((uint8_t)w, line);
}

void Compiler::_read(int expr) {
  const Expr &x = ast.exprs[expr];
  int line = x.token.line;
  int w = _width(info.expr_types[expr]);

  if (info.ref_kinds[expr] == steav_typecheck::REF_NATIVE_CONST) {
    const steav_typecheck::NativeRef &ref = info.natives[info.ref_index[expr]];
    double value = steav_stdlib::module_at(ref.module)->constants[ref.index].value;
    _emit_byte(OP_CONSTANT, line);
    _emit_u16(_constant(make_lek_thom(value), line), line);
    return;
  }

  Place p;
  if (_place(expr, p)) {
    _emit_get(p, w, line);
    return;
  }

  // a temporary struct (`f().x`, `f()[1]`): build it all, keep one part
  _expr(x.left);
  if (x.kind == steav_ast::EXPR_INDEX) {
    _expr(x.right);
    _emit_bytes(OP_STRUCT_AT, (uint8_t)info.ref_index[expr], line);
    return;
  }
  _emit_bytes(OP_STRUCT_FIELD, (uint8_t)info.ref_index[expr], line);
  _emit_bytes((uint8_t)w, (uint8_t)_width(info.expr_types[x.left]), line);
}

void Compiler::_assign(int expr) {
  const Expr &x = ast.exprs[expr];
  int line = x.token.line;
  int w = _width(info.expr_types[x.left]);
  Place p;
  if (!_place(x.left, p)) {
    _error(line, "invalid assignment target"); // the checker should've caught it
    return;
  }
  if (x.token.type == steav_frontend::TOKEN_EQUAL) {
    _expr_coerced(x.right);
  } else {
    // a op= b: copy whatever the place pushed, read it, op, write back
    int pushed = (p.kind == P_FIELD ? 1 : p.kind == P_INDEX ? 2 : 0) + (p.dynamic ? 1 : 0);
    if (pushed > 0) _emit_bytes(OP_DUP, (uint8_t)pushed, line);
    _emit_get(p, w, line);
    _expr(x.right);
    steav_frontend::TokenType op;
    switch (x.token.type) {
    case steav_frontend::TOKEN_PLUS_EQUAL: op = steav_frontend::TOKEN_PLUS; break;
    case steav_frontend::TOKEN_MINUS_EQUAL: op = steav_frontend::TOKEN_MINUS; break;
    case steav_frontend::TOKEN_STAR_EQUAL: op = steav_frontend::TOKEN_STAR; break;
    default: op = steav_frontend::TOKEN_SLASH; break;
    }
    _emit_binary_op(op, info.expr_overload[expr], line);
  }
  _emit_set(p, w, line);
}

/* ----------------------------------------------------------------- helpers */

static std::string type_ref_name(const steav_ast::Ast &ast, int ref) {
  const steav_ast::TypeRef &t = ast.types[ref];
  std::string name;
  if (t.kind == steav_ast::TYPEREF_ARRAY) {
    name = "[" + type_ref_name(ast, t.elem) + "]";
  } else {
    if (t.has_qualifier) name = std::string(t.qualifier.start, t.qualifier.length) + ".";
    name += std::string(t.name.start, t.name.length);
  }
  if (t.optional) name += "?";
  return name;
}

/* `rupamun dot(a: Vec3, b: Vec3) -> LekThom;` in a stdlib module gets its
 * body from C++, looked up by "dot(Vec3,Vec3)" / "Vec3.length()" */
void Compiler::_bind_native(int d, ObjFunction *fn) {
  const Decl &decl = ast.decls[d];
  std::string sig;
  if (decl.owner >= 0)
    sig = std::string(ast.decls[decl.owner].name.start, ast.decls[decl.owner].name.length) + ".";
  sig += std::string(decl.name.start, decl.name.length) + "(";
  for (size_t i = 0; i < decl.params.size(); i++) {
    if (i > 0) sig += ",";
    sig += type_ref_name(ast, decl.params[i].type);
  }
  sig += ")";

  const std::string &module = ast.modules[decl.module].name;
  int si = steav_stdlib::find_module(module.c_str(), (int)module.size());
  const steav_stdlib::Module *m = si >= 0 ? steav_stdlib::module_at(si) : nullptr;
  for (int i = 0; m != nullptr && i < m->bound_count; i++) {
    if (sig == m->bound[i].signature) {
      fn->native = m->bound[i].fn;
      return;
    }
  }
  std::string message = "no C++ implementation for " + module + " " + sig;
  _error(decl.name.line, message.c_str());
}

ObjStructType *Compiler::_struct_type(int record) {
  if (struct_types[record] != nullptr) return struct_types[record];
  const Record &rec = info.records[record];
  const Decl &decl = ast.decls[rec.decl];
  ObjStructType *desc = heap.allocate<ObjStructType>(OBJ_STRUCT_TYPE);
  desc->name = std::string(decl.name.start, decl.name.length);
  struct_types[record] = desc;
  for (const steav_typecheck::FieldInfo &f : rec.fields) {
    StructFieldDesc fd;
    fd.name = std::string(f.name.start, f.name.length);
    fd.width = f.width;
    fd.optional = info.types[f.type].optional;
    fd.desc = _is_struct(f.type) ? _struct_type(info.decl_record[info.types[f.type].decl])
                                 : nullptr;
    desc->fields.push_back(fd);
  }
  return desc;
}

int Compiler::_constant(const Value &value, int line) {
  int idx = _chunk().add_constant(value);
  if (idx > 0xffff) {
    _error(line, "too many constants in one rupamun");
    return 0;
  }
  return idx;
}

// the checker already pinned the literal's type, parse it as that
void Compiler::_number(int expr) {
  const Expr &x = ast.exprs[expr];
  std::string lexeme(x.token.start, x.token.length);
  Value v;
  switch (info.types[info.expr_types[expr]].kind) {
  case steav_typecheck::TYPE_LEK_KUT:
    v = make_lek_kut((int32_t)strtol(lexeme.c_str(), nullptr, 10));
    break;
  case steav_typecheck::TYPE_LEK_THOM_KLANG:
    v = make_lek_thom_klang((int64_t)strtoll(lexeme.c_str(), nullptr, 10));
    break;
  default:
    v = make_lek_thom(strtod(lexeme.c_str(), nullptr));
    break;
  }
  _emit_byte(OP_CONSTANT, x.token.line);
  _emit_u16(_constant(v, x.token.line), x.token.line);
}

void Compiler::_string(int expr) {
  const Expr &x = ast.exprs[expr];
  ObjString *s = heap.allocate<ObjString>(OBJ_STRING);
  s->chars = std::string(x.token.start + 1, x.token.length - 2); // no quotes
  _emit_byte(OP_CONSTANT, x.token.line);
  _emit_u16(_constant(make_obj(s), x.token.line), x.token.line);
}

int Compiler::_emit_jump(uint8_t op, int line) {
  _emit_byte(op, line);
  _emit_u16(0xffff, line);
  return (int)_chunk().code.size() - 2;
}

void Compiler::_patch_jump(int offset) {
  int jump = (int)_chunk().code.size() - offset - 2;
  if (jump > 0xffff) _error(_chunk().lines[offset], "too much code to jump over");
  _chunk().code[offset] = (uint8_t)((jump >> 8) & 0xff);
  _chunk().code[offset + 1] = (uint8_t)(jump & 0xff);
}

void Compiler::_emit_loop(int loop_start, int line) {
  _emit_byte(OP_LOOP, line);
  int offset = (int)_chunk().code.size() - loop_start + 2;
  if (offset > 0xffff) _error(line, "loop body too large");
  _emit_u16(offset, line);
}

void Compiler::_error(int line, const char *message) {
  this->had_error = true;
  if (steav_capture_diagnostic(line, nullptr, 0, message)) return;
  char compile_err_str[512];
  snprintf(compile_err_str, sizeof(compile_err_str), "[line %d] Error: %s", line,
           message);
  STEAV_LOGGING_LOG(compile_err_str, STEAV_LOGGING_COMPILE_ERROR);
}

} // namespace steav_compiler
