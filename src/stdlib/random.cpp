#include "stdlib/random.h"
#include "math/pcg32.h"

namespace steav_stdlib {

using steav_vm::Value;

/* one rng for the process, default seed is keo's (98737613, seq 1) so
 * same script == same numbers until it calls seed */
static PCG32 steav_rng;

PCG32 &shared_rng() { return steav_rng; }

/* the samplers that return a Vec3 (random_unit_vector, ...) live in
 * vector, since that's where Vec3 is declared */
struct RandomNatives {
  static SteavStatus seed(const Value *args, Value &out) {
    steav_rng = PCG32((uint64_t)(uint32_t)args[0].lek_kut);
    out = steav_vm::make_nil();
    return STEAV_LOGGING_OK;
  }
  // seed + stream, keo's PCG32(seed, seq)
  static SteavStatus seed_stream(const Value *args, Value &out) {
    steav_rng = PCG32((uint64_t)(uint32_t)args[0].lek_kut,
                      (uint64_t)(uint32_t)args[1].lek_kut);
    out = steav_vm::make_nil();
    return STEAV_LOGGING_OK;
  }
  static SteavStatus uniform(const Value *args, Value &out) {
    out = steav_vm::make_lek_thom(steav_rng.rand_uniform());
    return STEAV_LOGGING_OK;
  }
  static SteavStatus interval(const Value *args, Value &out) {
    out = steav_vm::make_lek_thom(
        steav_rng.random_interval((float)args[0].lek_thom, (float)args[1].lek_thom));
    return STEAV_LOGGING_OK;
  }
};

static const NativeFn RANDOM_FUNCTIONS[] = {
    {"seed", 1, {NATIVE_LEK_KUT}, NATIVE_ORT_MEAN, RandomNatives::seed},
    {"seed_stream", 2, {NATIVE_LEK_KUT, NATIVE_LEK_KUT}, NATIVE_ORT_MEAN,
     RandomNatives::seed_stream},
    {"uniform", 0, {}, NATIVE_LEK_THOM, RandomNatives::uniform},
    {"interval", 2, {NATIVE_LEK_THOM, NATIVE_LEK_THOM}, NATIVE_LEK_THOM,
     RandomNatives::interval},
};

const Module RANDOM_MODULE = {
    "random",
    RANDOM_FUNCTIONS,
    (int)(sizeof(RANDOM_FUNCTIONS) / sizeof(RANDOM_FUNCTIONS[0])),
    nullptr,
    0,
    nullptr,
    nullptr,
    0,
};

} // namespace steav_stdlib
