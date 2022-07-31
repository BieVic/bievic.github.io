// Factoring by running a p-bit array multiplier backwards.
//
// Build an n x n array multiplier out of invertible AND gates and full adders,
// clamp the product bits to N, and sample until the factor bits multiply to N.
// Three samplers, each tuned separately per size on its own instances:
//   0  Gibbs at fixed beta, with restarts
//   1  simulated annealing, beta ramped linearly from 0, with restarts
//   2  parallel tempering, 8 replicas, swap attempts after every sweep
// Sweeps always count one pass over all free p-bits, so a PT round costs 8.
//
// Gate penalties in 0/1 variables (0 on valid rows, >=1 on invalid ones):
//   AND: ab - 2(a+b)c + 3c        FA: (a + b + cin - s - 2 cout)^2
//
// clang -O2 -o factor factor.c -lm && ./factor <factor_bits> <mode> <cap_sweeps>
#include "pbit.h"

#define MAXV 4096
#define MAXDEG 64

typedef struct {
    int nv, nfree;
    double Q[MAXV][MAXDEG]; int nbr[MAXV][MAXDEG]; int deg[MAXV]; // 0/1 couplings
    double c[MAXV];     // 0/1 linear terms
    double J[MAXV][MAXDEG], h[MAXV]; // Ising form
    int fixed[MAXV];    // -1 free, else clamped to 0/1
    int free_list[MAXV];
    int p[32], q[32], n;
} net_t;

static void add_q(net_t *g, int a, int b, double w) {
    if (a == b) { g->c[a] += w; return; } // x^2 = x
    for (int side = 0; side < 2; side++) {
        int u = side ? b : a, v = side ? a : b, k;
        for (k = 0; k < g->deg[u]; k++) if (g->nbr[u][k] == v) break;
        if (k == g->deg[u]) {
            if (k == MAXDEG) { fprintf(stderr, "degree overflow\n"); exit(1); }
            g->nbr[u][k] = v; g->Q[u][k] = 0; g->deg[u]++;
        }
        g->Q[u][k] += w;
    }
}

static int new_var(net_t *g) { g->fixed[g->nv] = -1; return g->nv++; }

// A clamped 0 per use, so no single node ends up with an enormous fan-out.
static int new_zero(net_t *g) { int z = new_var(g); g->fixed[z] = 0; return z; }

static void gate_and(net_t *g, int a, int b, int y) {
    add_q(g, a, b, 1); add_q(g, a, y, -2); add_q(g, b, y, -2); g->c[y] += 3;
}

// Expands (sum_k w_k x_k)^2 for a + b + cin - s - 2 cout.
static void gate_fa(net_t *g, int a, int b, int cin, int s, int cout) {
    int v[5] = {a, b, cin, s, cout};
    double w[5] = {1, 1, 1, -1, -2};
    for (int i = 0; i < 5; i++) {
        g->c[v[i]] += w[i] * w[i];
        for (int j = i + 1; j < 5; j++) add_q(g, v[i], v[j], 2 * w[i] * w[j]);
    }
}

// n-bit x n-bit multiplier with ripple-carry rows; both factors have top and
// bottom bits fixed to 1, which is what an n-bit odd factor means.
static void build(net_t *g, int n, uint64_t N) {
    memset(g, 0, sizeof *g);
    g->n = n;
    for (int i = 0; i < n; i++) { g->p[i] = new_var(g); g->q[i] = new_var(g); }
    g->fixed[g->p[0]] = g->fixed[g->q[0]] = 1;
    g->fixed[g->p[n - 1]] = g->fixed[g->q[n - 1]] = 1;

    int acc[64]; // running sum, bit by bit
    for (int i = 0; i < 2 * n; i++) acc[i] = new_zero(g);

    for (int j = 0; j < n; j++) {
        int carry = new_zero(g);
        for (int i = 0; i < n; i++) {
            int a = new_var(g);
            gate_and(g, g->p[i], g->q[j], a);
            int s = new_var(g), co = new_var(g);
            gate_fa(g, acc[i + j], a, carry, s, co);
            acc[i + j] = s;
            carry = co;
        }
        // Ripple the leftover carry into the bits above.
        for (int k = n + j; k < 2 * n; k++) {
            int s = new_var(g), co = new_var(g);
            gate_fa(g, acc[k], carry, new_zero(g), s, co);
            acc[k] = s;
            carry = co;
        }
        g->fixed[carry] = 0; // no overflow out of 2n bits
    }
    for (int i = 0; i < 2 * n; i++) {
        if (g->fixed[acc[i]] >= 0 && g->fixed[acc[i]] != ((N >> i) & 1)) { fprintf(stderr, "clamp conflict\n"); exit(1); }
        g->fixed[acc[i]] = (N >> i) & 1;
    }

    // To Ising form: x = (1 + m) / 2. Field on m_i from the linear term is c_i/2
    // and from each coupling Q_ij x_i x_j is Q_ij/4 (m_i + m_j + m_i m_j).
    // We store E = -sum J m m - sum h m.
    for (int i = 0; i < g->nv; i++) {
        double s = g->c[i] / 2;
        for (int k = 0; k < g->deg[i]; k++) { g->J[i][k] = -g->Q[i][k] / 4; s += g->Q[i][k] / 4; }
        g->h[i] = -s;
    }
    g->nfree = 0;
    for (int i = 0; i < g->nv; i++) if (g->fixed[i] < 0) g->free_list[g->nfree++] = i;
}

