def wb(h, w, m):
    acc = 0
    for y in range(h):
        for x in range(w):
            k = 0
            while k < m:
                if k > x:
                    break
                k = k + 1
            acc = acc + k
    return acc

def wb_flt(h, w, m):
    acc = 0
    for y in range(h):
        ci = y * 1.0
        for x in range(w):
            cr = x * 1.0
            z = 0.0
            k = 0
            while k < m:
                if z > 3.0:
                    break
                z = z + cr + ci
                k = k + 1
            acc = acc + k
    return acc

print("wb(3,4,10) =", wb(3, 4, 10))
print("wb_flt(3,4,10) =", wb_flt(3, 4, 10))
