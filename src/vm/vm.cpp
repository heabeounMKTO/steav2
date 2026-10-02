#include "vm/vm.h"
#include "compiler/compiler.h"
#include "frontend/parser.h"
#include "frontend/scanner.h"
#include "logging/steav_diagnostics.h"
#include "stdlib/module.h"
#include "typecheck/type_checker.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>

#include "debug/disassembler.h"

namespace steav_vm {

/* ----------------------------------------------------------------- modules */

static bool parse_into(steav_ast::Ast &ast, int module, const char *source) {
  std::vector<steav_frontend::Token> tokens;
  steav_frontend::Scanner scanner(source);
  bool ok = scanner.scan_tokens(tokens) == STEAV_LOGGING_OK;
  steav_frontend::Parser parser(tokens, module, module == 0);
  return parser.parse(ast) == STEAV_LOGGING_OK && ok;
}

/* every `yok` at the top level of a module pulls that module into the same
 * Ast (once), .sts modules get parsed, native ones just get an entry */
static bool load_imports(steav_ast::Ast &ast, int module) {
  bool ok = true;
  std::vector<int> program = ast.modules[module].program;
  for (int d : program) {
    const steav_ast::Decl &decl = ast.decls[d];
    if (decl.kind != steav_ast::DECL_STMT) continue;
    const steav_ast::Stmt &s = ast.stmts[decl.stmt];
    if (s.kind != steav_ast::STMT_IMPORT) continue;
    std::string resolved = ast.resolve_import(module, s.name);
    if (ast.find_module(resolved.c_str(), (int)resolved.size()) >= 0) continue;

    if (s.name.type == steav_frontend::TOKEN_STRING) {
      std::ifstream file(resolved.c_str());
      std::stringstream buf;
      if (!file || !(buf << file.rdbuf())) {
        std::string msg = "couldn't open \"" + resolved + "\"";
        if (steav_capture_diagnostic(s.name.line, s.name.start, s.name.length, msg)) {
          ok = false;
          continue;
        }
        char import_err_str[512];
        snprintf(import_err_str, sizeof(import_err_str),
                 "[line %d] Error at '%.*s': couldn't open \"%s\"", s.name.line,
                 s.name.length, s.name.start, resolved.c_str());
        STEAV_LOGGING_LOG(import_err_str, STEAV_LOGGING_COMPILE_ERROR);
        ok = false;
        continue;
      }
      ast.file_sources.push_back(buf.str());
      steav_ast::Module m;
      m.name = resolved;
      m.is_file = true;
      m.dir = steav_ast::path_dirname(resolved);
      ast.modules.push_back(m);
      int idx = (int)ast.modules.size() - 1;
      const char *text = ast.file_sources.back().c_str();
      if (!parse_into(ast, idx, text)) ok = false;
      else if (!load_imports(ast, idx)) ok = false;
      continue;
    }

    int si = steav_stdlib::find_module(s.name.start, s.name.length);
    if (si < 0) {
      ok = false;
      std::string msg = "no module named '" + std::string(s.name.start, s.name.length) + "'";
      if (steav_capture_diagnostic(s.name.line, s.name.start, s.name.length, msg)) continue;
      char import_err_str[512];
      snprintf(import_err_str, sizeof(import_err_str),
               "[line %d] Error at '%.*s': no module named '%.*s'", s.name.line,
               s.name.length, s.name.start, s.name.length, s.name.start);
      STEAV_LOGGING_LOG(import_err_str, STEAV_LOGGING_COMPILE_ERROR);
      ok = false;
      continue;
    }
    const steav_stdlib::Module *sm = steav_stdlib::module_at(si);
    steav_ast::Module m;
    m.name = sm->name;
    m.is_native = sm->source == nullptr;
    m.is_stdlib = true;
    ast.modules.push_back(m);
    int idx = (int)ast.modules.size() - 1;
    if (!m.is_native) {
      if (!parse_into(ast, idx, sm->source)) ok = false;
      else if (!load_imports(ast, idx)) ok = false;
    }
  }
  return ok;
}

// dependencies first, so a module's globals exist before anyone uses them
static bool order_modules(steav_ast::Ast &ast, int module, std::vector<int> &state) {
  if (state[module] == 2) return true;
  if (state[module] == 1) {
    std::string msg = "import cycle through '" + ast.modules[module].name + "'";
    if (steav_capture_diagnostic(0, nullptr, 0, msg)) return false;
    char cycle_err_str[512];
    snprintf(cycle_err_str, sizeof(cycle_err_str), "import cycle through '%s'",
             ast.modules[module].name.c_str());
    STEAV_LOGGING_LOG(cycle_err_str, STEAV_LOGGING_COMPILE_ERROR);
    return false;
  }
  state[module] = 1;
  for (int d : ast.modules[module].program) {
    const steav_ast::Decl &decl = ast.decls[d];
    if (decl.kind != steav_ast::DECL_STMT) continue;
    const steav_ast::Stmt &s = ast.stmts[decl.stmt];
    if (s.kind != steav_ast::STMT_IMPORT) continue;
    std::string resolved = ast.resolve_import(module, s.name);
    int dep = ast.find_module(resolved.c_str(), (int)resolved.size());
    if (dep >= 0 && !order_modules(ast, dep, state)) return false;
  }
  state[module] = 2;
  ast.module_order.push_back(module);
  return true;
}

// source -> tokens -> ast (+ imported modules) -> type checked
static SteavStatus frontend(const char *source, const char *path, steav_ast::Ast &ast,
                            steav_typecheck::TypeInfo &info) {
  steav_ast::Module main_module;
  if (path != nullptr) {
    main_module.name = steav_ast::path_normalize(path);
    main_module.dir = steav_ast::path_dirname(main_module.name);
  } else {
    main_module.name = "<main>";
    main_module.dir = ".";
  }
  ast.modules.push_back(main_module);
  bool parsed = parse_into(ast, 0, source);
  // keep going on a parse error, the imports can still have their own
  if (!load_imports(ast, 0) || !parsed) return STEAV_LOGGING_COMPILE_ERROR;
  std::vector<int> state(ast.modules.size(), 0);
  if (!order_modules(ast, 0, state)) return STEAV_LOGGING_COMPILE_ERROR;

  /* type errors get caught here, before anything runs (not mid-render) */
  steav_typecheck::TypeChecker checker(ast);
  return checker.check(info);
}

SteavStatus check_source(const char *source, const char *path) {
  steav_ast::Ast ast;
  steav_typecheck::TypeInfo info;
  return frontend(source, path, ast, info);
}

SteavStatus VM::interpret(const char *source, const char *path) {
  steav_ast::Ast ast;
  steav_typecheck::TypeInfo info;
  if (frontend(source, path, ast, info) != STEAV_LOGGING_OK)
    return STEAV_LOGGING_COMPILE_ERROR;

  ObjFunction *script = nullptr;
  steav_compiler::Compiler compiler(ast, info, heap);
  if (compiler.compile(script, roots) != STEAV_LOGGING_OK)
    return STEAV_LOGGING_COMPILE_ERROR;

  globals.assign(info.global_slots, make_nil());
  heap.vm = this;
  reset_stack();
  _call(script, 0);
  SteavStatus status = _run();
  heap.vm = nullptr;
  return status;
}

SteavStatus VM::disassemble(const char *source, const char *path) {
  steav_ast::Ast ast;
  steav_typecheck::TypeInfo info;
  if (frontend(source, path, ast, info) != STEAV_LOGGING_OK)
    return STEAV_LOGGING_COMPILE_ERROR;

  ObjFunction *script = nullptr;
  steav_compiler::Compiler compiler(ast, info, heap);
  if (compiler.compile(script, roots) != STEAV_LOGGING_OK)
    return STEAV_LOGGING_COMPILE_ERROR;

  for (ObjFunction *fn : roots)
    steav_debug::disassemble_chunk(fn->chunk, fn->name.c_str());
  return STEAV_LOGGING_OK;
}

/* --------------------------------------------------------------------- run */

bool VM::_call(ObjFunction *function, int arg_slots) {
  if (frame_count == STEAV_VM_FRAMES_MAX ||
      stack_top - stack + 256 > STEAV_VM_STACK_MAX)
    return false;
  CallFrame &frame = frames[frame_count++];
  frame.function = function;
  frame.ip = function->chunk.code.data();
  frame.slots = stack_top - arg_slots;
  return true;
}

SteavStatus VM::_runtime_error(const char *message) {
  CallFrame &top = frames[frame_count - 1];
  size_t at = top.ip - top.function->chunk.code.data() - 1;
  char runtime_err_str[1024];
  int n = snprintf(runtime_err_str, sizeof(runtime_err_str),
                   "[line %d] runtime error: %s", top.function->chunk.lines[at],
                   message);
  for (int i = frame_count - 1; i >= 0 && n < (int)sizeof(runtime_err_str); i--) {
    CallFrame &f = frames[i];
    size_t fat = f.ip - f.function->chunk.code.data() - 1;
    n += snprintf(runtime_err_str + n, sizeof(runtime_err_str) - n,
                  "\n      in %s (line %d)", f.function->name.c_str(),
                  f.function->chunk.lines[fat]);
  }
  STEAV_LOGGING_LOG(runtime_err_str, STEAV_LOGGING_RUNTIME_ERROR);
  reset_stack();
  return STEAV_LOGGING_RUNTIME_ERROR;
}

static inline bool values_equal(const Value &a, const Value &b) {
  switch (a.type) {
  case VAL_NIL: return b.type == VAL_NIL;
  case VAL_BOOL: return a.boolean == b.boolean;
  case VAL_LEK_KUT: return a.lek_kut == b.lek_kut;
  case VAL_LEK_THOM: return a.lek_thom == b.lek_thom;
  case VAL_LEK_THOM_KLANG: return a.lek_thom_klang == b.lek_thom_klang;
  case VAL_OBJ:
    if (a.obj->type == OBJ_STRING && b.obj->type == OBJ_STRING)
      return ((ObjString *)a.obj)->chars == ((ObjString *)b.obj)->chars;
    return a.obj == b.obj;
  }
  return false;
}

static inline double as_double(const Value &v) {
  switch (v.type) {
  case VAL_LEK_KUT: return (double)v.lek_kut;
  case VAL_LEK_THOM_KLANG: return (double)v.lek_thom_klang;
  default: return v.lek_thom;
  }
}

static inline int64_t as_int64(const Value &v) {
  switch (v.type) {
  case VAL_LEK_KUT: return v.lek_kut;
  case VAL_LEK_THOM_KLANG: return v.lek_thom_klang;
  default:
    if (std::isnan(v.lek_thom)) return 0;
    if (v.lek_thom >= 9.2e18) return INT64_MAX;
    if (v.lek_thom <= -9.2e18) return INT64_MIN;
    return (int64_t)v.lek_thom; // truncates toward zero
  }
}

static int struct_width(const ObjStructType *desc) {
  int w = 0;
  for (const StructFieldDesc &f : desc->fields) w += f.width;
  return w;
}

SteavStatus VM::_run() {
  CallFrame *frame = &frames[frame_count - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_U16() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->function->chunk.constants[READ_U16()])

/* the checker proved both sides have the same number type, the tag only
 * says which union member. LekKut wraps (two's complement) instead of UB */
#define BINARY_ARITH(op)                                                       \
  do {                                                                         \
    Value b = pop();                                                           \
    Value &a = stack_top[-1];                                                  \
    switch (a.type) {                                                          \
    case VAL_LEK_KUT:                                                          \
      a.lek_kut = (int32_t)((uint32_t)a.lek_kut op(uint32_t) b.lek_kut);       \
      break;                                                                   \
    case VAL_LEK_THOM_KLANG:                                                   \
      a.lek_thom_klang =                                                       \
          (int64_t)((uint64_t)a.lek_thom_klang op(uint64_t) b.lek_thom_klang); \
      break;                                                                   \
    default:                                                                   \
      a.lek_thom = a.lek_thom op b.lek_thom;                                   \
      break;                                                                   \
    }                                                                          \
  } while (0)

#define BINARY_COMPARE(op)                                                     \
  do {                                                                         \
    Value b = pop();                                                           \
    Value a = pop();                                                           \
    bool r;                                                                    \
    switch (a.type) {                                                          \
    case VAL_LEK_KUT: r = a.lek_kut op b.lek_kut; break;                       \
    case VAL_LEK_THOM_KLANG: r = a.lek_thom_klang op b.lek_thom_klang; break;  \
    default: r = a.lek_thom op b.lek_thom; break;                              \
    }                                                                          \
    push(make_bool(r));                                                        \
  } while (0)

  while (true) {
#ifdef STEAV_DEBUG_TRACE
    printf("          ");
    for (Value *slot = stack; slot < stack_top; slot++) {
      printf("[ ");
      slot->print();
      printf(" ]");
    }
    printf("\n");
    steav_debug::disassemble_instruction(
        frame->function->chunk,
        (int)(frame->ip - frame->function->chunk.code.data()));
#endif
    uint8_t instruction = READ_BYTE();
    switch (instruction) {
    case OP_CONSTANT:
      push(READ_CONSTANT());
      break;
    case OP_NIL:
      push(make_nil());
      break;
    case OP_NIL_N: {
      int n = READ_BYTE();
      for (int i = 0; i < n; i++) push(make_nil());
      break;
    }
    case OP_TRUE:
      push(make_bool(true));
      break;
    case OP_FALSE:
      push(make_bool(false));
      break;
    case OP_POPN:
      this->stack_top -= READ_BYTE();
      break;
    case OP_DUP: {
      int n = READ_BYTE();
      memcpy(stack_top, stack_top - n, n * sizeof(Value));
      this->stack_top += n;
      break;
    }

    case OP_GET_LOCAL: {
      int slot = READ_BYTE();
      int w = READ_BYTE();
      memcpy(stack_top, frame->slots + slot, w * sizeof(Value));
      this->stack_top += w;
      break;
    }
    case OP_SET_LOCAL: {
      int slot = READ_BYTE();
      int w = READ_BYTE();
      memmove(frame->slots + slot, stack_top - w, w * sizeof(Value));
      break;
    }
    case OP_GET_GLOBAL: {
      int idx = READ_U16();
      int w = READ_BYTE();
      memcpy(stack_top, &globals[idx], w * sizeof(Value));
      this->stack_top += w;
      break;
    }
    case OP_SET_GLOBAL: {
      int idx = READ_U16();
      int w = READ_BYTE();
      memcpy(&globals[idx], stack_top - w, w * sizeof(Value));
      break;
    }

    case OP_STRUCT_FIELD: {
      int off = READ_BYTE();
      int w = READ_BYTE();
      int total = READ_BYTE();
      Value *base = stack_top - total;
      memmove(base, base + off, w * sizeof(Value));
      this->stack_top = base + w;
      break;
    }
    case OP_GET_FIELD: {
      int off = READ_BYTE();
      int w = READ_BYTE();
      ObjInstance *instance = (ObjInstance *)pop().obj;
      memcpy(stack_top, &instance->fields[off], w * sizeof(Value));
      this->stack_top += w;
      break;
    }
    case OP_SET_FIELD: {
      int off = READ_BYTE();
      int w = READ_BYTE();
      Value *vals = stack_top - w;
      ObjInstance *instance = (ObjInstance *)vals[-1].obj;
      memcpy(&instance->fields[off], vals, w * sizeof(Value));
      memmove(vals - 1, vals, w * sizeof(Value));
      this->stack_top--;
      break;
    }
    case OP_GET_INDEX: {
      int off = READ_BYTE();
      int w = READ_BYTE();
      int32_t idx = pop().lek_kut;
      ObjArray *array = (ObjArray *)pop().obj;
      int32_t count = (int32_t)(array->items.size() / array->elem_width);
      if (idx < 0 || idx >= count) {
        char index_err_str[128];
        snprintf(index_err_str, sizeof(index_err_str),
                 "index %d is out of bounds, the array has %d items", idx, count);
        return _runtime_error(index_err_str);
      }
      memcpy(stack_top, &array->items[(size_t)idx * array->elem_width + off],
             w * sizeof(Value));
      this->stack_top += w;
      break;
    }
    case OP_SET_INDEX: {
      int off = READ_BYTE();
      int w = READ_BYTE();
      Value *vals = stack_top - w;
      int32_t idx = vals[-1].lek_kut;
      ObjArray *array = (ObjArray *)vals[-2].obj;
      int32_t count = (int32_t)(array->items.size() / array->elem_width);
      if (idx < 0 || idx >= count) {
        char index_err_str[128];
        snprintf(index_err_str, sizeof(index_err_str),
                 "index %d is out of bounds, the array has %d items", idx, count);
        return _runtime_error(index_err_str);
      }
      memcpy(&array->items[(size_t)idx * array->elem_width + off], vals,
             w * sizeof(Value));
      memmove(vals - 2, vals, w * sizeof(Value));
      this->stack_top -= 2;
      break;
    }

#define READ_FIELD_INDEX(n)                                                    \
  int32_t at = pop().lek_kut;                                                  \
  if (at < 0 || at >= (n)) {                                                   \
    char at_err_str[128];                                                      \
    snprintf(at_err_str, sizeof(at_err_str),                                   \
             "index %d is out of bounds, the sampoan has %d fields", at, (n)); \
    return _runtime_error(at_err_str);                                         \
  }
    case OP_GET_LOCAL_AT: {
      int slot = READ_BYTE();
      int n = READ_BYTE();
      READ_FIELD_INDEX(n);
      push(frame->slots[slot + at]);
      break;
    }
    case OP_SET_LOCAL_AT: {
      int slot = READ_BYTE();
      int n = READ_BYTE();
      Value v = pop();
      READ_FIELD_INDEX(n);
      frame->slots[slot + at] = v;
      push(v);
      break;
    }
    case OP_GET_GLOBAL_AT: {
      int idx = READ_U16();
      int n = READ_BYTE();
      READ_FIELD_INDEX(n);
      push(globals[idx + at]);
      break;
    }
    case OP_SET_GLOBAL_AT: {
      int idx = READ_U16();
      int n = READ_BYTE();
      Value v = pop();
      READ_FIELD_INDEX(n);
      globals[idx + at] = v;
      push(v);
      break;
    }
    case OP_GET_FIELD_AT: {
      int off = READ_BYTE();
      int n = READ_BYTE();
      READ_FIELD_INDEX(n);
      ObjInstance *instance = (ObjInstance *)pop().obj;
      push(instance->fields[off + at]);
      break;
    }
    case OP_SET_FIELD_AT: {
      int off = READ_BYTE();
      int n = READ_BYTE();
      Value v = pop();
      READ_FIELD_INDEX(n);
      ObjInstance *instance = (ObjInstance *)pop().obj;
      instance->fields[off + at] = v;
      push(v);
      break;
    }
    case OP_GET_INDEX_AT:
    case OP_SET_INDEX_AT: {
      bool set = instruction == OP_SET_INDEX_AT;
      int off = READ_BYTE();
      int n = READ_BYTE();
      Value v = set ? pop() : make_nil();
      READ_FIELD_INDEX(n);
      int32_t idx = pop().lek_kut;
      ObjArray *array = (ObjArray *)pop().obj;
      int32_t count = (int32_t)(array->items.size() / array->elem_width);
      if (idx < 0 || idx >= count) {
        char index_err_str[128];
        snprintf(index_err_str, sizeof(index_err_str),
                 "index %d is out of bounds, the array has %d items", idx, count);
        return _runtime_error(index_err_str);
      }
      Value &slot = array->items[(size_t)idx * array->elem_width + off + at];
      if (set) slot = v;
      push(slot);
      break;
    }
    case OP_STRUCT_AT: {
      int n = READ_BYTE();
      READ_FIELD_INDEX(n);
      Value v = stack_top[-n + at];
      this->stack_top -= n;
      push(v);
      break;
    }
#undef READ_FIELD_INDEX

    case OP_ADD: {
      Value &a = stack_top[-2];
      if (a.type == VAL_OBJ) {
        // Ahsor + Ahsor, both stay on the stack while allocating (gc)
        ObjString *s = heap.allocate<ObjString>(OBJ_STRING);
        s->chars = ((ObjString *)stack_top[-2].obj)->chars +
                   ((ObjString *)stack_top[-1].obj)->chars;
        this->stack_top -= 2;
        push(make_obj(s));
        heap.charge(s, s->chars.size());
        break;
      }
      BINARY_ARITH(+);
      break;
    }
    case OP_SUBTRACT:
      BINARY_ARITH(-);
      break;
    case OP_MULTIPLY:
      BINARY_ARITH(*);
      break;
    case OP_DIVIDE: {
      Value b = pop();
      Value &a = stack_top[-1];
      switch (a.type) {
      case VAL_LEK_KUT:
        if (b.lek_kut == 0) return _runtime_error("LekKut division by zero");
        a.lek_kut = b.lek_kut == -1 ? (int32_t)(0u - (uint32_t)a.lek_kut)
                                    : a.lek_kut / b.lek_kut;
        break;
      case VAL_LEK_THOM_KLANG:
        if (b.lek_thom_klang == 0)
          return _runtime_error("LekThomKlang division by zero");
        a.lek_thom_klang = b.lek_thom_klang == -1
                               ? (int64_t)(0ull - (uint64_t)a.lek_thom_klang)
                               : a.lek_thom_klang / b.lek_thom_klang;
        break;
      default:
        a.lek_thom = a.lek_thom / b.lek_thom;
        break;
      }
      break;
    }
    case OP_NEGATE: {
      Value &a = stack_top[-1];
      switch (a.type) {
      case VAL_LEK_KUT: a.lek_kut = (int32_t)(0u - (uint32_t)a.lek_kut); break;
      case VAL_LEK_THOM_KLANG:
        a.lek_thom_klang = (int64_t)(0ull - (uint64_t)a.lek_thom_klang);
        break;
      default: a.lek_thom = -a.lek_thom; break;
      }
      break;
    }
    case OP_NOT:
      stack_top[-1].boolean = !stack_top[-1].boolean;
      break;
    case OP_EQUAL: {
      Value b = pop();
      Value a = pop();
      push(make_bool(values_equal(a, b)));
      break;
    }
    case OP_GREATER:
      BINARY_COMPARE(>);
      break;
    case OP_GREATER_EQUAL:
      BINARY_COMPARE(>=);
      break;
    case OP_LESS:
      BINARY_COMPARE(<);
      break;
    case OP_LESS_EQUAL:
      BINARY_COMPARE(<=);
      break;
    case OP_CONVERT: {
      ValueType to = (ValueType)READ_BYTE();
      Value &a = stack_top[-1];
      if (to == VAL_LEK_THOM) {
        a = make_lek_thom(as_double(a));
      } else if (to == VAL_LEK_THOM_KLANG) {
        a = make_lek_thom_klang(as_int64(a));
      } else {
        int64_t n = as_int64(a);
        if (n > INT32_MAX) n = INT32_MAX;
        if (n < INT32_MIN) n = INT32_MIN;
        a = make_lek_kut((int32_t)n);
      }
      break;
    }

    case OP_IS_NONE: {
      int w = READ_BYTE();
      bool none = stack_top[-w].type == VAL_NIL;
      this->stack_top -= w;
      push(make_bool(none));
      break;
    }
    case OP_WRAP_SOME: {
      int w = READ_BYTE();
      Value *vals = stack_top - w;
      memmove(vals + 1, vals, w * sizeof(Value));
      vals[0] = make_bool(true);
      this->stack_top++;
      break;
    }

    case OP_JUMP: {
      uint16_t offset = READ_U16();
      frame->ip += offset;
      break;
    }
    case OP_JUMP_IF_FALSE: {
      uint16_t offset = READ_U16();
      if (!stack_top[-1].boolean) frame->ip += offset;
      break;
    }
    case OP_LOOP: {
      uint16_t offset = READ_U16();
      frame->ip -= offset;
      break;
    }

    case OP_CALL: {
      ObjFunction *function = (ObjFunction *)READ_CONSTANT().obj;
      int arg_slots = READ_BYTE();
      if (function->native != nullptr) {
        // C++ rupamun (vector & co): no frame, args in, result slots out
        Value *args = stack_top - arg_slots;
        function->native(args, native_out);
        memcpy(args, native_out, function->ret_slots * sizeof(Value));
        this->stack_top = args + function->ret_slots;
        break;
      }
      if (!_call(function, arg_slots)) return _runtime_error("stack overflow");
      frame = &frames[frame_count - 1];
      break;
    }
    case OP_CALL_NATIVE: {
      int module = READ_BYTE();
      int fn = READ_BYTE();
      const steav_stdlib::NativeFn &native =
          steav_stdlib::module_at(module)->functions[fn];
      Value out = make_nil();
      if (native.fn(stack_top - native.arity, out) != STEAV_LOGGING_OK) {
        char native_err_str[256];
        snprintf(native_err_str, sizeof(native_err_str), "%s.%s failed",
                 steav_stdlib::module_at(module)->name, native.name);
        return _runtime_error(native_err_str);
      }
      this->stack_top -= native.arity;
      push(out);
      break;
    }
    case OP_CONSTRUCT_INSTANCE: {
      ObjClass *klass = (ObjClass *)READ_CONSTANT().obj;
      int arg_slots = READ_BYTE();
      ObjInstance *instance = heap.allocate<ObjInstance>(OBJ_INSTANCE);
      instance->klass = klass;
      instance->fields.assign(klass->field_slots, make_nil());
      // nis goes under the args, init returns it
      Value *args = stack_top - arg_slots;
      memmove(args + 1, args, arg_slots * sizeof(Value));
      args[0] = make_obj(instance);
      this->stack_top++;
      heap.charge(instance, klass->field_slots * sizeof(Value));
      if (klass->init != nullptr) {
        if (!_call(klass->init, arg_slots + 1)) return _runtime_error("stack overflow");
        frame = &frames[frame_count - 1];
      }
      break;
    }
    case OP_ARRAY: {
      int count = READ_U16();
      int w = READ_BYTE();
      int desc = READ_U16();
      bool optional = READ_BYTE() != 0;
      // the items stay on the stack while allocating, so the gc sees them
      ObjArray *array = heap.allocate<ObjArray>(OBJ_ARRAY);
      array->elem_width = w;
      array->elem_optional = optional;
      if (desc != STEAV_NO_STRUCT_TYPE)
        array->elem_desc = (ObjStructType *)frame->function->chunk.constants[desc].obj;
      Value *items = stack_top - count * w;
      array->items.assign(items, stack_top);
      this->stack_top = items;
      push(make_obj(array));
      heap.charge(array, array->items.size() * sizeof(Value));
      break;
    }
    case OP_ARRAY_LEN: {
      ObjArray *array = (ObjArray *)pop().obj;
      push(make_lek_kut((int32_t)(array->items.size() / array->elem_width)));
      break;
    }
    case OP_ARRAY_PUSH: {
      int w = READ_BYTE();
      heap.charge(stack_top[-w - 1].obj, w * sizeof(Value));
      Value *vals = stack_top - w;
      ObjArray *array = (ObjArray *)vals[-1].obj;
      array->items.insert(array->items.end(), vals, vals + w);
      this->stack_top = vals - 1;
      push(make_nil());
      break;
    }

    case OP_PRINT:
      pop().print();
      printf("\n");
      break;
    case OP_PRINT_STRUCT: {
      ObjStructType *desc = (ObjStructType *)READ_CONSTANT().obj;
      bool optional = READ_BYTE() != 0;
      int w = struct_width(desc) + (optional ? 1 : 0);
      print_slots(stack_top - w, desc, optional);
      printf("\n");
      this->stack_top -= w;
      break;
    }
    case OP_TO_STRING: {
      // read the value before popping, then allocate: same "stay on the
      // stack while allocating" gc discipline as string-concat OP_ADD
      std::string chars = to_string(stack_top[-1]);
      this->stack_top--;
      ObjString *s = heap.allocate<ObjString>(OBJ_STRING);
      s->chars = std::move(chars);
      push(make_obj(s));
      heap.charge(s, s->chars.size());
      break;
    }
    case OP_TO_STRING_STRUCT: {
      ObjStructType *desc = (ObjStructType *)READ_CONSTANT().obj;
      bool optional = READ_BYTE() != 0;
      int w = struct_width(desc) + (optional ? 1 : 0);
      std::string chars = to_string_slots(stack_top - w, desc, optional);
      this->stack_top -= w;
      ObjString *s = heap.allocate<ObjString>(OBJ_STRING);
      s->chars = std::move(chars);
      push(make_obj(s));
      heap.charge(s, s->chars.size());
      break;
    }
    case OP_RETURN: {
      int w = READ_BYTE();
      Value *result = stack_top - w;
      Value *base = frame->slots;
      this->frame_count--;
      memmove(base, result, w * sizeof(Value));
      this->stack_top = base + w;
      if (frame_count == 0) return STEAV_LOGGING_OK;
      frame = &frames[frame_count - 1];
      break;
    }
    default: {
      char unknown_op_str[512];
      snprintf(unknown_op_str, sizeof(unknown_op_str), "unknown opcode %d",
               instruction);
      return _runtime_error(unknown_op_str);
    }
    }
  }

#undef READ_BYTE
#undef READ_U16
#undef READ_CONSTANT
#undef BINARY_ARITH
#undef BINARY_COMPARE
}

} // namespace steav_vm
