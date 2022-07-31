// Does the cold end of an anneal hold the answer?
//
// factor.c stops as soon as the factor bits multiply to N, checked every sweep. That flatters
// a hot sampler: at beta = 0 each sweep is a uniform draw of the factor bits, so its "search"
// is blind guessing by construction, and long, slow ramps win the tuning by staying hot.
//
// Here the state is read once, at the end of each anneal, which is what relaxing into a low
// energy state means. Cost per success is ramp length / P(success). The hot row is the control:
// beta = 0, checked every sweep, which should come out at 1.0 times blind.
//
// Runs on one core with a fixed budget per cell.
#define main factor_main
#include "factor.c"
#undef main

int main(void) {
    const double finals[] = {3.0, 6.0};
    const long long ramps[] = {100, 1000, 10000, 100000};
    static net_t g;
    static signed char m[MAXV];
    static uint64_t primes[200000];
    printf("[\n");
    int first = 1;
    for (int n = 4; n <= 9; n++) {
        rng_t r;
        rng_seed(&r, 7000 + n);
        int np = 0;
        for (uint64_t x = (1ULL << (n - 1)) | 1; x < (1ULL << n) && np < 200000; x += 2) if (is_prime(x)) primes[np++] = x;
        double blind = pow(4, n - 2) / 2;
        uint64_t A[12], B[12];
        for (int i = 0; i < 12; i++) { A[i] = primes[rng_u64(&r) % np]; B[i] = primes[rng_u64(&r) % np]; }

        // Control: beta = 0, checked every sweep.
        double hot_total = 0; int hot_runs = 0;
        for (int i = 0; i < 12; i++) {
            build(&g, n, A[i] * B[i]);
            for (int k = 0; k < 5; k++) { hot_total += solve(&g, A[i] * B[i], 0, 0.0, 1000000000LL, 1000000000LL, &r); hot_runs++; }
        }
        printf("%s  {\"n\": %d, \"kind\": \"hot\", \"blind\": %.0f, \"mean_sweeps\": %.1f, \"ratio\": %.3f}", first ? "" : ",\n", n, blind, hot_total / hot_runs, hot_total / hot_runs / blind);
        first = 0;

        for (int fi = 0; fi < 2; fi++) for (int ri = 0; ri < 4; ri++) {
            long long L = ramps[ri];
            double budget = fmax(200 * blind, 24.0 * L);
            int per_inst = (int)ceil(budget / L / 12);
            long long anneals = 0, wins = 0;
            for (int i = 0; i < 12; i++) {
                uint64_t N = A[i] * B[i];
                build(&g, n, N);
                for (int k = 0; k < per_inst; k++) {
                    randomise(&g, m, &r);
                    for (long long t = 0; t < L; t++) sweep(&g, m, finals[fi] * (t + 1) / L, &r);
                    anneals++;
                    wins += solved(&g, m, N);
                }
            }
            double p = (double)wins / anneals;
            printf(",\n  {\"n\": %d, \"kind\": \"cold\", \"beta\": %.1f, \"ramp\": %lld, \"anneals\": %lld, \"wins\": %lld, \"p\": %.5g, \"sweeps_per_success\": %.6g, \"ratio\": %.4g, \"blind\": %.0f}",
                   n, finals[fi], L, anneals, wins, p, wins ? L / p : -1.0, wins ? L / p / blind : -1.0, blind);
            fflush(stdout);
        }
    }
    printf("\n]\n");
    return 0;
}
