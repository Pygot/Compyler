def t4(w, h):
    out = []
    for y in range(h):
        ci = y * 2.0 / h - 1.0
        for x in range(w):
            cr = x * 3.0 / w - 2.0
            out.append((ci, cr))
    return out

print("t4(2,2)", t4(2, 2))
