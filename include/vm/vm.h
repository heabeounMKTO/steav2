#ifndef STEAV_VM_VM_H
#define STEAV_VM_VM_H

#include "logging/steav_logger.h"
#include "vm/gc.h"
#include "vm/object.h"
#include "vm/value.h"
#include <vector>
#define STEAV_VM_FRAMES_MAX 256
#define STEAV_VM_STACK_MAX (STEAV_VM_FRAMES_MAX * 256)

namespace steav_vm {

/* calls are static (the callee is a constant, not a stack slot), so a
 * frame's slots start right at its first argument */
struct CallFrame {
  ObjFunction *function;
  const uint8_t *ip;
  Value *slots; // this frame's locals start here on the value stack
};

/* parse + import + type check only, nothing compiled or run. errors go
 * wherever steav_diagnostic_sink says (logging/steav_diagnostics.h).
 * `path`: the source's own file, if it has one — anchors a `yok "..."`
 * file import to that file's directory. Without one (an embedded snippet
 * with no file on disk) relative imports resolve against the current
 * working directory instead. */
SteavStatus check_source(const char *source, const char *path = nullptr);

struct VM {
  VM() : frame_count(0), stack_top(stack) {}
  ~VM() { heap.free_objects(); }

  // source -> tokens -> ast -> type check -> bytecode -> run
  SteavStatus interpret(const char *source, const char *path = nullptr);

  inline void push(const Value &value) { *stack_top++ = value; }
  inline Value pop() { return *--stack_top; }
  inline void reset_stack() {
    this->stack_top = this->stack;
    this->frame_count = 0;
  }

  Heap heap;

private:
  CallFrame frames[STEAV_VM_FRAMES_MAX];
  int frame_count;
  Value stack[STEAV_VM_STACK_MAX];
  Value *stack_top;
  Value native_out[256]; // a C++ rupamun's result, one buffer so calls don't init one
  std::vector<Value> globals;       // flattened, indexed by the checker
  std::vector<ObjFunction *> roots; // every compiled function, for the gc

  friend struct Heap; // gc walks the stack + frames for roots

  SteavStatus _run();
  SteavStatus _runtime_error(const char *message);
  bool _call(ObjFunction *function, int arg_slots);
};

} // namespace steav_vm
#endif // STEAV_VM_VM_H
