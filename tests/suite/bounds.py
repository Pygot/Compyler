def fill(n):
    a = [0] * n
    i = 0
    while i < n:
        a[i] = i * 3
        i = i + 1
    s = 0
    i = 0
    while i < n:
        s = s + a[i]
        i = i + 1
    return s


def inner_mut(n, k):
    a = [0] * n
    i = 0
    total = 0
    while i < n:
        j = 0
        while j < k:
            a[i] = a[i] + 1
            i = i + 1
            j = j + 1
            if i >= n:
                break
        total = total + 1
        if i >= n:
            break
    s = 0
    i = 0
    while i < n:
        s = s + a[i]
        i = i + 1
    return s, total


def over_bound(n, m):
    a = [7] * n
    s = 0
    i = 0
    try:
        while i < m:
            s = s + a[i]
            i = i + 1
    except IndexError:
        return s, "caught", i
    return s, "clean", i


def neg_bound(n):
    a = [5] * 4
    s = 0
    i = 0
    while i < n:
        s = s + a[i]
        i = i + 1
    return s


def mutate_n(n):
    a = [1] * n
    s = 0
    i = 0
    while i < n:
        s = s + a[i]
        if i == 2:
            n = n - 1
        i = i + 1
    return s, n


def break_guard(n):
    a = [0] * n
    c = 0
    while 1:
        if c >= n:
            break
        a[c] = c * c
        c = c + 2
    s = 0
    i = 0
    while i < n:
        s = s + a[i]
        i = i + 1
    return s


def sieve_shape(n):
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


def lol_rows(n):
    rows = []
    i = 0
    while i < n:
        r = [0] * n
        j = 0
        while j < n:
            r[j] = i * n + j
            j = j + 1
        rows.append(r)
        i = i + 1
    s = 0
    i = 0
    while i < n:
        row = rows[i]
        j = 0
        while j < n:
            s = s + row[j]
            j = j + 1
        i = i + 1
    return s


def empty_arr():
    a = [0] * 0
    s = 0
    i = 0
    n = 0
    while i < n:
        s = s + a[i]
        i = i + 1
    return s, len(a)


def exact_edge(n):
    a = [2] * n
    s = 0
    i = 0
    while i < n:
        s = s + a[i] * (i + 1)
        i = i + 1
    return s, a[n - 1] if n > 0 else -1


def float_arr(n):
    a = [0.5] * n
    i = 0
    while i < n:
        a[i] = a[i] * float(i)
        i = i + 1
    s = 0.0
    i = 0
    while i < n:
        s = s + a[i]
        i = i + 1
    return round(s, 6)


def cond_store(n, flag):
    a = [1] * n
    i = 0
    while i < n:
        if flag > 0:
            a[i] = 9
        i = i + 1
    s = 0
    i = 0
    while i < n:
        s = s + a[i]
        i = i + 1
    return s


def continue_shape(n):
    a = [3] * n
    s = 0
    i = 0
    while i < n:
        if i % 3 == 0:
            i = i + 1
            continue
        s = s + a[i]
        i = i + 1
    return s


def recreate(n):
    a = [1] * n
    s = 0
    i = 0
    while i < n:
        s = s + a[i]
        i = i + 1
    a = [2] * (n * 2)
    i = 0
    while i < n * 2:
        s = s + a[i]
        i = i + 1
    return s


def over_raw(n, m):
    a = [7] * n
    s = 0
    i = 0
    while i < m:
        s = s + a[i]
        i = i + 1
    return s


def rloop(n):
    a = [0] * n
    for i in range(n):
        a[i] = i * 2
    s = 0
    for i in range(n):
        s = s + a[i]
    return s


def rloop2(a, lo, hi):
    s = 0
    for i in range(lo, hi):
        s = s + a[i]
    return s


def rstep(n):
    a = [1] * n
    for j in range(0, n, 3):
        a[j] = 0
    s = 0
    for i in range(n):
        s = s + a[i]
    return s


def rmut(n):
    a = [5] * n
    s = 0
    for i in range(n):
        if i == 2:
            i = i + 100
        s = s + a[i]
    return s


def rback(n):
    a = [3] * n
    s = 0
    for i in range(n - 1, -1, -1):
        s = s + a[i] * i
    return s


def accover(n):
    s = 4611686018427387000
    i = 0
    while i < n:
        s = s + 4611686018427387000
        i = i + 1
    return s


def accmul(n):
    s = 3
    i = 0
    while i < n:
        s = s * 1000003
        i = i + 1
    return s


def accdead(n):
    s = 4611686018427387904
    i = 0
    while i < n:
        s = s + 4611686018427387904
        i = i + 1
    s = 7
    return s + n


def augbig(n):
    a = [4611686018427387904] * n
    i = 0
    while i < n:
        a[i] += 4611686018427387904
        i = i + 1
    return a[0]


def narrowmix(n):
    a = [0] * n
    b = [0] * n
    c = [-5] * n
    i = 0
    while i < n:
        a[i] = (i % 251) - 124
        b[i] = i * 3
        i = i + 1
    s = 0
    i = 0
    while i < n:
        s = s + a[i] * 2 + b[i] - c[i]
        i = i + 1
    return s


def param_index(a, i, n):
    if i < n:
        return a[i]
    return -1


def param_loop(a, i, n):
    s = 0
    while i < n:
        s = s + a[i]
        i = i + 1
    return s


print(fill(50))
try:
    print(over_raw(5, 9))
except IndexError:
    print("raw caught")
print(over_raw(5, 5))
print(over_raw(6, 2))
print(inner_mut(10, 3))
print(inner_mut(9, 4))
print(over_bound(5, 9))
print(over_bound(5, 5))
print(over_bound(0, 3))
print(neg_bound(-2))
print(neg_bound(0))
print(neg_bound(3))
print(mutate_n(8))
print(break_guard(11))
print(sieve_shape(100))
print(sieve_shape(2))
print(lol_rows(6))
print(empty_arr())
print(exact_edge(7))
print(exact_edge(1))
print(float_arr(9))
print(cond_store(6, 1))
print(cond_store(6, 0))
print(continue_shape(10))
print(recreate(5))
print(narrowmix(1000))
print(narrowmix(0))
print(narrowmix(1))
print(augbig(3))
print(accover(0))
print(accover(3))
print(accover(50))
print(accmul(4))
print(accmul(20))
print(accdead(0))
print(accdead(5))
print(rloop(200))
print(rloop(0))
print(rloop2([4, 5, 6, 7], 1, 3))
print(rloop2([4, 5, 6, 7], 0, 4))
print(rloop2([4, 5, 6, 7], -2, 2))
try:
    print(rloop2([4, 5, 6, 7], 0, 9))
except IndexError:
    print("rloop2 caught")
print(rloop2([4, 5, 6, 7], 3, 1))
print(rstep(20))
try:
    print(rmut(50))
except IndexError:
    print("rmut caught")
print(rmut(2))
print(rback(9))
print(param_index([10, 20, 30, 40, 50], 2, 5))
print(param_index([10, 20, 30, 40, 50], -3, 5))
print(param_index([10, 20, 30, 40, 50], -1, 5))
print(param_index([10, 20, 30, 40, 50], 7, 5))
print(param_loop([1, 2, 3, 4], 0, 4))
print(param_loop([1, 2, 3, 4], -2, 3))
try:
    print(param_loop([1, 2, 3, 4], -9, 3))
except IndexError:
    print("param caught")
