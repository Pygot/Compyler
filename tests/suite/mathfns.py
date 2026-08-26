import math
import sys

OUT = []


def f_sqrt(x):
    return math.sqrt(x)


def f_log(x):
    return math.log(x)


def f_log2(x):
    return math.log2(x)


def f_log10(x):
    return math.log10(x)


def f_log1p(x):
    return math.log1p(x)


def f_asin(x):
    return math.asin(x)


def f_acos(x):
    return math.acos(x)


def f_exp(x):
    return math.exp(x)


def f_trio(x):
    return math.sin(x) + math.cos(x) + math.tan(x)


def f_hyp(x):
    return math.sinh(x) + math.cosh(x) + math.tanh(x)


def f_atan2(a, b):
    return math.atan2(a, b)


def f_hypot(a, b):
    return math.hypot(a, b)


def f_fmod(a, b):
    return math.fmod(a, b)


def f_copysign(a, b):
    return math.copysign(a, b)


def f_deg(x):
    return math.degrees(x)


def f_rad(x):
    return math.radians(x)


def f_fabs(x):
    return math.fabs(x)


def f_expm1(x):
    return math.expm1(x)


def f_atan(x):
    return math.atan(x)


def f_float(x):
    return float(x)


def f_int(x):
    return int(x)


def f_abs(x):
    return abs(x)


def f_expr(i):
    return math.sqrt(i * i + 1.0) + math.sin(i + 0.5) * math.cos(i - 0.5)


def f_loop(n):
    s = 0.0
    i = 0
    while i < n:
        s = s + math.sqrt(float(i) + 1.0) - math.sin(float(i) * 0.001)
        i = i + 1
    return s


def f_mixed(n):
    s = 0.0
    i = 0
    while i < n:
        s = s + abs(float(i) - 500.0) + math.fabs(float(-i))
        i = i + 1
    return s


ONE = (f_sqrt, f_log, f_log2, f_log10, f_log1p, f_asin, f_acos, f_exp,
       f_trio, f_hyp, f_deg, f_rad, f_fabs, f_expm1, f_atan, f_float, f_int, f_abs)

TWO = (f_atan2, f_hypot, f_fmod, f_copysign)

VALS = (0, 1, -1, 2, -2, 4, 0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 2.5, -2.5,
        1e-12, -1e-12, 1e300, -1e300, 700.0, 710.0, -710.0,
        1000000, -1000000, 2 ** 62, -(2 ** 62), 9007199254740993,
        0.9999999999, 1.0000000001, 3.141592653589793)


def show(v):
    if isinstance(v, float):
        return repr(v)
    return str(v)


def main():
    for fn in ONE:
        for v in VALS:
            try:
                OUT.append("%s(%s) = %s" % (fn.__name__, show(v), show(fn(v))))
            except Exception as e:
                OUT.append("%s(%s) ! %s" % (fn.__name__, show(v), type(e).__name__))
    for fn in TWO:
        for a in VALS:
            for b in (0, 1, -1, 0.0, 2.5, -2.5, 1e300):
                try:
                    OUT.append("%s(%s,%s) = %s" % (fn.__name__, show(a), show(b), show(fn(a, b))))
                except Exception as e:
                    OUT.append("%s(%s,%s) ! %s" % (fn.__name__, show(a), show(b), type(e).__name__))
    for i in range(-20, 21):
        OUT.append("expr %d = %s" % (i, show(f_expr(i))))
    for n in (0, 1, 10, 1000):
        OUT.append("loop %d = %s" % (n, show(f_loop(n))))
        OUT.append("mixed %d = %s" % (n, show(f_mixed(n))))

    saved = math.sqrt
    math.sqrt = lambda x: 4242.0
    OUT.append("patched sqrt = %s" % show(f_sqrt(9)))
    OUT.append("patched loop = %s" % show(f_loop(5)))
    math.sqrt = saved
    OUT.append("restored sqrt = %s" % show(f_sqrt(9)))

    real_float = float
    try:
        OUT.append("shadow float = %s" % show(f_float(3)))
    finally:
        del real_float

    for line in OUT:
        print(line)
    print("N", len(OUT))
    return 0


sys.exit(main())
