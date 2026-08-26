import math
import time

PARAM = [30, 2000000, 2000000, 50000, 80000, 300, 200, 80, 50000, 40,
         20000, 500, 110, 10, 200000, 5000000, 400000, 16, 2000000, 100000, 20, 700000]


def fib(n):
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)


def forsum(n):
    s = 0
    for i in range(n):
        s = s + i * i
    return s


def intloop(n):
    s = 0
    i = 0
    while i < n:
        s = s + i * i - (i >> 3)
        i = i + 1
    return s


def collatz(limit):
    best = 0
    besti = 0
    i = 1
    while i < limit:
        n = i
        steps = 0
        while n != 1:
            if n % 2 == 0:
                n = n // 2
            else:
                n = 3 * n + 1
            steps = steps + 1
        if steps > best:
            best = steps
            besti = i
        i = i + 1
    return besti


def primes(n):
    count = 0
    i = 2
    while i < n:
        j = 2
        isp = 1
        while j * j <= i:
            if i % j == 0:
                isp = 0
                break
            j = j + 1
        count = count + isp
        i = i + 1
    return count


def mandel(w, h, maxit):
    total = 0
    for y in range(h):
        ci = y * 2.0 / h - 1.0
        for x in range(w):
            cr = x * 3.0 / w - 2.0
            zr = 0.0
            zi = 0.0
            k = 0
            while k < maxit:
                zr2 = zr * zr
                zi2 = zi * zi
                if zr2 + zi2 > 4.0:
                    break
                zi = 2.0 * zr * zi + ci
                zr = zr2 - zi2 + cr
                k = k + 1
            total = total + k
    return total


def listsum(data, reps):
    s = 0
    n = len(data)
    r = 0
    while r < reps:
        i = 0
        while i < n:
            s = s + data[i]
            i = i + 1
        r = r + 1
    return s


def nbody(steps):
    n = 5
    x = [0.0] * n
    y = [0.0] * n
    vx = [0.0] * n
    vy = [0.0] * n
    m = [0.0] * n
    i = 0
    while i < n:
        x[i] = float(i) + 1.0
        y[i] = float(i) * 0.5
        m[i] = 1.0 + float(i) * 0.1
        i = i + 1
    dt = 0.001
    s = 0
    while s < steps:
        i = 0
        while i < n:
            fx = 0.0
            fy = 0.0
            j = 0
            while j < n:
                if j != i:
                    dx = x[j] - x[i]
                    dy = y[j] - y[i]
                    d2 = dx * dx + dy * dy + 0.01
                    inv = m[j] / (d2 * math.sqrt(d2))
                    fx = fx + dx * inv
                    fy = fy + dy * inv
                j = j + 1
            vx[i] = vx[i] + fx * dt
            vy[i] = vy[i] + fy * dt
            i = i + 1
        i = 0
        while i < n:
            x[i] = x[i] + vx[i] * dt
            y[i] = y[i] + vy[i] * dt
            i = i + 1
        s = s + 1
    e = 0.0
    i = 0
    while i < n:
        e = e + m[i] * (vx[i] * vx[i] + vy[i] * vy[i])
        i = i + 1
    return e


