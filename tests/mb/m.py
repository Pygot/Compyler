def mandel(w, h, maxit):
    total = 0
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
            total = total + k
    return total

def probe(w, h, maxit):
    y = 0
    ci = y * 2.0 / h - 1.0
    x = 0
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
    return (ci, cr, zr, zi, k)

print("mandel small:", mandel(4, 3, 10))
print("probe:", probe(200, 150, 60))
