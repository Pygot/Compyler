def dv(a, b):
    return a * 3.0 / b - 2.0

def mixed(x, w):
    t1 = x * 3.0
    t2 = t1 / w
    t3 = t2 - 2.0
    return (t1, t2, t3)

def onlydiv(a, b):
    return a / b

print("dv(1,2)     =", dv(1, 2))
print("dv(0,2)     =", dv(0, 2))
print("mixed(1,2)  =", mixed(1, 2))
print("onlydiv(3.0,2) =", onlydiv(3.0, 2))
print("onlydiv(3,2)   =", onlydiv(3, 2))
print("onlydiv(3.0,2.0)=", onlydiv(3.0, 2.0))
