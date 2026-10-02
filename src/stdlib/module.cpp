#include "stdlib/module.h"
#include "stdlib/math.h"
#include "stdlib/random.h"
#include "stdlib/vector.h"

namespace steav_stdlib {


/* the index into this is what OP_CALL_NATIVE carries, so only append */
static const Module *MODULES[] = {&VECTOR_MODULE, &RANDOM_MODULE, &MATH_MODULE};

int module_count() { return (int)(sizeof(MODULES) / sizeof(MODULES[0])); }

const Module *module_at(int index) { return MODULES[index]; }

int find_module(const char *name, int length) {
  for (int i = 0; i < module_count(); i++) {
    if ((int)strlen(MODULES[i]->name) == length &&
        memcmp(MODULES[i]->name, name, length) == 0)
      return i;
  }
  return -1;
}

} // namespace steav_stdlib
