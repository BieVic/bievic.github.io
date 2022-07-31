// The invertible AND gate of Camsari et al. (PRX 2017, Eq. 10), checked two ways:
// exact Boltzmann probabilities by enumeration, against the p-bit update rule.
//
//   m_i = sgn(rand(-1,1) + tanh(beta * I_i)),  I_i = sum_j J_ij m_j + h_i
//
// clang -O2 -o and_gate and_gate.c -lm && ./and_gate
#include "pbit.h"

static const int J[3][3] = {{0, -1, 2}, {-1, 0, 2}, {2, 2, 0}};
static const int h[3] = {1, 1, -2};

// E(m) = -sum_{i<j} J_ij m_i m_j - sum_i h_i m_i. Bit 2 of s is A, bit 1 B, bit 0 C.
static int energy(int s) {
    int m[3] = {(s >> 2 & 1) * 2 - 1, (s >> 1 & 1) * 2 - 1, (s & 1) * 2 - 1}, e = 0;
    for (int i = 0; i < 3; i++) {
        e -= h[i] * m[i];
        for (int j = i + 1; j < 3; j++) e -= J[i][j] * m[i] * m[j];
    }
    return e;
}

// clamp: -1 leaves a p-bit free, otherwise holds it at 0 or 1 (indexed A, B, C).
static void run(const char *name, const int clamp[3], double beta, long sweeps, rng_t *r) {
    double exact[8] = {0}, Z = 0;
    long hist[8] = {0};
    for (int s = 0; s < 8; s++) {
        int ok = 1;
        for (int i = 0; i < 3; i++) if (clamp[i] >= 0 && (s >> (2 - i) & 1) != clamp[i]) ok = 0;
        if (ok) { exact[s] = exp(-beta * energy(s)); Z += exact[s]; }
    }
    int m[3];
    for (int i = 0; i < 3; i++) m[i] = clamp[i] >= 0 ? clamp[i] * 2 - 1 : (rng_u64(r) & 1) * 2 - 1;
    for (long t = 0; t < sweeps; t++) {
        for (int i = 0; i < 3; i++) {
            if (clamp[i] >= 0) continue;
            double I = h[i];
            for (int j = 0; j < 3; j++) I += J[i][j] * m[j];
            m[i] = rng_sym(r) + tanh(beta * I) > 0 ? 1 : -1;
        }
        hist[(m[0] > 0) << 2 | (m[1] > 0) << 1 | (m[2] > 0)]++;
    }
    printf("%s (beta=%.2f)\n  ABC   E   exact   measured\n", name, beta);
    double tv = 0;
    for (int s = 0; s < 8; s++) {
        double p = exact[s] / Z, q = (double)hist[s] / sweeps;
        tv += fabs(p - q) / 2;
        if (exact[s] > 0) printf("  %d%d%d %3d  %.4f  %.4f\n", s >> 2 & 1, s >> 1 & 1, s & 1, energy(s), p, q);
    }
    printf("  total variation distance %.5f\n\n", tv);
}

int main(void) {
    rng_t r;
    rng_seed(&r, 42);
    long n = 20000000;
    run("free", (int[]){-1, -1, -1}, 1.0, n, &r);
    run("output clamped to 1", (int[]){-1, -1, 1}, 1.0, n, &r);
    run("output clamped to 0", (int[]){-1, -1, 0}, 1.0, n, &r);
    run("inputs clamped to 1,1", (int[]){1, 1, -1}, 1.0, n, &r);
    run("free, hot", (int[]){-1, -1, -1}, 0.1, n, &r);
    return 0;
}
