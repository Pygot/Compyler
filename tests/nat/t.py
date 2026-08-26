def add(a, b):
    return a + b

def loop(n):
    s = 0
    for i in range(n):
        s = s + i * i
    return s

def idx(lst, i):
    return lst[i] * 2

def cmpx(a, b):
    if a < b:
        return "lt"
    elif a == b:
        return "eq"
    return "gt"

print("add   ", add(2, 3), add(2.5, 0.5), add("a", "b"), add(2**70, 1))
print("loop  ", loop(10), loop(0), loop(1))
print("idx   ", idx([1, 2, 3], 1), idx((4, 5), -1))
print("cmpx  ", cmpx(1, 2), cmpx(2, 2), cmpx(3, 2))
print("ovf   ", add(2**62, 2**62))
try:
    idx([1], 9)
except IndexError as e:
    print("except", type(e).__name__)
