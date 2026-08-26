import sys

OUT = []


def shl(a, b):
    return a << b


def shr(a, b):
    return a >> b


def mask(a, m):
    return a & m


def orop(a, b):
    return a | b


def xorop(a, b):
    return a ^ b


def rot32(x, k):
    return ((x << k) | (x >> (32 - k))) & 0xFFFFFFFF


def loop_shl(n):
    x = 1
    i = 0
    while i < n:
        x = x << 1
        i = i + 1
    return x


def loop_mix(n):
    x = 1
    i = 0
    while i < n:
        x = x ^ (i * 2654435761)
        x = ((x << 7) | (x >> 25)) & 0xFFFFFFFF
        x = x + (x >> 11)
        x = x & 0xFFFFFFFF
        i = i + 1
    return x


def bigconst(i):
    a = 0xFFFFFFFF
    b = 0x7FFFFFFFFFFFFFFF
    c = -9223372036854775808
    d = 2654435761
    e = 4294967296
    return (a + i, b - i, c + i, d * i, e | i, a & i, b >> i, d ^ i)


def promote(n):
    x = 1
    i = 0
    total = 0
    while i < n:
        x = x * 3
        total = total + (x & 0xFF)
        i = i + 1
    return (x, total)


def negshift(a, b):
    return (a >> b, a << b, -a >> b, -a << b)


def main():
    for b in range(0, 66):
        OUT.append("shl 1 %d = %d" % (b, shl(1, b)))
        OUT.append("shl -1 %d = %d" % (b, shl(-1, b)))
        OUT.append("shl 12345 %d = %d" % (b, shl(12345, b)))
        OUT.append("shl -98765 %d = %d" % (b, shl(-98765, b)))
        OUT.append("shr big %d = %d" % (b, shr(0x7FFFFFFFFFFFFFFF, b)))
        OUT.append("shr neg %d = %d" % (b, shr(-0x7FFFFFFFFFFFFFFF, b)))

    for a in (0, 1, -1, 255, 4294967295, 4294967296, 2 ** 62, 2 ** 63 - 1,
              -(2 ** 63), 2 ** 64, -(2 ** 64), 2 ** 100):
        for b in (0, 1, 7, 31, 32, 62, 63, 64, 65):
            try:
                OUT.append("SL %d %d = %d" % (a, b, shl(a, b)))
            except Exception as e:
                OUT.append("SL %d %d ! %s" % (a, b, type(e).__name__))
            try:
                OUT.append("SR %d %d = %d" % (a, b, shr(a, b)))
            except Exception as e:
                OUT.append("SR %d %d ! %s" % (a, b, type(e).__name__))
        for m in (0xFF, 0xFFFFFFFF, 0x7FFFFFFFFFFFFFFF, -1, 0):
            OUT.append("AND %d %d = %d" % (a, m, mask(a, m)))
            OUT.append("OR %d %d = %d" % (a, m, orop(a, m)))
            OUT.append("XOR %d %d = %d" % (a, m, xorop(a, m)))

    for k in range(1, 32):
        OUT.append("rot %d = %d" % (k, rot32(0xDEADBEEF, k)))

    for n in (0, 1, 10, 62, 63, 64, 100, 200):
        OUT.append("loopshl %d = %d" % (n, loop_shl(n)))

    OUT.append("loopmix = %d" % loop_mix(100000))

    for i in (0, 1, 2, 63, 1000):
        OUT.append("bigconst %d = %s" % (i, bigconst(i)))

    for n in (0, 1, 40, 41, 45, 100):
        OUT.append("promote %d = %s" % (n, promote(n)))

    for a in (1, 1000, 2 ** 40):
        for b in (0, 3, 40, 62, 63, 64):
            try:
                OUT.append("neg %d %d = %s" % (a, b, negshift(a, b)))
            except Exception as e:
                OUT.append("neg %d %d ! %s" % (a, b, type(e).__name__))

    try:
        shl(1, -1)
        OUT.append("negcount no error")
    except ValueError as e:
        OUT.append("negcount ValueError %s" % e)
    try:
        shr(1, -1)
        OUT.append("negcount2 no error")
    except ValueError as e:
        OUT.append("negcount2 ValueError %s" % e)

    for line in OUT:
        print(line)
    print("N", len(OUT))
    return 0


sys.exit(main())
