// How far can you unroll before running out of registers?
// Sum doubles with K independent accumulators for K = 1 ... 64.
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 4096
#define REPS 20000

#define DEFINE_SUM(K)                                                    \
    __attribute__((noinline)) double sum_##K(const double *a, long n) { \
        double s[K] = {0};                                               \
        for (long i = 0; i < n; i += K)                                  \
            for (int k = 0; k < K; k++) s[k] += a[i + k];                \
        double t = 0;                                                    \
        for (int k = 0; k < K; k++) t += s[k];                           \
        return t;                                                        \
    }

DEFINE_SUM(1) DEFINE_SUM(2) DEFINE_SUM(4) DEFINE_SUM(8) DEFINE_SUM(16) DEFINE_SUM(32) DEFINE_SUM(64)

static double now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

static void run(int k, double (*f)(const double *, long), const double *a) {
    double best = 1e9, check = 0;
    for (int trial = 0; trial < 7; trial++) {
        double t0 = now();
        for (int r = 0; r < REPS; r++) {
            __asm__ volatile("" ::: "memory");
            check += f(a, N);
        }
        double ns = (now() - t0) * 1e9 / ((double)REPS * N);
        if (ns < best) best = ns;
    }
    printf("%2d accumulators: %.3f ns/element (checksum %.6g)\n", k, best, check);
}

int main(void) {
    double *a = malloc(N * sizeof(double));
    for (int i = 0; i < N; i++) a[i] = (double)(i % 100) * 0.01;
    run(1, sum_1, a); run(2, sum_2, a); run(4, sum_4, a); run(8, sum_8, a);
    run(16, sum_16, a); run(32, sum_32, a); run(64, sum_64, a);
    return 0;
}
