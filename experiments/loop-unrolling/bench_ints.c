// Integer sum, vectorisation disabled: does removing loop bookkeeping alone help?
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 4096
#define REPS 20000

__attribute__((noinline)) long sum_ints(const int *a, long n) {
    long s = 0;
    for (long i = 0; i < n; i++) s += a[i];
    return s;
}

static double now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

int main(void) {
    int *a = malloc(N * sizeof(int));
    for (int i = 0; i < N; i++) a[i] = i % 100;
    double best = 1e9;
    long check = 0;
    for (int trial = 0; trial < 7; trial++) {
        double t0 = now();
        for (int r = 0; r < REPS; r++) {
            __asm__ volatile("" ::: "memory");
            check += sum_ints(a, N);
        }
        double ns = (now() - t0) * 1e9 / ((double)REPS * N);
        if (ns < best) best = ns;
    }
    printf("%.3f ns/element (checksum %ld)\n", best, check);
    return 0;
}
