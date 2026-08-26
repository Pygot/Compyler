import hashlib
import io
from PIL import Image


def checker_hash(n):
    img = Image.new("RGB", (n, n))
    px = img.load()
    for y in range(n):
        for x in range(n):
            v = 255 if (x // 8 + y // 8) % 2 == 0 else 0
            px[x, y] = (v, (x * 3) % 256, (y * 5) % 256)
    buf = io.BytesIO()
    img.save(buf, format="BMP")
    return hashlib.sha256(buf.getvalue()).hexdigest()[:16]
