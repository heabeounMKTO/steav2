/*
 * PCG32 implementation , reference:
 * https://www.pcg-random.org/index.html
 *
 *
 * */

#ifndef STEAV_MATH_RANDOM_PCG32_H
#define STEAV_MATH_RANDOM_PCG32_H
#include <stdint.h>

struct PCG32 {
  uint64_t state;
  uint64_t inc;

  // Better seeding: separate sequence + state
  PCG32(uint64_t seed = 98737613, uint64_t seq = 1) {
    state = 0;
    inc = (seq << 1u) | 1u;
    next_u32();
    state += seed;
    next_u32();
  }

  uint32_t next_u32() {
    uint64_t oldstate = state;

    state = oldstate * 6364136223846793005ULL + inc;

    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;

    uint32_t rot = oldstate >> 59u;

    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
  }
  float rand_uniform() { return rand_next_f32(); }
  float rand_next_f32() { return (next_u32() >> 8) * (1.0f / 16777216.0f); }
  float random_interval(float min, float max) {
    return min + (max - min) * (rand_uniform());
  }
};

static PCG32 pcg32_rng;

#endif // STEAV_MATH_RANDOM_PCG32_H
