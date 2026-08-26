G = 41

def add(a, b):
    return a + b

def use_global():
    return G + 1

print("add", add(1, 2))
print("glob", use_global())
