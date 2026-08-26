def mandel_dbg(w, h, maxit):
    out = []
    for y in range(h):
        ci = y * 2.0 / h - 1.0
        for x in range(w):
            cr = x * 3.0 / w - 2.0
            zr = 0.0
            zi = 0.0
            k = 0
            while k < maxit:
                zr2 = zr * zr
                zi2 = zi * zi
                if zr2 + zi2 > 4.0:
                    break
                zi = 2.0 * zr * zi + ci
                zr = zr2 - zi2 + cr
                k = k + 1
            out.append((x, y, cr, ci, k))
    return out

for row in mandel_dbg(2, 2, 10):
    print(row)
