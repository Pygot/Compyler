import sys

BIG = 2 ** 62
HUGE = 2 ** 70
GLOBAL_N = 41


def add(a, b):
    return a + b


def sub(a, b):
    return a - b


def mul(a, b):
    return a * b


def floordiv(a, b):
    return a // b


def truediv(a, b):
    return a / b


def mod(a, b):
    return a % b


def bit_and(a, b):
    return a & b


def bit_or(a, b):
    return a | b


def bit_xor(a, b):
    return a ^ b


def shl(a, b):
    return a << b


def shr(a, b):
    return a >> b


def power(a, b):
    return a ** b


def neg(a):
    return -a


def inv(a):
    return ~a


def lnot(a):
    return not a


def lt(a, b):
    return a < b


def le(a, b):
    return a <= b


def eq(a, b):
    return a == b


def ne(a, b):
    return a != b


def gt(a, b):
    return a > b


def ge(a, b):
    return a >= b


def is_(a, b):
    return a is b


def isnot(a, b):
    return a is not b


def contains(a, b):
    return a in b


def notcontains(a, b):
    return a not in b


def getidx(seq, i):
    return seq[i]


def setidx(lst, i, v):
    lst[i] = v
    return lst


def getattr_real(o):
    return o.real


def branch_lt(a, b):
    if a < b:
        return "lt"
    return "ge"


def branch_none(v):
    if v is None:
        return "none"
    return "some"


def truthy(v):
    if v:
        return "T"
    return "F"


def loop_range(n):
    t = 0
    for i in range(n):
        t = t + i
    return t


def loop_range3(a, b, c):
    t = []
    for i in range(a, b, c):
        t.append(i)
    return t


def loop_seq(seq):
    t = 0
    for x in seq:
        t = t + 1
    return t


def loop_break(n):
    t = 0
    for i in range(n):
        if i == 5:
            break
        t = t + i
    return t


def loop_continue(n):
    t = 0
    for i in range(n):
        if i % 2 == 0:
            continue
        t = t + i
    return t


def while_loop(n):
    i = 0
    t = 0
    while i < n:
        t = t + i * i
        i = i + 1
    return t


def nested(n, m):
    t = 0
    for i in range(n):
        for j in range(m):
            t = t + i * j
    return t


def unpack(pair):
    a, b = pair
    return b, a


def build_list(a, b):
    return [a, b, a + b]


def build_tuple(a, b):
    return (a, b, a * b)


def helper(n):
    return n * 2


def call_chain(n):
    return helper(n) + helper(n + 1)


def recurse(n):
    if n <= 0:
        return 0
    return n + recurse(n - 1)


def mutual_a(n):
    if n <= 0:
        return 0
    return mutual_b(n - 1)


def mutual_b(n):
    if n <= 0:
        return 1
    return mutual_a(n - 1)


def use_global():
    return GLOBAL_N + 1


def set_global(v):
    global GLOBAL_N
    GLOBAL_N = v
    return GLOBAL_N


def mixed_float(a, b):
    return a * 2.0 + b / 4.0 - 1.5


def str_concat(s, t):
    return s + t


def str_repeat(s, n):
    return s * n


def call_len(s):
    return len(s)


def method_call(s, t):
    return s.find(t)


def probe(label, fn, *args):
    try:
        r = fn(*args)
    except Exception as e:
        r = type(e).__name__
    print("%-12s %r" % (label, r))


INTS = (0, 1, -1, 7, -7, 1000003, BIG, -BIG, HUGE, -HUGE)
DIVS = (1, -1, 3, -3, 7, BIG)
FLOATS = (0.0, 1.0, -1.5, 3.25, 1e18, -2.5e-7)


