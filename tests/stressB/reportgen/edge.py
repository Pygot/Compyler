import math
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    import decimal


def shadowed_len(a):
    len = lambda x: 999
    return len(a)


def monkey_sqrt():
    real = math.sqrt
    vals = []
    vals.append(round(math.sqrt(49.0), 3))
    math.sqrt = lambda x: -1.0
    vals.append(round(math.sqrt(49.0), 3))
    math.sqrt = real
    vals.append(round(math.sqrt(2.0), 6))
    return vals


def neg_index_param(a, i, n):
    if i < n:
        return a[i]
    return -1


def big_acc(n):
    s = 3
    i = 0
    while i < n:
        s = s * 999999937
        i = i + 1
    return s


def aug_map(n):
    d = {}
    i = 0
    while i < 11:
        d[i] = 0
        i = i + 1
    i = 0
    while i < n:
        d[i % 11] += i * 3
        i = i + 1
    s = 0
    i = 0
    while i < 11:
        s = s + d[i] * (i + 1)
        i = i + 1
    return s


def cond_rows(n, flag):
    a = []
    b = []
    i = 0
    while i < n:
        ra = [i] * n
        rb = [float(i) + 0.5] * n
        a.append(ra)
        b.append(rb)
        i = i + 1
    s = 0.0
    i = 0
    while i < n:
        row = a[i] if flag > 0 else b[i]
        s = s + row[0] + row[n - 1]
        i = i + 1
    return s


def long_string():
    s = "x" * 100 + "žščř" + "y" * 50
    h = 0
    for ch in s:
        h = (h * 7 + ord(ch)) & 0xFFFF
    return len(s), h


def gen_sum(n):
    def gen():
        for i in range(n):
            yield i * i
    return sum(gen())


def closure_add(k):
    def inner(x):
        return x + k
    return inner(5) + inner(6)


def star_args(*args, **kw):
    return sum(args) + len(kw)


def try_div(a, b):
    try:
        return a // b
    except ZeroDivisionError:
        return -999