static inline uint64_t read_int(const signed char *m, const int *bits, int n) {
    uint64_t v = 0;
    for (int i = 0; i < n; i++) v |= (uint64_t)(m[bits[i]] > 0) << i;
    return v;
}

static inline void sweep(const net_t *g, signed char *m, double beta, rng_t *r) {
    for (int f = 0; f < g->nfree; f++) {
        int i = g->free_list[f];
        double I = g->h[i];
        for (int k = 0; k < g->deg[i]; k++) I += g->J[i][k] * m[g->nbr[i][k]];
        m[i] = rng_sym(r) + tanh(beta * I) > 0 ? 1 : -1;
    }
}

static void randomise(const net_t *g, signed char *m, rng_t *r) {
    for (int i = 0; i < g->nv; i++)
        m[i] = g->fixed[i] >= 0 ? g->fixed[i] * 2 - 1 : (rng_u64(r) & 1) * 2 - 1;
}

static inline int solved(const net_t *g, const signed char *m, uint64_t N) {
    return read_int(m, g->p, g->n) * read_int(m, g->q, g->n) == N;
}

static double energy(const net_t *g, const signed char *m) {
    double e = 0;
    for (int f = 0; f < g->nfree; f++) {
        int i = g->free_list[f];
        e -= g->h[i] * m[i];
        for (int k = 0; k < g->deg[i]; k++) {
            int j = g->nbr[i][k];
            // free-free pairs counted once; free-clamped pairs always counted
            if (g->fixed[j] >= 0 || j > i) e -= g->J[i][k] * m[i] * m[j];
        }
    }
    return e;
}

#define REPLICAS 8

// Returns sweeps used, or -1 on timeout. beta is the fixed or final inverse temperature.
static long long solve(const net_t *g, uint64_t N, int mode, double beta, long long restart,
                       long long max_sweeps, rng_t *r) {
    static signed char ms[REPLICAS][MAXV];
    long long sweeps = 0;
    if (mode == 2) {
        signed char *rep[REPLICAS];
        double b[REPLICAS];
        for (int k = 0; k < REPLICAS; k++) {
            rep[k] = ms[k];
            randomise(g, rep[k], r);
            b[k] = 0.2 * pow(beta / 0.2, (double)k / (REPLICAS - 1)); // geometric ladder
        }
        for (;;) {
            for (int k = 0; k < REPLICAS; k++) {
                sweep(g, rep[k], b[k], r);
                sweeps++;
                if (solved(g, rep[k], N)) return sweeps;
                if (sweeps >= max_sweeps) return -1;
            }
            for (int k = 0; k + 1 < REPLICAS; k++) {
                double d = (b[k] - b[k + 1]) * (energy(g, rep[k]) - energy(g, rep[k + 1]));
                if (d >= 0 || (rng_sym(r) + 1) / 2 < exp(d)) { signed char *t = rep[k]; rep[k] = rep[k + 1]; rep[k + 1] = t; }
            }
        }
    }
    signed char *m = ms[0];
    for (;;) {
        randomise(g, m, r);
        for (long long t = 0; t < restart; t++) {
            sweep(g, m, mode == 1 ? beta * (t + 1) / restart : beta, r);
            sweeps++;
            if (solved(g, m, N)) return sweeps;
            if (sweeps >= max_sweeps) return -1;
        }
    }
}

static int is_prime(uint64_t x) {
    if (x < 2) return 0;
    for (uint64_t d = 2; d * d <= x; d++) if (x % d == 0) return 0;
    return 1;
}

