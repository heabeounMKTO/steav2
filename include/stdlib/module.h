#ifndef STEAV_STDLIB_MODULE_H
#define STEAV_STDLIB_MODULE_H

#include "logging/steav_logger.h"
#include "vm/object.h"
#include <cstdio>
#include <cstring>

#define STEAV_STDLIB_MAX_PARAMS 4

namespace steav_stdlib {

/* module functions (math.sqrt, random.uniform) only ever see numbers, so
 * this is all the type checker needs to know about their signatures.
 * C++ that works on sampoans (vector) goes through BoundFn instead */
enum NativeType { NATIVE_LEK_THOM, NATIVE_LEK_KUT, NATIVE_ORT_MEAN };

// arity is checked statically, writes the result into `out`
typedef SteavStatus (*NativeFnPtr)(const steav_vm::Value *args,
                                   steav_vm::Value &out);

struct NativeFn {
  const char *name;
  int arity;
  NativeType params[STEAV_STDLIB_MAX_PARAMS];
  NativeType ret;
  NativeFnPtr fn;
};

/* the C++ side of a `rupamun ...;` declared in a stdlib .sts module,
 * matched by signature: "dot(Vec3,Vec3)", "+(Vec3,LekThom)",
 * "Vec3.length()" (methods get the receiver first in args) */
struct BoundFn {
  const char *signature;
  steav_vm::NativeSlotsFn fn;
};

// a named LekThom exposed by a module, e.g. `PI`
struct Constant {
  const char *name;
  double value;
};

/* loaded with `yok <name>;` or `yok <name> jea <alias>;`. either native
 * (functions + constants in C++), or a .sts `source` whose body-less
 * `rupamun ...;` declarations get their C++ from `bound` */
struct Module {
  const char *name;
  const NativeFn *functions;
  int function_count;
  const Constant *constants;
  int constant_count;
  const char *source; // .sts module, nullptr = native
  const BoundFn *bound; // C++ bodies for the .sts module's `rupamun ...;`
  int bound_count;
};

int module_count();
const Module *module_at(int index);
int find_module(const char *name, int length); // -1 = no such module

} // namespace steav_stdlib
#endif // STEAV_STDLIB_MODULE_H
