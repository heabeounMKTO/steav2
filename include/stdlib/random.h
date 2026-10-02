#ifndef STEAV_STDLIB_RANDOM_H
#define STEAV_STDLIB_RANDOM_H

#include "math/pcg32.h"
#include "stdlib/module.h"

namespace steav_stdlib {

extern const Module RANDOM_MODULE; // impl in random.cpp

// the one PCG32 `random` uses, vector's random_* helpers draw from it too
PCG32 &shared_rng();

} // namespace steav_stdlib
#endif // STEAV_STDLIB_RANDOM_H
