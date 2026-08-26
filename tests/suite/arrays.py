import sys

OUT = []


def condlol(n, flag):
    a = []
    b = []
    i = 0
    while i < n:
        ra = [0] * n
        rb = [0.5] * n
        j = 0
        while j < n:
            ra[j] = i * n + j
            j = j + 1
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


def rowalias(n):
    a = []
    i = 0
    while i < n:
        r = [0.0] * n
        j = 0
        while j < n:
            r[j] = float(i * n + j)
            j = j + 1
        a.append(r)
        i = i + 1
    row = a[1]
    row[0] = 999.5
    s = 0.0
    i = 0
    while i < n:
        rr = a[i]
        j = 0
        while j < n:
            s = s + rr[j]
            j = j + 1
        i = i + 1
    return s + a[1][0] * 3.0 + row[n - 1] * 7.0


def aliaswrite(n):
    a = []
    i = 0
    while i < n:
        r = [7] * n
        a.append(r)
        i = i + 1
    x = a[0]
    y = a[0]
    x[2] = 100
    return y[2] * 10000 + a[0][2] * 100 + x[2]


def augarr(n):
    a = [0] * n
    i = 0
    while i < n:
        a[i] += i
        a[i] -= 1
        a[i] *= 3
        i = i + 1
    s = 0
    i = 0
    while i < n:
        s = s + a[i]
        i = i + 1
    return s


def augrow(n):
    m = []
    i = 0
    while i < n:
        r = [1.5] * n
        m.append(r)
        i = i + 1
    i = 0
    while i < n:
        m[i][i] += 2.5
        i = i + 1
    row = m[1]
    row[0] += 100.0
    s = 0.0
    i = 0
    while i < n:
        j = 0
        while j < n:
            s = s + m[i][j]
            j = j + 1
        i = i + 1
    return s


def augmap(n):
    d = {}
    i = 0
    while i < 7:
        d[i] = 0
        i = i + 1
    i = 0
    while i < n:
        d[i % 7] += i
        i = i + 1
    s = 0
    i = 0
    while i < 7:
        s = s + d[i] * (i + 1)
        i = i + 1
    return s


def rd(a, i):
    return a[i]


def wr(a, i, v):
    a[i] = v
    return a[i]


def total(a):
    s = 0
    n = len(a)
    i = 0
    while i < n:
        s = s + a[i]
        i = i + 1
    return s


def ftotal(a):
    s = 0.0
    n = len(a)
    i = 0
    while i < n:
        s = s + a[i]
        i = i + 1
    return s


def make_int(n):
    a = [0] * n
    i = 0
    while i < n:
        a[i] = i * i - (i >> 1)
        i = i + 1
    s = 0
    i = 0
    while i < n:
        s = s + a[i]
        i = i + 1
    return s


def make_flt(n):
    a = [1.5] * n
    i = 0
    while i < n:
        a[i] = a[i] * float(i)
        i = i + 1
    s = 0.0
    i = 0
    while i < n:
        s = s + a[i]
        i = i + 1
    return s


def negidx(a):
    return a[-1] + a[-2] * a[0]


def scale(a, k):
    n = len(a)
    i = 0
    while i < n:
        a[i] = a[i] * k
        i = i + 1
    return 0


def both(a, b):
    n = len(a)
    m = len(b)
    i = 0
    while i < n:
        if i < m:
            b[i] = b[i] + float(a[i])
        i = i + 1
    return 0


def swap2(a):
    t = a[0]
    a[0] = a[1]
    a[1] = t
    return 0


def crecreate(n, reps):
    s = 0
    r = 0
    while r < reps:
        a = [0] * n
        i = 0
        while i < n:
            a[i] = i + r
            i = i + 1
        s = s + a[n - 1]
        r = r + 1
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


def queens(n):
    cols = [0] * n
    return qsolve(cols, 0, n)


def main():
    ints = [10, 20, 30, 40, 50]
    flts = [0.5, 1.5, 2.5]

    for i in (0, 2, 4, -1, -5):
        OUT.append("rd %d = %d" % (i, rd(ints, i)))
    for i in (5, -6, 100):
        try:
            rd(ints, i)
            OUT.append("rd %d no error" % i)
        except IndexError as e:
            OUT.append("rd %d IndexError %s" % (i, e))

    OUT.append("wr = %d %s" % (wr(ints, 1, 99), ints))
    OUT.append("wr neg = %d %s" % (wr(ints, -1, -7), ints))
    try:
        wr(ints, 9, 1)
        OUT.append("wr oob no error")
    except IndexError:
        OUT.append("wr oob IndexError")

    OUT.append("total = %d" % total(ints))
    OUT.append("ftotal = %r" % ftotal(flts))
    OUT.append("total [] = %d" % total([]))
    OUT.append("make_int = %d" % make_int(1000))
    OUT.append("make_int 0 = %d" % make_int(0))
    OUT.append("make_flt = %r" % make_flt(100))
    OUT.append("negidx = %r" % negidx(flts))

    sc = [1, 2, 3, 4]
    scale(sc, 5)
    OUT.append("scale = %s" % sc)

    ia = [1, 2, 3]
    fb = [10.0, 20.0]
    both(ia, fb)
    OUT.append("both = %s %s" % (ia, fb))

    al = [7, 8]
    swap2(al)
    OUT.append("swap = %s" % al)

    OUT.append("recreate = %d" % crecreate(50, 30))
    OUT.append("heapsort = %d" % heapsort(2000))
    OUT.append("queens = %d" % queens(8))

    mixed = [1, "x", 3]
    try:
        OUT.append("mixed total = %d" % total(mixed))
    except TypeError:
        OUT.append("mixed TypeError")

    bigs = [2 ** 70, 5]
    OUT.append("bigs = %d" % total(bigs))

    bools = [True, False, True]
    OUT.append("bools = %d" % total(bools))

    subint = [0, 2 ** 62, 2 ** 62]
    try:
        OUT.append("overflow = %d" % total(subint))
    except Exception as e:
        OUT.append("overflow ! %s" % type(e).__name__)

    same = [3, 4]
    both_same = total(same) + total(same)
    OUT.append("twice = %d" % both_same)

    fl = [1.5, 2.5]
    scale_f = ftotal(fl)
    fl[0] = 9.5
    OUT.append("visible = %r %r" % (scale_f, ftotal(fl)))

    class L(list):
        pass

    sub = L([1, 2, 3])
    OUT.append("subclass = %d" % total(sub))

    OUT.append("condlol a = %r" % condlol(4, 1))
    OUT.append("condlol b = %r" % condlol(4, 0))
    OUT.append("rowalias = %r" % (rowalias(5),))
    OUT.append("aliaswrite = %r" % (aliaswrite(3),))
    OUT.append("augarr = %d" % augarr(60))
    OUT.append("augarr0 = %d" % augarr(0))
    OUT.append("augrow = %r" % augrow(6))
    OUT.append("augmap = %d" % augmap(40))

    for line in OUT:
        print(line)
    print("N", len(OUT))
    return 0


sys.exit(main())