def spectral(n):
    u = [1.0] * n
    v = [0.0] * n
    it = 0
    while it < 10:
        i = 0
        while i < n:
            t = 0.0
            j = 0
            while j < n:
                t = t + u[j] / float((i + j) * (i + j + 1) // 2 + i + 1)
                j = j + 1
            v[i] = t
            i = i + 1
        i = 0
        while i < n:
            t = 0.0
            j = 0
            while j < n:
                t = t + v[j] / float((j + i) * (j + i + 1) // 2 + j + 1)
                j = j + 1
            u[i] = t
            i = i + 1
        it = it + 1
    a = 0.0
    b = 0.0
    i = 0
    while i < n:
        a = a + u[i] * v[i]
        b = b + v[i] * v[i]
        i = i + 1
    return math.sqrt(a / b)


def matmul(n):
    a = []
    b = []
    i = 0
    while i < n:
        ra = [0.0] * n
        rb = [0.0] * n
        j = 0
        while j < n:
            ra[j] = float(i * n + j) * 0.5
            rb[j] = float(j * n + i) * 0.25
            j = j + 1
        a.append(ra)
        b.append(rb)
        i = i + 1
    c = 0.0
    i = 0
    while i < n:
        ai = a[i]
        j = 0
        while j < n:
            t = 0.0
            k = 0
            while k < n:
                t = t + ai[k] * b[k][j]
                k = k + 1
            c = c + t
            j = j + 1
        i = i + 1
    return c


def queens(n):
    cols = [0] * n
    return qsolve(cols, 0, n)


def qsolve(cols, row, n):
    if row == n:
        return 1
    count = 0
    c = 0
    while c < n:
        ok = 1
        r = 0
        while r < row:
            d = cols[r] - c
            if d < 0:
                d = -d
            if cols[r] == c or d == row - r:
                ok = 0
                break
            r = r + 1
        if ok == 1:
            cols[row] = c
            count = count + qsolve(cols, row + 1, n)
        c = c + 1
    return count


def heapsort(n):
    a = [0] * n
    seed = 12345
    i = 0
    while i < n:
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        a[i] = seed
        i = i + 1
    i = n // 2 - 1
    while i >= 0:
        sift(a, i, n)
        i = i - 1
    e = n - 1
    while e > 0:
        t = a[0]
        a[0] = a[e]
        a[e] = t
        sift(a, 0, e)
        e = e - 1
    s = 0
    i = 0
    while i < n:
        s = (s + a[i] * (i + 1)) & 0x7FFFFFFF
        i = i + 1
    return s


def sift(a, root, end):
    while 1:
        c = root * 2 + 1
        if c >= end:
            break
        if c + 1 < end and a[c] < a[c + 1]:
            c = c + 1
        if a[root] >= a[c]:
            break
        t = a[root]
        a[root] = a[c]
        a[c] = t
        root = c


def bitops(n):
    x = 1
    i = 0
    while i < n:
        x = x ^ (i * 2654435761)
        x = ((x << 7) | (x >> 25)) & 0xFFFFFFFF
        x = x + (x >> 11)
        x = x & 0xFFFFFFFF
        i = i + 1
    return x


def gcdloop(n):
    s = 0
    i = 1
    while i < n:
        a = i
        b = n - i
        while b != 0:
            t = a % b
            a = b
            b = t
        s = s + a
        i = i + 1
    return s


def btree(d):
    if d == 0:
        return (None, None)
    return (btree(d - 1), btree(d - 1))


def bcheck(t):
    if t[0] is None:
        return 1
    return 1 + bcheck(t[0]) + bcheck(t[1])


def binarytrees(d):
    s = 0
    i = 0
    while i < 4:
        s = s + bcheck(btree(d))
        i = i + 1
    return s


def sieve(n):
    f = [1] * n
    i = 2
    while i * i < n:
        if f[i] == 1:
            j = i * i
            while j < n:
                f[j] = 0
                j = j + i
        i = i + 1
    c = 0
    i = 2
    while i < n:
        c = c + f[i]
        i = i + 1
    return c


def dotprod(n, reps):
    a = [0.0] * n
    b = [0.0] * n
    i = 0
    while i < n:
        a[i] = float(i) * 0.5
        b[i] = float(n - i) * 0.25
        i = i + 1
    s = 0.0
    r = 0
    while r < reps:
        t = 0.0
        i = 0
        while i < n:
            t = t + a[i] * b[i]
            i = i + 1
        s = s + t
        r = r + 1
    return s


def trig(n):
    s = 0.0
    i = 0
    while i < n:
        x = float(i) * 0.0001
        s = s + math.sin(x) * math.cos(x) + math.sqrt(x + 1.0)
        i = i + 1
    return s


def fannkuch(n):
    perm = [0] * n
    perm1 = [0] * n
    count = [0] * n
    i = 0
    while i < n:
        perm1[i] = i
        i = i + 1
    maxflips = 0
    checksum = 0
    r = n
    sign = 1
    while 1:
        while r != 1:
            count[r - 1] = r
            r = r - 1
        i = 0
        while i < n:
            perm[i] = perm1[i]
            i = i + 1
        flips = 0
        k = perm[0]
        while k != 0:
            lo = 0
            hi = k
            while lo < hi:
                t = perm[lo]
                perm[lo] = perm[hi]
                perm[hi] = t
                lo = lo + 1
                hi = hi - 1
            flips = flips + 1
            k = perm[0]
        if flips > maxflips:
            maxflips = flips
        checksum = checksum + sign * flips
        sign = -sign
        while 1:
            if r == n:
                return checksum * 1000000 + maxflips
            p0 = perm1[0]
            i = 0
            while i < r:
                perm1[i] = perm1[i + 1]
                i = i + 1
            perm1[r] = p0
            count[r] = count[r] - 1
            if count[r] > 0:
                break
            r = r + 1


def taylor(n):
    s = 0.0
    k = 1
    sign = 1.0
    while k <= n:
        s = s + sign / float(2 * k - 1)
        sign = -sign
        k = k + 1
    return 4.0 * s


def strhash(n):
    h = 0
    i = 0
    while i < n:
        s = f"{i}:{i * i}"
        j = 0
        m = len(s)
        while j < m:
            h = (h * 31 + ord(s[j])) & 0xFFFFFFFF
            j = j + 1
        i = i + 1
    return h


def dictops(n):
    d = {}
    i = 0
    while i < n:
        d[i] = i * 7 % n
        i = i + 1
    s = 0
    i = 0
    k = 1
    while i < n:
        k = (k * 1103515245 + 12345) % n
        s = s + d[k]
        i = i + 1
    return s


CASES = (
    ("fib", fib, (PARAM[0],), 1),
    ("forsum", forsum, (PARAM[1],), 1),
    ("intloop", intloop, (PARAM[2],), 1),
    ("collatz", collatz, (PARAM[3],), 1),
    ("primes", primes, (PARAM[4],), 1),
    ("mandel", mandel, (PARAM[5], PARAM[6], PARAM[7]), 1),
    ("listsum", listsum, (list(range(PARAM[8])), PARAM[9]), 1),
    ("nbody", nbody, (PARAM[10],), 1),
    ("spectral", spectral, (PARAM[11],), 1),
    ("matmul", matmul, (PARAM[12],), 1),
    ("queens", queens, (PARAM[13],), 1),
    ("heapsort", heapsort, (PARAM[14],), 1),
    ("bitops", bitops, (PARAM[15],), 1),
    ("gcdloop", gcdloop, (PARAM[16],), 1),
    ("binarytrees", binarytrees, (PARAM[17],), 1),
    ("sieve", sieve, (PARAM[18],), 1),
    ("dotprod", dotprod, (PARAM[19], 20), 1),
    ("trig", trig, (PARAM[20] * 50000,), 1),
    ("fannkuch", fannkuch, (9,), 1),
    ("taylor", taylor, (10000000,), 1),
    ("strhash", strhash, (PARAM[21],), 1),
    ("dictops", dictops, (PARAM[21],), 1),
)


def main():
    total = 0.0
    for name, fn, args, reps in CASES:
        best = -1.0
        r = 0
        for _trial in range(3):
            t0 = time.perf_counter()
            k = 0
            while k < reps:
                r = fn(*args)
                k = k + 1
            dt = (time.perf_counter() - t0) * 1000.0
            if best < 0.0 or dt < best:
                best = dt
        total += best
        if isinstance(r, float):
            print("%-14s %10.2f  %.6f" % (name, best, r))
        else:
            print("%-14s %10.2f  %d" % (name, best, r))
    print("%-14s %10.2f" % ("TOTAL", total))


main()
