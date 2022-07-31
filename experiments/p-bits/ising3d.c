// How fast is a plain CPU at the p-computer benchmark? A 3D +-1 Edwards-Anderson
// spin glass (degree 6) on a chessboard schedule, one thread, nothing vectorised.
// Measures p-bit updates per second, which is what the FPGA papers call flips/s.
//
// clang -O2 -o ising3d ising3d.c -lm && ./ising3d
#include "pbit.h"

int main(void) {
    const int sizes[] = {37, 100};
    for (int si = 0; si < 2; si++) {
        int L = sizes[si];
        long n = (long)L * L * L;
        signed char *m = malloc(n), *Jx = malloc(n), *Jy = malloc(n), *Jz = malloc(n);
        rng_t r; rng_seed(&r, 7);
        for (long i = 0; i < n; i++) {
            m[i] = (rng_u64(&r) & 1) * 2 - 1;
            Jx[i] = (rng_u64(&r) & 1) * 2 - 1;
            Jy[i] = (rng_u64(&r) & 1) * 2 - 1;
            Jz[i] = (rng_u64(&r) & 1) * 2 - 1;
        }
        double beta = 1.0; // p(+1) as a 64-bit threshold, one entry per possible field
        uint64_t table[13];
        for (int I = -6; I <= 6; I++) table[I + 6] = (uint64_t)(0.5 * (1 + tanh(beta * I)) * 0x1p63) * 2;
        int even = L % 2 == 0, sweeps = L == 37 ? 400 : 40;
        double t0 = now_s();
        long long updates = 0, sum = 0;
        for (int t = 0; t < sweeps; t++) {
            for (int colour = 0; colour < 2; colour++) {
                for (int x = 0; x < L; x++) for (int y = 0; y < L; y++) for (int z = 0; z < L; z++) {
                    if ((x + y + z) % 2 != colour) continue;
                    long i = ((long)x * L + y) * L + z;
                    int xm = x ? x - 1 : L - 1, xp = x + 1 < L ? x + 1 : 0;
                    int ym = y ? y - 1 : L - 1, yp = y + 1 < L ? y + 1 : 0;
                    int zm = z ? z - 1 : L - 1, zp = z + 1 < L ? z + 1 : 0;
                    long ixm = ((long)xm * L + y) * L + z, ixp = ((long)xp * L + y) * L + z;
                    long iym = ((long)x * L + ym) * L + z, iyp = ((long)x * L + yp) * L + z;
                    long izm = ((long)x * L + y) * L + zm, izp = ((long)x * L + y) * L + zp;
                    int I = Jx[i] * m[ixp] + Jx[ixm] * m[ixm] + Jy[i] * m[iyp] + Jy[iym] * m[iym] + Jz[i] * m[izp] + Jz[izm] * m[izm];
                    m[i] = rng_u64(&r) < table[I + 6] ? 1 : -1;
                    updates++;
                }
            }
        }
        double dt = now_s() - t0;
        for (long i = 0; i < n; i++) sum += m[i];
        printf("L=%d N=%ld even=%d: %.3g updates/s (%.2f ns/update), single thread, sum=%lld\n", L, n, even, updates / dt, 1e9 * dt / updates, sum);
    }
    return 0;
}
