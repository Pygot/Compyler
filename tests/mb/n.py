def single(h):
    acc = 0
    for y in range(h):
        acc = acc + y
    return acc

def nest(h, w):
    acc = 0
    for y in range(h):
        for x in range(w):
            acc = acc + y * 10 + x
    return acc

def nest_mid(h, w):
    acc = 0
    for y in range(h):
        t = y * 100
        for x in range(w):
            acc = acc + t + x
    return acc

print("single(5) =", single(5))
print("nest(3,4) =", nest(3, 4))
print("nest_mid(3,4) =", nest_mid(3, 4))
