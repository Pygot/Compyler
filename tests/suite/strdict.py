import sys

OUT = []


def fmt1(i):
    return len(f"{i}")


def fmt2(i, j):
    s = f"{i}:{j}"
    return len(s)


def hashit(n):
    h = 0
    i = 0
    while i < n:
        s = f"{i}x{i * i}"
        j = 0
        m = len(s)
        while j < m:
            h = (h * 31 + ord(s[j])) & 0xFFFFFFFF
            j = j + 1
        i = i + 1
    return h


def pick(i, j):
    s = f"{i}abc{j}"
    return ord(s[0]) * 1000000 + ord(s[-1]) + ord(s[2])


def oob(i):
    s = f"{i}"
    return ord(s[10])


def uni():
    s = f"{5}é"
    return len(s)


def dmk(n):
    d = {}
    i = 0
    while i < n:
        d[i * 3 - n] = i
        i = i + 1
    s = 0
    i = 0
    while i < n:
        s = s + d[i * 3 - n]
        i = i + 1
    return s + len(d)


def dover():
    d = {}
    d[7] = 1
    d[7] = 2
    d[-7] = 3
    return d[7] * 10 + d[-7] + len(d)


def dmiss(k):
    d = {}
    d[1] = 100
    return d[k]


def dgrow(n):
    d = {}
    i = 0
    while i < n:
        d[i] = i + 1
        i = i + 1
    t = 0
    i = 0
    while i < n:
        t = t + d[i]
        i = i + 1
    return t


def dloop(n, reps):
    total = 0
    r = 0
    while r < reps:
        d = {}
        i = 0
        while i < n:
            d[i] = r * i
            i = i + 1
        total = total + d[n - 1]
        r = r + 1
    return total


def main():
    for v in (0, 5, -5, 123456, -99999, 2 ** 40):
        OUT.append("fmt1 %d = %d" % (v, fmt1(v)))
    OUT.append("fmt2 = %d" % fmt2(-12, 345))
    OUT.append("hashit = %d" % hashit(3000))
    OUT.append("pick = %d" % pick(7, 9))
    try:
        oob(3)
        OUT.append("oob no error")
    except IndexError:
        OUT.append("oob IndexError")
    OUT.append("uni = %d" % uni())
    OUT.append("dmk = %d" % dmk(500))
    OUT.append("dover = %d" % dover())
    OUT.append("dmiss hit = %d" % dmiss(1))
    try:
        dmiss(2)
        OUT.append("dmiss no error")
    except KeyError as e:
        OUT.append("dmiss KeyError %r" % (e.args[0],))
    OUT.append("dgrow = %d" % dgrow(10000))
    OUT.append("dgrow 0 = %d" % dgrow(0))
    OUT.append("dloop = %d" % dloop(64, 200))

    for line in OUT:
        print(line)
    print("N", len(OUT))
    return 0


sys.exit(main())
