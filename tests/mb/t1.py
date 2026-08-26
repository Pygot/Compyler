def t1(h):
    out = []
    for y in range(h):
        ci = y * 2.0 / h - 1.0
        out.append(ci)
    return out

def t2(h):
    out = []
    for y in range(h):
        ci = y / h
        out.append(ci)
    return out

def t3(h):
    out = []
    y = 0
    while y < h:
        out.append(y / h)
        y = y + 1
    return out

print("t1(2)", t1(2))
print("t2(2)", t2(2))
print("t3(2)", t3(2))
