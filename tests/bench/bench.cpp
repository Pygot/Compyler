#include <cstdio>
#include <cmath>
#include <cstdint>
#include <chrono>
#include <vector>
#include <unordered_map>

typedef long long i64;

static volatile i64 PARAM[22] = {30, 2000000, 2000000, 50000, 80000, 300, 200, 80, 50000, 40,
                                 20000, 500, 110, 10, 200000, 5000000, 400000, 16, 2000000, 100000, 20, 700000};

static i64 fib(i64 n)
{
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

static i64 forsum(i64 n)
{
    i64 s = 0;
    for (i64 i = 0; i < n; i++) s = s + i * i;
    return s;
}

static i64 intloop(i64 n)
{
    i64 s = 0, i = 0;
    while (i < n) { s = s + i * i - (i >> 3); i = i + 1; }
    return s;
}

static i64 collatz(i64 limit)
{
    i64 best = 0, besti = 0, i = 1;
    while (i < limit) {
        i64 n = i, steps = 0;
        while (n != 1) {
            if (n % 2 == 0) n = n / 2; else n = 3 * n + 1;
            steps = steps + 1;
        }
        if (steps > best) { best = steps; besti = i; }
        i = i + 1;
    }
    return besti;
}

static i64 primes(i64 n)
{
    i64 count = 0, i = 2;
    while (i < n) {
        i64 j = 2, isp = 1;
        while (j * j <= i) {
            if (i % j == 0) { isp = 0; break; }
            j = j + 1;
        }
        count = count + isp;
        i = i + 1;
    }
    return count;
}

static i64 mandel(i64 w, i64 h, i64 maxit)
{
    i64 total = 0;
    for (i64 y = 0; y < h; y++) {
        double ci = (double)y * 2.0 / (double)h - 1.0;
        for (i64 x = 0; x < w; x++) {
            double cr = (double)x * 3.0 / (double)w - 2.0;
            double zr = 0.0, zi = 0.0;
            i64 k = 0;
            while (k < maxit) {
                double zr2 = zr * zr, zi2 = zi * zi;
                if (zr2 + zi2 > 4.0) break;
                zi = 2.0 * zr * zi + ci;
                zr = zr2 - zi2 + cr;
                k = k + 1;
            }
            total = total + k;
        }
    }
    return total;
}

static i64 listsum(std::vector<i64> &data, i64 reps)
{
    i64 s = 0, n = (i64)data.size(), r = 0;
    while (r < reps) {
        i64 i = 0;
        while (i < n) { s = s + data[(size_t)i]; i = i + 1; }
        r = r + 1;
    }
    return s;
}

static double nbody(i64 steps)
{
    const int n = 5;
    double x[n] = {0}, y[n] = {0}, vx[n] = {0}, vy[n] = {0}, m[n] = {0};
    for (int i = 0; i < n; i++) {
        x[i] = (double)i + 1.0;
        y[i] = (double)i * 0.5;
        m[i] = 1.0 + (double)i * 0.1;
    }
    double dt = 0.001;
    for (i64 s = 0; s < steps; s++) {
        for (int i = 0; i < n; i++) {
            double fx = 0.0, fy = 0.0;
            for (int j = 0; j < n; j++) {
                if (j != i) {
                    double dx = x[j] - x[i], dy = y[j] - y[i];
                    double d2 = dx * dx + dy * dy + 0.01;
                    double inv = m[j] / (d2 * sqrt(d2));
                    fx = fx + dx * inv;
                    fy = fy + dy * inv;
                }
            }
            vx[i] = vx[i] + fx * dt;
            vy[i] = vy[i] + fy * dt;
        }
        for (int i = 0; i < n; i++) { x[i] = x[i] + vx[i] * dt; y[i] = y[i] + vy[i] * dt; }
    }
    double e = 0.0;
    for (int i = 0; i < n; i++) e = e + m[i] * (vx[i] * vx[i] + vy[i] * vy[i]);
    return e;
}

static double spectral(i64 n)
{
    std::vector<double> u((size_t)n, 1.0), v((size_t)n, 0.0);
    for (int it = 0; it < 10; it++) {
        for (i64 i = 0; i < n; i++) {
            double t = 0.0;
            for (i64 j = 0; j < n; j++) t = t + u[(size_t)j] / (double)((i + j) * (i + j + 1) / 2 + i + 1);
            v[(size_t)i] = t;
        }
        for (i64 i = 0; i < n; i++) {
            double t = 0.0;
            for (i64 j = 0; j < n; j++) t = t + v[(size_t)j] / (double)((j + i) * (j + i + 1) / 2 + j + 1);
            u[(size_t)i] = t;
        }
    }
    double a = 0.0, b = 0.0;
    for (i64 i = 0; i < n; i++) { a = a + u[(size_t)i] * v[(size_t)i]; b = b + v[(size_t)i] * v[(size_t)i]; }
    return sqrt(a / b);
}

static double matmul(i64 n)
{
    std::vector<std::vector<double> > a, b;
    for (i64 i = 0; i < n; i++) {
        std::vector<double> ra((size_t)n, 0.0), rb((size_t)n, 0.0);
        for (i64 j = 0; j < n; j++) {
            ra[(size_t)j] = (double)(i * n + j) * 0.5;
            rb[(size_t)j] = (double)(j * n + i) * 0.25;
        }
        a.push_back(ra);
        b.push_back(rb);
    }
    double c = 0.0;
    for (i64 i = 0; i < n; i++) {
        std::vector<double> &ai = a[(size_t)i];
        for (i64 j = 0; j < n; j++) {
            double t = 0.0;
            for (i64 k = 0; k < n; k++) t = t + ai[(size_t)k] * b[(size_t)k][(size_t)j];
            c = c + t;
        }
    }
    return c;
}

static i64 qsolve(std::vector<i64> &cols, i64 row, i64 n)
{
    if (row == n) return 1;
    i64 count = 0;
    for (i64 c = 0; c < n; c++) {
        i64 ok = 1;
        for (i64 r = 0; r < row; r++) {
            i64 d = cols[(size_t)r] - c;
            if (d < 0) d = -d;
            if (cols[(size_t)r] == c || d == row - r) { ok = 0; break; }
        }
        if (ok == 1) {
            cols[(size_t)row] = c;
            count = count + qsolve(cols, row + 1, n);
        }
    }
    return count;
}

static i64 queens(i64 n)
{
    std::vector<i64> cols((size_t)n, 0);
    return qsolve(cols, 0, n);
}

static void sift(std::vector<i64> &a, i64 root, i64 end)
{
    for (;;) {
        i64 c = root * 2 + 1;
        if (c >= end) break;
        if (c + 1 < end && a[(size_t)c] < a[(size_t)(c + 1)]) c = c + 1;
        if (a[(size_t)root] >= a[(size_t)c]) break;
        i64 t = a[(size_t)root];
        a[(size_t)root] = a[(size_t)c];
        a[(size_t)c] = t;
        root = c;
    }
}

static i64 heapsort(i64 n)
{
    std::vector<i64> a((size_t)n, 0);
    i64 seed = 12345;
    for (i64 i = 0; i < n; i++) {
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        a[(size_t)i] = seed;
    }
    for (i64 i = n / 2 - 1; i >= 0; i--) sift(a, i, n);
    for (i64 e = n - 1; e > 0; e--) {
        i64 t = a[0];
        a[0] = a[(size_t)e];
        a[(size_t)e] = t;
        sift(a, 0, e);
    }
    i64 s = 0;
    for (i64 i = 0; i < n; i++) s = (s + a[(size_t)i] * (i + 1)) & 0x7FFFFFFF;
    return s;
}

static i64 bitops(i64 n)
{
    i64 x = 1;
    for (i64 i = 0; i < n; i++) {
        x = x ^ (i * 2654435761LL);
        x = ((x << 7) | (x >> 25)) & 0xFFFFFFFFLL;
        x = x + (x >> 11);
        x = x & 0xFFFFFFFFLL;
    }
    return x;
}

static i64 gcdloop(i64 n)
{
    i64 s = 0;
    for (i64 i = 1; i < n; i++) {
        i64 a = i, b = n - i;
        while (b != 0) { i64 t = a % b; a = b; b = t; }
        s = s + a;
    }
    return s;
}

struct Node { Node *l, *r; };

static Node *btree(i64 d)
{
    Node *n = new Node;
    if (d == 0) { n->l = 0; n->r = 0; return n; }
    n->l = btree(d - 1);
    n->r = btree(d - 1);
    return n;
}

static i64 bcheck(Node *t)
{
    if (!t->l) return 1;
    return 1 + bcheck(t->l) + bcheck(t->r);
}

static void bfree(Node *t)
{
    if (t->l) { bfree(t->l); bfree(t->r); }
    delete t;
}

static i64 binarytrees(i64 d)
{
    i64 s = 0;
    for (int i = 0; i < 4; i++) {
        Node *t = btree(d);
        s = s + bcheck(t);
        bfree(t);
    }
    return s;
}

static i64 sieve(i64 n)
{
    std::vector<char> f((size_t)n, 1);
    for (i64 i = 2; i * i < n; i++) {
        if (f[(size_t)i] == 1) {
            for (i64 j = i * i; j < n; j += i) f[(size_t)j] = 0;
        }
    }
    i64 c = 0;
    for (i64 i = 2; i < n; i++) c = c + f[(size_t)i];
    return c;
}

static double dotprod(i64 n, i64 reps)
{
    std::vector<double> a((size_t)n, 0.0), b((size_t)n, 0.0);
    for (i64 i = 0; i < n; i++) {
        a[(size_t)i] = (double)i * 0.5;
        b[(size_t)i] = (double)(n - i) * 0.25;
    }
    double s = 0.0;
    for (i64 r = 0; r < reps; r++) {
        double t = 0.0;
        for (i64 i = 0; i < n; i++) t = t + a[(size_t)i] * b[(size_t)i];
        s = s + t;
    }
    return s;
}

static double trig(i64 n)
{
    double s = 0.0;
    for (i64 i = 0; i < n; i++) {
        double x = (double)i * 0.0001;
        s = s + sin(x) * cos(x) + sqrt(x + 1.0);
    }
    return s;
}

static i64 fannkuch(i64 n)
{
    std::vector<i64> perm((size_t)n, 0), perm1((size_t)n, 0), count((size_t)n, 0);
    i64 i, maxflips = 0, checksum = 0, r = n, sign = 1;
    for (i = 0; i < n; i++) perm1[(size_t)i] = i;
    for (;;) {
        while (r != 1) { count[(size_t)(r - 1)] = r; r = r - 1; }
        for (i = 0; i < n; i++) perm[(size_t)i] = perm1[(size_t)i];
        i64 flips = 0, k = perm[0];
        while (k != 0) {
            i64 lo = 0, hi = k;
            while (lo < hi) {
                i64 t = perm[(size_t)lo];
                perm[(size_t)lo] = perm[(size_t)hi];
                perm[(size_t)hi] = t;
                lo = lo + 1;
                hi = hi - 1;
            }
            flips = flips + 1;
            k = perm[0];
        }
        if (flips > maxflips) maxflips = flips;
        checksum = checksum + sign * flips;
        sign = -sign;
        for (;;) {
            if (r == n) return checksum * 1000000 + maxflips;
            i64 p0 = perm1[0];
            for (i = 0; i < r; i++) perm1[(size_t)i] = perm1[(size_t)(i + 1)];
            perm1[(size_t)r] = p0;
            count[(size_t)r] = count[(size_t)r] - 1;
            if (count[(size_t)r] > 0) break;
            r = r + 1;
        }
    }
}

static double taylor(i64 n)
{
    double s = 0.0, sign = 1.0;
    i64 k = 1;
    while (k <= n) {
        s = s + sign / (double)(2 * k - 1);
        sign = -sign;
        k = k + 1;
    }
    return 4.0 * s;
}

static i64 strhash(i64 n)
{
    i64 h = 0;
    char buf[64];
    for (i64 i = 0; i < n; i++) {
        int m = snprintf(buf, sizeof(buf), "%lld:%lld", i, i * i);
        for (int j = 0; j < m; j++) h = (h * 31 + (unsigned char)buf[j]) & 0xFFFFFFFFLL;
    }
    return h;
}

static i64 dictops(i64 n)
{
    std::unordered_map<i64, i64> d;
    for (i64 i = 0; i < n; i++) d[i] = i * 7 % n;
    i64 s = 0, k = 1;
    for (i64 i = 0; i < n; i++) { k = (k * 1103515245 + 12345) % n; s = s + d[k]; }
    return s;
}

static double now_ms(void)
{
    using namespace std::chrono;
    return (double)duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count() / 1e6;
}

static double TOTAL = 0.0;

static void report_i(const char *name, double ms, i64 r)
{
    TOTAL += ms;
    printf("%-14s %10.2f  %lld\n", name, ms, r);
}

static void report_f(const char *name, double ms, double r)
{
    TOTAL += ms;
    printf("%-14s %10.2f  %.6f\n", name, ms, r);
}

#define TIME_I(name, expr)                                              \
    do {                                                                \
        double best = -1.0; i64 r = 0;                                  \
        for (int t = 0; t < 3; t++) {                                   \
            double t0 = now_ms(); r = (expr);                           \
            double dt = now_ms() - t0;                                  \
            if (best < 0.0 || dt < best) best = dt;                     \
        }                                                               \
        report_i(name, best, r);                                        \
    } while (0)

#define TIME_F(name, expr)                                              \
    do {                                                                \
        double best = -1.0, r = 0.0;                                    \
        for (int t = 0; t < 3; t++) {                                   \
            double t0 = now_ms(); r = (expr);                           \
            double dt = now_ms() - t0;                                  \
            if (best < 0.0 || dt < best) best = dt;                     \
        }                                                               \
        report_f(name, best, r);                                        \
    } while (0)

static i64 ZERO_I;

int main(void)
{
    ZERO_I = 0;
    std::vector<i64> data((size_t)PARAM[8]);
    for (i64 i = 0; i < PARAM[8]; i++) data[(size_t)i] = i;

    TIME_I("fib", fib(PARAM[0]));
    TIME_I("forsum", forsum(PARAM[1]));
    TIME_I("intloop", intloop(PARAM[2]));
    TIME_I("collatz", collatz(PARAM[3]));
    TIME_I("primes", primes(PARAM[4]));
    TIME_I("mandel", mandel(PARAM[5], PARAM[6], PARAM[7]));
    TIME_I("listsum", listsum(data, PARAM[9]));
    TIME_F("nbody", nbody(PARAM[10]));
    TIME_F("spectral", spectral(PARAM[11]));
    TIME_F("matmul", matmul(PARAM[12]));
    TIME_I("queens", queens(PARAM[13]));
    TIME_I("heapsort", heapsort(PARAM[14]));
    TIME_I("bitops", bitops(PARAM[15]));
    TIME_I("gcdloop", gcdloop(PARAM[16]));
    TIME_I("binarytrees", binarytrees(PARAM[17]));
    TIME_I("sieve", sieve(PARAM[18]));
    TIME_F("dotprod", dotprod(PARAM[19], 20));
    TIME_F("trig", trig(PARAM[20] * 50000));
    TIME_I("fannkuch", fannkuch(9 + ZERO_I));
    TIME_F("taylor", taylor(10000000 + ZERO_I));
    TIME_I("strhash", strhash(PARAM[21]));
    TIME_I("dictops", dictops(PARAM[21]));

    printf("%-14s %10.2f\n", "TOTAL", TOTAL);
    return 0;
}