def main():
    for a in INTS:
        for b in DIVS:
            probe("add", add, a, b)
            probe("sub", sub, a, b)
            probe("mul", mul, a, b)
            probe("floordiv", floordiv, a, b)
            probe("truediv", truediv, a, b)
            probe("mod", mod, a, b)
            probe("and", bit_and, a, b)
            probe("or", bit_or, a, b)
            probe("xor", bit_xor, a, b)
            probe("lt", lt, a, b)
            probe("le", le, a, b)
            probe("eq", eq, a, b)
            probe("ne", ne, a, b)
            probe("gt", gt, a, b)
            probe("ge", ge, a, b)
    for a in INTS:
        probe("div0", floordiv, a, 0)
        probe("mod0", mod, a, 0)
        probe("truediv0", truediv, a, 0)
        probe("neg", neg, a)
        probe("inv", inv, a)
        probe("lnot", lnot, a)
        for b in (0, 1, 3, 40, 62, 96):
            probe("shl", shl, a, b)
            probe("shr", shr, a, b)
        probe("shlneg", shl, a, -1)
        probe("shrneg", shr, a, -1)
    for a in (0, 1, -1, 2, 3, -3, 10):
        for b in (0, 1, 2, 5, 20, 64, 65):
            probe("pow", power, a, b)
    for a in FLOATS:
        for b in (1.0, -2.0, 0.5, 0.0):
            probe("fadd", add, a, b)
            probe("fsub", sub, a, b)
            probe("fmul", mul, a, b)
            probe("ftruediv", truediv, a, b)
            probe("ffloordiv", floordiv, a, b)
            probe("fmod", mod, a, b)
            probe("flt", lt, a, b)
            probe("feq", eq, a, b)
            probe("fmixed", mixed_float, a, b)
        probe("fneg", neg, a)
        probe("fnot", lnot, a)
        probe("fand", bit_and, a, 1.0)
    for a in (1, 2, BIG, HUGE):
        for b in (1.0, 0.5, -3.5, 0.0):
            probe("mixadd", add, a, b)
            probe("mixmul", mul, a, b)
            probe("mixdiv", truediv, a, b)
            probe("mixlt", lt, a, b)
            probe("mixeq", eq, a, b)

    probe("is", is_, 1, 1)
    probe("isnone", is_, None, None)
    probe("isnot", isnot, 1, 2)
    probe("in-list", contains, 3, [1, 2, 3])
    probe("notin", notcontains, 9, [1, 2, 3])
    probe("in-str", contains, "a", "cat")
    probe("in-dict", contains, "k", {"k": 1})
    probe("in-bad", contains, 1, 5)
    probe("idx-list", getidx, [10, 20, 30], 1)
    probe("idx-neg", getidx, [10, 20, 30], -1)
    probe("idx-oob", getidx, [10, 20, 30], 9)
    probe("idx-tuple", getidx, (10, 20, 30), 0)
    probe("idx-toob", getidx, (10, 20), 5)
    probe("idx-str", getidx, "hello", 2)
    probe("idx-dict", getidx, {"k": 9}, "k")
    probe("idx-missing", getidx, {"k": 9}, "z")
    probe("idx-bad", getidx, 5, 0)
    probe("setidx", setidx, [1, 2, 3], 1, 99)
    probe("setidx-neg", setidx, [1, 2, 3], -1, 99)
    probe("setidx-oob", setidx, [1, 2], 9, 0)
    probe("attr", getattr_real, 5)
    probe("attr-bad", getattr_real, "s")
    probe("branch", branch_lt, 1, 2)
    probe("branch2", branch_lt, 2, 1)
    for v in (0, 1, "", "x", [], [0], None, 0.0, 1.5, BIG):
        probe("truthy", truthy, v)
        probe("isnone", branch_none, v)
    probe("loop", loop_range, 10)
    probe("loop0", loop_range, 0)
    probe("loopneg", loop_range, -5)
    probe("loop3", loop_range3, 1, 10, 3)
    probe("loop3neg", loop_range3, 10, 1, -2)
    probe("loop3zero", loop_range3, 1, 10, 0)
    probe("loopseq", loop_seq, [1, 2, 3, 4])
    probe("loopstr", loop_seq, "abcde")
    probe("loopdict", loop_seq, {"a": 1, "b": 2})
    probe("loopbad", loop_seq, 5)
    probe("break", loop_break, 20)
    probe("continue", loop_continue, 10)
    probe("while", while_loop, 100)
    probe("nested", nested, 5, 6)
    probe("unpack", unpack, (1, 2))
    probe("unpack-bad", unpack, (1, 2, 3))
    probe("blist", build_list, 3, 4)
    probe("btuple", build_tuple, 3, 4)
    probe("callchain", call_chain, 5)
    probe("recurse", recurse, 50)
    probe("mutual", mutual_a, 10)
    probe("global", use_global)
    probe("setglobal", set_global, 77)
    probe("global2", use_global)
    probe("strcat", str_concat, "ab", "cd")
    probe("strrep", str_repeat, "ab", 3)
    probe("len", call_len, "hello")
    probe("len-bad", call_len, 5)
    probe("method", method_call, "hello", "ll")
    probe("overflow1", add, BIG, BIG)
    probe("overflow2", mul, BIG, 4)
    probe("overflow3", sub, -BIG, BIG)
    probe("overflow4", neg, -(2 ** 63))
    probe("bigshift", shl, 1, 200)
    print("done", sys.version_info[0], sys.version_info[1])


main()