// Odd candidates tried, from 3 up to the smaller factor.
static long long trial_division(uint64_t N, double *secs) {
    double t0 = now_s();
    long long tried = 0;
    volatile uint64_t sink = N;
    for (uint64_t d = 3; d * d <= sink; d += 2) { tried++; if (sink % d == 0) break; }
    *secs = now_s() - t0;
    return tried;
}

static int cmp_ll(const void *a, const void *b) {
    long long x = *(const long long *)a, y = *(const long long *)b;
    return (x > y) - (x < y);
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: factor <factor_bits> <mode> <cap_sweeps>\n"); return 2; }
    int n = atoi(argv[1]), mode = atoi(argv[2]);
    long long cap = atoll(argv[3]);
    int instances = 12, runs = 5, tune_trials = 6;
    static net_t g;
    rng_t r;
    rng_seed(&r, 2026 + 100 * n + mode);

    const double betas[] = {0.35, 0.5, 0.7, 1.0, 1.4, 2.0, 3.0};
    const long long restarts[] = {100, 1000, 10000, 100000, 1000000};
    int nb = 7, nr = mode == 2 ? 1 : 5;

    static uint64_t primes[200000]; int np = 0;
    for (uint64_t x = (1ULL << (n - 1)) | 1; x < (1ULL << n) && np < 200000; x += 2) if (is_prime(x)) primes[np++] = x;

    // Tuning gets its own random instances, and a tenth of the budget per trial.
    double best_beta = 1; long long best_restart = restarts[0]; double best_cost = 1e300;
    long long tune_cap = cap / 10;
    for (int bi = 0; bi < nb; bi++) for (int ri = 0; ri < nr; ri++) {
        double total = 0;
        for (int trial = 0; trial < tune_trials && total < best_cost; trial++) {
            uint64_t a = primes[rng_u64(&r) % np], b = primes[rng_u64(&r) % np];
            build(&g, n, a * b);
            long long s = solve(&g, a * b, mode, betas[bi], restarts[ri], tune_cap, &r);
            total += s < 0 ? tune_cap * 2.0 : s;
        }
        if (total < best_cost) { best_cost = total; best_beta = betas[bi]; best_restart = restarts[ri]; }
    }

    static long long sw[4096]; long long td[4096]; int ns = 0, fails = 0;
    double t_sampler = 0, t_td = 0, sum_sweeps = 0;
    int freebits = 0, total_nodes = 0;
    for (int inst = 0; inst < instances; inst++) {
        uint64_t a = primes[rng_u64(&r) % np], b = primes[rng_u64(&r) % np], N = a * b;
        build(&g, n, N);
        freebits = g.nfree; total_nodes = g.nv;
        double secs;
        td[inst] = trial_division(N, &secs);
        t_td += secs;
        for (int k = 0; k < runs; k++) {
            double t0 = now_s();
            long long s = solve(&g, N, mode, best_beta, best_restart, cap, &r);
            t_sampler += now_s() - t0;
            if (s < 0) { fails++; s = cap; }
            sw[ns++] = s;
            sum_sweeps += s;
        }
    }
    qsort(sw, ns, sizeof sw[0], cmp_ll);
    qsort(td, instances, sizeof td[0], cmp_ll);
    // Blind guessing: uniform over the 4^(n-2) free factor bit patterns; pq and qp both win
    // (taking the factors as distinct), so 4^(n-2)/2 guesses on average.
    double blind_mean = pow(4, n - 2) / 2;
    printf("{\"factor_bits\": %d, \"mode\": %d, \"free_pbits\": %d, \"nodes\": %d, \"beta\": %.2f, \"restart\": %lld, "
           "\"runs\": %d, \"timeouts\": %d, \"median_sweeps\": %lld, \"q25_sweeps\": %lld, \"q75_sweeps\": %lld, "
           "\"mean_sweeps\": %.6g, \"blind_mean_guesses\": %.6g, \"median_trial_div\": %lld, "
           "\"ns_per_update\": %.4g, \"sampler_secs_per_run\": %.6g, \"td_secs_per_inst\": %.6g}\n",
           n, mode, freebits, total_nodes, best_beta, best_restart, ns, fails, sw[ns / 2], sw[ns / 4], sw[3 * ns / 4],
           sum_sweeps / ns, blind_mean, td[instances / 2],
           1e9 * t_sampler / (sum_sweeps * freebits), t_sampler / ns, t_td / instances);
    return 0;
}
