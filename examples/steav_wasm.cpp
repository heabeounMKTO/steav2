/* the browser's way into steav2: a tiny C API over steav_vm, compiled with
 * emscripten (BUILD_STEAV_WASM in CMakeLists.txt / docs/cli.md#wasm).
 * stdout / stderr go wherever the JS side points Module.print / Module.printErr.
 *
 * hover comes from lsp_core.cpp, the same implementation steav2-lsp uses,
 * so the playground shows exactly what an editor with the language server
 * does — no source-file-inclusion hack to keep the two in sync. */

#include "frontend/scanner.h"
#include "logging/steav_diagnostics.h"
#include "../lsp/lsp_core.h"
#include "vm/vm.h"
#include <cstdio>
#include <cstring>
#include <string>

#include <emscripten/emscripten.h>

#ifndef STEAV_WASM_VERSION
#define STEAV_WASM_VERSION "unknown"
#endif
#ifndef STEAV_WASM_COMMIT
#define STEAV_WASM_COMMIT "unknown"
#endif

static void append_json_string(std::string &out, const std::string &s) {
  out += '"';
  for (unsigned char c : s) {
    switch (c) {
    case '"': out += "\\\""; break;
    case '\\': out += "\\\\"; break;
    case '\n': out += "\\n"; break;
    case '\r': out += "\\r"; break;
    case '\t': out += "\\t"; break;
    default:
      if (c < 0x20) {
        char buf[8];
        snprintf(buf, sizeof(buf), "\\u%04x", c);
        out += buf;
      } else {
        out += (char)c;
      }
    }
  }
  out += '"';
}

extern "C" {

static std::string last_stats = "{}";

/* numbers for the playground's stats window, from outside the VM: the
 * frontend timed on its own, then the heap walked once the script is done
 * (every object is still on heap.objects until the VM is freed) */
static void record_stats(const char *source, double frontend_ms, double interpret_ms,
                         steav_vm::VM &vm) {
  int tokens = 0, lines = 1;
  steav_frontend::Scanner scanner(source);
  while (true) {
    steav_frontend::Token t = scanner.scan_token();
    if (t.type == steav_frontend::TOKEN_EOF) break;
    tokens++;
  }
  for (const char *c = source; *c; c++)
    if (*c == '\n') lines++;

  long counts[6] = {0, 0, 0, 0, 0, 0};
  long object_bytes = 0, functions = 0, natives = 0, code_bytes = 0, constants = 0;
  for (steav_vm::Obj *o = vm.heap.objects; o != nullptr; o = o->next) {
    if (o->type >= 0 && o->type < 6) counts[o->type]++;
    object_bytes += (long)o->size;
    if (o->type == steav_vm::OBJ_FUNCTION) {
      steav_vm::ObjFunction *f = (steav_vm::ObjFunction *)o;
      if (f->native != nullptr) {
        natives++;
      } else {
        functions++;
        code_bytes += (long)f->chunk.code.size();
        constants += (long)f->chunk.constants.size();
      }
    }
  }

  char buf[1024];
  snprintf(buf, sizeof(buf),
           "{\"source_bytes\":%ld,\"source_lines\":%d,\"tokens\":%d,"
           "\"frontend_ms\":%.3f,\"interpret_ms\":%.3f,"
           "\"functions\":%ld,\"natives\":%ld,\"code_bytes\":%ld,\"constants\":%ld,"
           "\"heap_bytes\":%ld,\"gc_threshold\":%ld,\"object_bytes\":%ld,"
           "\"objects\":{\"string\":%ld,\"function\":%ld,\"class\":%ld,"
           "\"instance\":%ld,\"array\":%ld,\"struct_type\":%ld}}",
           (long)strlen(source), lines, tokens, frontend_ms, interpret_ms, functions, natives,
           code_bytes, constants, (long)vm.heap.bytes_allocated, (long)vm.heap.next_gc,
           object_bytes, counts[0], counts[1], counts[2], counts[3], counts[4], counts[5]);
  last_stats = buf;
}

/* same exit codes as the steav CLI: 0 ok, 65 compile error, 70 runtime error */
EMSCRIPTEN_KEEPALIVE int steav_run(const char *source) {
  // the frontend on its own, errors swallowed (interpret reports them)
  std::vector<SteavDiagnostic> ignored;
  steav_diagnostic_sink = &ignored;
  double t0 = emscripten_get_now();
  steav_vm::check_source(source);
  double frontend_ms = emscripten_get_now() - t0;
  steav_diagnostic_sink = nullptr;

  // heap allocated: VM embeds its whole value stack (STEAV_VM_STACK_MAX
  // slots) as a member, several MB, too big for a stack frame under
  // Emscripten's much smaller default execution stack
  steav_vm::VM *vm = new steav_vm::VM();
  double t1 = emscripten_get_now();
  SteavStatus status = vm->interpret(source);
  double interpret_ms = emscripten_get_now() - t1;
  fflush(stdout);
  fflush(stderr);
  record_stats(source, frontend_ms, interpret_ms, *vm);
  delete vm;
  if (status == STEAV_LOGGING_COMPILE_ERROR) return 65;
  if (status == STEAV_LOGGING_RUNTIME_ERROR) return 70;
  return 0;
}

/* JSON about the last steav_run, valid until the next one */
EMSCRIPTEN_KEEPALIVE const char *steav_last_stats() { return last_stats.c_str(); }

/* steav2-lsp's hover at a 0-based line / character: `null`, or
 * {"contents": {"kind": "markdown", "value": "..."}} */
EMSCRIPTEN_KEEPALIVE const char *steav_hover(const char *source, int line, int character) {
  static std::string json;
  json = steav_lsp::hover(source, line, character).dump();
  return json.c_str();
}

/* parse + type check only. returns a JSON array of
 * {"line", "col", "length", "message"}, col = -1 when only the line is known.
 * the string stays valid until the next call */
EMSCRIPTEN_KEEPALIVE const char *steav_check(const char *source) {
  static std::string json;
  std::vector<SteavDiagnostic> found;
  steav_diagnostic_sink = &found;
  steav_vm::check_source(source);
  steav_diagnostic_sink = nullptr;

  const char *begin = source;
  const char *end = source + strlen(source);
  json = "[";
  for (size_t i = 0; i < found.size(); i++) {
    const SteavDiagnostic &d = found[i];
    int line = d.line;
    int col = -1;
    int length = d.length;
    std::string message = d.message;
    if (d.at != nullptr && d.at >= begin && d.at <= end) {
      const char *line_start = d.at;
      while (line_start > begin && line_start[-1] != '\n') line_start--;
      col = (int)(d.at - line_start);
    } else if (d.at != nullptr) {
      // points into a yok'd stdlib module's source, not ours
      line = 1;
      col = 0;
      length = 0;
      message = "in a yok'd module: " + message;
    }
    if (i > 0) json += ',';
    json += "{\"line\":" + std::to_string(line) + ",\"col\":" + std::to_string(col) +
            ",\"length\":" + std::to_string(length) + ",\"message\":";
    append_json_string(json, message);
    json += '}';
  }
  json += ']';
  return json.c_str();
}

EMSCRIPTEN_KEEPALIVE const char *steav_version() {
  return STEAV_WASM_VERSION " (" STEAV_WASM_COMMIT ")";
}

} // extern "C"
