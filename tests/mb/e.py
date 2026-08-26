def probe2(w, h, maxit):
    out = []
    for y in range(h):
        ci = y * 2.0 / h - 1.0
        for x in range(w):
            out.append((x, y, w, h, maxit, ci))
            cr = x * 3.0 / w - 2.0
            out.append(("after", w, h, cr))
    return out

for r in probe2(2, 2, 7):
    print(r)
