import sys

OUT = []


def imin(a, b):
    return min(a, b) * 1000 + max(a, b)


def fmin(a, b):
    return min(a, b)


def fmax(a, b):
    return max(a, b)


def fnansel(n, k):
    big = float(n) * 1e308
    big = big * 10.0
    z = big - big
    if k == 0:
        return min(z, 1.0)
    if k == 1:
        return max(z, 2.0)
    if k == 2:
        return min(1.0, z)
    return max(2.0, z)


def fzsel(n, k):
    z = float(n) * 0.0
    nz = -z
    if k == 0:
        return min(z, nz)
    return min(nz, z)


def clamp(x, lo, hi):
    v = x
    i = 0
    while i < 3:
        v = max(lo, min(v, hi))
        i = i + 1
    return v


def f_len(x):
    return len(x)


def f_ord(x):
    return ord(x)


def f_abs(x):
    return abs(x)


def f_chr(x):
    return chr(x)


def f_float(x):
    return float(x)


def f_int(x):
    return int(x)


def f_mix(s):
    t = 0
    i = 0
    n = len(s)
    while i < n:
        t = t + ord(s[i]) + abs(i - 5)
        i = i + 1
    return t


def f_roundtrip(n):
    out = []
    i = 0
    while i < n:
        out.append(chr(ord(chr(i + 65))))
        i = i + 1
    return "".join(out)


def f_conv(v):
    return (float(v), int(v), abs(v))


class Weird:
    def __repr__(self):
        return '<Weird>'

    def __len__(self):
        return 7

    def __abs__(self):
        return "abs!"

    def __float__(self):
        return 2.5

    def __int__(self):
        return 9

    def __index__(self):
        return 3


def shadowed(x):
    return len(x)


def show(v):
    if isinstance(v, float):
        return repr(v)
    return repr(v)


def main():
    for v in ([], [1, 2, 3], (), (1,), "", "abc", "héllo", {}, {1: 2, 3: 4},
              b"", b"xyz", set(), frozenset([1]), bytearray(b"ab"), range(9),
              Weird()):
        try:
            OUT.append("len %s = %s" % (type(v).__name__, f_len(v)))
        except Exception as e:
            OUT.append("len %s ! %s" % (type(v).__name__, type(e).__name__))

    for v in (0, 1, -1, 3.5, None, [1]):
        try:
            OUT.append("lenbad %r ! %s" % (v, "no error"))
            f_len(v)
        except TypeError as e:
            OUT.append("lenbad %r TypeError" % (v,))

    for v in ("a", "Z", "0", "é", "中", "\U0001F600", "\x00", b"\x00", b"\xff",
              bytes([65]), "ab", "", b"ab", 5, None):
        try:
            OUT.append("ord %r = %s" % (v, f_ord(v)))
        except Exception as e:
            OUT.append("ord %r ! %s" % (v, type(e).__name__))

    for v in (0, 1, -1, 5, -5, 2 ** 62, -(2 ** 62), 2 ** 63 - 1, -(2 ** 63),
              2 ** 100, -(2 ** 100), 0.0, -0.0, 1.5, -1.5, float("inf"),
              float("-inf"), True, False, Weird(), "s", None, 3 + 4j):
        try:
            OUT.append("abs %r = %r" % (v, f_abs(v)))
        except Exception as e:
            OUT.append("abs %r ! %s" % (v, type(e).__name__))

    for v in (0, 65, 0x10FFFF, 0x110000, -1, 128, 255, 256, 0xD800, 1.5, None, True):
        try:
            OUT.append("chr %r = %r" % (v, f_chr(v)))
        except Exception as e:
            OUT.append("chr %r ! %s" % (v, type(e).__name__))

    for v in (0, 1, -1, 2 ** 62, 2 ** 63, 2 ** 100, -(2 ** 100), 1.5, -1.5,
              "3.5", "  7 ", "abc", b"2.5", True, False, None, Weird(),
              float("inf"), float("nan")):
        try:
            OUT.append("float %r = %r" % (v, f_float(v)))
        except Exception as e:
            OUT.append("float %r ! %s" % (v, type(e).__name__))

    for v in (0, 1, -1, 1.9, -1.9, 0.5, -0.5, 2 ** 62, 2 ** 63, 2 ** 100,
              1e18, 1e19, -1e19, 9.2233720368547758e18, -9.2233720368547758e18,
              "42", " -7 ", "0x10", b"12", True, False, None, Weird(),
              float("inf"), float("nan")):
        try:
            OUT.append("int %r = %r" % (v, f_int(v)))
        except Exception as e:
            OUT.append("int %r ! %s" % (v, type(e).__name__))

    OUT.append("mix = %d" % f_mix("compyler benchmark"))
    OUT.append("rt = %s" % f_roundtrip(26))
    for v in (3, -3, 2.5, -2.5):
        OUT.append("conv %r = %r" % (v, f_conv(v)))

    global len
    real_len = len

    def fake_len(x):
        return 4242

    len = fake_len
    OUT.append("shadow = %s" % shadowed([1, 2, 3]))
    OUT.append("shadow str = %s" % shadowed("abcdef"))
    len = real_len
    OUT.append("restored = %s" % shadowed([1, 2, 3]))

    OUT.append("imin = %d" % imin(3, 7))
    OUT.append("imin2 = %d" % imin(7, 3))
    OUT.append("imin3 = %d" % imin(-5, -5))
    OUT.append("imin4 = %d" % imin(2 ** 62, -2 ** 62))
    OUT.append("fmin = %r" % fmin(1.5, 2.5))
    OUT.append("fmin2 = %r" % fmin(2.5, 1.5))
    OUT.append("fminz = %r" % fmin(0.0, -0.0))
    OUT.append("fminz2 = %r" % fmin(-0.0, 0.0))
    OUT.append("fminnan = %r" % fmin(float("nan"), 1.0))
    OUT.append("fminnan2 = %r" % fmin(1.0, float("nan")))
    OUT.append("fmaxnan = %r" % fmax(float("nan"), 1.0))
    OUT.append("fmaxinf = %r" % fmax(float("inf"), 1e308))
    OUT.append("mixed = %r" % min(2, 3.5))
    OUT.append("mixed2 = %r" % max(2, 1.5))
    OUT.append("clamp = %d" % clamp(15, 0, 10))
    OUT.append("clamp2 = %d" % clamp(-4, 0, 10))
    OUT.append("clamp3 = %d" % clamp(5, 0, 10))
    OUT.append("tnan0 = %r" % fnansel(2, 0))
    OUT.append("tnan1 = %r" % fnansel(2, 1))
    OUT.append("tnan2 = %r" % fnansel(2, 2))
    OUT.append("tnan3 = %r" % fnansel(2, 3))
    OUT.append("tz0 = %r" % fzsel(1, 0))
    OUT.append("tz1 = %r" % fzsel(1, 1))

    for line in OUT:
        print(line)
    print("N", len(OUT))
    return 0


sys.exit(main())
