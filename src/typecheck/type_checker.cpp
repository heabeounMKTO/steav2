#include "typecheck/type_checker.h"
#include "logging/steav_diagnostics.h"
#include "stdlib/module.h"
#include <cstdarg>
#include <cstdlib>
#include <cstring>

namespace steav_typecheck {

using steav_ast::Decl;
using steav_ast::Expr;
using steav_ast::Stmt;
using steav_ast::TypeRef;
using steav_frontend::Token;
using steav_frontend::TokenType;

static inline bool tok_is(const Token &t, const char *s) {
  return (int)strlen(s) == t.length && memcmp(t.start, s, t.length) == 0;
}
static inline bool tok_eq(const Token &a, const Token &b) {
  return a.length == b.length && memcmp(a.start, b.start, a.length) == 0;
}
static inline std::string tok_str(const Token &t) {
  return std::string(t.start, t.length);
}

/* ---------------------------------------------------------------- TypeInfo */

int TypeInfo::intern(const Type &t) {
  for (int i = 0; i < (int)types.size(); i++) {
    if (types_equal(types[i], t)) return i;
  }
  types.push_back(t);
  return (int)types.size() - 1;
}

/* sampoan: all its fields side by side (+1 flag slot when optional),
 * everything else is a single Value */
int TypeInfo::width(int type) const {
  if (type < 0) return 1;
  const Type &t = types[type];
  if (t.kind != TYPE_STRUCT) return 1;
  int w = records[decl_record[t.decl]].slots;
  return t.optional ? w + 1 : w;
}

int TypeInfo::base(int type) {
  Type t = types[type];
  t.optional = false;
  return intern(t);
}

int TypeInfo::optional_of(int type) {
  Type t = types[type];
  t.optional = true;
  return intern(t);
}

std::string TypeInfo::type_name(int type, const steav_ast::Ast &ast) const {
  if (type < 0) return "<error>";
  const Type &t = types[type];
  std::string name;
  switch (t.kind) {
  case TYPE_LEK_KUT: name = "LekKut"; break;
  case TYPE_LEK_THOM: name = "LekThom"; break;
  case TYPE_LEK_THOM_KLANG: name = "LekThomKlang"; break;
  case TYPE_AHSOR: name = "Ahsor"; break;
  case TYPE_BOOLEAN: name = "Boolean"; break;
  case TYPE_ORT_MEAN: name = "OrtMean"; break;
  case TYPE_STRUCT:
  case TYPE_CLASS: name = tok_str(ast.decls[t.decl].name); break;
  case TYPE_ARRAY: name = "[" + type_name(t.elem, ast) + "]"; break;
  case TYPE_NONE: return "sone";
  }
  if (t.optional) name += "?";
  return name;
}

/* ------------------------------------------------------------------ passes */

SteavStatus TypeChecker::check(TypeInfo &info) {
  this->ti = &info;
  size_t ne = ast.exprs.size();
  size_t nd = ast.decls.size();
  info.expr_types.assign(ne, -1);
  info.expr_coerce.assign(ne, -1);
  info.expr_overload.assign(ne, -1);
  info.ref_kinds.assign(ne, REF_NONE);
  info.ref_index.assign(ne, -1);
  info.narrowed.assign(ne, 0);
  info.none_side.assign(ne, 0);
  info.str_side.assign(ne, 0);
  info.call_kinds.assign(ne, CALL_NONE);
  info.call_targets.assign(ne, -1);
  info.decl_record.assign(nd, -1);
  info.fun_sigs.assign(nd, FunSig());
  info.decl_type.assign(nd, -1);
  info.decl_slot.assign(nd, -1);
  info.decl_global.assign(nd, 0);
  info.scopes.assign(ast.modules.size(), ModuleScope());

  this->t_lek_kut = info.intern(make_type(TYPE_LEK_KUT));
  this->t_lek_thom = info.intern(make_type(TYPE_LEK_THOM));
  this->t_lek_thom_klang = info.intern(make_type(TYPE_LEK_THOM_KLANG));
  this->t_ahsor = info.intern(make_type(TYPE_AHSOR));
  this->t_boolean = info.intern(make_type(TYPE_BOOLEAN));
  this->t_ort_mean = info.intern(make_type(TYPE_ORT_MEAN));
  this->t_none = info.intern(make_type(TYPE_NONE));

  // 1. names: sampoan / tnak / rupamun / imports, per module
  for (int m : ast.module_order) _declare_module(m);

  // 2. sampoan + tnak layouts (nested sampoans first, cycles are errors)
  for (int r = 0; r < (int)info.records.size(); r++) _layout_record(r);

  // 3. signatures + operator overloads
  for (int m : ast.module_order) {
    for (int d : ast.modules[m].program) {
      const Decl &decl = ast.decls[d];
      if (decl.kind == steav_ast::DECL_FUN) _declare_signatures(d);
      if (decl.kind == steav_ast::DECL_STRUCT ||
          decl.kind == steav_ast::DECL_CLASS) {
        for (int method : decl.methods) _declare_signatures(method);
      }
    }
  }

  // 4. top level code in order, this is where globals get declared
  for (int m : ast.module_order) _check_top_level(m);

  // 5. function + method bodies, every global is known by now
  for (int m : ast.module_order) {
    for (int d : ast.modules[m].program) {
      const Decl &decl = ast.decls[d];
      if (decl.kind == steav_ast::DECL_FUN) _check_fun_body(d);
      if (decl.kind == steav_ast::DECL_STRUCT ||
          decl.kind == steav_ast::DECL_CLASS) {
        for (int method : decl.methods) _check_fun_body(method);
      }
    }
  }

  return had_error ? STEAV_LOGGING_COMPILE_ERROR : STEAV_LOGGING_OK;
}

void TypeChecker::_declare_module(int module) {
  this->cur_module = module;
  const steav_ast::Module &mod = ast.modules[module];
  ModuleScope &scope = ti->scopes[module];

  if (mod.is_native) {
    int si = steav_stdlib::find_module(mod.name.c_str(), (int)mod.name.size());
    const steav_stdlib::Module *sm = steav_stdlib::module_at(si);
    for (int i = 0; i < sm->function_count; i++) {
      Symbol s;
      s.kind = SYM_NATIVE_FUN;
      s.name = sm->functions[i].name;
      s.index = (int)ti->natives.size();
      NativeRef ref = {si, i};
      ti->natives.push_back(ref);
      scope.symbols.push_back(s);
    }
    for (int i = 0; i < sm->constant_count; i++) {
      Symbol s;
      s.kind = SYM_NATIVE_CONST;
      s.name = sm->constants[i].name;
      s.type = t_lek_thom;
      s.index = (int)ti->natives.size();
      NativeRef ref = {si, i};
      ti->natives.push_back(ref);
      scope.symbols.push_back(s);
    }
    return;
  }

  for (int d : mod.program) {
    const Decl &decl = ast.decls[d];
    switch (decl.kind) {
    case steav_ast::DECL_STRUCT:
    case steav_ast::DECL_CLASS: {
      bool is_class = decl.kind == steav_ast::DECL_CLASS;
      Record r;
      r.decl = d;
      r.is_class = is_class;
      r.type = ti->intern(is_class ? make_class_type(d) : make_struct_type(d));
      int ri = (int)ti->records.size();
      ti->records.push_back(r);
      ti->decl_record[d] = ri;
      Symbol s;
      s.kind = is_class ? SYM_CLASS : SYM_STRUCT;
      s.name = tok_str(decl.name);
      s.decl = d;
      s.record = ri;
      _declare_symbol(module, s, decl.name);
      break;
    }
    case steav_ast::DECL_FUN:
      if (decl.fun_name_kind == steav_ast::FUN_INIT) {
        _error(decl.name, "init only makes sense inside a tnak");
      } else if (decl.fun_name_kind == steav_ast::FUN_NAMED) {
        Symbol s;
        s.kind = SYM_FUN;
        s.name = tok_str(decl.name);
        s.decl = d;
        _declare_symbol(module, s, decl.name);
      }
      break;
    case steav_ast::DECL_STMT:
      if (ast.stmts[decl.stmt].kind == steav_ast::STMT_IMPORT)
        _import(module, ast.stmts[decl.stmt]);
      break;
    case steav_ast::DECL_VAR:
      break; // globals get declared in order, in _check_top_level
    }
  }
}

void TypeChecker::_import(int module, const Stmt &stmt) {
  std::string resolved = ast.resolve_import(module, stmt.name);
  int target = ast.find_module(resolved.c_str(), (int)resolved.size());
  if (target < 0) {
    _error(stmt.name, "no module named '%.*s'", stmt.name.length,
           stmt.name.start);
    return;
  }
  ModuleScope &scope = ti->scopes[module];
  if (stmt.has_alias) {
    std::string alias = tok_str(stmt.alias);
    for (const std::string &a : scope.alias_names) {
      if (a == alias) {
        _error(stmt.alias, "'%s' is already a module alias", alias.c_str());
        return;
      }
    }
    scope.alias_names.push_back(alias);
    scope.alias_modules.push_back(target);
    return;
  }
  for (int b : scope.bare_imports) {
    if (b == target) return; // importing twice is fine, it's the same names
  }
  scope.bare_imports.push_back(target);
  scope.bare_import_at.push_back(stmt.name);
}

/* runs once the imported modules are fully declared (globals too), so
 * every name that `yok` would drop into this module is known */
void TypeChecker::_check_import_collisions(int module) {
  ModuleScope &scope = ti->scopes[module];
  for (size_t i = 0; i < scope.bare_imports.size(); i++) {
    const ModuleScope &imported = ti->scopes[scope.bare_imports[i]];
    for (const Symbol &s : imported.symbols) {
      if (_lookup_own(module, s.name.c_str(), (int)s.name.size()) != nullptr) {
        const steav_ast::Module &target = ast.modules[scope.bare_imports[i]];
        if (target.is_file)
          _error(scope.bare_import_at[i],
                 "'%s' from this module clashes with a name declared here, use "
                 "`yok \"%s\" jea <alias>;` instead",
                 s.name.c_str(), target.name.c_str());
        else
          _error(scope.bare_import_at[i],
                 "'%s' from this module clashes with a name declared here, use "
                 "`yok %s jea <alias>;` instead",
                 s.name.c_str(), target.name.c_str());
      }
      for (size_t j = 0; j < i; j++) {
        if (_lookup_own(scope.bare_imports[j], s.name.c_str(),
                        (int)s.name.size()) != nullptr) {
          _error(scope.bare_import_at[i], "'%s' is exported by both '%s' and '%s'",
                 s.name.c_str(),
                 ast.modules[scope.bare_imports[j]].name.c_str(),
                 ast.modules[scope.bare_imports[i]].name.c_str());
        }
      }
    }
  }
}

void TypeChecker::_layout_record(int record) {
  Record &rec = ti->records[record];
  if (rec.state == 2) return;
  const Decl &decl = ast.decls[rec.decl];
  if (rec.state == 1) {
    _error(decl.name, "sampoan '%.*s' contains itself, it would be infinitely big",
           decl.name.length, decl.name.start);
    return;
  }
  rec.state = 1;

  int offset = 0;
  for (int f : decl.fields) {
    const Decl &field = ast.decls[f];
    for (const FieldInfo &other : rec.fields) {
      if (tok_eq(other.name, field.name))
        _error(field.name, "field '%.*s' is declared twice", field.name.length,
               field.name.start);
    }
    int t = _resolve_type_ref(decl.module, field.var_type);
    if (t < 0) continue;
    if (ti->types[t].kind == TYPE_STRUCT)
      _layout_record(ti->decl_record[ti->types[t].decl]);
    if (ti->types[t].kind == TYPE_ORT_MEAN) {
      _error(field.name, "a field can't be OrtMean");
      continue;
    }
    ti->decl_type[f] = t;
    FieldInfo fi;
    fi.name = field.name;
    fi.type = t;
    fi.offset = offset;
    fi.width = ti->width(t);
    ti->records[record].fields.push_back(fi);
    offset += fi.width;
  }

  Record &done = ti->records[record];
  for (size_t i = 0; i < decl.methods.size(); i++) {
    const Decl &method = ast.decls[decl.methods[i]];
    if (method.fun_name_kind == steav_ast::FUN_OPERATOR) {
      _error(method.name, "operator overloads go at the top level, not inside a type");
      continue;
    }
    if (method.fun_name_kind == steav_ast::FUN_INIT) {
      if (!done.is_class)
        _error(method.name, "a sampoan can't have init, it's built from its fields in order");
      else
        done.init = decl.methods[i];
    }
    for (const FieldInfo &f : done.fields) {
      if (tok_eq(f.name, method.name))
        _error(method.name, "'%.*s' is both a field and a method", method.name.length,
               method.name.start);
    }
    for (size_t j = 0; j < i; j++) {
      if (tok_eq(ast.decls[decl.methods[j]].name, method.name))
        _error(method.name, "method '%.*s' is declared twice", method.name.length,
               method.name.start);
    }
  }
  done.slots = offset;
  done.state = 2;
  if (!done.is_class && done.slots > 255)
    _error(decl.name, "sampoan '%.*s' is too big (%d slots, max 255)",
           decl.name.length, decl.name.start, done.slots);
}

void TypeChecker::_declare_signatures(int d) {
  const Decl &decl = ast.decls[d];
  FunSig sig;
  if (decl.owner >= 0) {
    sig.record = ti->decl_record[decl.owner];
    sig.param_slots = ti->width(ti->records[sig.record].type);
  }
  for (const steav_ast::Param &p : decl.params) {
    int t = _resolve_type_ref(decl.module, p.type);
    if (t >= 0 && ti->types[t].kind == TYPE_ORT_MEAN) {
      _error(p.name, "a parameter can't be OrtMean");
      t = -1;
    }
    sig.params.push_back(t);
    sig.param_slots += ti->width(t);
  }
  sig.ret = decl.return_type < 0 ? t_ort_mean
                                 : _resolve_type_ref(decl.module, decl.return_type);
  if (decl.fun_name_kind == steav_ast::FUN_INIT && sig.ret != t_ort_mean)
    _error(decl.name, "init doesn't return anything, drop the '->'");
  if (sig.param_slots > 255)
    _error(decl.name, "too many parameter slots (max 255)");
  ti->fun_sigs[d] = sig;

  if (decl.fun_name_kind != steav_ast::FUN_OPERATOR) return;
  TokenType op = decl.name.type;
  int arity = (int)decl.params.size();
  if (arity != 2 && !(op == steav_frontend::TOKEN_MINUS && arity == 1)) {
    _error(decl.name, op == steav_frontend::TOKEN_MINUS
                          ? "operator '-' takes one parameter (negation) or two"
                          : "operator '%.*s' takes two parameters",
           decl.name.length, decl.name.start);
    return;
  }
  for (int p : sig.params) {
    if (p < 0) return;
  }
  if (sig.ret < 0) return;
  int lhs = sig.params[0];
  int rhs = arity == 2 ? sig.params[1] : -1;
  if (_find_overload(op, arity, lhs, rhs) >= 0) {
    _error(decl.name, "operator '%.*s' is already declared for these types",
           decl.name.length, decl.name.start);
    return;
  }
  OperatorOverload ov;
  ov.op = op;
  ov.arity = arity;
  ov.lhs = lhs;
  ov.rhs = rhs;
  ov.decl = d;
  ti->overloads.push_back(ov);
}

void TypeChecker::_check_top_level(int module) {
  this->cur_module = module;
  this->cur_fun = -1;
  this->cur_record = -1;
  this->scope_depth = 0;
  this->slot_top = 0;
  locals.clear();
  narrows.clear();
  _check_import_collisions(module);

  for (int d : ast.modules[module].program) {
    const Decl &decl = ast.decls[d];
    if (decl.kind == steav_ast::DECL_VAR) {
      _check_decl(d);
    } else if (decl.kind == steav_ast::DECL_STMT &&
               ast.stmts[decl.stmt].kind != steav_ast::STMT_IMPORT) {
      _check_decl(d);
    }
  }
}

void TypeChecker::_check_fun_body(int d) {
  const Decl &decl = ast.decls[d];
  const FunSig &sig = ti->fun_sigs[d];
  if (decl.is_native) {
    // the compiler binds it to its C++ implementation
    if (!ast.modules[decl.module].is_stdlib)
      _error(decl.name, "'%.*s' needs a body, only stdlib modules can declare "
                        "rupamuns implemented in C++",
             decl.name.length, decl.name.start);
    return;
  }
  this->cur_module = decl.module;
  this->cur_fun = d;
  this->cur_record = sig.record;
  this->scope_depth = 1;
  this->loop_depth = 0;
  this->slot_top = 0;
  locals.clear();
  narrows.clear();

  // nis sits in front of the params
  if (cur_record >= 0) this->slot_top = ti->width(ti->records[cur_record].type);
  for (size_t i = 0; i < decl.params.size(); i++) {
    for (size_t j = 0; j < i; j++) {
      if (tok_eq(decl.params[j].name, decl.params[i].name))
        _error(decl.params[i].name, "parameter '%.*s' is declared twice",
               decl.params[i].name.length, decl.params[i].name.start);
    }
    LocalVar l;
    l.name = decl.params[i].name;
    l.type = sig.params[i];
    l.slot = slot_top;
    l.depth = 1;
    locals.push_back(l);
    this->slot_top += ti->width(sig.params[i]);
  }

  for (int b : decl.body) _check_decl(b);

  if (sig.ret >= 0 && sig.ret != t_ort_mean && !_returns(decl.body))
    _error(decl.name, "'%.*s' returns %s but can reach the end without a morvenh",
           decl.name.length, decl.name.start, _name(sig.ret).c_str());

  this->cur_fun = -1;
  this->cur_record = -1;
  this->scope_depth = 0;
}

// does every path through these decls hit a morvenh
bool TypeChecker::_returns(const std::vector<int> &decls) const {
  for (int d : decls) {
    const Decl &decl = ast.decls[d];
    if (decl.kind != steav_ast::DECL_STMT) continue;
    const Stmt &s = ast.stmts[decl.stmt];
    if (s.kind == steav_ast::STMT_RETURN) return true;
    if (s.kind == steav_ast::STMT_BLOCK && _returns(s.body)) return true;
    if (s.kind == steav_ast::STMT_IF && s.else_branch >= 0) {
      std::vector<int> then_branch(1, s.then_branch);
      std::vector<int> else_branch(1, s.else_branch);
      if (_returns(then_branch) && _returns(else_branch)) return true;
    }
  }
  return false;
}

/* ------------------------------------------------------ decls + statements */

void TypeChecker::_check_decl(int d) {
  const Decl &decl = ast.decls[d];
  switch (decl.kind) {
  case steav_ast::DECL_VAR: {
    int t = -1;
    if (decl.var_type >= 0) {
      t = _resolve_type_ref(cur_module, decl.var_type);
      if (t >= 0 && ti->types[t].kind == TYPE_ORT_MEAN) {
        _error(decl.name, "a variable can't be OrtMean");
        t = -1;
      }
      if (decl.initializer >= 0) {
        if (t >= 0)
          _expect(decl.initializer, t);
        else
          _check_expr(decl.initializer, -1);
      }
    } else if (decl.initializer >= 0) {
      t = _check_expr(decl.initializer, -1);
      if (t >= 0 && ti->types[t].kind == TYPE_NONE) {
        _error(decl.name,
               "can't tell what '%.*s' is from a bare sone, give it a type: "
               "akthe %.*s: T? = sone;",
               decl.name.length, decl.name.start, decl.name.length,
               decl.name.start);
        t = -1;
      } else if (t >= 0 && ti->types[t].kind == TYPE_ORT_MEAN) {
        _error(decl.name, "that expression is OrtMean, there's nothing to store");
        t = -1;
      }
    } else {
      _error(decl.name, "'%.*s' needs a type or an initializer",
             decl.name.length, decl.name.start);
    }
    ti->decl_type[d] = t;

    if (cur_fun < 0 && scope_depth == 0) {
      Symbol s;
      s.kind = SYM_GLOBAL;
      s.name = tok_str(decl.name);
      s.decl = d;
      s.type = t;
      s.index = ti->global_slots;
      ti->decl_global[d] = 1;
      ti->decl_slot[d] = ti->global_slots;
      ti->global_slots += ti->width(t);
      if (ti->global_slots > 65535)
        _error(decl.name, "too many globals (max 65535 slots)");
      _declare_symbol(cur_module, s, decl.name);
    } else {
      _declare_local(d, t);
    }
    break;
  }
  case steav_ast::DECL_STMT:
    _check_stmt(decl.stmt);
    break;
  case steav_ast::DECL_FUN:
  case steav_ast::DECL_STRUCT:
  case steav_ast::DECL_CLASS:
    _error(decl.name, "'%.*s' can only be declared at the top level",
           decl.name.length, decl.name.start);
    break;
  }
}

void TypeChecker::_check_stmt(int si) {
  const Stmt &s = ast.stmts[si];
  switch (s.kind) {
  case steav_ast::STMT_EXPR:
    _check_expr(s.expr, -1);
    break;
  case steav_ast::STMT_PRINT:
    _check_expr(s.expr, -1);
    break;
  case steav_ast::STMT_BLOCK:
    _begin_scope();
    for (int d : s.body) _check_decl(d);
    _end_scope();
    break;
  case steav_ast::STMT_IF: {
    _expect(s.expr, t_boolean);

    /* `ber (x != sone)` narrows x to T inside the then branch,
     * `ber (x == sone)` narrows it in the minjengte */
    Narrow n;
    bool when_true = false;
    bool has_narrow = _narrowing(s.expr, n, when_true);

    if (has_narrow && when_true) narrows.push_back(n);
    _check_decl(s.then_branch);
    if (has_narrow && when_true) narrows.pop_back();
    if (s.else_branch >= 0) {
      if (has_narrow && !when_true) narrows.push_back(n);
      _check_decl(s.else_branch);
      if (has_narrow && !when_true) narrows.pop_back();
    }
    break;
  }
  case steav_ast::STMT_WHILE: {
    _expect(s.expr, t_boolean);
    // the condition is rechecked every time around, so the body can narrow
    this->loop_depth++;
    Narrow n;
    bool when_true = false;
    bool has_narrow = _narrowing(s.expr, n, when_true) && when_true;
    if (has_narrow) narrows.push_back(n);
    _check_decl(s.then_branch);
    if (has_narrow) narrows.pop_back();
    this->loop_depth--;
    break;
  }
  case steav_ast::STMT_FOR:
    _begin_scope();
    if (s.initializer >= 0) _check_decl(s.initializer);
    if (s.expr >= 0) _expect(s.expr, t_boolean);
    this->loop_depth++;
    if (s.increment >= 0) _check_expr(s.increment, -1);
    _check_decl(s.then_branch);
    this->loop_depth--;
    _end_scope();
    break;
  case steav_ast::STMT_RETURN: {
    if (cur_fun < 0) {
      _error(s.keyword, "morvenh outside of a rupamun");
      if (s.expr >= 0) _check_expr(s.expr, -1);
      break;
    }
    const Decl &fun = ast.decls[cur_fun];
    int ret = ti->fun_sigs[cur_fun].ret;
    if (fun.fun_name_kind == steav_ast::FUN_INIT) {
      if (s.expr >= 0) _error(s.keyword, "init can't return a value");
    } else if (s.expr < 0) {
      if (ret >= 0 && ret != t_ort_mean)
        _error(s.keyword, "expected a %s to return", _name(ret).c_str());
    } else if (ret >= 0) {
      _expect(s.expr, ret);
    }
    break;
  }
  case steav_ast::STMT_IMPORT:
    _error(s.keyword, "yok only works at the top level");
    break;
  }
}

/* ------------------------------------------------------------- expressions */

int TypeChecker::_expect(int expr, int want) {
  int t = _check_expr(expr, want);
  if (want < 0) return t;
  if (!_coerce(expr, t, want)) return -1;
  return want;
}

// same type, or T into a T? (records the wrap for the compiler)
bool TypeChecker::_coerce(int expr, int actual, int want) {
  if (actual < 0 || want < 0) return false;
  if (actual == want) return true;
  if (ti->types[want].optional) {
    if (ti->types[actual].kind == TYPE_NONE) {
      // a bare sone, pin it (and any parens around it) to T?
      for (int e = expr;; e = ast.exprs[e].right) {
        ti->expr_types[e] = want;
        if (ast.exprs[e].kind != steav_ast::EXPR_GROUPING) break;
      }
      return true;
    }
    if (ti->base(want) == actual) {
      ti->expr_coerce[expr] = want;
      return true;
    }
  }
  const char *hint = "";
  if (is_number_kind(ti->types[actual].kind) &&
      is_number_kind(ti->types[want].kind))
    hint = " (numbers never convert implicitly, use LekThom(x) / LekKut(x))";
  _error(ast.exprs[expr].token, "expected %s but got %s%s", _name(want).c_str(),
         _name(actual).c_str(), hint);
  return false;
}

int TypeChecker::_check_expr(int expr, int expected) {
  const Expr &x = ast.exprs[expr];
  int t = -1;
  switch (x.kind) {
  case steav_ast::EXPR_LITERAL:
    t = _check_literal(expr, expected);
    break;
  case steav_ast::EXPR_GROUPING:
    t = _check_expr(x.right, expected);
    break;
  case steav_ast::EXPR_VARIABLE:
    t = _check_variable(expr);
    break;
  case steav_ast::EXPR_THIS:
    if (cur_record < 0) {
      _error(x.token, "nis only means something inside a method");
    } else {
      ti->ref_kinds[expr] = REF_LOCAL;
      ti->ref_index[expr] = 0;
      t = ti->records[cur_record].type;
    }
    break;
  case steav_ast::EXPR_UNARY:
    t = _check_unary(expr, expected);
    break;
  case steav_ast::EXPR_BINARY:
    t = _check_binary(expr, expected);
    break;
  case steav_ast::EXPR_LOGICAL: {
    int l = _expect(x.left, t_boolean);
    int r = _expect(x.right, t_boolean);
    t = (l >= 0 && r >= 0) ? t_boolean : -1;
    break;
  }
  case steav_ast::EXPR_ASSIGN:
    t = _check_assign(expr);
    break;
  case steav_ast::EXPR_GET:
    t = _check_get(expr);
    break;
  case steav_ast::EXPR_INDEX: {
    int o = _check_expr(x.left, -1);
    int i = _expect(x.right, t_lek_kut);
    if (o < 0 || i < 0) break;
    Type ot = ti->types[o];
    if (ot.kind == TYPE_STRUCT && !ot.optional) {
      // v[0] is v.x, for sampoans whose fields are all one type (Vec3 & co)
      int r = ti->decl_record[ot.decl];
      t = _struct_elem(r);
      if (t < 0) {
        _error(x.token, "can only index a sampoan whose fields all have the same "
                        "one slot type (like Vec3), %s doesn't",
               _name(o).c_str());
        break;
      }
      ti->ref_kinds[expr] = REF_STRUCT_INDEX;
      ti->ref_index[expr] = (int)ti->records[r].fields.size();
      break;
    }
    if (ot.kind != TYPE_ARRAY || ot.optional) {
      _error(x.token, "can only index into an array or a sampoan like Vec3, this is %s",
             _name(o).c_str());
      break;
    }
    t = ot.elem;
    break;
  }
  case steav_ast::EXPR_CALL:
    t = _check_call(expr);
    break;
  case steav_ast::EXPR_ARRAY:
    t = _check_array(expr, expected);
    break;
  }
  ti->expr_types[expr] = t;
  return t;
}

int TypeChecker::_check_literal(int expr, int expected) {
  const Token &tok = ast.exprs[expr].token;
  switch (tok.type) {
  case steav_frontend::TOKEN_TRUE:
  case steav_frontend::TOKEN_FALSE:
    return t_boolean;
  case steav_frontend::TOKEN_STRING:
    return t_ahsor;
  case steav_frontend::TOKEN_NIL:
    if (expected >= 0 && ti->types[expected].optional) return expected;
    return t_none;
  case steav_frontend::TOKEN_NUMBER: {
    /* untyped until here: take the number type the context wants,
     * otherwise it's a Lek (= LekThom) */
    bool has_dot = memchr(tok.start, '.', tok.length) != nullptr;
    int want = expected >= 0 ? ti->base(expected) : -1;
    if (want >= 0 && is_number_kind(ti->types[want].kind)) {
      TypeKind k = ti->types[want].kind;
      if (has_dot && k != TYPE_LEK_THOM) {
        _error(tok, "'%.*s' isn't a whole number, it can't be a %s", tok.length,
               tok.start, _name(want).c_str());
        return -1;
      }
      if (k == TYPE_LEK_KUT && strtoll(tok.start, nullptr, 10) > 2147483647LL) {
        _error(tok, "'%.*s' doesn't fit in a LekKut", tok.length, tok.start);
        return -1;
      }
      return want;
    }
    return t_lek_thom;
  }
  default:
    return -1;
  }
}

int TypeChecker::_strip_grouping(int expr) const {
  while (ast.exprs[expr].kind == steav_ast::EXPR_GROUPING)
    expr = ast.exprs[expr].right;
  return expr;
}

bool TypeChecker::_is_nil_literal(int expr) const {
  const Expr &x = ast.exprs[_strip_grouping(expr)];
  return x.kind == steav_ast::EXPR_LITERAL &&
         x.token.type == steav_frontend::TOKEN_NIL;
}

// a number literal or arithmetic made only of them, `-(2 * 3)`
bool TypeChecker::_is_untyped_literal(int expr) const {
  const Expr &x = ast.exprs[expr];
  switch (x.kind) {
  case steav_ast::EXPR_LITERAL:
    return x.token.type == steav_frontend::TOKEN_NUMBER;
  case steav_ast::EXPR_GROUPING:
    return _is_untyped_literal(x.right);
  case steav_ast::EXPR_UNARY:
    return x.token.type == steav_frontend::TOKEN_MINUS &&
           _is_untyped_literal(x.right);
  case steav_ast::EXPR_BINARY:
    switch (x.token.type) {
    case steav_frontend::TOKEN_PLUS:
    case steav_frontend::TOKEN_MINUS:
    case steav_frontend::TOKEN_STAR:
    case steav_frontend::TOKEN_SLASH:
      return _is_untyped_literal(x.left) && _is_untyped_literal(x.right);
    default:
      return false;
    }
  default:
    return false;
  }
}

/* `v * 2` where v: Vec3, if there's exactly one `*` overload with Vec3 on
 * that side, the literal becomes whatever the other param wants */
int TypeChecker::_overload_hint(TokenType op, int known, bool known_is_lhs) {
  if (is_number_kind(ti->types[known].kind) && !ti->types[known].optional)
    return known;
  int found = -1;
  for (const OperatorOverload &ov : ti->overloads) {
    if (ov.op != op || ov.arity != 2) continue;
    if ((known_is_lhs ? ov.lhs : ov.rhs) != known) continue;
    int other = known_is_lhs ? ov.rhs : ov.lhs;
    if (found >= 0 && found != other) return -1;
    found = other;
  }
  return found;
}

int TypeChecker::_check_unary(int expr, int expected) {
  const Expr &x = ast.exprs[expr];
  if (x.token.type == steav_frontend::TOKEN_BANG) {
    return _expect(x.right, t_boolean) >= 0 ? t_boolean : -1;
  }
  int hint = (expected >= 0 && _is_untyped_literal(x.right)) ? ti->base(expected) : -1;
  int t = _check_expr(x.right, hint);
  if (t < 0) return -1;
  if (is_number_kind(ti->types[t].kind) && !ti->types[t].optional) return t;
  int ov = _find_overload(steav_frontend::TOKEN_MINUS, 1, t, -1);
  if (ov >= 0) {
    ti->expr_overload[expr] = ti->overloads[ov].decl;
    return ti->fun_sigs[ti->overloads[ov].decl].ret;
  }
  _error(x.token, "no unary '-' for %s, declare rupamun -(a: %s) -> ...",
         _name(t).c_str(), _name(t).c_str());
  return -1;
}

int TypeChecker::_check_binary(int expr, int expected) {
  const Expr &x = ast.exprs[expr];
  TokenType op = x.token.type;
  bool is_eq = op == steav_frontend::TOKEN_EQUAL_EQUAL ||
               op == steav_frontend::TOKEN_BANG_EQUAL;
  bool is_arith = op == steav_frontend::TOKEN_PLUS ||
                  op == steav_frontend::TOKEN_MINUS ||
                  op == steav_frontend::TOKEN_STAR ||
                  op == steav_frontend::TOKEN_SLASH;

  // x == sone / x != sone, only for optionals
  if (is_eq) {
    bool lnil = _is_nil_literal(x.left);
    bool rnil = _is_nil_literal(x.right);
    if (lnil && rnil) {
      _error(x.token, "comparing sone with sone");
      return -1;
    }
    if (lnil || rnil) {
      int other = lnil ? x.right : x.left;
      int t = _check_expr(other, -1);
      if (t < 0) return -1;
      if (!ti->types[t].optional) {
        _error(x.token,
               "only optionals (T?) can be compared against sone, %s is never sone",
               _name(t).c_str());
        return -1;
      }
      ti->none_side[expr] = rnil ? 1 : 2;
      ti->expr_types[lnil ? x.left : x.right] = t;
      return t_boolean;
    }
  }

  bool l_lit = _is_untyped_literal(x.left);
  bool r_lit = _is_untyped_literal(x.right);
  int lt, rt;
  if (l_lit && !r_lit) {
    rt = _check_expr(x.right, -1);
    if (rt < 0) return -1;
    lt = _check_expr(x.left, _overload_hint(op, rt, false));
  } else if (r_lit && !l_lit) {
    lt = _check_expr(x.left, -1);
    if (lt < 0) return -1;
    rt = _check_expr(x.right, _overload_hint(op, lt, true));
  } else if (l_lit && r_lit) {
    int hint = -1;
    if (is_arith && expected >= 0) {
      int b = ti->base(expected);
      if (is_number_kind(ti->types[b].kind)) hint = b;
    }
    lt = _check_expr(x.left, hint);
    rt = _check_expr(x.right, hint);
  } else {
    lt = _check_expr(x.left, -1);
    rt = _check_expr(x.right, -1);
  }
  if (lt < 0 || rt < 0) return -1;

  int ov = _find_overload(op, 2, lt, rt);
  if (ov >= 0) {
    ti->expr_overload[expr] = ti->overloads[ov].decl;
    return ti->fun_sigs[ti->overloads[ov].decl].ret;
  }

  Type a = ti->types[lt];
  if (lt == rt && !a.optional) {
    bool number = is_number_kind(a.kind);
    if (is_arith && number) return lt;
    if (op == steav_frontend::TOKEN_PLUS && a.kind == TYPE_AHSOR) return lt;
    if (!is_arith && !is_eq && number) return t_boolean;
    if (is_eq && (number || a.kind == TYPE_BOOLEAN || a.kind == TYPE_AHSOR))
      return t_boolean;
  }

  // jongyeytha's own formatting, auto-concat: Ahsor + T or T + Ahsor for any
  // T that isn't itself Ahsor (plain Ahsor + Ahsor is the case just above)
  if (op == steav_frontend::TOKEN_PLUS) {
    Type b = ti->types[rt];
    bool l_str = a.kind == TYPE_AHSOR && !a.optional;
    bool r_str = b.kind == TYPE_AHSOR && !b.optional;
    if (l_str != r_str) {
      ti->str_side[expr] = l_str ? 2 : 1;
      return l_str ? lt : rt;
    }
  }

  const char *hint = "";
  if (lt != rt && is_number_kind(a.kind) && is_number_kind(ti->types[rt].kind))
    hint = " (numbers never mix implicitly, convert one with LekThom(x) / LekKut(x))";
  _error(x.token, "no operator '%.*s' for %s and %s%s", x.token.length,
         x.token.start, _name(lt).c_str(), _name(rt).c_str(), hint);
  return -1;
}

int TypeChecker::_check_variable(int expr) {
  const Token &tok = ast.exprs[expr].token;

  int li = _find_local(tok);
  if (li >= 0) {
    const LocalVar &l = locals[li];
    ti->ref_kinds[expr] = REF_LOCAL;
    ti->ref_index[expr] = l.slot;
    if (l.type >= 0 && _is_narrowed(REF_LOCAL, l.slot)) {
      ti->narrowed[expr] = 1;
      return ti->base(l.type);
    }
    return l.type;
  }

  // bare field names inside a method mean nis.field
  if (cur_record >= 0) {
    int fi = _find_field(cur_record, tok);
    if (fi >= 0) {
      const FieldInfo &f = ti->records[cur_record].fields[fi];
      ti->ref_kinds[expr] = REF_SELF_FIELD;
      ti->ref_index[expr] = f.offset;
      return f.type;
    }
    if (_find_method(cur_record, tok) >= 0) {
      _error(tok, "'%.*s' is a method, call it with ()", tok.length, tok.start);
      return -1;
    }
  }

  const Symbol *sym = _lookup(cur_module, tok);
  if (sym != nullptr) {
    switch (sym->kind) {
    case SYM_GLOBAL:
      ti->ref_kinds[expr] = REF_GLOBAL;
      ti->ref_index[expr] = sym->index;
      if (sym->type >= 0 && _is_narrowed(REF_GLOBAL, sym->index)) {
        ti->narrowed[expr] = 1;
        return ti->base(sym->type);
      }
      return sym->type;
    case SYM_NATIVE_CONST:
      ti->ref_kinds[expr] = REF_NATIVE_CONST;
      ti->ref_index[expr] = sym->index;
      return sym->type;
    default:
      _error(tok, "'%.*s' is a %s, not a value", tok.length, tok.start,
             sym->kind == SYM_STRUCT ? "sampoan"
             : sym->kind == SYM_CLASS ? "tnak"
                                      : "rupamun");
      return -1;
    }
  }
  if (_lookup_alias(cur_module, tok) >= 0) {
    _error(tok, "'%.*s' is a module, use %.*s.<name>", tok.length, tok.start,
           tok.length, tok.start);
    return -1;
  }
  if (_builtin_type(tok) >= 0) {
    _error(tok, "'%.*s' is a type, not a value", tok.length, tok.start);
    return -1;
  }
  _error(tok, "undefined name '%.*s'", tok.length, tok.start);
  return -1;
}

int TypeChecker::_check_get(int expr) {
  const Expr &x = ast.exprs[expr];
  const Expr &o = ast.exprs[x.left];

  // vec.something through a `jea` alias
  if (o.kind == steav_ast::EXPR_VARIABLE && _find_local(o.token) < 0) {
    int alias = _lookup_alias(cur_module, o.token);
    if (alias >= 0) {
      ti->ref_kinds[x.left] = REF_MODULE;
      ti->ref_index[x.left] = alias;
      const Symbol *sym = _lookup_own(alias, x.token.start, x.token.length);
      if (sym == nullptr) {
        _error(x.token, "module '%.*s' has no '%.*s'", o.token.length,
               o.token.start, x.token.length, x.token.start);
        return -1;
      }
      if (sym->kind == SYM_GLOBAL) {
        ti->ref_kinds[expr] = REF_GLOBAL;
        ti->ref_index[expr] = sym->index;
        return sym->type;
      }
      if (sym->kind == SYM_NATIVE_CONST) {
        ti->ref_kinds[expr] = REF_NATIVE_CONST;
        ti->ref_index[expr] = sym->index;
        return sym->type;
      }
      _error(x.token, "'%.*s' is not a value, call it", x.token.length,
             x.token.start);
      return -1;
    }
  }

  int ot = _check_expr(x.left, -1);
  if (ot < 0) return -1;
  Type ty = ti->types[ot];
  if (ty.optional) {
    _error(x.token, "this is %s, it might be sone: check it with ber (x != sone) first",
           _name(ot).c_str());
    return -1;
  }
  if (ty.kind == TYPE_STRUCT || ty.kind == TYPE_CLASS) {
    int r = ti->decl_record[ty.decl];
    int fi = _find_field(r, x.token);
    if (fi < 0) {
      if (_find_method(r, x.token) >= 0)
        _error(x.token, "'%.*s' is a method, call it with ()", x.token.length,
               x.token.start);
      else
        _error(x.token, "%s has no field '%.*s'", _name(ot).c_str(),
               x.token.length, x.token.start);
      return -1;
    }
    const FieldInfo &f = ti->records[r].fields[fi];
    ti->ref_kinds[expr] = ty.kind == TYPE_STRUCT ? REF_STRUCT_FIELD : REF_CLASS_FIELD;
    ti->ref_index[expr] = f.offset;
    return f.type;
  }
  if (ty.kind == TYPE_ARRAY) {
    _error(x.token, "arrays only have .len() and .push(x)");
    return -1;
  }
  _error(x.token, "%s has no fields", _name(ot).c_str());
  return -1;
}

// can this be written through? reports why not
bool TypeChecker::_assignable(int expr) {
  const Expr &x = ast.exprs[expr];
  switch (x.kind) {
  case steav_ast::EXPR_GROUPING:
    return _assignable(x.right);
  case steav_ast::EXPR_VARIABLE:
    switch (ti->ref_kinds[expr]) {
    case REF_LOCAL:
    case REF_GLOBAL:
      return true;
    case REF_SELF_FIELD:
      if (ti->records[cur_record].is_class) return true;
      _error(x.token, "a sampoan method gets a copy of nis, it can't change "
                      "'%.*s' (mutate the struct from outside instead)",
             x.token.length, x.token.start);
      return false;
    default:
      _error(x.token, "can't assign to '%.*s'", x.token.length, x.token.start);
      return false;
    }
  case steav_ast::EXPR_THIS:
    if (ti->records[cur_record].is_class) return true;
    _error(x.token, "a sampoan method gets a copy of nis, it can't be changed");
    return false;
  case steav_ast::EXPR_GET:
    switch (ti->ref_kinds[expr]) {
    case REF_CLASS_FIELD:
    case REF_GLOBAL:
      return true;
    case REF_STRUCT_FIELD:
      return _assignable(x.left);
    default:
      _error(x.token, "can't assign to '%.*s'", x.token.length, x.token.start);
      return false;
    }
  case steav_ast::EXPR_INDEX:
    // writing v[i] writes into v, so v has to be writable too
    if (ti->ref_kinds[expr] == REF_STRUCT_INDEX) return _assignable(x.left);
    return true;
  default:
    _error(x.token, "can't assign into a temporary value, store it in an akthe first");
    return false;
  }
}

int TypeChecker::_check_assign(int expr) {
  const Expr &x = ast.exprs[expr];
  const Expr &target = ast.exprs[x.left];
  bool compound = x.token.type != steav_frontend::TOKEN_EQUAL;
  int tt;
  Narrow *narrow = nullptr;
  if (target.kind == steav_ast::EXPR_VARIABLE) {
    tt = _check_variable(x.left);
    /* `x += y` on a narrowed x writes a T into it, so it stays narrowed.
     * a plain `x = ...` can write sone, so that ends the narrowing */
    if (tt >= 0 && ti->narrowed[x.left] && !compound) {
      // writing the whole variable, it takes its declared T? again
      narrow = _find_narrow(ti->ref_kinds[x.left], ti->ref_index[x.left]);
      ti->narrowed[x.left] = 0;
      tt = ti->optional_of(tt);
      if (loop_depth > narrow->loop_depth) {
        _error(target.token,
               "can't assign to '%.*s' in a loop inside the check against sone, "
               "the next time around would read it as if it was still checked",
               target.token.length, target.token.start);
        tt = -1;
      }
    }
  } else {
    tt = _check_expr(x.left, -1);
  }
  ti->expr_types[x.left] = tt; // the compiler takes the write's width from here
  if (tt >= 0 && !_assignable(x.left)) tt = -1;
  if (tt < 0) {
    _check_expr(x.right, -1);
    return -1;
  }
  if (compound) return _check_compound(expr, tt);
  int vt = _expect(x.right, tt);
  // the value can still read the narrowed x (`x = x.next`), after that it's T?
  if (narrow != nullptr) narrow->killed = true;
  return vt >= 0 ? tt : -1;
}

static TokenType compound_op(TokenType t) {
  switch (t) {
  case steav_frontend::TOKEN_PLUS_EQUAL: return steav_frontend::TOKEN_PLUS;
  case steav_frontend::TOKEN_MINUS_EQUAL: return steav_frontend::TOKEN_MINUS;
  case steav_frontend::TOKEN_STAR_EQUAL: return steav_frontend::TOKEN_STAR;
  default: return steav_frontend::TOKEN_SLASH;
  }
}

/* `a op= b` is `a = a op b` with a evaluated once: same overload / builtin
 * lookup as the binary op, and it has to give back a's type */
int TypeChecker::_check_compound(int expr, int tt) {
  const Expr &x = ast.exprs[expr];
  TokenType op = compound_op(x.token.type);
  int hint = _is_untyped_literal(x.right) ? _overload_hint(op, tt, true) : -1;
  int rt = _check_expr(x.right, hint);
  if (rt < 0) return -1;

  int result = -1;
  int ov = _find_overload(op, 2, tt, rt);
  if (ov >= 0) {
    ti->expr_overload[expr] = ti->overloads[ov].decl;
    result = ti->fun_sigs[ti->overloads[ov].decl].ret;
  } else {
    Type a = ti->types[tt];
    if (tt == rt && !a.optional &&
        (is_number_kind(a.kind) ||
         (op == steav_frontend::TOKEN_PLUS && a.kind == TYPE_AHSOR)))
      result = tt;
  }
  if (result < 0) {
    _error(x.token, "no operator '%.*s' for %s and %s", x.token.length - 1,
           x.token.start, _name(tt).c_str(), _name(rt).c_str());
    return -1;
  }
  if (result != tt) {
    _error(x.token, "'%.*s' has to give back a %s, but this gives a %s",
           x.token.length, x.token.start, _name(tt).c_str(), _name(result).c_str());
    return -1;
  }
  return tt;
}

// the one type every field has, if it's a one slot type. -1 = not indexable
int TypeChecker::_struct_elem(int record) const {
  const Record &rec = ti->records[record];
  if (rec.fields.empty()) return -1;
  int t = rec.fields[0].type;
  for (const FieldInfo &f : rec.fields) {
    if (f.type != t || f.width != 1) return -1;
  }
  return t;
}

bool TypeChecker::_check_args(int expr, const std::vector<int> &params) {
  const Expr &x = ast.exprs[expr];
  bool ok = true;
  if (x.args.size() != params.size()) {
    _error(x.token, "expected %d argument%s but got %d", (int)params.size(),
           params.size() == 1 ? "" : "s", (int)x.args.size());
    ok = false;
  }
  for (size_t i = 0; i < x.args.size(); i++) {
    int want = (ok && params[i] >= 0) ? params[i] : -1;
    if (want >= 0) {
      if (_expect(x.args[i], want) < 0) ok = false;
    } else {
      _check_expr(x.args[i], -1);
    }
  }
  return ok;
}

int TypeChecker::_native_type(int kind) const {
  switch (kind) {
  case steav_stdlib::NATIVE_LEK_KUT: return t_lek_kut;
  case steav_stdlib::NATIVE_LEK_THOM: return t_lek_thom;
  default: return t_ort_mean;
  }
}

int TypeChecker::_call_symbol(int expr, const Symbol *sym) {
  switch (sym->kind) {
  case SYM_STRUCT: {
    const Record &rec = ti->records[sym->record];
    std::vector<int> params;
    for (const FieldInfo &f : rec.fields) params.push_back(f.type);
    ti->call_kinds[expr] = CALL_CONSTRUCT_STRUCT;
    ti->call_targets[expr] = sym->record;
    int type = rec.type;
    return _check_args(expr, params) ? type : -1;
  }
  case SYM_CLASS: {
    const Record &rec = ti->records[sym->record];
    std::vector<int> params;
    if (rec.init >= 0) params = ti->fun_sigs[rec.init].params;
    ti->call_kinds[expr] = CALL_CONSTRUCT_CLASS;
    ti->call_targets[expr] = sym->record;
    int type = rec.type;
    return _check_args(expr, params) ? type : -1;
  }
  case SYM_FUN: {
    ti->call_kinds[expr] = CALL_FUNCTION;
    ti->call_targets[expr] = sym->decl;
    FunSig sig = ti->fun_sigs[sym->decl];
    return _check_args(expr, sig.params) ? sig.ret : -1;
  }
  case SYM_NATIVE_FUN: {
    const NativeRef &ref = ti->natives[sym->index];
    const steav_stdlib::NativeFn &nf =
        steav_stdlib::module_at(ref.module)->functions[ref.index];
    std::vector<int> params;
    for (int i = 0; i < nf.arity; i++) params.push_back(_native_type(nf.params[i]));
    ti->call_kinds[expr] = CALL_NATIVE;
    ti->call_targets[expr] = sym->index;
    return _check_args(expr, params) ? _native_type(nf.ret) : -1;
  }
  default: {
    const Token &tok = ast.exprs[expr].token;
    _error(tok, "'%s' is a value, not something you can call", sym->name.c_str());
    return -1;
  }
  }
}

int TypeChecker::_check_call(int expr) {
  const Expr &x = ast.exprs[expr];
  const Expr &c = ast.exprs[x.left];

  if (c.kind == steav_ast::EXPR_VARIABLE) {
    const Token &tok = c.token;
    if (_find_local(tok) >= 0 ||
        (cur_record >= 0 && _find_field(cur_record, tok) >= 0)) {
      _error(tok, "'%.*s' is a variable, not a rupamun", tok.length, tok.start);
      _check_args(expr, std::vector<int>(x.args.size(), -1));
      return -1;
    }
    if (cur_record >= 0) {
      int mi = _find_method(cur_record, tok);
      if (mi >= 0) {
        if (ast.decls[mi].fun_name_kind == steav_ast::FUN_INIT) {
          _error(tok, "init only runs when the tnak is constructed");
          return -1;
        }
        ti->call_kinds[expr] = CALL_METHOD_IMPLICIT;
        ti->call_targets[expr] = mi;
        FunSig sig = ti->fun_sigs[mi];
        return _check_args(expr, sig.params) ? sig.ret : -1;
      }
    }
    // LekThom(i), LekKut(x), LekThomKlang(i)
    int bt = _builtin_type(tok);
    if (bt >= 0) {
      if (!is_number_kind(ti->types[bt].kind)) {
        _error(tok, "only number types can be called to convert");
        return -1;
      }
      if (x.args.size() != 1) {
        _error(x.token, "a conversion takes exactly one number");
        return -1;
      }
      int at = _check_expr(x.args[0], -1);
      if (at < 0) return -1;
      if (!is_number_kind(ti->types[at].kind) || ti->types[at].optional) {
        _error(x.token, "can only convert numbers, this is %s", _name(at).c_str());
        return -1;
      }
      ti->call_kinds[expr] = CALL_CAST;
      ti->call_targets[expr] = ti->types[bt].kind;
      return bt;
    }
    const Symbol *sym = _lookup(cur_module, tok);
    if (sym == nullptr) {
      _error(tok, "undefined name '%.*s'", tok.length, tok.start);
      _check_args(expr, std::vector<int>(x.args.size(), -1));
      return -1;
    }
    return _call_symbol(expr, sym);
  }

  if (c.kind == steav_ast::EXPR_GET) {
    const Expr &o = ast.exprs[c.left];
    if (o.kind == steav_ast::EXPR_VARIABLE && _find_local(o.token) < 0) {
      int alias = _lookup_alias(cur_module, o.token);
      if (alias >= 0) {
        ti->ref_kinds[c.left] = REF_MODULE;
        ti->ref_index[c.left] = alias;
        const Symbol *sym = _lookup_own(alias, c.token.start, c.token.length);
        if (sym == nullptr) {
          _error(c.token, "module '%.*s' has no '%.*s'", o.token.length,
                 o.token.start, c.token.length, c.token.start);
          return -1;
        }
        return _call_symbol(expr, sym);
      }
    }

    int ot = _check_expr(c.left, -1);
    if (ot < 0) return -1;
    Type ty = ti->types[ot];
    if (ty.optional) {
      _error(c.token, "this is %s, it might be sone: check it with ber (x != sone) first",
             _name(ot).c_str());
      return -1;
    }
    if (ty.kind == TYPE_STRUCT || ty.kind == TYPE_CLASS) {
      int r = ti->decl_record[ty.decl];
      int mi = _find_method(r, c.token);
      if (mi < 0) {
        _error(c.token, "%s has no method '%.*s'", _name(ot).c_str(),
               c.token.length, c.token.start);
        return -1;
      }
      if (ast.decls[mi].fun_name_kind == steav_ast::FUN_INIT) {
        _error(c.token, "init only runs when the tnak is constructed");
        return -1;
      }
      ti->call_kinds[expr] = CALL_METHOD;
      ti->call_targets[expr] = mi;
      FunSig sig = ti->fun_sigs[mi];
      return _check_args(expr, sig.params) ? sig.ret : -1;
    }
    if (ty.kind == TYPE_ARRAY) {
      if (tok_is(c.token, "len")) {
        ti->call_kinds[expr] = CALL_ARRAY_LEN;
        return _check_args(expr, std::vector<int>()) ? t_lek_kut : -1;
      }
      if (tok_is(c.token, "push")) {
        ti->call_kinds[expr] = CALL_ARRAY_PUSH;
        return _check_args(expr, std::vector<int>(1, ty.elem)) ? t_ort_mean : -1;
      }
      _error(c.token, "arrays only have .len() and .push(x)");
      return -1;
    }
    _error(c.token, "%s has no methods", _name(ot).c_str());
    return -1;
  }

  _error(x.token, "can only call a rupamun, sampoan or tnak by name");
  return -1;
}

int TypeChecker::_check_array(int expr, int expected) {
  const Expr &x = ast.exprs[expr];
  int want_elem = -1;
  if (expected >= 0) {
    Type et = ti->types[ti->base(expected)];
    if (et.kind == TYPE_ARRAY) want_elem = et.elem;
  }
  if (x.args.empty()) {
    if (want_elem < 0) {
      _error(x.token, "can't tell what [] holds, give it a type: akthe xs: [T] = [];");
      return -1;
    }
    return ti->intern(make_array_type(want_elem));
  }
  int elem = want_elem;
  size_t start = 0;
  if (elem < 0) {
    elem = _check_expr(x.args[0], -1);
    if (elem < 0) return -1;
    if (ti->types[elem].kind == TYPE_NONE || ti->types[elem].kind == TYPE_ORT_MEAN) {
      _error(x.token, "can't tell what this array holds, give it a type");
      return -1;
    }
    start = 1;
  }
  bool ok = true;
  for (size_t i = start; i < x.args.size(); i++) {
    if (_expect(x.args[i], elem) < 0) ok = false;
  }
  return ok ? ti->intern(make_array_type(elem)) : -1;
}

/* ------------------------------------------------------------------- names */

int TypeChecker::_builtin_type(const Token &name) const {
  if (tok_is(name, "Lek") || tok_is(name, "LekThom")) return t_lek_thom;
  if (tok_is(name, "LekKut")) return t_lek_kut;
  if (tok_is(name, "LekThomKlang")) return t_lek_thom_klang;
  if (tok_is(name, "Ahsor")) return t_ahsor;
  if (tok_is(name, "Boolean")) return t_boolean;
  if (tok_is(name, "OrtMean")) return t_ort_mean;
  return -1;
}

int TypeChecker::_resolve_type_ref(int module, int type_ref) {
  if (type_ref < 0) return -1;
  const TypeRef &tr = ast.types[type_ref];
  if (tr.kind == steav_ast::TYPEREF_ARRAY) {
    int elem = _resolve_type_ref(module, tr.elem);
    if (elem < 0) return -1;
    return ti->intern(make_array_type(elem, tr.optional));
  }

  const Symbol *sym = nullptr;
  if (tr.has_qualifier) {
    int alias = _lookup_alias(module, tr.qualifier);
    if (alias < 0) {
      _error(tr.qualifier, "'%.*s' is not a module alias", tr.qualifier.length,
             tr.qualifier.start);
      return -1;
    }
    sym = _lookup_own(alias, tr.name.start, tr.name.length);
  } else {
    int bt = _builtin_type(tr.name);
    if (bt >= 0) return tr.optional ? ti->optional_of(bt) : bt;
    sym = _lookup(module, tr.name);
  }
  if (sym == nullptr || (sym->kind != SYM_STRUCT && sym->kind != SYM_CLASS)) {
    _error(tr.name, "unknown type '%.*s'", tr.name.length, tr.name.start);
    return -1;
  }
  int t = ti->records[sym->record].type;
  return tr.optional ? ti->optional_of(t) : t;
}

const Symbol *TypeChecker::_lookup_own(int module, const char *name,
                                       int length) const {
  for (const Symbol &s : ti->scopes[module].symbols) {
    if ((int)s.name.size() == length && s.name.compare(0, length, name, length) == 0)
      return &s;
  }
  return nullptr;
}

// own names first, then everything `yok`ed bare (not transitively)
const Symbol *TypeChecker::_lookup(int module, const Token &name) const {
  const Symbol *s = _lookup_own(module, name.start, name.length);
  if (s != nullptr) return s;
  for (int b : ti->scopes[module].bare_imports) {
    s = _lookup_own(b, name.start, name.length);
    if (s != nullptr) return s;
  }
  return nullptr;
}

int TypeChecker::_lookup_alias(int module, const Token &name) const {
  const ModuleScope &scope = ti->scopes[module];
  for (size_t i = 0; i < scope.alias_names.size(); i++) {
    const std::string &a = scope.alias_names[i];
    if ((int)a.size() == name.length && a.compare(0, name.length, name.start, name.length) == 0)
      return scope.alias_modules[i];
  }
  return -1;
}

bool TypeChecker::_declare_symbol(int module, const Symbol &symbol, const Token &at) {
  if (_lookup_own(module, symbol.name.c_str(), (int)symbol.name.size()) != nullptr) {
    _error(at, "'%s' is already declared (sampoan, tnak, rupamun and top level "
               "akthe share one namespace)",
           symbol.name.c_str());
    return false;
  }
  if (symbol.kind == SYM_GLOBAL) {
    for (int b : ti->scopes[module].bare_imports) {
      if (_lookup_own(b, symbol.name.c_str(), (int)symbol.name.size()) != nullptr) {
        _error(at, "'%s' is already imported from '%s'", symbol.name.c_str(),
               ast.modules[b].name.c_str());
        return false;
      }
    }
  }
  ti->scopes[module].symbols.push_back(symbol);
  return true;
}

int TypeChecker::_find_local(const Token &name) const {
  for (int i = (int)locals.size() - 1; i >= 0; i--) {
    if (tok_eq(locals[i].name, name)) return i;
  }
  return -1;
}

int TypeChecker::_find_field(int record, const Token &name) const {
  const std::vector<FieldInfo> &fields = ti->records[record].fields;
  for (int i = 0; i < (int)fields.size(); i++) {
    if (tok_eq(fields[i].name, name)) return i;
  }
  return -1;
}

// returns the method's decl
int TypeChecker::_find_method(int record, const Token &name) const {
  const Decl &decl = ast.decls[ti->records[record].decl];
  for (int m : decl.methods) {
    if (tok_eq(ast.decls[m].name, name)) return m;
  }
  return -1;
}

int TypeChecker::_find_overload(TokenType op, int arity, int lhs, int rhs) const {
  for (int i = 0; i < (int)ti->overloads.size(); i++) {
    const OperatorOverload &ov = ti->overloads[i];
    if (ov.op == op && ov.arity == arity && ov.lhs == lhs && ov.rhs == rhs)
      return i;
  }
  return -1;
}

void TypeChecker::_declare_local(int d, int type) {
  const Decl &decl = ast.decls[d];
  for (int i = (int)locals.size() - 1; i >= 0 && locals[i].depth == scope_depth; i--) {
    if (tok_eq(locals[i].name, decl.name)) {
      _error(decl.name, "'%.*s' is already declared in this scope",
             decl.name.length, decl.name.start);
      break;
    }
  }
  LocalVar l;
  l.name = decl.name;
  l.type = type;
  l.slot = slot_top;
  l.depth = scope_depth;
  locals.push_back(l);
  ti->decl_slot[d] = slot_top;
  this->slot_top += ti->width(type);
  if (slot_top > 255) _error(decl.name, "too many local slots in one rupamun (max 255)");
}

bool TypeChecker::_is_narrowed(RefKind kind, int index) const {
  for (const Narrow &n : narrows) {
    if (!n.killed && n.global == (kind == REF_GLOBAL) && n.index == index) return true;
  }
  return false;
}

TypeChecker::Narrow *TypeChecker::_find_narrow(RefKind kind, int index) {
  for (int i = (int)narrows.size() - 1; i >= 0; i--) {
    Narrow &n = narrows[i];
    if (!n.killed && n.global == (kind == REF_GLOBAL) && n.index == index) return &n;
  }
  return nullptr;
}

/* is `cond` a (checked) `x != sone` / `x == sone` on a local or global?
 * when_true: the narrowing holds when cond is ok (!=), otherwise when ort */
bool TypeChecker::_narrowing(int cond, Narrow &narrow, bool &when_true) const {
  int c = _strip_grouping(cond);
  if (ast.exprs[c].kind != steav_ast::EXPR_BINARY || ti->none_side[c] == 0)
    return false;
  int var = _strip_grouping(ti->none_side[c] == 1 ? ast.exprs[c].left
                                                  : ast.exprs[c].right);
  RefKind k = ti->ref_kinds[var];
  if (ast.exprs[var].kind != steav_ast::EXPR_VARIABLE ||
      (k != REF_LOCAL && k != REF_GLOBAL))
    return false;
  when_true = ast.exprs[c].token.type == steav_frontend::TOKEN_BANG_EQUAL;
  narrow.global = k == REF_GLOBAL;
  narrow.index = ti->ref_index[var];
  narrow.loop_depth = loop_depth;
  narrow.killed = false;
  return true;
}

void TypeChecker::_begin_scope() { this->scope_depth++; }

void TypeChecker::_end_scope() {
  this->scope_depth--;
  while (!locals.empty() && locals.back().depth > scope_depth) {
    this->slot_top -= ti->width(locals.back().type);
    locals.pop_back();
  }
}

void TypeChecker::_error(const Token &at, const char *fmt, ...) {
  this->had_error = true;
  char msg[512];
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);
  if (steav_capture_diagnostic(at.line, at.start, at.length, msg)) return;
  char type_err_str[768];
  snprintf(type_err_str, sizeof(type_err_str), "[line %d] Error at '%.*s': %s",
           at.line, at.length, at.start, msg);
  STEAV_LOGGING_LOG(type_err_str, STEAV_LOGGING_COMPILE_ERROR);
}

} // namespace steav_typecheck
