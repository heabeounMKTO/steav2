#include "stdlib/math.h"
#include "math/constants.h"

namespace steav_stdlib {

using steav_vm::Value;
using steav_vm::make_lek_thom;

/* every math native is LekThom in, LekThom out. the checker already
 * proved the arg types, so no tag checks */
struct MathNatives {
  static SteavStatus sqrt(const Value *args, Value &out) {
    out = make_lek_thom(::sqrt(args[0].lek_thom));
    return STEAV_LOGGING_OK;
  }
  static SteavStatus abs(const Value *args, Value &out) {
    out = make_lek_thom(::fabs(args[0].lek_thom));
    return STEAV_LOGGING_OK;
  }
  static SteavStatus floor(const Value *args, Value &out) {
    out = make_lek_thom(::floor(args[0].lek_thom));
    return STEAV_LOGGING_OK;
  }
  static SteavStatus ceil(const Value *args, Value &out) {
    out = make_lek_thom(::ceil(args[0].lek_thom));
    return STEAV_LOGGING_OK;
  }
  static SteavStatus min(const Value *args, Value &out) {
    out = make_lek_thom(::fmin(args[0].lek_thom, args[1].lek_thom));
    return STEAV_LOGGING_OK;
  }
  static SteavStatus max(const Value *args, Value &out) {
    out = make_lek_thom(::fmax(args[0].lek_thom, args[1].lek_thom));
    return STEAV_LOGGING_OK;
  }
  static SteavStatus pow(const Value *args, Value &out) {
    out = make_lek_thom(::pow(args[0].lek_thom, args[1].lek_thom));
    return STEAV_LOGGING_OK;
  }
  static SteavStatus sin(const Value *args, Value &out) {
    out = make_lek_thom(::sin(args[0].lek_thom));
    return STEAV_LOGGING_OK;
  }
  static SteavStatus cos(const Value *args, Value &out) {
    out = make_lek_thom(::cos(args[0].lek_thom));
    return STEAV_LOGGING_OK;
  }
  static SteavStatus tan(const Value *args, Value &out) {
    out = make_lek_thom(::tan(args[0].lek_thom));
    return STEAV_LOGGING_OK;
  }
  static SteavStatus clamp(const Value *args, Value &out) {
    double x = args[0].lek_thom;
    double lo = args[1].lek_thom;
    double hi = args[2].lek_thom;
    out = make_lek_thom(x < lo ? lo : (x > hi ? hi : x));
    return STEAV_LOGGING_OK;
  }
  static SteavStatus rad2deg(const Value *args, Value &out) {
    out = make_lek_thom(RAD2DEG(args[0].lek_thom));
    return STEAV_LOGGING_OK;
  }
  static SteavStatus deg2rad(const Value *args, Value &out) {
    out = make_lek_thom(DEG2RAD(args[0].lek_thom));
    return STEAV_LOGGING_OK;
  }
};

#define D NATIVE_LEK_THOM
static const NativeFn MATH_FUNCTIONS[] = {
    {"sqrt", 1, {D}, D, MathNatives::sqrt},
    {"abs", 1, {D}, D, MathNatives::abs},
    {"floor", 1, {D}, D, MathNatives::floor},
    {"ceil", 1, {D}, D, MathNatives::ceil},
    {"min", 2, {D, D}, D, MathNatives::min},
    {"max", 2, {D, D}, D, MathNatives::max},
    {"pow", 2, {D, D}, D, MathNatives::pow},
    {"sin", 1, {D}, D, MathNatives::sin},
    {"cos", 1, {D}, D, MathNatives::cos},
    {"tan", 1, {D}, D, MathNatives::tan},
    {"clamp", 3, {D, D, D}, D, MathNatives::clamp},
    {"rad2deg", 1, {D}, D, MathNatives::rad2deg},
    {"deg2rad", 1, {D}, D, MathNatives::deg2rad},
};
#undef D

// same values as keo, just without the M_ prefix
static const Constant MATH_CONSTANTS[] = {
    {"INFINITY", INFINITY}, {"PI", M_PI},           {"TAU", M_TAU},
    {"PI_2", M_PI_2},       {"PI_4", M_PI_4},       {"SQRT2", M_SQRT2},
    {"SQRT1_2", M_SQRT1_2}, {"SQRT3", M_SQRT3},     {"SQRT1_3", M_SQRT1_3},
    {"INV_PI", M_1_PI},     {"E", M_E},             {"LOG2E", M_LOG2E},
    {"LOG10E", M_LOG10E},   {"LN2", M_LN2},         {"LN10", M_LN10},
    {"NEAR_ZERO", NEAR_ZERO},
};

const Module MATH_MODULE = {
    "math",
    MATH_FUNCTIONS,
    (int)(sizeof(MATH_FUNCTIONS) / sizeof(MATH_FUNCTIONS[0])),
    MATH_CONSTANTS,
    (int)(sizeof(MATH_CONSTANTS) / sizeof(MATH_CONSTANTS[0])),
    nullptr,
    nullptr,
    0,
};

} // namespace steav_stdlib
