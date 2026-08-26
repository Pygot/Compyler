import sys


def basic(a, b, name):
    return f"{a}+{b}={a + b} for {name}"


def conversions(x):
    return f"{x!s}|{x!r}|{x!a}"


def specs(v, w, p):
    return f"[{v:>10}] [{v:<10}] [{v:^10}] [{w:.3f}] [{w:08.2f}] [{p:x}] [{p:#b}] [{p:,}]"


def nested(v, n):
    return f"{v:.{n}f}"


def mixed(items, d):
    out = []
    for i, it in enumerate(items):
        out.append(f"{i:02d}:{it!r}:{d.get(it, 0):+d}")
    return "".join(out)


def singles(x):
    return f"{x}"


def empty_and_literal(x):
    return f"literal only" + f"{x}" + f"{{escaped}}" + f"{x:d}"


def deep(a, b, c, d, e):
    return f"{a}{b}{c}{d}{e}{a + b}{c * 2}{d!r}{e:>4}"


def unicode_case(s):
    return f"<{s}> len={len(s)} up={s.upper()!r}"


def numeric(i, f_):
    return f"{i}|{i:e}|{f_:g}|{f_:%}|{i / 7:.10f}|{-i:+}|{f_:.0f}"


def hot(n):
    t = 0
    for i in range(n):
        s = f"{i}:{i * i}"
        t += len(s)
    return t


class P:
    def __init__(self, x):
        self.x = x

    def __repr__(self):
        return "P(%d)" % self.x

    def __str__(self):
        return "p%d" % self.x

    def __format__(self, spec):
        if not spec:
            return str(self)
        return format(self.x, spec)


def objs(p):
    return f"{p}|{p!r}|{p!s}|{p:05d}|{p:>8}"


def main():
    r = []
    r.append(basic(3, 4, "abc"))
    r.append(basic(-2 ** 40, 2 ** 40, "big"))
    r.append(conversions("é中"))
    r.append(conversions(3.5))
    r.append(conversions([1, {"k": None}]))
    r.append(specs("ab", 3.14159265, 48879))
    r.append(specs("", 0.0005, 1))
    r.append(nested(3.14159265, 4))
    r.append(nested(2.5, 0))
    r.append(mixed(["a", "b", "c"], {"a": 1, "c": -3}))
    r.append(singles(None))
    r.append(singles(True))
    r.append(singles(2 ** 70))
    r.append(empty_and_literal(7))
    r.append(deep(1, 2, "x", [1], "y"))
    r.append(unicode_case("héllo"))
    r.append(numeric(1234567, 0.125))
    r.append(str(hot(20000)))
    r.append(objs(P(42)))
    for line in r:
        print(line)
    print("N", len(r))
    return 0


sys.exit(main())
