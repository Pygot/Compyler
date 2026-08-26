import hashlib
import io
import os
import sys

OUT = []


def emit(k, v):
    OUT.append("%s|%s" % (k, v))


def t_certifi():
    import certifi
    p = certifi.where()
    emit("certifi_exists", os.path.isfile(p))
    with open(p, "rb") as f:
        data = f.read()
    emit("certifi_size_ok", len(data) > 50000)
    emit("certifi_pem", data.count(b"BEGIN CERTIFICATE") > 100)


def t_zoneinfo():
    import zoneinfo
    from datetime import datetime
    keys = zoneinfo.available_timezones()
    emit("tz_count_ok", len(keys) > 300)
    for name in ("UTC", "Europe/Prague", "America/New_York", "Asia/Tokyo"):
        z = zoneinfo.ZoneInfo(name)
        d = datetime(2024, 7, 1, 12, 0, tzinfo=z)
        emit("tz_" + name.replace("/", "_"), d.strftime("%Y-%m-%d %H:%M %Z %z"))
        d2 = datetime(2024, 1, 15, 12, 0, tzinfo=z)
        emit("tzw_" + name.replace("/", "_"), d2.strftime("%Z %z"))


def t_metadata():
    import requests
    from importlib.metadata import version, distribution
    for name in ("certifi", "requests", "idna"):
        try:
            emit("meta_" + name, bool(version(name)))
            d = distribution(name)
            emit("meta_files_" + name, d.files is not None)
        except Exception as e:
            emit("meta_" + name, "ERR " + type(e).__name__)


def t_encodings():
    text = "příliš žluťoučký kůň 中文 \U0001F600"
    for enc in ("utf-8", "utf-16", "utf-32", "cp1250", "cp1252", "latin-1",
                "iso8859-2", "cp852", "mac-roman", "koi8-r", "big5", "shift_jis",
                "gb2312", "euc-jp", "cp437", "cp850", "idna", "punycode"):
        try:
            if enc in ("idna", "punycode"):
                b = "example".encode(enc)
            else:
                b = text.encode(enc, "replace")
            emit("enc_" + enc, hashlib.md5(b).hexdigest()[:12])
        except Exception as e:
            emit("enc_" + enc, "ERR " + type(e).__name__)


def t_pygments():
    from pygments import highlight
    from pygments.lexers import PythonLexer, get_lexer_by_name
    from pygments.formatters import HtmlFormatter, TerminalFormatter
    src = "def f(x):\n    return x + 1\n"
    h = highlight(src, PythonLexer(), HtmlFormatter())
    emit("pyg_html", hashlib.md5(h.encode()).hexdigest()[:16])
    emit("pyg_lexers", get_lexer_by_name("json").name)
    emit("pyg_styles", len(HtmlFormatter().style.styles) > 10)
    t = highlight(src, PythonLexer(), TerminalFormatter())
    emit("pyg_term_len", len(t) > len(src))


def t_jinja():
    import jinja2
    env = jinja2.Environment(autoescape=True)
    tpl = env.from_string("{% for i in items %}{{ i }}-{{ loop.index }};{% endfor %}|{{ raw }}")
    emit("jinja", tpl.render(items=["a", "b", "c"], raw="<x>"))
    emit("jinja_filters", len(env.filters) > 30)


def t_idna_charset():
    import idna
    import charset_normalizer
    emit("idna_enc", idna.encode("bücher.de").decode())
    emit("idna_dec", idna.decode(b"xn--bcher-kva.de"))
    r = charset_normalizer.from_bytes("žluťoučký".encode("cp1250")).best()
    emit("charset", r is not None)


