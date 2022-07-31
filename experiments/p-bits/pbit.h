// Shared bits for the p-bit experiments: a fast PRNG and a sparse Ising model.
#pragma once
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// xoshiro256**, seeded through splitmix64. Our reference "good" randomness.
typedef struct { uint64_t s[4]; } rng_t;

static inline uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

static inline uint64_t rng_u64(rng_t *r) {
    uint64_t *s = r->s;
    uint64_t out = rotl(s[1] * 5, 7) * 9, t = s[1] << 17;
    s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3]; s[2] ^= t;
    s[3] = rotl(s[3], 45);
    return out;
}

static inline void rng_seed(rng_t *r, uint64_t seed) {
    for (int i = 0; i < 4; i++) {
        uint64_t z = (seed += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        r->s[i] = z ^ (z >> 31);
    }
}

// Uniform on (-1, 1).
static inline double rng_sym(rng_t *r) { return (double)(rng_u64(r) >> 11) * 0x1p-52 - 1.0 + 0x1p-53; }

static inline double now_s(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + 1e-9 * t.tv_nsec;
}
