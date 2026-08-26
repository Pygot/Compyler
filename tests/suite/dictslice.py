import sys

OUT = []


def mk_empty():
    return {}


def mk_lit(a, b, c):
    return {"a": a, "b": b, "c": c}


def mk_mixed(k, v):
    return {k: v, 1: "one", 2.5: "float", (1, 2): "tuple", None: "none", True: "bool"}


def mk_dyn(n):
    d = {}
    i = 0
    while i < n:
        d[i] = i * i
        i = i + 1
    return d


def mk_const_key(x, y):
    return {"alpha": x, "beta": y, "gamma": x + y, "delta": x - y}


def get_set(d, k, v):
    d[k] = v
    return d[k]


def missing(d, k):
    return d[k]


def comp(n):
    return {i: i * 3 for i in range(n)}


def comp2(items):
    return {k: len(k) for k in items}


def sl_list(a, i, j):
    return a[i:j]


def sl_open_l(a, j):
    return a[:j]


def sl_open_r(a, i):
    return a[i:]


def sl_all(a):
    return a[:]


def sl_str(s, i, j):
    return s[i:j]


def sl_tuple(t, i, j):
    return t[i:j]


def sl_store(a, i, j, v):
    a[i:j] = v
    return a


def sl_del_like(a, i, j):
    a[i:j] = []
    return a


def sl_loop(a, n):
    t = 0
    i = 0
    while i < n:
        t = t + len(a[i:i + 3])
        i = i + 1
    return t


def dict_loop(n):
    d = {}
    i = 0
    while i < n:
        d[i] = i * 7 % 97
        i = i + 1
    s = 0
    i = 0
    while i < n:
        s = s + d[i]
        i = i + 1
    return s


def main():
    OUT.append(str(mk_empty()))
    OUT.append(str(mk_lit(1, "x", None)))
    OUT.append(str(mk_lit([1], (2,), {3: 4})))
    OUT.append(str(sorted(mk_mixed("k", "v").items(), key=lambda kv: repr(kv))))
    OUT.append(str(mk_dyn(0)))
    OUT.append(str(mk_dyn(8)))
    OUT.append(str(len(mk_dyn(500))))
    OUT.append(str(mk_const_key(10, 3)))
    OUT.append(str(mk_const_key(2.5, 0.5)))

    d = {}
    OUT.append(str(get_set(d, "k", 1)))
    OUT.append(str(get_set(d, 1, "k")))
    OUT.append(str(get_set(d, (1, 2), [3])))
    OUT.append(str(sorted(d.items(), key=lambda kv: repr(kv))))

    for k in ("nope", 999, None, (9, 9)):
        try:
            missing(d, k)
            OUT.append("missing %r no error" % (k,))
        except KeyError as e:
            OUT.append("missing %r KeyError %r" % (k, e.args[0]))

    class Boom:
        def __hash__(self):
            raise TypeError("unhashable boom")

    try:
        missing(d, Boom())
        OUT.append("boom no error")
    except TypeError as e:
        OUT.append("boom TypeError %s" % e)

    try:
        get_set(d, [1, 2], 1)
        OUT.append("list key no error")
    except TypeError as e:
        OUT.append("list key TypeError")

    OUT.append(str(comp(0)))
    OUT.append(str(comp(6)))
    OUT.append(str(comp2(["a", "bb", "ccc"])))
    OUT.append(str(dict_loop(0)))
    OUT.append(str(dict_loop(1000)))

    base = list(range(20))
    txt = "abcdefghijklmnopqrst"
    tup = tuple(range(20))
    for i in (-30, -5, -1, 0, 1, 5, 19, 20, 30):
        for j in (-30, -5, -1, 0, 1, 5, 19, 20, 30):
            OUT.append("sl %d %d = %s" % (i, j, sl_list(base, i, j)))
            OUT.append("st %d %d = %r" % (i, j, sl_str(txt, i, j)))
            OUT.append("tp %d %d = %s" % (i, j, sl_tuple(tup, i, j)))
    for j in (0, 3, 20, 40, -3):
        OUT.append("openl %d = %s" % (j, sl_open_l(base, j)))
        OUT.append("openr %d = %s" % (j, sl_open_r(base, j)))
    OUT.append("all = %s" % sl_all(base))

    a = list(range(10))
    OUT.append("store = %s" % sl_store(a, 2, 5, ["x", "y"]))
    OUT.append("store2 = %s" % sl_store(a, 0, 0, [-1]))
    OUT.append("store3 = %s" % sl_store(a, 100, 200, [99]))
    OUT.append("dellike = %s" % sl_del_like(list(range(10)), 3, 7))

    try:
        sl_store(list(range(5)), 1, 3, 7)
        OUT.append("store int no error")
    except TypeError:
        OUT.append("store int TypeError")

    OUT.append("slloop = %d" % sl_loop(base, 20))

    for line in OUT:
        print(line)
    print("N", len(OUT))
    return 0


sys.exit(main())
