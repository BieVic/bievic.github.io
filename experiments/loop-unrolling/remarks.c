// Ask clang to explain its unrolling decisions: clang -O2 -Rpass=loop-unroll -Rpass-missed=loop-unroll
long sum_dynamic(const int *a, long n) {          // trip count only known at run time
    long s = 0;
    for (long i = 0; i < n; i++) s += a[i];
    return s;
}

long sum_fixed(const int *a) {                    // trip count known at compile time
    long s = 0;
    for (int i = 0; i < 8; i++) s += a[i];
    return s;
}

long sum_pragma(const int *a, long n) {           // the programmer insists
    long s = 0;
#pragma clang loop vectorize(disable) unroll_count(4)
    for (long i = 0; i < n; i++) s += a[i];
    return s;
}

long sum_big_body(const int *a, long n) {         // a large body: is unrolling still worth it?
    long s = 0;
    for (long i = 0; i < n; i++) {
        long x = a[i];
        s += (x * x) ^ (x >> 3) ^ (x * 7 + 1) ^ (x * x * x) ^ (x << 5) ^ (x * 13 - 2)
           ^ (x * 21 + 3) ^ (x * 34 - 5) ^ (x * 55 + 8) ^ (x * 89 - 13) ^ (x * 144 + 21);
    }
    return s;
}
