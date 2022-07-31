# Loop unrolling

Benchmark code for the blog post "Loop unrolling, or: what this site is named after".

- `sum.c`: the plain integer and double sums; compile with `-S` to look at the generated assembly
- `bench.c`: summing doubles with 1, 2, 4 and 8 independent accumulators
- `bench_ints.c`: the integer sum, to compare unrolling and vectorisation settings
- `bench_regs.c`: 1 to 64 independent accumulators, to find where the registers run out
- `remarks.c`: three small loops to ask clang about its unrolling decisions

```sh
clang -O2 -o bench bench.c && ./bench
clang -O2 -ffast-math -o bench bench.c && ./bench
clang -O2 -fno-vectorize -fno-unroll-loops -o bench_ints bench_ints.c && ./bench_ints
clang -O2 -fno-vectorize -o bench_ints bench_ints.c && ./bench_ints
clang -O2 -o bench_ints bench_ints.c && ./bench_ints
clang -O2 -S -o - sum.c   # the assembly
clang -O2 -o bench_regs bench_regs.c && ./bench_regs
clang -O2 -S -o - bench_regs.c | grep -i spill   # the 64-accumulator spills
clang -O2 -fno-vectorize -Rpass=loop-unroll -c -o /dev/null remarks.c
clang -O2 -Rpass=slp-vectorizer -c -o /dev/null remarks.c
```

The numbers in the post were measured on an Apple M2 with Homebrew clang 23.1.4.
