def hash_lines(n):
    h = 0
    i = 0
    while i < n:
        s = f"{i}|{i * i}|{i % 97}"
        j = 0
        m = len(s)
        while j < m:
            h = (h * 131 + ord(s[j])) & 0xFFFFFFFF
            j = j + 1
        i = i + 1
    return h


def format_table(vals):
    out = []
    for i, v in enumerate(vals):
        out.append(f"{i:>3} {v:<5} {v * v:08d}")
    return "|".join(out)


def unicode_mix():
    parts = ["příliš žluťoučký kůň", "úpěl ďábelské ódy", "☃"]
    joined = " / ".join(parts)
    return len(joined), joined.upper()[:20]
