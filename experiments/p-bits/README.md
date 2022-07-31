# p-bits

Code for the blog post "A weighted coin with an input wire".

- `pbit.h`: a PRNG and a timer, shared by everything else
- `and_gate.c`: the invertible AND gate from Camsari et al. (2017), sampled with the p-bit rule and compared against exact Boltzmann probabilities
- `factor.c`: factoring by clamping the output of a p-bit array multiplier, with three samplers
- `run_factor.sh`: runs `factor` over every size and sampler, six at a time. **It doesn't finish in reasonable time.** I stopped it after three hours; `results/factor/` holds what had finished (Gibbs to n=9, tempering to n=10, annealing to n=11, nothing at n=12)
- `cold_read.c`: reads only the state at the end of each anneal. `factor.c` checks every sweep, which rewards a hot sampler for blind guessing, so this is the test the post relies on. One core, about seven minutes
- `analyse.py`: merges the results and fits the growth rate
- `lfsr.c`: a full adder sampled with xoshiro and three LFSR arrangements, against the exact distribution
- `ising3d.c`: p-bit updates per second on one CPU core, on a 3D spin-glass lattice

```sh
clang -O2 -o and_gate and_gate.c -lm && ./and_gate
./run_factor.sh && python3 analyse.py   # read the note above first
clang -O2 -o cold_read cold_read.c -lm && ./cold_read > results/cold_read.json
clang -O2 -o lfsr lfsr.c -lm && ./lfsr > results/lfsr.json
clang -O2 -o ising3d ising3d.c -lm && ./ising3d
```

The numbers in the post were measured on an Apple M2 with Homebrew clang 23.1.4. `results/` holds the output of the runs the post quotes. Sweep counts are deterministic for a given seed; timings aren't.
