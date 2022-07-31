// Summing doubles with 1, 2, 4 and 8 independent accumulators.
// The array fits in L1 cache, so the loop is limited by the CPU, not by memory.
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 4096
#define REPS 20000

__attribute__((noinline)) double sum1(const double *a, long n) {
    double s = 0.0;
    for (long i = 0; i < n; i++) s += a[i];
    return s;
}

__attribute__((noinline)) double sum2(const double *a, long n) {
    double s0 = 0.0, s1 = 0.0;
    for (long i = 0; i < n; i += 2) { s0 += a[i]; s1 += a[i + 1]; }
    return s0 + s1;
}

__attribute__((noinline)) double sum4(const double *a, long n) {
    double s0 = 0.0, s1 = 0.0, s2 = 0.0, s3 = 0.0;
    for (long i = 0; i < n; i += 4) {
        s0 += a[i]; s1 += a[i + 1]; s2 += a[i + 2]; s3 += a[i + 3];
    }
    return (s0 + s1) + (s2 + s3);
}

__attribute__((noinline)) double sum8(const double *a, long n) {
    double s[8] = {0};
    for (long i = 0; i < n; i += 8)
        for (int k = 0; k < 8; k++) s[k] += a[i + k];
    return ((s[0] + s[1]) + (s[2] + s[3])) + ((s[4] + s[5]) + (s[6] + s[7]));
}

static double now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

static void run(const char *name, double (*f)(const double *, long), const double *a) {
    double best = 1e9, check = 0;
    for (int trial = 0; trial < 7; trial++) {
        double t0 = now();
        for (int r = 0; r < REPS; r++) {
            __asm__ volatile("" ::: "memory");  // stop the compiler from hoisting the call out of the loop
            check += f(a, N);
        }
        double ns = (now() - t0) * 1e9 / ((double)REPS * N);
        if (ns < best) best = ns;
    }
    printf("%-6s %.3f ns/element  (checksum %.6g)\n", name, best, check);
}

int main(void) {
    double *a = malloc(N * sizeof(double));
    for (int i = 0; i < N; i++) a[i] = (double)(i % 100) * 0.01;
    run("sum1", sum1, a);
    run("sum2", sum2, a);
    run("sum4", sum4, a);
    run("sum8", sum8, a);
    return 0;
}
