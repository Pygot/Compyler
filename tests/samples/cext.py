import array
import binascii
import bz2
import decimal
import hashlib
import io
import json
import lzma
import math
import pickle
import re
import socket
import sqlite3
import struct
import sys
import zlib
from decimal import Decimal

import numpy as np

OUT = []


def emit(k, v):
    OUT.append("%s|%s" % (k, v))


def t_numpy():
    a = np.arange(4096, dtype=np.float64).reshape(64, 64)
    b = (a * 0.5) + 1.0
    c = a.dot(b)
    emit("np_version_ok", np.__version__.split(".")[0].isdigit())
    emit("np_sum", "%.6f" % float(c.sum()))
    emit("np_trace", "%.6f" % float(np.trace(c)))
    emit("np_max", "%.6f" % float(c.max()))
    emit("np_dtype", c.dtype.str)
    i = np.arange(1, 1001, dtype=np.int64)
    emit("np_int_sum", int(i.sum()))
    emit("np_int_prodmod", int((i % 7).prod()))
    emit("np_sort", int(np.sort(i[::-1])[0]))
    emit("np_where", int(np.where(i % 97 == 0)[0].size))
    f = np.linspace(0.0, math.pi, 257)
    emit("np_sin", "%.9f" % float(np.sin(f).sum()))
    emit("np_std", "%.9f" % float(f.std()))
    emit("np_lin", "%.6f" % float(np.linalg.norm(a)))


def t_sqlite():
    con = sqlite3.connect(":memory:")
    cur = con.cursor()
    cur.execute("create table t (id integer primary key, name text, val real)")
    cur.executemany("insert into t (name, val) values (?, ?)",
                    [("n%d" % i, i * 1.5) for i in range(2000)])
    con.commit()
    emit("sq_lib", sqlite3.sqlite_version.count(".") == 2)
    emit("sq_count", cur.execute("select count(*) from t").fetchone()[0])
    emit("sq_sum", "%.4f" % cur.execute("select sum(val) from t").fetchone()[0])
    emit("sq_like", cur.execute("select count(*) from t where name like 'n1%'").fetchone()[0])
    cur.execute("create index ix on t (val)")
    emit("sq_top", cur.execute("select name from t order by val desc limit 1").fetchone()[0])
    con.close()


def t_compress():
    data = (b"compyler native compression payload " * 512)
    z = zlib.compress(data, 9)
    emit("zlib_len", len(z))
    emit("zlib_rt", zlib.decompress(z) == data)
    emit("crc32", zlib.crc32(data) & 0xFFFFFFFF)
    emit("adler32", zlib.adler32(data) & 0xFFFFFFFF)
    b = bz2.compress(data, 9)
    emit("bz2_rt", bz2.decompress(b) == data)
    x = lzma.compress(data, preset=6)
    emit("lzma_rt", lzma.decompress(x) == data)
    emit("b2a", binascii.hexlify(data[:16]).decode())


def t_hash():
    data = b"compyler" * 4096
    for name in ("md5", "sha1", "sha256", "sha512", "blake2b", "sha3_256"):
        emit("h_" + name, hashlib.new(name, data).hexdigest()[:32])
    emit("pbkdf2", hashlib.pbkdf2_hmac("sha256", b"pw", b"salt", 4096).hex()[:32])


def t_struct_array():
    buf = struct.pack("<3sHIqd", b"cpy", 65535, 4294967295, -9007199254740993, 1.5)
    emit("st_size", len(buf))
    emit("st_unpack", str(struct.unpack("<3sHIqd", buf)))
    a = array.array("d", [i * 0.25 for i in range(1000)])
    emit("arr_sum", "%.4f" % sum(a))
    a2 = array.array("i", range(1000))
    emit("arr_bytes", len(a2.tobytes()))
    emit("arr_rev", a2[::-1][0])


def t_json_pickle():
    obj = {"a": [1, 2, 3], "b": {"c": 1.25, "d": None, "e": True},
           "s": "unicode é中", "n": list(range(200))}
    s = json.dumps(obj, sort_keys=True, separators=(",", ":"))
    emit("json_len", len(s))
    emit("json_rt", json.loads(s) == obj)
    emit("json_hash", hashlib.sha256(s.encode()).hexdigest()[:32])
    for proto in (2, 4, 5):
        p = pickle.dumps(obj, protocol=proto)
        emit("pickle_%d" % proto, pickle.loads(p) == obj)


def t_re():
    pat = re.compile(r"(\w+)@(\w+)\.(com|org|net)")
    text = " ".join("user%d@host%d.com" % (i, i) for i in range(500))
    m = pat.findall(text)
    emit("re_count", len(m))
    emit("re_last", "".join(m[-1]))
    emit("re_sub", len(pat.sub("X", text)))
    emit("re_split", len(re.split(r"\s+", text)))
    emit("re_uni", bool(re.match(r"^\w+$", "é中abc", re.UNICODE)))


def t_decimal():
    decimal.getcontext().prec = 50
    a = Decimal(1) / Decimal(7)
    emit("dec_div", str(a))
    emit("dec_sqrt", str(Decimal(2).sqrt()))
    emit("dec_exp", str(Decimal(1).exp()))
    emit("dec_ln", str(Decimal(10).ln()))
    emit("dec_impl", decimal.__name__)


def t_socket_io():
    emit("sock_htons", socket.htons(1234))
    emit("sock_ntohl", socket.ntohl(3735928559))
    emit("sock_aton", binascii.hexlify(socket.inet_aton("192.168.1.1")).decode())
    emit("sock_ntoa", socket.inet_ntoa(b"\x08\x08\x04\x04"))
    bio = io.BytesIO()
    for i in range(1000):
        bio.write(struct.pack("<I", i))
    emit("bio_len", len(bio.getvalue()))
    sio = io.StringIO()
    for i in range(1000):
        sio.write("%d\n" % i)
    emit("sio_lines", sio.getvalue().count("\n"))


def t_optional():
    try:
        from lxml import etree
        root = etree.Element("root")
        for i in range(100):
            etree.SubElement(root, "child", id=str(i)).text = "v%d" % i
        xml = etree.tostring(root)
        emit("lxml_len", len(xml))
        emit("lxml_find", len(etree.fromstring(xml).findall("child")))
    except ImportError:
        emit("lxml", "absent")
    try:
        from PIL import Image
        im = Image.new("RGB", (128, 128))
        for x in range(0, 128, 8):
            for y in range(0, 128, 8):
                im.putpixel((x, y), (x * 2, y * 2, 128))
        emit("pil_size", str(im.size))
        emit("pil_sum", sum(im.tobytes()[:4096]))
        buf = io.BytesIO()
        im.save(buf, "PNG")
        emit("pil_png", buf.getvalue()[:4] == b"\x89PNG")
    except ImportError:
        emit("pil", "absent")


def main():
    t_numpy()
    t_sqlite()
    t_compress()
    t_hash()
    t_struct_array()
    t_json_pickle()
    t_re()
    t_decimal()
    t_socket_io()
    t_optional()
    for line in OUT:
        print(line)
    print("DIGEST", hashlib.sha256("\n".join(OUT).encode()).hexdigest())
    print("COUNT", len(OUT))
    return 0


sys.exit(main())
