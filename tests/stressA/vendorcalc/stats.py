import numpy as np


def mean_of_squares(n):
    a = np.arange(n, dtype=np.int64)
    return int((a * a).mean())


def normalized(n):
    m = np.arange(n * n, dtype=np.float64).reshape(n, n)
    col = m.sum(axis=0)
    total = float(col.sum())
    return round(total / (n * n), 6)