def t_crypto():
    from cryptography.hazmat.primitives import hashes
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    from cryptography.fernet import Fernet
    d = hashes.Hash(hashes.SHA256())
    d.update(b"compyler")
    emit("crypto_sha", d.finalize().hex()[:32])
    key = b"0" * 32
    iv = b"1" * 16
    c = Cipher(algorithms.AES(key), modes.CBC(iv)).encryptor()
    emit("crypto_aes", (c.update(b"A" * 32) + c.finalize()).hex()[:32])
    f = Fernet(Fernet.generate_key())
    emit("crypto_fernet", f.decrypt(f.encrypt(b"secret")) == b"secret")


def t_scipy():
    import numpy as np
    import scipy.linalg
    import scipy.special
    import scipy.optimize
    a = np.array([[4.0, 1.0], [1.0, 3.0]])
    emit("scipy_det", "%.9f" % float(scipy.linalg.det(a)))
    emit("scipy_inv", "%.9f" % float(scipy.linalg.inv(a).sum()))
    emit("scipy_gamma", "%.9f" % float(scipy.special.gamma(5.5)))
    emit("scipy_erf", "%.9f" % float(scipy.special.erf(1.0)))
    r = scipy.optimize.minimize_scalar(lambda x: (x - 2.0) ** 2)
    emit("scipy_opt", "%.6f" % float(r.x))
    emit("scipy_eig", "%.9f" % float(np.sort(scipy.linalg.eigvalsh(a))[0]))


def t_pil_plugins():
    from PIL import Image
    import io as _io
    im = Image.new("RGB", (64, 48), (10, 20, 30))
    for x in range(0, 64, 4):
        im.putpixel((x, x % 48), (255, 128, 0))
    got = []
    for fmt in ("PNG", "JPEG", "BMP", "GIF", "TIFF", "WEBP", "PPM"):
        try:
            buf = _io.BytesIO()
            im.convert("RGB" if fmt != "GIF" else "P").save(buf, fmt)
            b = buf.getvalue()
            back = Image.open(_io.BytesIO(b))
            got.append("%s:%dx%d" % (fmt, back.size[0], back.size[1]))
        except Exception as e:
            got.append("%s:ERR" % fmt)
    emit("pil_formats", ",".join(got))


def t_lxml_catalog():
    from lxml import etree
    xsd = b'''<?xml version="1.0"?>
<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">
  <xs:element name="root" type="xs:string"/>
</xs:schema>'''
    schema = etree.XMLSchema(etree.fromstring(xsd))
    emit("lxml_valid", schema.validate(etree.fromstring(b"<root>ok</root>")))
    emit("lxml_invalid", schema.validate(etree.fromstring(b"<other/>")))
    emit("lxml_xpath", etree.fromstring(b"<a><b n='1'/><b n='2'/></a>").xpath("//b/@n"))
    emit("lxml_ver", etree.LXML_VERSION[0] > 0)


def t_pywin32():
    import win32api
    import win32con
    emit("win32_ver_ok", isinstance(win32api.GetVersionEx(), tuple))
    emit("win32_const", win32con.MB_OK)


def t_cffi():
    import cffi
    ffi = cffi.FFI()
    emit("cffi_sizeof", ffi.sizeof("int"))
    emit("cffi_new", ffi.new("int[4]")[0])


TESTS = (
    ("certifi", t_certifi),
    ("zoneinfo", t_zoneinfo),
    ("metadata", t_metadata),
    ("encodings", t_encodings),
    ("pygments", t_pygments),
    ("jinja2", t_jinja),
    ("idna", t_idna_charset),
    ("cryptography", t_crypto),
    ("scipy", t_scipy),
    ("pil", t_pil_plugins),
    ("lxml", t_lxml_catalog),
    ("pywin32", t_pywin32),
    ("cffi", t_cffi),
)


def main():
    for name, fn in TESTS:
        try:
            fn()
        except Exception as e:
            emit("FAILED_" + name, "%s: %s" % (type(e).__name__, e))
    for line in OUT:
        print(line)
    print("DIGEST", hashlib.sha256("\n".join(OUT).encode()).hexdigest())
    print("COUNT", len(OUT))
    return 0


sys.exit(main())
