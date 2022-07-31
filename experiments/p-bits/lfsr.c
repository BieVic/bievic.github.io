// Does the random number generator matter? A full-adder p-circuit (5 p-bits),
// compared against its exact Boltzmann distribution, with four sources of noise.
//
//   xoshiro      xoshiro256**, one fresh 32-bit word per update
//   lfsr-word    one 32-bit LFSR per p-bit, stepped once per update, full register
//                read as the random word (the "known danger": neighbouring words
//                are the same bits shifted by one)
//   lfsr-shift32 same LFSRs, but stepped 32 times between reads, so words don't overlap
//   lfsr-shared  one LFSR for the whole circuit, stepped once per update
//
// KL(measured || exact) as a function of sweeps. If the generator is fine, KL keeps
// falling like 1/sweeps. If it is biased, KL flattens out.
//
// clang -O2 -o lfsr lfsr.c -lm && ./lfsr > lfsr.json
#include "pbit.h"

#define N 5
static double J[N][N], h[N];

static void build(void) {
    double w[N] = {1, 1, 1, -1, -2}; // (a + b + cin - s - 2 cout)^2
    double Q[N][N] = {{0}}, c[N];
    for (int i = 0; i < N; i++) {
        c[i] = w[i] * w[i];
        for (int j = i + 1; j < N; j++) Q[i][j] = Q[j][i] = 2 * w[i] * w[j];
    }
    for (int i = 0; i < N; i++) {
        double s = c[i] / 2;
        for (int j = 0; j < N; j++) { J[i][j] = -Q[i][j] / 4; s += Q[i][j] / 4; }
        h[i] = -s;
    }
}

static double energy(int s) {
    int m[N];
    for (int i = 0; i < N; i++) m[i] = (s >> i & 1) * 2 - 1;
    double e = 0;
    for (int i = 0; i < N; i++) {
        e -= h[i] * m[i];
        for (int j = i + 1; j < N; j++) e -= J[i][j] * m[i] * m[j];
    }
    return e;
}

// Fibonacci LFSR, x^32 + x^22 + x^2 + x + 1, maximal length.
static inline uint32_t lfsr_step(uint32_t x) {
    uint32_t bit = (x ^ (x >> 10) ^ (x >> 30) ^ (x >> 31)) & 1;
    return (x >> 1) | (bit << 31);
}

enum { XOSHIRO, LFSR_WORD, LFSR_SHIFT32, LFSR_SHARED, NKINDS };
static const char *names[NKINDS] = {"xoshiro", "lfsr-word", "lfsr-shift32", "lfsr-shared"};

int main(void) {
    build();
    double beta = 1.0, p[32], Z = 0;
    for (int s = 0; s < 32; s++) { p[s] = exp(-beta * energy(s)); Z += p[s]; }
    for (int s = 0; s < 32; s++) p[s] /= Z;

    const long long total = 100000000LL; // 10^8 sweeps
    const int seeds = 8;
    printf("[\n");
    int first = 1;
    for (int kind = 0; kind < NKINDS; kind++) {
        double kl_sum[11] = {0}; long long marks[11]; int nm = 0;
        for (long long c = 1000; c <= total; c = c * 10) { marks[nm++] = c; if (c * 3 <= total) marks[nm++] = c * 3; }
        for (int seed = 0; seed < seeds; seed++) {
            rng_t r; rng_seed(&r, 1000 + seed);
            uint32_t reg[N], shared;
            for (int i = 0; i < N; i++) { do reg[i] = (uint32_t)rng_u64(&r); while (!reg[i]); }
            shared = reg[0];
            int m[N];
            for (int i = 0; i < N; i++) m[i] = (rng_u64(&r) & 1) * 2 - 1;
            long long hist[32] = {0};
            int mark = 0;
            for (long long t = 1; t <= total; t++) {
                for (int i = 0; i < N; i++) {
                    uint32_t word;
                    switch (kind) {
                    case XOSHIRO: word = (uint32_t)(rng_u64(&r) >> 32); break;
                    case LFSR_WORD: reg[i] = lfsr_step(reg[i]); word = reg[i]; break;
                    case LFSR_SHIFT32: for (int k = 0; k < 32; k++) reg[i] = lfsr_step(reg[i]); word = reg[i]; break;
                    default: shared = lfsr_step(shared); word = shared; break;
                    }
                    double I = h[i];
                    for (int j = 0; j < N; j++) I += J[i][j] * m[j];
                    // word/2^32 is uniform on [0,1); p(+1) = (1 + tanh(beta I)) / 2
                    m[i] = word * 0x1p-32 < 0.5 * (1 + tanh(beta * I)) ? 1 : -1;
                }
                int s = 0;
                for (int i = 0; i < N; i++) s |= (m[i] > 0) << i;
                hist[s]++;
                if (mark < nm && t == marks[mark]) {
                    double kl = 0;
                    for (int x = 0; x < 32; x++) if (hist[x]) { double q = (double)hist[x] / t; kl += q * log(q / p[x]); }
                    kl_sum[mark++] += kl;
                }
            }
        }
        for (int i = 0; i < nm; i++) {
            printf("%s  {\"rng\": \"%s\", \"sweeps\": %lld, \"kl\": %.6g}", first ? "" : ",\n", names[kind], marks[i], kl_sum[i] / seeds);
            first = 0;
        }
        fflush(stdout);
    }
    printf("\n]\n");
    return 0;
}
