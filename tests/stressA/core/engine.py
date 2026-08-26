import math


def fib(n):
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)


def grid_sum(n):
    g = []
    i = 0
    while i < n:
        row = [0.0] * n
        j = 0
        while j < n:
            row[j] = float(i * n + j) * 0.25
            j = j + 1
        g.append(row)
        i = i + 1
    s = 0.0
    i = 0
    while i < n:
        r = g[i]
        j = 0
        while j < n:
            s = s + r[j] * math.sqrt(float(j) + 1.0)
            j = j + 1
        i = i + 1
    return round(s, 4)


def histogram(n):
    h = [0] * 64
    x = 99991
    i = 0
    while i < n:
        x = (x * 1103515245 + 12345) & 0x7FFFFFFF
        h[x & 63] += 1
        i = i + 1
    s = 0
    for i in range(64):
        s = s + h[i] * (i + 1)
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


def clamp_all(lo, hi):
    s = 0
    for v in range(-100, 300, 7):
        s = s + max(lo, min(v, hi))
    return s


def acc_overflow(n):
    s = 500000000000000000
    i = 0
    while i < n:
        s = s + 500000000000000000
        i = i + 1
    return s


def mixed_ranges(n):
    a = [0] * n
    for i in range(n):
        a[i] = (i % 251) - 120
    total = 0
    for i in range(0, n, 3):
        total = total + a[i]
    back = 0
    for i in range(n - 1, -1, -1):
        back = back + a[i] * 2
    return total, back


def row_ops(n):
    m = []
    i = 0
    while i < n:
        r = [1] * n
        m.append(r)
        i = i + 1
    i = 0
    while i < n:
        m[i][i] += i
        i = i + 1
    row = m[2]
    row[0] += 1000
    s = 0
    i = 0
    while i < n:
        rr = m[i]
        j = 0
        while j < n:
            s = s + rr[j]
            j = j + 1
        i = i + 1
    return s, m[2][0]
