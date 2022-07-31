// A plain loop: what does the compiler make of it?
long sum_ints(const int *a, long n) {
    long s = 0;
    for (long i = 0; i < n; i++)
        s += a[i];
    return s;
}

double sum_doubles(const double *a, long n) {
    double s = 0.0;
    for (long i = 0; i < n; i++)
        s += a[i];
    return s;
}
