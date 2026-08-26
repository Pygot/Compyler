#include "comp.h"
#include "scan.h"
#include "pe.h"
#include "../nc/nc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cpy_py       PY;
static cpy_itemlist ITEMS;
static cpy_set       SEEN_MOD, EXCLUDES, HIDDEN, BUILTINS, MISSING, NATSEEN;
static cpy_set       DYNPKG, DYNPKG_DONE;
static char       **QUEUE;
static int          QN, QCAP;
typedef struct { char *mod; char *par; } why_e;
static why_e       *WHY;
static int          NWHY, CWHY;
static char        *PRUNE_WHY;
static char         WHY_CTX[512];
static wchar_t      CUR_FILE[MAX_PATH * 2];
static wchar_t     *PY_BASE, *PY_LIB, *PY_DLLS, *PY_DLL_PATH, *APP_DIR;
static int          IN_VENV, VENV_SYSSITE, SIZE_REPORT, PRUNE, TOOLCHAIN_WHY, HAS_HOOK;
static int          SCAN_APP, APP_USES_MP;
static char         CUR_PKG[512];
static char         LAST_FROM[512];

static void set_cur_pkg(const char *pkg);
static char        *PY_DLL_NAME;
static long         PYC_MAGIC;
static int          OPTIMIZE, VERBOSE, STRIP = 1, NO_DEFAULT_EX, DROP_TESTS;
static int          N_COMPILED, N_REUSED;
static nc_ctx      *NC;
static int          NATIVE = 1, NC_FUNCS;
static wchar_t     *NC_WORK;
static struct { wchar_t *src; char *dest; } *APPMODS;
static int          NAPPMODS, CAPPMODS;
static int          KEEP_BUILD;

typedef struct {
    wchar_t    *path;
    const char *prefix;
    int         is_site;
} root_t;

static root_t ROOTS[32];
static int    NROOTS;

static wchar_t *DLLDIRS[64];
static int      NDLLDIRS;

static char    *LIBDIRS[128];
static int      NLIBDIRS;

typedef struct {
    const char    *trigger;
    const wchar_t *dir;
    const char    *dest;
    struct { const wchar_t *prefix; const char *env; } items[4];
    const wchar_t *drop[4];
} data_rule;

static const data_rule DATA_RULES[] = {
    { "_tkinter", L"tcl", "lib",
      { { L"tcl", "TCL_LIBRARY" }, { L"tk", "TK_LIBRARY" }, { NULL, NULL } },
      { L"demos", NULL } },
    { NULL, NULL, NULL, { { NULL, NULL } }, { NULL } }
};

static struct { const char *name; char *path; } DATAENV[16];
static int              NDATAENV;
static const wchar_t *const *DROPDIRS;
static char    *PTHLINES[64];
static int      NPTH;

typedef struct {
    char    *name;
    wchar_t *dir;
} distmap_e;

typedef struct {
    char    *name;
    wchar_t *path;
} dllidx_e;

static dllidx_e *DLLIDX;
static int       NDLLIDX, CDLLIDX, DLLIDX_READY;

static distmap_e *DISTMAP;
static int        NDIST, CDIST;

static const char *BASE_MODULES[] = {
    "encodings", "codecs", "io", "abc", "os", "ntpath", "genericpath", "stat",
    "_collections_abc", "types", "warnings", "importlib",
    "traceback", "linecache", "contextlib", "functools", "collections", "operator",
    "keyword", "reprlib", "enum", "re", "sre_compile", "sre_parse", "sre_constants",
    "copyreg", "weakref", "_weakrefset", "heapq", "textwrap", NULL
};

static const char *DEFAULT_EXCLUDES[] = {
    "test", "idlelib", "lib2to3", "turtledemo", "ensurepip", "pip", "wheel",
    "pytest", "_pytest", "hypothesis", "nose", "Cython", "cython", "numba",
    "llvmlite", "mypy", "sphinx", "PyInstaller", "setuptools", "pkg_resources",
    "_distutils_hack", "pydoc_data", "doctest", NULL
};

static const char *NON_WINDOWS[] = {
    "posix", "pwd", "grp", "termios", "fcntl", "resource", "syslog", "readline",
    "crypt", "spwd", "nis", "_posixshmem", "_posixsubprocess", "_scproxy", "java",
    "org", "vms_lib", "os2", "ce", "riscos", "__pypy__", "_dummy_thread",
    "cStringIO", "cPickle", "copy_reg", "StringIO", "ConfigParser", "Queue",
    "httplib", "HTMLParser", "urlparse", "thread", "exceptions", "new", "sets",
    "_winreg", "dummy_thread", "commands", "urllib2", "cjkcodecs",
    "__main__", "_frozen_importlib", "_frozen_importlib_external", "sitecustomize",
    "usercustomize", "__builtin__", "builtin", NULL
};

static const char *SKIP_EXT[] = {
    ".pyi", ".pyx", ".pxd", ".pxi", ".c", ".h", ".hpp", ".cpp", ".cc", ".lib",
    ".exp", ".obj", ".a", ".pdb", ".whl", ".src", ".po", ".pot", NULL
};

static const char *SYS_DLL[] = {
    "api-ms-", "ext-ms-", "vcruntime", "msvcp", "msvcr", "ucrtbase", "concrt",
    "kernel32", "kernelbase", "user32", "gdi32", "advapi32", "ws2_32", "ole32",
    "oleaut32", "shell32", "shlwapi", "comctl32", "comdlg32", "crypt32", "ntdll",
    "rpcrt4", "secur32", "iphlpapi", "winmm", "wldap32", "version", "psapi",
    "setupapi", "bcrypt", "ncrypt", "cabinet", "imm32", "dbghelp", "netapi32",
    "mpr", "userenv", "powrprof", "opengl32", "glu32", "dxgi", "d3d", "uxtheme",
    "dwmapi", "combase", "sechost", "python3", "winspool", "mswsock", "dnsapi",
    "normaliz", "wsock32", "wininet", "urlmon", "propsys", "cfgmgr32", "msimg32",
    "usp10", "gdiplus", "pdh", "wtsapi32", "authz", "activeds", "odbc32", NULL
};

static void die(const char *msg)
{
    fprintf(stderr, "compyler: %s\n", msg);
    ExitProcess(1);
}

static void diew(const wchar_t *msg, const wchar_t *arg)
{
    fwprintf(stderr, L"compyler: %s %s\n", msg, arg ? arg : L"");
    ExitProcess(1);
}

static int has_ext(const wchar_t *name, const wchar_t *ext)
{
    size_t n = wcslen(name), e = wcslen(ext);
    return n > e && !_wcsicmp(name + n - e, ext);
}

static int in_list(const char *name, const char **list)
{
    int i;
    for (i = 0; list[i]; i++)
        if (!_strnicmp(name, list[i], strlen(list[i]))) return 1;
    return 0;
}

static const char *why_of(const char *mod)
{
    int i;
    for (i = 0; i < NWHY; i++)
        if (!strcmp(WHY[i].mod, mod)) return WHY[i].par;
    return NULL;
}

static void why_record(const char *name)
{
    if (why_of(name)) return;
    if (NWHY == CWHY) {
        CWHY = CWHY ? CWHY * 2 : 256;
        WHY = (why_e *)cpy_xrealloc(WHY, sizeof(*WHY) * (size_t)CWHY);
    }
    WHY[NWHY].mod = cpy_adup(name);
    WHY[NWHY].par = cpy_adup(CUR_PKG[0] ? CUR_PKG :
                             (WHY_CTX[0] ? WHY_CTX : "(baseline)"));
    NWHY++;
}

static void queue_push(const char *name)
{
    why_record(name);
    if (QN == QCAP) {
        QCAP = QCAP ? QCAP * 2 : 256;
        QUEUE = (char **)cpy_xrealloc(QUEUE, sizeof(char *) * (size_t)QCAP);
    }
    QUEUE[QN++] = cpy_adup(name);
}

static void why_report(void)
{
    int i;
    if (!PRUNE_WHY) return;
    for (i = 0; i < NWHY; i++) {
        size_t pl = strlen(PRUNE_WHY);
        const char *m = WHY[i].mod;
        const char *p;
        int hops = 0;
        if (strcmp(PRUNE_WHY, "all") &&
            (strncmp(m, PRUNE_WHY, pl) || (m[pl] && m[pl] != '.')))
            continue;
        fprintf(stderr, "  %s", m);
        p = WHY[i].par;
        while (p && strcmp(p, "(baseline)") && hops++ < 20) {
            fprintf(stderr, " <- %s", p);
            if (!strcmp(p, m)) break;
            p = why_of(p);
        }
        if (p && !strcmp(p, "(baseline)")) fprintf(stderr, " <- (baseline)");
        fputc('\n', stderr);
    }
}

static void top_of(const char *dotted, char *out, size_t n)
{
    const char *d = strchr(dotted, '.');
    size_t l = d ? (size_t)(d - dotted) : strlen(dotted);
    if (l >= n) l = n - 1;
    memcpy(out, dotted, l);
    out[l] = 0;
}

static void on_import(void *ud, const char *name, size_t len, size_t full, int level)
{
    char buf[512];
    (void)ud;

    if (SCAN_APP && len == 15 && !memcmp(name, "multiprocessing", 15)) APP_USES_MP = 1;
    if (level == -1) {
        char top[256];
        if (!PRUNE) return;
        if (full > 0 && len > 0 && len < sizeof(top)) {
            memcpy(top, name, len);
            top[len] = 0;
        } else {
            if (!CUR_PKG[0]) return;
            top_of(CUR_PKG, top, sizeof(top));
            if (!strcmp(top, "importlib") || !strcmp(top, "pkgutil") ||
                !strcmp(top, "runpy"))
                return;
        }
        if (cpy_set_add(&DYNPKG, top)) {
            if (PRUNE_WHY)
                fprintf(stderr, "  dynpkg %s (scanning %s / %ls)\n",
                        top, CUR_PKG, CUR_FILE);
            queue_push(top);
        }
        return;
    }

    if (level == -2) {
        if (!PRUNE || !LAST_FROM[0] || full == 0 || full + strlen(LAST_FROM) + 2 >= sizeof(buf))
            return;
        _snprintf(buf, sizeof(buf) - 1, "%s.%.*s", LAST_FROM, (int)full, name);
        buf[sizeof(buf) - 1] = 0;
        if (cpy_set_has(&SEEN_MOD, buf)) return;
        queue_push(buf);
        return;
    }

    LAST_FROM[0] = 0;

    if (!PRUNE) {
        if (level > 0 || len == 0 || len >= sizeof(buf)) return;
        memcpy(buf, name, len);
        buf[len] = 0;
        if (cpy_set_has(&SEEN_MOD, buf)) return;
        queue_push(buf);
        return;
    }

    if (level > 0) {
        char base[512];
        int up = level - 1;
        if (!CUR_PKG[0]) return;
        strncpy(base, CUR_PKG, sizeof(base) - 1);
        base[sizeof(base) - 1] = 0;
        while (up-- > 0) {
            char *d = strrchr(base, '.');
            if (!d) { base[0] = 0; break; }
            *d = 0;
        }
        if (!base[0]) return;
        if (full) {
            if (full + strlen(base) + 2 >= sizeof(buf)) return;
            _snprintf(buf, sizeof(buf) - 1, "%s.%.*s", base, (int)full, name);
        } else {
            strncpy(buf, base, sizeof(buf) - 1);
        }
        buf[sizeof(buf) - 1] = 0;
    } else {
        if (full == 0 || full >= sizeof(buf)) return;
        memcpy(buf, name, full);
        buf[full] = 0;
    }

    strncpy(LAST_FROM, buf, sizeof(LAST_FROM) - 1);
    LAST_FROM[sizeof(LAST_FROM) - 1] = 0;
    if (cpy_set_has(&SEEN_MOD, buf)) return;
    queue_push(buf);
}

static uint32_t unix_time(const FILETIME *ft)
{
    uint64_t t = ((uint64_t)ft->dwHighDateTime << 32) | ft->dwLowDateTime;
    if (t < 116444736000000000ULL) return 0;
    return (uint32_t)((t - 116444736000000000ULL) / 10000000ULL);
}

static unsigned char *reuse_pyc(const wchar_t *src, size_t *out_len)
{
    wchar_t dir[MAX_PATH * 4], cand[MAX_PATH * 4];
    const wchar_t *base;
    wchar_t stem[256];
    WIN32_FILE_ATTRIBUTE_DATA fa;
    unsigned char *d;
    size_t len = 0, sl;
    uint32_t mt, sz;

    if (!GetFileAttributesExW(src, GetFileExInfoStandard, &fa)) return NULL;
    mt = unix_time(&fa.ftLastWriteTime);
    sz = fa.nFileSizeLow;

    wcsncpy(dir, src, MAX_PATH * 4 - 1);
    dir[MAX_PATH * 4 - 1] = 0;
    base = wcsrchr(dir, L'\\');
    if (!base) return NULL;
    *(wchar_t *)base = 0;
    base++;
    wcsncpy(stem, base, 255);
    stem[255] = 0;
    sl = wcslen(stem);
    if (sl < 4) return NULL;
    stem[sl - 3] = 0;

    if (OPTIMIZE > 0)
        _snwprintf(cand, MAX_PATH * 4 - 1, L"%s\\__pycache__\\%s.cpython-%d%d.opt-%d.pyc",
                   dir, stem, PY.version / 100, PY.version % 100, OPTIMIZE);
    else
        _snwprintf(cand, MAX_PATH * 4 - 1, L"%s\\__pycache__\\%s.cpython-%d%d.pyc",
                   dir, stem, PY.version / 100, PY.version % 100);
    cand[MAX_PATH * 4 - 1] = 0;

    d = cpy_read_file(cand, &len);
    if (!d) return NULL;
    if (len < 20) { free(d); return NULL; }
    if (*(uint32_t *)d != (uint32_t)PYC_MAGIC) { free(d); return NULL; }
    if (*(uint32_t *)(d + 4) != 0) { free(d); return NULL; }
    if (*(uint32_t *)(d + 8) != mt || *(uint32_t *)(d + 12) != sz) { free(d); return NULL; }
    *out_len = len;
    return d;
}

static unsigned char *make_pyc(const wchar_t *src, const unsigned char *source, size_t slen,
                               const char *display, size_t *out_len, int native)
{
    PyObj code, by;
    unsigned char *pyc, *tmp = NULL;
    char *bytes;
    cpy_ssize n;

    if (!native) {
        pyc = reuse_pyc(src, out_len);
        if (pyc) { N_REUSED++; return pyc; }
    }

    if (slen && source[slen - 1] != '\n') {
        tmp = (unsigned char *)cpy_xmalloc(slen + 2);
        memcpy(tmp, source, slen);
        tmp[slen] = '\n';
        tmp[slen + 1] = 0;
        source = tmp;
    }

    code = PY.Py_CompileStringExFlags((const char *)source, display, CPY_FILE_INPUT, NULL, OPTIMIZE);
    free(tmp);
    if (!code) {
        if (VERBOSE) {
            fprintf(stderr, "compyler: skipping (does not compile) %s\n", display);
            PY.PyErr_Print();
        }
        PY.PyErr_Clear();
        return NULL;
    }
    if (native && NC && nc_ready(NC)) {
        char mod[256];
        char *dot;
        const char *base = strrchr(display, '\\');
        strncpy(mod, base ? base + 1 : display, sizeof(mod) - 1);
        mod[sizeof(mod) - 1] = 0;
        dot = strrchr(mod, '.');
        if (dot) *dot = 0;
        {
            PyObj repl = nc_transform(NC, code, mod);
            if (repl) { PY.Py_DecRef(code); code = repl; }
        }
    }
    by = PY.PyMarshal_WriteObjectToString(code, CPY_MARSHAL_VERSION);
    PY.Py_DecRef(code);
    if (!by) { PY.PyErr_Clear(); return NULL; }
    bytes = PY.PyBytes_AsString(by);
    n = PY.PyBytes_Size(by);
    pyc = (unsigned char *)cpy_xmalloc((size_t)n + 16);
    *(uint32_t *)pyc = (uint32_t)PYC_MAGIC;
    *(uint32_t *)(pyc + 4) = 0;
    *(uint32_t *)(pyc + 8) = 0;
    *(uint32_t *)(pyc + 12) = (uint32_t)slen;
    memcpy(pyc + 16, bytes, (size_t)n);
    PY.Py_DecRef(by);
    *out_len = (size_t)n + 16;
    N_COMPILED++;
    return pyc;
}

static void handle_py(const wchar_t *src, const char *dest, int scan)
{
    unsigned char *source, *pyc;
    size_t slen = 0, plen = 0;

    if (cpy_set_has(&ITEMS.seen, dest)) return;
    source = cpy_read_file(src, &slen);
    if (!source) return;
    wcsncpy(CUR_FILE, src, MAX_PATH * 2 - 1);
    CUR_FILE[MAX_PATH * 2 - 1] = 0;
    if (scan) {
        int wasapp = SCAN_APP;
        SCAN_APP = !strncmp(dest, "app", 3) && (dest[3] == '\\' || dest[3] == 0);
        cpy_scan_imports((const char *)source, slen, on_import, NULL);
        SCAN_APP = wasapp;
    }
    {
        int is_app = NATIVE && !strncmp(dest, "app\\", 4);
        if (is_app) {
            if (NAPPMODS == CAPPMODS) {
                CAPPMODS = CAPPMODS ? CAPPMODS * 2 : 32;
                APPMODS = (void *)cpy_xrealloc(APPMODS, sizeof(*APPMODS) * (size_t)CAPPMODS);
            }
            APPMODS[NAPPMODS].src = cpy_wdup(src);
            APPMODS[NAPPMODS].dest = cpy_adup(dest);
            NAPPMODS++;
        }
        pyc = make_pyc(src, source, slen, dest, &plen, is_app);
    }
    free(source);
    if (pyc) cpy_items_add_blob(&ITEMS, dest, pyc, plen);
}

typedef struct {
    const wchar_t *srcdir;
    const char    *destdir;
} nat_ctx;

static void handle_native(const wchar_t *src, const char *dest);

static int is_test_dir(const wchar_t *n);

static void index_dlls(const wchar_t *dir, int depth)
{
    wchar_t *pat = cpy_wjoin(dir, L"*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    free(pat);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        wchar_t *full;
        if (fd.cFileName[0] == L'.') continue;
        full = cpy_wjoin(dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (depth < 5 && _wcsicmp(fd.cFileName, L"__pycache__") && !is_test_dir(fd.cFileName))
                index_dlls(full, depth + 1);
        } else if (has_ext(fd.cFileName, L".dll")) {
            char *an = cpy_w_to_utf8(fd.cFileName, -1);
            int i, seen = 0;
            for (i = 0; i < NDLLIDX; i++)
                if (!_stricmp(DLLIDX[i].name, an)) { seen = 1; break; }
            if (!seen) {
                if (NDLLIDX == CDLLIDX) {
                    CDLLIDX = CDLLIDX ? CDLLIDX * 2 : 256;
                    DLLIDX = (dllidx_e *)cpy_xrealloc(DLLIDX, sizeof(dllidx_e) * (size_t)CDLLIDX);
                }
                DLLIDX[NDLLIDX].name = an;
                DLLIDX[NDLLIDX].path = cpy_wdup(full);
                NDLLIDX++;
                an = NULL;
            }
            free(an);
        }
        free(full);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static wchar_t *find_indexed_dll(const char *name)
{
    int i;
    if (!DLLIDX_READY) {
        DLLIDX_READY = 1;
        for (i = 0; i < NROOTS; i++) index_dlls(ROOTS[i].path, 0);
        if (VERBOSE) fprintf(stderr, "  dll index: %d entries\n", NDLLIDX);
    }
    for (i = 0; i < NDLLIDX; i++)
        if (!_stricmp(DLLIDX[i].name, name)) return cpy_wdup(DLLIDX[i].path);
    return NULL;
}

static void add_libdir(const char *d)
{
    int i;
    if (!d || !d[0]) return;
    for (i = 0; i < NLIBDIRS; i++) if (!_stricmp(LIBDIRS[i], d)) return;
    if (NLIBDIRS < 128) LIBDIRS[NLIBDIRS++] = cpy_adup(d);
}

static void on_dll(void *ud, const char *name)
{
    nat_ctx *c = (nat_ctx *)ud;
    wchar_t *wname, *cand = NULL;
    char dest[1024];
    int i;

    if (in_list(name, SYS_DLL)) return;
    wname = cpy_utf8_to_w(name, -1);

    cand = cpy_wjoin(c->srcdir, wname);
    if (!cpy_file_exists(cand)) {
        free(cand);
        cand = NULL;
        for (i = 0; i < NDLLDIRS; i++) {
            wchar_t *t = cpy_wjoin(DLLDIRS[i], wname);
            if (cpy_file_exists(t)) { cand = t; break; }
            free(t);
        }
        if (!cand) cand = find_indexed_dll(name);
    }
    free(wname);
    if (!cand) return;
    if (c->destdir[0]) _snprintf(dest, sizeof(dest) - 1, "%s\\%s", c->destdir, name);
    else _snprintf(dest, sizeof(dest) - 1, "%s", name);
    dest[sizeof(dest) - 1] = 0;
    add_libdir(c->destdir);
    if (!cpy_set_has(&ITEMS.seen, dest)) handle_native(cand, dest);
    free(cand);
}

static void handle_native(const wchar_t *src, const char *dest)
{
    unsigned char *d;
    size_t len = 0;
    char destdir[1024];
    wchar_t *srcdir;
    nat_ctx ctx;
    char *slash;

    {
        char *key = cpy_w_to_utf8(src, -1);
        int fresh;
        _strlwr(key);
        fresh = cpy_set_add(&NATSEEN, key);
        free(key);
        if (!fresh) return;
    }
    if (!cpy_items_add_file(&ITEMS, dest, src)) return;

    d = cpy_read_file(src, &len);
    if (!d) return;

    strncpy(destdir, dest, sizeof(destdir) - 1);
    destdir[sizeof(destdir) - 1] = 0;
    slash = strrchr(destdir, '\\');
    if (slash) *slash = 0; else destdir[0] = 0;

    srcdir = cpy_wdup(src);
    { wchar_t *s = wcsrchr(srcdir, L'\\'); if (s) *s = 0; }

    ctx.srcdir = srcdir;
    ctx.destdir = destdir;
    cpy_pe_imports(d, len, on_dll, &ctx);
    free(d);
    free(srcdir);
}

static void collect_tree(const wchar_t *dir, const char *destdir, int scan);

static void dispatch_file(const wchar_t *src, const wchar_t *name, const char *destdir, int scan)
{
    char dest[1024];
    char *aname = cpy_w_to_utf8(name, -1);
    int i;

    if (has_ext(name, L".py")) {
        size_t n = strlen(aname);
        aname[n - 3] = 0;
        _snprintf(dest, sizeof(dest) - 1, "%s\\%s.pyc", destdir, aname);
        dest[sizeof(dest) - 1] = 0;
        handle_py(src, dest, scan);
    } else if (has_ext(name, L".pyd") || has_ext(name, L".dll")) {
        _snprintf(dest, sizeof(dest) - 1, "%s\\%s", destdir, aname);
        dest[sizeof(dest) - 1] = 0;
        handle_native(src, dest);
    } else {
        if (STRIP) {
            for (i = 0; SKIP_EXT[i]; i++) {
                size_t el = strlen(SKIP_EXT[i]), nl = strlen(aname);
                if (nl > el && !_stricmp(aname + nl - el, SKIP_EXT[i])) { free(aname); return; }
            }
        }
        _snprintf(dest, sizeof(dest) - 1, "%s\\%s", destdir, aname);
        dest[sizeof(dest) - 1] = 0;
        cpy_items_add_file(&ITEMS, dest, src);
    }
    free(aname);
}

static int is_test_dir(const wchar_t *n)
{
    return !_wcsicmp(n, L"tests") || !_wcsicmp(n, L"test") || !_wcsicmp(n, L"benchmarks");
}

static void collect_tree(const wchar_t *dir, const char *destdir, int scan)
{
    wchar_t *pat = cpy_wjoin(dir, L"*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    free(pat);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        wchar_t *full;
        if (fd.cFileName[0] == L'.' &&
            (!fd.cFileName[1] || (fd.cFileName[1] == L'.' && !fd.cFileName[2]))) continue;
        full = cpy_wjoin(dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            char sub[1024];
            char *an;
            int sub_scan = scan;
            if (!_wcsicmp(fd.cFileName, L"__pycache__") || !_wcsicmp(fd.cFileName, L".git")) {
                free(full);
                continue;
            }
            if (DROPDIRS && STRIP) {
                int dz, skip = 0;
                for (dz = 0; DROPDIRS[dz]; dz++)
                    if (!_wcsicmp(fd.cFileName, DROPDIRS[dz])) { skip = 1; break; }
                if (skip) { free(full); continue; }
            }
            if (is_test_dir(fd.cFileName)) {
                if (DROP_TESTS) { free(full); continue; }
                if (STRIP) sub_scan = 0;
            }
            an = cpy_w_to_utf8(fd.cFileName, -1);
            _snprintf(sub, sizeof(sub) - 1, "%s\\%s", destdir, an);
            sub[sizeof(sub) - 1] = 0;
            free(an);
            collect_tree(full, sub, sub_scan);
        } else {
            if (STRIP && !_wcsicmp(fd.cFileName, L"conftest.py")) { free(full); continue; }
            dispatch_file(full, fd.cFileName, destdir, scan);
        }
        free(full);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static wchar_t *find_ext_module(const wchar_t *root, const char *name)
{
    wchar_t *wname = cpy_utf8_to_w(name, -1);
    wchar_t pat[MAX_PATH * 2];
    wchar_t *cand;
    WIN32_FIND_DATAW fd;
    HANDLE h;

    _snwprintf(pat, MAX_PATH * 2 - 1, L"%s\\%s.pyd", root, wname);
    pat[MAX_PATH * 2 - 1] = 0;
    if (cpy_file_exists(pat)) { free(wname); return cpy_wdup(pat); }

    _snwprintf(pat, MAX_PATH * 2 - 1, L"%s\\%s.*.pyd", root, wname);
    pat[MAX_PATH * 2 - 1] = 0;
    free(wname);
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    cand = cpy_wjoin(root, fd.cFileName);
    FindClose(h);
    return cand;
}

static void add_distinfo(const char *name, const char *destdir)
{
    int i;
    for (i = 0; i < NDIST; i++) {
        if (!_stricmp(DISTMAP[i].name, name)) {
            wchar_t *base = wcsrchr(DISTMAP[i].dir, L'\\');
            char *an = cpy_w_to_utf8(base ? base + 1 : DISTMAP[i].dir, -1);
            char sub[1024];
            _snprintf(sub, sizeof(sub) - 1, "%s\\%s", destdir, an);
            sub[sizeof(sub) - 1] = 0;
            free(an);
            collect_tree(DISTMAP[i].dir, sub, 0);
        }
    }
}

static void add_libs_dir(const wchar_t *root, const char *name, const char *destdir)
{
    wchar_t pat[MAX_PATH * 2];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    wchar_t *wname = cpy_utf8_to_w(name, -1);

    _snwprintf(pat, MAX_PATH * 2 - 1, L"%s\\%s.libs", root, wname);
    pat[MAX_PATH * 2 - 1] = 0;
    free(wname);
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    FindClose(h);
    {
        wchar_t *dir = cpy_wjoin(root, fd.cFileName);
        char *an = cpy_w_to_utf8(fd.cFileName, -1);
        char sub[1024];
        _snprintf(sub, sizeof(sub) - 1, "%s\\%s", destdir, an);
        sub[sizeof(sub) - 1] = 0;
        collect_tree(dir, sub, 0);
        add_libdir(sub);
        if (NDLLDIRS < 64) DLLDIRS[NDLLDIRS++] = dir;
        else free(dir);
        free(an);
    }
}

static void collect_data_rule(const data_rule *rule)
{
    wchar_t *droot = cpy_wjoin(PY_BASE, rule->dir);
    wchar_t *pat;
    WIN32_FIND_DATAW fd;
    HANDLE h;

    if (!cpy_dir_exists(droot)) { free(droot); return; }
    pat = cpy_wjoin(droot, L"*");
    h = FindFirstFileW(pat, &fd);
    free(pat);
    if (h == INVALID_HANDLE_VALUE) { free(droot); return; }
    do {
        int q;
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (fd.cFileName[0] == L'.') continue;
        for (q = 0; q < 4 && rule->items[q].prefix; q++) {
            wchar_t *src;
            char *an;
            char dest[512];
            size_t pl = wcslen(rule->items[q].prefix);
            if (_wcsnicmp(fd.cFileName, rule->items[q].prefix, pl)) continue;
            src = cpy_wjoin(droot, fd.cFileName);
            an = cpy_w_to_utf8(fd.cFileName, -1);
            _snprintf(dest, sizeof(dest) - 1, "%s%s%s",
                      rule->dest, rule->dest[0] ? "\\" : "", an);
            dest[sizeof(dest) - 1] = 0;
            DROPDIRS = rule->drop;
            collect_tree(src, dest, 0);
            DROPDIRS = NULL;
            if (rule->items[q].env && wcschr(fd.cFileName + pl, L'.') && NDATAENV < 16) {
                int seen = 0, z;
                for (z = 0; z < NDATAENV; z++)
                    if (!strcmp(DATAENV[z].name, rule->items[q].env)) { seen = 1; break; }
                if (!seen) {
                    DATAENV[NDATAENV].name = rule->items[q].env;
                    DATAENV[NDATAENV].path = cpy_adup(dest);
                    NDATAENV++;
                }
            }
            free(an);
            free(src);
            break;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    free(droot);
}

static void collect_root_dlls(const char *skip)
{
    wchar_t *pat = cpy_wjoin(PY_BASE, L"*.dll");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    free(pat);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        wchar_t *src;
        char *an;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        an = cpy_w_to_utf8(fd.cFileName, -1);
        if (skip && !_stricmp(an, skip)) { free(an); continue; }
        src = cpy_wjoin(PY_BASE, fd.cFileName);
        cpy_items_add_file(&ITEMS, an, src);
        free(src);
        free(an);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void set_cur_pkg(const char *pkg)
{
    strncpy(CUR_PKG, pkg ? pkg : "", sizeof(CUR_PKG) - 1);
    CUR_PKG[sizeof(CUR_PKG) - 1] = 0;
}

static int name_exact(const wchar_t *dir, const wchar_t *name)
{
    wchar_t *pat = cpy_wjoin(dir, name);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    int ok = 0;
    free(pat);
    if (h == INVALID_HANDLE_VALUE) return 0;
    ok = !wcscmp(fd.cFileName, name);
    FindClose(h);
    return ok;
}

static int file_exact(const wchar_t *dir, const wchar_t *stem, const wchar_t *ext)
{
    wchar_t *n = (wchar_t *)cpy_xmalloc((wcslen(stem) + wcslen(ext) + 2) * sizeof(wchar_t));
    int ok;
    wcscpy(n, stem);
    wcscat(n, ext);
    ok = name_exact(dir, n);
    free(n);
    return ok;
}

static void collect_pkg_data(const wchar_t *dir, const char *destdir)
{
    wchar_t *pat = cpy_wjoin(dir, L"*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    free(pat);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        wchar_t *full;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (has_ext(fd.cFileName, L".py") || has_ext(fd.cFileName, L".pyc") ||
            has_ext(fd.cFileName, L".pyd") || has_ext(fd.cFileName, L".dll")) continue;
        full = cpy_wjoin(dir, fd.cFileName);
        dispatch_file(full, fd.cFileName, destdir, 0);
        free(full);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static wchar_t *find_ext_file(const wchar_t *dir, const wchar_t *stem)
{
    wchar_t pat[MAX_PATH * 2];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    wchar_t *res = NULL;
    _snwprintf(pat, MAX_PATH * 2 - 1, L"%s\\%s*.pyd", dir, stem);
    pat[MAX_PATH * 2 - 1] = 0;
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    do {
        size_t sl = wcslen(stem);
        if (wcsncmp(fd.cFileName, stem, sl)) continue;
        if (fd.cFileName[sl] != L'.') continue;
        res = cpy_wjoin(dir, fd.cFileName);
        break;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return res;
}

static int dir_has_ext_module(const wchar_t *dir)
{
    wchar_t *pat = cpy_wjoin(dir, L"*.pyd");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    free(pat);
    if (h == INVALID_HANDLE_VALUE) return 0;
    FindClose(h);
    return 1;
}

static cpy_set EXTPKG, PUREPKG;

static int dir_has_ext_rec(const wchar_t *dir, int depth)
{
    wchar_t *pat;
    WIN32_FIND_DATAW fd;
    HANDLE h;
    int found = 0;
    if (dir_has_ext_module(dir)) return 1;
    if (depth <= 0) return 0;
    pat = cpy_wjoin(dir, L"*");
    h = FindFirstFileW(pat, &fd);
    free(pat);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        wchar_t *sub;
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        if (!wcscmp(fd.cFileName, L"__pycache__")) continue;
        sub = cpy_wjoin(dir, fd.cFileName);
        found = dir_has_ext_rec(sub, depth - 1);
        free(sub);
        if (found) break;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return found;
}

static int pkg_is_binary(const char *top, const wchar_t *dir)
{
    if (cpy_set_has(&EXTPKG, top)) return 1;
    if (cpy_set_has(&PUREPKG, top)) return 0;
    if (dir_has_ext_rec(dir, 3)) {
        cpy_set_add(&EXTPKG, top);
        return 1;
    }
    cpy_set_add(&PUREPKG, top);
    return 0;
}

static void collect_dir_modules(const wchar_t *dir, const char *destdir, const char *pkg)
{
    wchar_t *pat = cpy_wjoin(dir, L"*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    free(pat);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        wchar_t *full;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (!has_ext(fd.cFileName, L".py") && !has_ext(fd.cFileName, L".pyd")) continue;
        full = cpy_wjoin(dir, fd.cFileName);
        set_cur_pkg(pkg);
        dispatch_file(full, fd.cFileName, destdir, 1);
        set_cur_pkg("");
        free(full);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void collect_whole_pkg(const char *top)
{
    int r;
    if (!cpy_set_add(&DYNPKG_DONE, top)) return;
    cpy_set_add(&SEEN_MOD, top);
    for (r = 0; r < NROOTS; r++) {
        wchar_t *w = cpy_utf8_to_w(top, -1);
        wchar_t *dir = cpy_wjoin(ROOTS[r].path, w);
        free(w);
        if (cpy_dir_exists(dir)) {
            char dest[1024];
            int oldlazy = cpy_scan_get_lazy();
            _snprintf(dest, sizeof(dest) - 1, "%s\\%s", ROOTS[r].prefix, top);
            dest[sizeof(dest) - 1] = 0;
            if (oldlazy && pkg_is_binary(top, dir)) cpy_scan_set_lazy(0);
            set_cur_pkg(top);
            collect_tree(dir, dest, 1);
            set_cur_pkg("");
            cpy_scan_set_lazy(oldlazy);
            if (ROOTS[r].is_site) {
                add_distinfo(top, ROOTS[r].prefix);
                add_libs_dir(ROOTS[r].path, top, ROOTS[r].prefix);
            }
            free(dir);
            return;
        }
        free(dir);
    }
}

static void collect_dotted(const char *dotted)
{
    char parts[16][128];
    int np = 0, r;
    const char *p = dotted;

    while (*p && np < 16) {
        const char *d = strchr(p, '.');
        size_t l = d ? (size_t)(d - p) : strlen(p);
        if (l == 0 || l >= 128) return;
        memcpy(parts[np], p, l);
        parts[np][l] = 0;
        np++;
        if (!d) break;
        p = d + 1;
    }
    if (!np) return;
    if (cpy_scan_get_lazy() && !strncmp(dotted, "importlib.resources", 19) &&
        cpy_set_add(&EXTPKG, "importlib:lazyres")) {
        queue_push("importlib.resources.readers");
        queue_push("importlib.resources._adapters");
        queue_push("importlib.resources._itertools");
        queue_push("importlib.readers");
        queue_push("importlib.abc");
        queue_push("importlib.machinery");
        queue_push("importlib.util");
    }
    if (cpy_set_has(&BUILTINS, parts[0])) return;
    if (cpy_set_has(&EXCLUDES, parts[0]) && !cpy_set_has(&HIDDEN, parts[0])) return;
    if (cpy_set_has(&EXCLUDES, dotted) && !cpy_set_has(&HIDDEN, dotted)) return;

    for (r = 0; r < NROOTS; r++) {
        wchar_t *wn0 = cpy_utf8_to_w(parts[0], -1);
        wchar_t *cur = cpy_wjoin(ROOTS[r].path, wn0);
        char dest[1024];
        char acc[512];
        int level = 0;

        _snprintf(dest, sizeof(dest) - 1, "%s\\%s", ROOTS[r].prefix, parts[0]);
        dest[sizeof(dest) - 1] = 0;
        strncpy(acc, parts[0], sizeof(acc) - 1);
        acc[sizeof(acc) - 1] = 0;

        if (!cpy_dir_exists(cur) || !name_exact(ROOTS[r].path, wn0)) {
            wchar_t *fpy, *fpyd;
            free(cur);
            if (np != 1) continue;
            {
                wchar_t *stem = cpy_wjoin(ROOTS[r].path, wn0);
                size_t n = wcslen(stem);
                fpy = (wchar_t *)cpy_xmalloc((n + 8) * sizeof(wchar_t));
                wcscpy(fpy, stem);
                wcscat(fpy, L".py");
                fpyd = find_ext_file(ROOTS[r].path, wn0);
                free(stem);
            }
            if (cpy_file_exists(fpy) && file_exact(ROOTS[r].path, wn0, L".py")) {
                char d2[1024];
                _snprintf(d2, sizeof(d2) - 1, "%s\\%s.pyc", ROOTS[r].prefix, parts[0]);
                d2[sizeof(d2) - 1] = 0;
                cpy_set_add(&SEEN_MOD, dotted);
                set_cur_pkg("");
                strncpy(WHY_CTX, dotted, sizeof(WHY_CTX) - 1);
                WHY_CTX[sizeof(WHY_CTX) - 1] = 0;
                handle_py(fpy, d2, 1);
                WHY_CTX[0] = 0;
                free(fpy);
                free(fpyd);
                return;
            }
            free(fpy);
            free(wn0);
            if (fpyd) {
                char d2[1024];
                char *bn = cpy_w_to_utf8(wcsrchr(fpyd, L'\\') + 1, -1);
                _snprintf(d2, sizeof(d2) - 1, "%s\\%s", ROOTS[r].prefix, bn);
                d2[sizeof(d2) - 1] = 0;
                free(bn);
                cpy_set_add(&SEEN_MOD, dotted);
                handle_native(fpyd, d2);
                free(fpyd);
                return;
            }
            continue;
        }

        {
            int oldlazy = cpy_scan_get_lazy();
            if (oldlazy && pkg_is_binary(parts[0], cur)) cpy_scan_set_lazy(0);
        for (level = 0; level < np; level++) {
            wchar_t *init = cpy_wjoin(cur, L"__init__.py");
            char idest[1024];

            if (cpy_file_exists(init)) {
                _snprintf(idest, sizeof(idest) - 1, "%s\\__init__.pyc", dest);
                idest[sizeof(idest) - 1] = 0;
                if (!cpy_set_has(&ITEMS.seen, idest)) {
                    collect_pkg_data(cur, dest);
                    if (level == 0 && ROOTS[r].is_site) {
                        add_distinfo(parts[0], ROOTS[r].prefix);
                        add_libs_dir(ROOTS[r].path, parts[0], ROOTS[r].prefix);
                    }
                    set_cur_pkg(acc);
                    handle_py(init, idest, 1);
                    if (dir_has_ext_module(cur)) collect_dir_modules(cur, dest, acc);
                }
            }
            free(init);

            if (level + 1 >= np) break;
            {
                wchar_t *wn = cpy_utf8_to_w(parts[level + 1], -1);
                wchar_t *sub = cpy_wjoin(cur, wn);
                wchar_t *spy = (wchar_t *)cpy_xmalloc((wcslen(sub) + 8) * sizeof(wchar_t));
                wchar_t *spyd = find_ext_file(cur, wn);
                char nacc[512];
                wcscpy(spy, sub);
                wcscat(spy, L".py");
                _snprintf(nacc, sizeof(nacc) - 1, "%s.%s", acc, parts[level + 1]);
                nacc[sizeof(nacc) - 1] = 0;

                if (cpy_dir_exists(sub) && name_exact(cur, wn)) {
                    char nd[1024];
                    _snprintf(nd, sizeof(nd) - 1, "%s\\%s", dest, parts[level + 1]);
                    nd[sizeof(nd) - 1] = 0;
                    strncpy(dest, nd, sizeof(dest) - 1);
                    dest[sizeof(dest) - 1] = 0;
                    strncpy(acc, nacc, sizeof(acc) - 1);
                    acc[sizeof(acc) - 1] = 0;
                    free(cur);
                    cur = sub;
                    free(spy);
                    free(spyd);
                    free(wn);
                    continue;
                }
                if (cpy_file_exists(spy) && file_exact(cur, wn, L".py")) {
                    char d2[1024];
                    _snprintf(d2, sizeof(d2) - 1, "%s\\%s.pyc", dest, parts[level + 1]);
                    d2[sizeof(d2) - 1] = 0;
                    set_cur_pkg(acc);
                    handle_py(spy, d2, 1);
                } else if (spyd) {
                    char d2[1024];
                    char *bn = cpy_w_to_utf8(wcsrchr(spyd, L'\\') + 1, -1);
                    _snprintf(d2, sizeof(d2) - 1, "%s\\%s", dest, bn);
                    d2[sizeof(d2) - 1] = 0;
                    free(bn);
                    handle_native(spyd, d2);
                    collect_dir_modules(cur, dest, acc);
                }
                free(spy);
                free(spyd);
                free(sub);
                free(wn);
                break;
            }
        }
        cpy_scan_set_lazy(oldlazy);
        }
        free(cur);
        free(wn0);
        cpy_set_add(&SEEN_MOD, dotted);
        return;
    }
}

static void collect_module(const char *name)
{
    int r;
    wchar_t *ns_dir = NULL;
    const char *ns_prefix = NULL;

    if (PRUNE) {
        char top[256];
        top_of(name, top, sizeof(top));
        {
            int dz;
            for (dz = 0; DATA_RULES[dz].trigger; dz++)
                if (!strcmp(name, DATA_RULES[dz].trigger)) collect_data_rule(&DATA_RULES[dz]);
        }
        if (cpy_set_has(&DYNPKG, top)) { collect_whole_pkg(top); return; }
        if (cpy_set_has(&SEEN_MOD, name)) return;
        collect_dotted(name);
        cpy_set_add(&SEEN_MOD, name);
        return;
    }
    if (!cpy_set_add(&SEEN_MOD, name)) return;
    {
        int dz;
        for (dz = 0; DATA_RULES[dz].trigger; dz++)
            if (!strcmp(name, DATA_RULES[dz].trigger)) collect_data_rule(&DATA_RULES[dz]);
    }
    if (cpy_set_has(&BUILTINS, name)) return;
    if (cpy_set_has(&EXCLUDES, name) && !cpy_set_has(&HIDDEN, name)) return;
    if (VERBOSE) fprintf(stderr, "  + %-28s [%d files]\n", name, ITEMS.n);

    for (r = 0; r < NROOTS; r++) {
        wchar_t *wname = cpy_utf8_to_w(name, -1);
        wchar_t *dir = cpy_wjoin(ROOTS[r].path, wname);
        wchar_t *init = cpy_wjoin(dir, L"__init__.py");
        wchar_t *initc = cpy_wjoin(dir, L"__init__.pyc");
        wchar_t *file, *ext;
        char dest[1024];

        if (cpy_dir_exists(dir)) {
            if (cpy_file_exists(init) || cpy_file_exists(initc)) {
                _snprintf(dest, sizeof(dest) - 1, "%s\\%s", ROOTS[r].prefix, name);
                dest[sizeof(dest) - 1] = 0;
                collect_tree(dir, dest, 1);
                if (ROOTS[r].is_site) {
                    add_distinfo(name, ROOTS[r].prefix);
                    add_libs_dir(ROOTS[r].path, name, ROOTS[r].prefix);
                }
                free(wname); free(dir); free(init); free(initc);
                return;
            }
            if (!ns_dir) { ns_dir = cpy_wdup(dir); ns_prefix = ROOTS[r].prefix; }
        }
        free(init);
        free(initc);
        free(dir);

        file = cpy_wjoin(ROOTS[r].path, wname);
        {
            wchar_t *py = (wchar_t *)cpy_xmalloc((wcslen(file) + 8) * sizeof(wchar_t));
            wcscpy(py, file);
            wcscat(py, L".py");
            if (cpy_file_exists(py)) {
                _snprintf(dest, sizeof(dest) - 1, "%s\\%s.pyc", ROOTS[r].prefix, name);
                dest[sizeof(dest) - 1] = 0;
                handle_py(py, dest, 1);
                if (ROOTS[r].is_site) add_distinfo(name, ROOTS[r].prefix);
                free(py); free(file); free(wname); free(ns_dir);
                return;
            }
            wcscpy(py, file);
            wcscat(py, L".pyc");
            if (cpy_file_exists(py)) {
                _snprintf(dest, sizeof(dest) - 1, "%s\\%s.pyc", ROOTS[r].prefix, name);
                dest[sizeof(dest) - 1] = 0;
                cpy_items_add_file(&ITEMS, dest, py);
                free(py); free(file); free(wname); free(ns_dir);
                return;
            }
            free(py);
        }
        free(file);
        free(wname);

        ext = find_ext_module(ROOTS[r].path, name);
        if (ext) {
            wchar_t *base = wcsrchr(ext, L'\\');
            char *an = cpy_w_to_utf8(base ? base + 1 : ext, -1);
            _snprintf(dest, sizeof(dest) - 1, "%s\\%s", ROOTS[r].prefix, an);
            dest[sizeof(dest) - 1] = 0;
            free(an);
            handle_native(ext, dest);
            if (ROOTS[r].is_site) {
                add_distinfo(name, ROOTS[r].prefix);
                add_libs_dir(ROOTS[r].path, name, ROOTS[r].prefix);
            }
            free(ext);
            free(ns_dir);
            return;
        }
    }

    if (ns_dir) {
        char dest[1024];
        _snprintf(dest, sizeof(dest) - 1, "%s\\%s", ns_prefix, name);
        dest[sizeof(dest) - 1] = 0;
        collect_tree(ns_dir, dest, 1);
        free(ns_dir);
        return;
    }
    {
        int k, known = 0;
        for (k = 0; NON_WINDOWS[k]; k++)
            if (!strcmp(name, NON_WINDOWS[k])) { known = 1; break; }
        if (!known) cpy_set_add(&MISSING, name);
    }
}

static void build_distmap(const wchar_t *site)
{
    wchar_t *pat = cpy_wjoin(site, L"*.dist-info");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    free(pat);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        wchar_t *dir, *tl;
        unsigned char *d;
        size_t len = 0;
        char stem[256];
        char *dash;
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        dir = cpy_wjoin(site, fd.cFileName);
        tl = cpy_wjoin(dir, L"top_level.txt");
        {
            char *an = cpy_w_to_utf8(fd.cFileName, -1);
            strncpy(stem, an, sizeof(stem) - 1);
            stem[sizeof(stem) - 1] = 0;
            free(an);
        }
        dash = strchr(stem, '-');
        if (dash) *dash = 0;
        for (dash = stem; *dash; dash++)
            if (*dash == '.') *dash = '_';

        d = cpy_read_file(tl, &len);
        free(tl);
        if (NDIST + 16 > CDIST) {
            CDIST = CDIST ? CDIST * 2 : 256;
            DISTMAP = (distmap_e *)cpy_xrealloc(DISTMAP, sizeof(distmap_e) * (size_t)CDIST);
        }
        DISTMAP[NDIST].name = cpy_adup(stem);
        DISTMAP[NDIST].dir = dir;
        NDIST++;
        if (d) {
            char *line = (char *)d;
            while (*line) {
                char *e = line;
                while (*e && *e != '\n' && *e != '\r') e++;
                if (e > line) {
                    char save = *e;
                    *e = 0;
                    if (NDIST + 2 > CDIST) {
                        CDIST *= 2;
                        DISTMAP = (distmap_e *)cpy_xrealloc(DISTMAP, sizeof(distmap_e) * (size_t)CDIST);
                    }
                    DISTMAP[NDIST].name = cpy_adup(line);
                    DISTMAP[NDIST].dir = dir;
                    NDIST++;
                    *e = save;
                }
                while (*e == '\n' || *e == '\r') e++;
                line = e;
            }
            free(d);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void add_site_path(const wchar_t *site, const wchar_t *dir)
{
    size_t sl = wcslen(site);
    char *prefix;
    int i;

    if (!cpy_dir_exists(dir)) return;
    for (i = 0; i < NROOTS; i++)
        if (!_wcsicmp(ROOTS[i].path, dir)) return;
    if (NROOTS >= 30 || NPTH >= 60) return;

    if (!_wcsnicmp(dir, site, sl) && dir[sl] == L'\\') {
        char *rel = cpy_w_to_utf8(dir + sl + 1, -1);
        size_t n = strlen(rel) + 32;
        prefix = (char *)cpy_xmalloc(n);
        _snprintf(prefix, n - 1, "Lib\\site-packages\\%s", rel);
        prefix[n - 1] = 0;
        free(rel);
    } else {
        const wchar_t *base = wcsrchr(dir, L'\\');
        char *an = cpy_w_to_utf8(base ? base + 1 : dir, -1);
        size_t n = strlen(an) + 16;
        prefix = (char *)cpy_xmalloc(n);
        _snprintf(prefix, n - 1, "extra\\%s", an);
        prefix[n - 1] = 0;
        free(an);
    }
    ROOTS[NROOTS].path = cpy_wdup(dir);
    ROOTS[NROOTS].prefix = prefix;
    ROOTS[NROOTS].is_site = 1;
    NROOTS++;
    PTHLINES[NPTH++] = cpy_adup(prefix);
    if (NDLLDIRS < 64) DLLDIRS[NDLLDIRS++] = cpy_wdup(dir);
}

static void scan_pth(const wchar_t *site)
{
    wchar_t *pat = cpy_wjoin(site, L"*.pth");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    free(pat);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        wchar_t *file = cpy_wjoin(site, fd.cFileName);
        size_t len = 0;
        unsigned char *d = cpy_read_file(file, &len);
        char *line;
        free(file);
        if (!d) continue;
        line = (char *)d;
        while (*line) {
            char *e = line;
            char save;
            char *t;
            while (*e && *e != '\n') e++;
            save = *e;
            *e = 0;
            while (*line == ' ' || *line == '\t') line++;
            t = line + strlen(line);
            while (t > line && (t[-1] == '\r' || t[-1] == ' ')) t--;
            *t = 0;
            if (*line && *line != '#' && _strnicmp(line, "import", 6)) {
                wchar_t *wl = cpy_utf8_to_w(line, -1);
                if (wl[0] && wl[1] == L':') {
                    add_site_path(site, wl);
                } else {
                    wchar_t *abs = cpy_wjoin(site, wl);
                    add_site_path(site, abs);
                    free(abs);
                }
                free(wl);
            }
            *e = save;
            while (*e == '\n' || *e == '\r') e++;
            line = e;
        }
        free(d);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static wchar_t *dir_of(const wchar_t *p)
{
    wchar_t *d = cpy_wdup(p);
    wchar_t *s = wcsrchr(d, L'\\');
    if (s) *s = 0;
    return d;
}

static char *venv_key(const char *text, const char *key)
{
    const char *line = text;
    size_t klen = strlen(key);
    while (*line) {
        const char *e = line;
        const char *eq;
        while (*e && *e != '\n') e++;
        eq = line;
        while (eq < e && *eq != '=') eq++;
        if (eq < e) {
            const char *ks = line, *ke = eq;
            while (ks < ke && (*ks == ' ' || *ks == '\t')) ks++;
            while (ke > ks && (ke[-1] == ' ' || ke[-1] == '\t')) ke--;
            if ((size_t)(ke - ks) == klen && !_strnicmp(ks, key, klen)) {
                const char *vs = eq + 1, *ve = e;
                char *out;
                while (vs < ve && (*vs == ' ' || *vs == '\t')) vs++;
                while (ve > vs && (ve[-1] == ' ' || ve[-1] == '\t' || ve[-1] == '\r')) ve--;
                out = (char *)cpy_xmalloc((size_t)(ve - vs) + 1);
                memcpy(out, vs, (size_t)(ve - vs));
                out[ve - vs] = 0;
                return out;
            }
        }
        while (*e == '\n' || *e == '\r') e++;
        line = e;
    }
    return NULL;
}

static int read_venv_cfg(const wchar_t *cfg, wchar_t **home, int *syssite)
{
    size_t len = 0;
    unsigned char *d = cpy_read_file(cfg, &len);
    char *v;

    *home = NULL;
    *syssite = 0;
    if (!d) return 0;
    v = venv_key((const char *)d, "home");
    if (v) { *home = cpy_utf8_to_w(v, -1); free(v); }
    v = venv_key((const char *)d, "include-system-site-packages");
    if (v) {
        if (!_stricmp(v, "true") || !_stricmp(v, "1") || !_stricmp(v, "yes")) *syssite = 1;
        free(v);
    }
    free(d);
    return 1;
}

static void setup_target(const wchar_t *user_python)
{
    wchar_t exe[MAX_PATH * 2];
    wchar_t *dir, *venv_site = NULL;
    WIN32_FIND_DATAW fd;
    HANDLE h;
    wchar_t pat[MAX_PATH * 2];

    if (user_python) {
        if (cpy_dir_exists(user_python)) {
            wchar_t *t = cpy_wjoin(user_python, L"python.exe");
            if (!cpy_file_exists(t)) {
                wchar_t *t2 = cpy_wjoin(user_python, L"Scripts\\python.exe");
                if (cpy_file_exists(t2)) { free(t); t = t2; }
                else free(t2);
            }
            wcsncpy(exe, t, MAX_PATH * 2 - 1);
            free(t);
        } else {
            wcsncpy(exe, user_python, MAX_PATH * 2 - 1);
        }
        exe[MAX_PATH * 2 - 1] = 0;
        if (!cpy_file_exists(exe)) diew(L"no python.exe at", exe);
    } else {
        if (!SearchPathW(NULL, L"python.exe", NULL, MAX_PATH * 2, exe, NULL))
            die("cannot find python.exe on PATH (use --python)");
    }

    dir = dir_of(exe);
    {
        wchar_t *cfg = cpy_wjoin(dir, L"pyvenv.cfg");
        wchar_t *up = dir_of(dir);
        wchar_t *cfg2 = cpy_wjoin(up, L"pyvenv.cfg");
        wchar_t *home = NULL;
        if (cpy_file_exists(cfg)) {
            read_venv_cfg(cfg, &home, &VENV_SYSSITE);
            IN_VENV = 1;
            venv_site = cpy_wjoin(dir, L"Lib\\site-packages");
        } else if (cpy_file_exists(cfg2)) {
            read_venv_cfg(cfg2, &home, &VENV_SYSSITE);
            IN_VENV = 1;
            venv_site = cpy_wjoin(up, L"Lib\\site-packages");
        }
        if (home) {
            wchar_t *probe = cpy_wjoin(home, L"Lib");
            if (!cpy_dir_exists(probe)) {
                wchar_t *hup = dir_of(home);
                wchar_t *probe2 = cpy_wjoin(hup, L"Lib");
                if (cpy_dir_exists(probe2)) { free(home); home = hup; }
                else free(hup);
                free(probe2);
            }
            free(probe);
            free(dir);
            dir = home;
        }
        free(cfg); free(cfg2); free(up);
    }

    PY_BASE = dir;
    PY_LIB = cpy_wjoin(PY_BASE, L"Lib");
    PY_DLLS = cpy_wjoin(PY_BASE, L"DLLs");
    if (!cpy_dir_exists(PY_LIB)) diew(L"no stdlib at", PY_LIB);

    _snwprintf(pat, MAX_PATH * 2 - 1, L"%s\\python3*.dll", PY_BASE);
    pat[MAX_PATH * 2 - 1] = 0;
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) diew(L"no pythonXY.dll in", PY_BASE);
    do {
        if (_wcsicmp(fd.cFileName, L"python3.dll") && !wcschr(fd.cFileName, L'_')) {
            PY_DLL_PATH = cpy_wjoin(PY_BASE, fd.cFileName);
            PY_DLL_NAME = cpy_w_to_utf8(fd.cFileName, -1);
            break;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    if (!PY_DLL_PATH) diew(L"no pythonXY.dll in", PY_BASE);

    ROOTS[NROOTS].path = APP_DIR;      ROOTS[NROOTS].prefix = "app";  ROOTS[NROOTS].is_site = 0; NROOTS++;
    ROOTS[NROOTS].path = PY_LIB;       ROOTS[NROOTS].prefix = "Lib";  ROOTS[NROOTS].is_site = 0; NROOTS++;
    ROOTS[NROOTS].path = PY_DLLS;      ROOTS[NROOTS].prefix = "DLLs"; ROOTS[NROOTS].is_site = 0; NROOTS++;
    if (venv_site && cpy_dir_exists(venv_site)) {
        ROOTS[NROOTS].path = venv_site;
        ROOTS[NROOTS].prefix = "Lib\\site-packages";
        ROOTS[NROOTS].is_site = 1;
        NROOTS++;
    }
    if (!IN_VENV || VENV_SYSSITE) {
        wchar_t *sp = cpy_wjoin(PY_LIB, L"site-packages");
        if (cpy_dir_exists(sp)) {
            ROOTS[NROOTS].path = sp;
            ROOTS[NROOTS].prefix = "Lib\\site-packages";
            ROOTS[NROOTS].is_site = 1;
            NROOTS++;
        }
    }

    DLLDIRS[NDLLDIRS++] = PY_BASE;
    DLLDIRS[NDLLDIRS++] = PY_DLLS;

    PTHLINES[NPTH++] = cpy_adup("app");
    PTHLINES[NPTH++] = cpy_adup("Lib");
    PTHLINES[NPTH++] = cpy_adup("DLLs");
    PTHLINES[NPTH++] = cpy_adup("Lib\\site-packages");

    {
        int i, base = NROOTS;
        for (i = 0; i < base; i++)
            if (ROOTS[i].is_site) scan_pth(ROOTS[i].path);
        for (i = 0; i < NROOTS; i++)
            if (ROOTS[i].is_site) build_distmap(ROOTS[i].path);
    }
}

static void start_python(void)
{
    SetEnvironmentVariableW(L"PYTHONHOME", PY_BASE);
    SetEnvironmentVariableW(L"PYTHONPATH", NULL);
    SetEnvironmentVariableW(L"PYTHONNOUSERSITE", L"1");
    SetEnvironmentVariableW(L"PYTHONDONTWRITEBYTECODE", L"1");
    if (!cpy_py_load(&PY, PY_DLL_PATH)) diew(L"cannot load", PY_DLL_PATH);
    PY.Py_InitializeEx(0);
    if (!PY.Py_IsInitialized()) die("target python failed to initialize");
    if (!PY.Py_CompileStringExFlags || !PY.PyMarshal_WriteObjectToString || !PY.PyImport_GetMagicNumber)
        die("target python does not export the compile API");
    PYC_MAGIC = PY.PyImport_GetMagicNumber();
    if (PY.PyRun_SimpleString && !VERBOSE)
        PY.PyRun_SimpleString("import warnings\nwarnings.simplefilter('ignore')\n");
    {
        PyObj names = PY.PySys_GetObject("builtin_module_names");
        if (names) {
            cpy_ssize i, n = PY.PyTuple_Size(names);
            for (i = 0; i < n; i++) {
                const char *s = PY.PyUnicode_AsUTF8(PY.PyTuple_GetItem(names, i));
                if (s) cpy_set_add(&BUILTINS, s);
            }
        }
        PY.PyErr_Clear();
    }
}

static char *CFG_BLOB;
static size_t CFG_LEN, CFG_CAP;

static void cfg_add(char kind, const char *text)
{
    size_t n = strlen(text) + 2;
    if (CFG_LEN + n > CFG_CAP) {
        while (CFG_LEN + n > CFG_CAP) CFG_CAP = CFG_CAP ? CFG_CAP * 2 : 1024;
        CFG_BLOB = (char *)cpy_xrealloc(CFG_BLOB, CFG_CAP);
    }
    CFG_BLOB[CFG_LEN++] = kind;
    memcpy(CFG_BLOB + CFG_LEN, text, n - 1);
    CFG_LEN += n - 1;
}

static wchar_t *nc_cache_path(const wchar_t *csrc, const wchar_t *rt_src)
{
    unsigned char *a, *b;
    size_t na = 0, nb = 0;
    uint64_t h = 1469598103934665603ULL;
    wchar_t root[MAX_PATH * 2], leaf[96];
    wchar_t *dir, *res;

    a = cpy_read_file(csrc, &na);
    if (!a) return NULL;
    h = cpy_fnv1a(a, na, h);
    free(a);
    b = cpy_read_file(rt_src, &nb);
    if (b) { h = cpy_fnv1a(b, nb, h); free(b); }
    h = cpy_fnv1a(PY_DLL_NAME, strlen(PY_DLL_NAME), h);
    {
        wchar_t wv[8];
        if (GetEnvironmentVariableW(L"COMPYLER_UNSAFE", wv, 8))
            h = cpy_fnv1a("wrapint", 7, h);
    }
    {
        char bd[600];
        nc_backend_desc(bd, sizeof(bd));
        h = cpy_fnv1a(bd, strlen(bd), h);
    }
    {
        wchar_t self[MAX_PATH * 2];
        WIN32_FILE_ATTRIBUTE_DATA fa;
        GetModuleFileNameW(NULL, self, MAX_PATH * 2);
        if (GetFileAttributesExW(self, GetFileExInfoStandard, &fa))
            h = cpy_fnv1a(&fa.ftLastWriteTime, sizeof(fa.ftLastWriteTime), h);
    }

    if (!GetEnvironmentVariableW(L"LOCALAPPDATA", root, MAX_PATH * 2)) return NULL;
    dir = cpy_wjoin(root, L"Compyler");
    {
        wchar_t *d2 = cpy_wjoin(dir, L"nc");
        free(dir);
        dir = d2;
    }
    if (!cpy_mkdirs(dir)) { free(dir); return NULL; }
    _snwprintf(leaf, 95, L"%016llx.pyd", (unsigned long long)h);
    leaf[95] = 0;
    res = cpy_wjoin(dir, leaf);
    free(dir);
    return res;
}

static void build_config(void)
{
    int i;
    cfg_add(CPY_CFG_DLL, "DLLs");
    for (i = 0; i < NLIBDIRS; i++) cfg_add(CPY_CFG_DLL, LIBDIRS[i]);
    for (i = 0; i < NPTH; i++) cfg_add(CPY_CFG_PATH, PTHLINES[i]);
    for (i = 0; i < NDATAENV; i++) {
        char rec[640];
        _snprintf(rec, sizeof(rec) - 1, "%s=%s", DATAENV[i].name, DATAENV[i].path);
        rec[sizeof(rec) - 1] = 0;
        cfg_add(CPY_CFG_ENV, rec);
    }
}

static char *build_hook_source(int want_mp)
{
    size_t cap = 1024, len = 0;
    char *s;

    if (!NC_FUNCS && !want_mp) return NULL;
    s = (char *)cpy_xmalloc(cap);
    len += (size_t)_snprintf(s + len, cap - len, "import sys\n");
    if (NC_FUNCS)
        len += (size_t)_snprintf(s + len, cap - len,
            "import _compyler_native as _cn\n"
            "_cn.install()\n");
    if (want_mp)
        len += (size_t)_snprintf(s + len, cap - len,
            "try:\n"
            "    import multiprocessing.spawn as _ms\n"
            "    _ms.WINEXE = False\n"
            "    _ms.freeze_support()\n"
            "except Exception:\n"
            "    pass\n");
    s[len] = 0;
    return s;
}

static void add_generated(const char *dest, const char *source)
{
    PyObj code, by;
    unsigned char *pyc;
    char *bytes;
    cpy_ssize n;

    code = PY.Py_CompileStringExFlags(source, dest, CPY_FILE_INPUT, NULL, OPTIMIZE);
    if (!code) { PY.PyErr_Print(); die("internal: generated module does not compile"); }
    by = PY.PyMarshal_WriteObjectToString(code, CPY_MARSHAL_VERSION);
    bytes = PY.PyBytes_AsString(by);
    n = PY.PyBytes_Size(by);
    pyc = (unsigned char *)cpy_xmalloc((size_t)n + 16);
    *(uint32_t *)pyc = (uint32_t)PYC_MAGIC;
    *(uint32_t *)(pyc + 4) = 0;
    *(uint32_t *)(pyc + 8) = 0;
    *(uint32_t *)(pyc + 12) = (uint32_t)strlen(source);
    memcpy(pyc + 16, bytes, (size_t)n);
    cpy_items_add_blob(&ITEMS, dest, pyc, (size_t)n + 16);
    PY.Py_DecRef(by);
    PY.Py_DecRef(code);
}

static void add_data_spec(const wchar_t *spec)
{
    wchar_t *copy = cpy_wdup(spec);
    wchar_t *sep = wcsrchr(copy, L';');
    char *dest;
    if (!sep) { free(copy); die("--add-data needs SRC;DEST"); }
    *sep = 0;
    dest = cpy_w_to_utf8(sep + 1, -1);
    if (cpy_dir_exists(copy)) {
        collect_tree(copy, dest, 0);
    } else if (cpy_file_exists(copy)) {
        wchar_t *base = wcsrchr(copy, L'\\');
        char *an = cpy_w_to_utf8(base ? base + 1 : copy, -1);
        char full[1024];
        _snprintf(full, sizeof(full) - 1, "%s\\%s", dest, an);
        full[sizeof(full) - 1] = 0;
        cpy_items_add_file(&ITEMS, full, copy);
        free(an);
    } else {
        fwprintf(stderr, L"compyler: --add-data source not found: %s\n", copy);
    }
    free(dest);
    free(copy);
}

static int selftest_fail(const char *why)
{
    if (VERBOSE) fprintf(stderr, "compyler: self-check stopped at %s\n", why);
    if (PY.PyErr_Occurred()) {
        if (VERBOSE) PY.PyErr_Print();
        PY.PyErr_Clear();
    }
    return 0;
}

static int native_selftest(const wchar_t *work, PyObj orig, PyObj xform)
{
    PyObj path, wdir, g1, g2, f1, f2, args, v1, v2, m, inst, empty;
    double a, b;

    path = PY.PySys_GetObject("path");
    if (!path) return selftest_fail("sys.path");
    wdir = PY.PyUnicode_FromWideChar(work, (cpy_ssize)wcslen(work));
    if (!wdir) return selftest_fail("work dir");
    PY.PyList_Insert(path, 0, wdir);
    PY.Py_DecRef(wdir);

    args = PY.PyTuple_New(2);
    PY.PyTuple_SetItem(args, 0, PY.PyLong_FromLong(7));
    PY.PyTuple_SetItem(args, 1, PY.PyLong_FromLong(3));

    g1 = PY.PyDict_New();
    v1 = PY.PyEval_EvalCode(orig, g1, g1);
    if (!v1) return selftest_fail("eval of the plain canary");
    PY.Py_DecRef(v1);
    f1 = PY.PyDict_GetItemString(g1, "__cpy_selftest__");
    if (!f1) return selftest_fail("plain canary function missing");
    v1 = PY.PyObject_Call(f1, args, NULL);
    if (!v1) return selftest_fail("call of the plain canary");

    m = PY.PyImport_ImportModule("_compyler_native");
    if (!m) return selftest_fail("import of the built extension");
    inst = PY.PyObject_GetAttrString(m, "install");
    empty = PY.PyTuple_New(0);
    if (inst) {
        PyObj r = PY.PyObject_Call(inst, empty, NULL);
        if (!r) return selftest_fail("install()");
        PY.Py_DecRef(r);
    } else {
        return selftest_fail("install attribute missing");
    }

    g2 = PY.PyDict_New();
    v2 = PY.PyEval_EvalCode(xform, g2, g2);
    if (!v2) return selftest_fail("eval of the transformed canary");
    PY.Py_DecRef(v2);
    f2 = PY.PyDict_GetItemString(g2, "__cpy_selftest__");
    if (!f2) return selftest_fail("transformed canary function missing");
    v2 = PY.PyObject_Call(f2, args, NULL);
    if (!v2) return selftest_fail("call of the transformed canary");

    a = PY.PyFloat_AsDouble(v1);
    b = PY.PyFloat_AsDouble(v2);
    PY.PyErr_Clear();
    if (a != b) {
        fprintf(stderr, "compyler: native self-check mismatch (%.17g vs %.17g)\n", a, b);
        return 0;
    }
    return 1;
}

static int verify_toolchain(void)
{
    wchar_t tmp[MAX_PATH * 2], work[MAX_PATH * 2], self[MAX_PATH * 2];
    wchar_t *marker = NULL, *csrc, *pyd, *rt_src, *rt_dst, *sd, *base;
    wchar_t stamp[128], wdll[64];
    nc_ctx *probe;
    PyObj orig = NULL, xform = NULL;
    int rc = 0;

    GetModuleFileNameW(NULL, self, MAX_PATH * 2);
    {
        WIN32_FILE_ATTRIBUTE_DATA fa;
        unsigned long long t = 0;
        if (GetFileAttributesExW(self, GetFileExInfoStandard, &fa))
            t = ((unsigned long long)fa.ftLastWriteTime.dwHighDateTime << 32) |
                fa.ftLastWriteTime.dwLowDateTime;
        {
            char bd[600];
            unsigned long long bh;
            nc_backend_desc(bd, sizeof(bd));
            bh = cpy_fnv1a(bd, strlen(bd), 1469598103934665603ULL);
            _snwprintf(stamp, 127, L"%s-%llu-%llx", PY.dll_name, t, bh);
        }
        stamp[127] = 0;
    }
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", tmp, MAX_PATH * 2)) {
        wchar_t *dir = cpy_wjoin(tmp, L"Compyler");
        marker = cpy_wjoin(dir, L".toolchain");
        cpy_mkdirs(dir);
        free(dir);
        {
            size_t n = 0;
            unsigned char *d = cpy_read_file(marker, &n);
            if (d) {
                wchar_t *have = cpy_utf8_to_w((const char *)d, -1);
                int same = !wcscmp(have, stamp);
                free(have);
                free(d);
                if (same) { free(marker); return 1; }
            }
        }
    }

    probe = nc_open(&PY, 0);
    if (!nc_ready(probe) || !nc_add_selftest(probe, &orig, &xform)) { free(marker); return 0; }

    GetTempPathW(MAX_PATH * 2, tmp);
    _snwprintf(work, MAX_PATH * 2 - 1, L"%scompyler-probe-%lu", tmp, (unsigned long)GetCurrentProcessId());
    work[MAX_PATH * 2 - 1] = 0;
    cpy_rmtree(work);
    cpy_mkdirs(work);
    csrc = cpy_wjoin(work, L"_compyler_native.c");
    pyd = cpy_wjoin(work, L"_compyler_native.pyd");
    sd = dir_of(self);
    rt_src = cpy_wjoin(sd, L"cpyrt.h");
    rt_dst = cpy_wjoin(work, L"cpyrt.h");
    free(sd);
    { wchar_t *w = cpy_utf8_to_w(PY_DLL_NAME, -1); wcsncpy(wdll, w, 63); wdll[63] = 0; free(w); }

    if (!cpy_file_exists(rt_src)) {
        fwprintf(stderr, L"compyler: cpyrt.h is missing next to compyler.exe (looked for %s)\n", rt_src);
        fputs("compyler: copy it from src/nc/cpyrt.h, native compilation disabled\n", stderr);
        TOOLCHAIN_WHY = 1;
    } else if (!nc_write(probe, csrc)) {
        fputs("compyler: could not write the probe source, native compilation disabled\n", stderr);
        TOOLCHAIN_WHY = 1;
    } else {
        CopyFileW(rt_src, rt_dst, FALSE);
        if (!nc_build_pyd(csrc, pyd, PY_BASE, wdll, work, VERBOSE)) {
            fputs("compyler: the c compiler could not build the probe, "
                  "rerun with -v to see its output\n", stderr);
            TOOLCHAIN_WHY = 1;
        } else {
            rc = native_selftest(work, orig, xform);
        }
    }
    base = cpy_wdup(work);
    free(csrc); free(pyd); free(rt_src); free(rt_dst);
    cpy_rmtree(base);
    free(base);

    if (rc && marker) {
        char *a = cpy_w_to_utf8(stamp, -1);
        cpy_write_file(marker, a, strlen(a));
        free(a);
    }
    free(marker);
    return rc;
}

static int starts_ci(const char *s, const char *lit)
{
    size_t n = strlen(lit);
    return !_strnicmp(s, lit, n);
}

static const char *size_bucket(const char *dest, char *buf, size_t n)
{
    const char *slash = strchr(dest, '\\');
    const char *q, *e;
    size_t len;

    if (!slash) return "interpreter and root dlls";
    if (starts_ci(dest, "lib\\tcl") || starts_ci(dest, "lib\\tk")) return "tcl and tk";
    if (starts_ci(dest, "Lib\\site-packages\\")) {
        char *dot;
        q = dest + strlen("Lib\\site-packages\\");
        e = strchr(q, '\\');
        len = e ? (size_t)(e - q) : strlen(q);
        if (len >= n) len = n - 1;
        memcpy(buf, q, len);
        buf[len] = 0;
        dot = strstr(buf, ".dist-info");
        if (dot) *dot = 0;
        dot = strstr(buf, ".libs");
        if (dot) *dot = 0;
        return buf;
    }
    if (starts_ci(dest, "Lib\\")) return "python standard library";
    if (starts_ci(dest, "DLLs\\")) return "stdlib extension modules";
    if (starts_ci(dest, "app\\")) return "your code";
    len = (size_t)(slash - dest);
    if (len >= n) len = n - 1;
    memcpy(buf, dest, len);
    buf[len] = 0;
    return buf;
}

static void size_report(void)
{
    typedef struct { char name[128]; unsigned long long u, c; int files; } bucket;
    bucket *bk = (bucket *)cpy_xmalloc(sizeof(bucket) * 512);
    int nb = 0, i, j;
    unsigned long long tu = 0, tc = 0;

    for (i = 0; i < ITEMS.n; i++) {
        char tmp[128];
        const char *b = size_bucket(ITEMS.v[i].dest, tmp, sizeof(tmp));
        for (j = 0; j < nb; j++) if (!strcmp(bk[j].name, b)) break;
        if (j == nb) {
            if (nb == 512) continue;
            strncpy(bk[nb].name, b, 127);
            bk[nb].name[127] = 0;
            bk[nb].u = bk[nb].c = 0;
            bk[nb].files = 0;
            nb++;
        }
        bk[j].u += ITEMS.v[i].usize;
        bk[j].c += ITEMS.v[i].csize ? ITEMS.v[i].csize : ITEMS.v[i].usize;
        bk[j].files++;
        tu += ITEMS.v[i].usize;
        tc += ITEMS.v[i].csize ? ITEMS.v[i].csize : ITEMS.v[i].usize;
    }
    for (i = 0; i < nb; i++)
        for (j = i + 1; j < nb; j++)
            if (bk[j].c > bk[i].c) { bucket t = bk[i]; bk[i] = bk[j]; bk[j] = t; }

    fprintf(stderr, "\ncompyler: size report, %d files\n", ITEMS.n);
    fprintf(stderr, "  %-34s %10s %10s %7s %6s\n", "component", "packed", "raw", "files", "share");
    fprintf(stderr, "  %.34s %.10s %.10s %.7s %.6s\n",
            "----------------------------------", "----------", "----------",
            "-------", "------");
    for (i = 0; i < nb; i++) {
        if (bk[i].c < 4096 && i > 24) continue;
        fprintf(stderr, "  %-34s %9.2fM %9.2fM %7d %5.1f%%\n",
                bk[i].name, (double)bk[i].c / 1048576.0, (double)bk[i].u / 1048576.0,
                bk[i].files, tc ? 100.0 * (double)bk[i].c / (double)tc : 0.0);
    }
    fprintf(stderr, "  %-34s %9.2fM %9.2fM %7d\n", "total payload",
            (double)tc / 1048576.0, (double)tu / 1048576.0, ITEMS.n);
    free(bk);
}

static unsigned long long exe_size(const wchar_t *p)
{
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (!GetFileAttributesExW(p, GetFileExInfoStandard, &fa)) return 0;
    return ((unsigned long long)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
}

static int launch(const wchar_t *exe)
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD code = 1;
    size_t cap = wcslen(exe) + 8;
    wchar_t *line = (wchar_t *)cpy_xmalloc(cap * sizeof(wchar_t));

    _snwprintf(line, cap - 1, L"\"%s\"", exe);
    line[cap - 1] = 0;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessW(NULL, line, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        fwprintf(stderr, L"compyler: cannot launch %s\n", exe);
        free(line);
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    free(line);
    return (int)code;
}

static int pid_alive(unsigned long pid)
{
    HANDLE h;
    DWORD code = 0;
    if (!pid) return 1;
    if (pid == GetCurrentProcessId()) return 1;
    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!h) return 0;
    if (GetExitCodeProcess(h, &code) && code == STILL_ACTIVE) { CloseHandle(h); return 1; }
    CloseHandle(h);
    return 0;
}

static unsigned long trailing_pid(const wchar_t *name)
{
    const wchar_t *p = name + wcslen(name);
    unsigned long v = 0, m = 1;
    if (p == name) return 0;
    while (p > name && p[-1] >= L'0' && p[-1] <= L'9') {
        p--;
        v += (unsigned long)(*p - L'0') * m;
        m *= 10;
    }
    if (p == name + wcslen(name)) return 0;
    if (p == name || p[-1] != L'-') return 0;
    return v;
}

static void sweep_temp(void)
{
    wchar_t tmp[MAX_PATH * 2], *pat;
    WIN32_FIND_DATAW fd;
    HANDLE h;
    FILETIME now;
    ULARGE_INTEGER tnow;

    if (!GetTempPathW(MAX_PATH * 2, tmp)) return;
    GetSystemTimeAsFileTime(&now);
    tnow.LowPart = now.dwLowDateTime;
    tnow.HighPart = now.dwHighDateTime;

    pat = cpy_wjoin(tmp, L"compyler-*");
    h = FindFirstFileW(pat, &fd);
    free(pat);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        ULARGE_INTEGER t;
        wchar_t *full;
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        t.LowPart = fd.ftLastWriteTime.dwLowDateTime;
        t.HighPart = fd.ftLastWriteTime.dwHighDateTime;
        if (pid_alive(trailing_pid(fd.cFileName))) {
            if (tnow.QuadPart <= t.QuadPart) continue;
            if (tnow.QuadPart - t.QuadPart < 36000000000ULL) continue;
        }
        full = cpy_wjoin(tmp, fd.cFileName);
        cpy_rmtree(full);
        free(full);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void revert_native(void)
{
    int k, redone = 0;
    NATIVE = 0;
    NC_FUNCS = 0;
    for (k = 0; k < NAPPMODS; k++) {
        size_t slen = 0, plen = 0;
        unsigned char *source = cpy_read_file(APPMODS[k].src, &slen);
        unsigned char *pyc;
        if (!source) continue;
        pyc = make_pyc(APPMODS[k].src, source, slen, APPMODS[k].dest, &plen, 0);
        free(source);
        if (pyc && cpy_items_replace_blob(&ITEMS, APPMODS[k].dest, pyc, plen)) redone++;
    }
    fprintf(stderr, "compyler: native module unusable, rebuilt %d app module(s) as bytecode\n",
            redone);
}

static void usage(void)
{
    fputs(
        "compyler - compile a python program into a native windows executable\n"
        "\n"
        "usage: compyler <script.py> [options]\n"
        "\n"
        "  -o, --out PATH        output executable (default: <script>.exe)\n"
        "      --name NAME       application name used for the runtime cache\n"
        "      --onedir          emit exe plus _internal folder instead of one file\n"
        "      --windowed        gui subsystem, no console window\n"
        "      --icon FILE       embed an .ico file\n"
        "      --python PATH     target interpreter (python.exe or its folder)\n"
        "  -O, -OO               bytecode optimization level 1 or 2\n"
        "      --hidden-import M force a module into the bundle\n"
        "      --exclude M       drop a module from the bundle\n"
        "      --add-data S;D    copy an extra file or folder into the bundle\n"
        "      --compress LEVEL  none, fast or max (default max, lzms)\n"
        "      --no-compress     store the payload uncompressed\n"
        "      --upx             run upx on the loader when it is available\n"
        "      --run             launch the produced executable once it is built\n"
        "      --no-prune        bundle whole package trees (default: prune to the import graph)\n"
        "      --prune-lazy      prune, and also skip imports inside function bodies\n"
        "      --why M           print why module M was bundled (or: all)\n"
        "      --no-strip        keep headers, stubs and build artifacts, scan test dirs\n"
        "      --strip-tests     omit bundled test suites entirely\n"
        "      --no-default-excludes  keep build and test tooling in the bundle\n"
        "      --no-native       skip native code generation, ship bytecode only\n"
        "      --wrap-int        codon style 64 bit wrapping ints in native code\n"
        "      --cc NAME         native code backend: clang, cl, or a clang-cl path\n"
        "      --keep-build      keep the native build dir with the generated C\n"
        "      --size-report     print what the payload is made of\n"
        "  -j N                  worker threads (default: cpu count)\n"
        "  -v                    verbose, lists every function compiled natively\n", stderr);
    ExitProcess(2);
}

int wmain(int argc, wchar_t **argv)
{
    wchar_t *script = NULL, *out = NULL, *icon = NULL, *py_opt = NULL;
    wchar_t *datas[64];
    int ndata = 0, onedir = 0, windowed = 0, compress = 1, jobs = 0;
    int algo = CPY_ALGO_MAX, upx = 0, run_after = 0;
    char *app_name = NULL;
    char entry_dest[512];
    cpy_packopt po;
    LARGE_INTEGER t0, t1, freq;
    int i;

    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);

    cpy_set_init(&SEEN_MOD, 4096);
    cpy_set_init(&EXCLUDES, 256);
    cpy_set_init(&HIDDEN, 256);
    cpy_set_init(&NATSEEN, 1024);
    cpy_set_init(&DYNPKG, 256);
    cpy_set_init(&DYNPKG_DONE, 256);
    cpy_set_init(&BUILTINS, 256);
    cpy_set_init(&MISSING, 256);
    cpy_set_init(&EXTPKG, 128);
    cpy_set_init(&PUREPKG, 256);
    cpy_items_init(&ITEMS);

    PRUNE = 1;
    for (i = 1; i < argc; i++) {
        wchar_t *a = argv[i];
        if (a[0] != L'-') {
            if (!script) script = a;
            else usage();
        } else if ((!wcscmp(a, L"-o") || !wcscmp(a, L"--out")) && i + 1 < argc) {
            out = argv[++i];
        } else if (!wcscmp(a, L"--name") && i + 1 < argc) {
            app_name = cpy_w_to_utf8(argv[++i], -1);
        } else if (!wcscmp(a, L"--onedir")) {
            onedir = 1;
        } else if (!wcscmp(a, L"--windowed") || !wcscmp(a, L"-w")) {
            windowed = 1;
        } else if (!wcscmp(a, L"--icon") && i + 1 < argc) {
            icon = argv[++i];
        } else if (!wcscmp(a, L"--python") && i + 1 < argc) {
            py_opt = argv[++i];
        } else if (!wcscmp(a, L"-O")) {
            OPTIMIZE = 1;
        } else if (!wcscmp(a, L"-OO")) {
            OPTIMIZE = 2;
        } else if (!wcscmp(a, L"--size-report")) {
            SIZE_REPORT = 1;
        } else if (!wcscmp(a, L"--no-prune")) {
            PRUNE = 0;
        } else if (!wcscmp(a, L"--wrap-int")) {
            SetEnvironmentVariableW(L"COMPYLER_UNSAFE", L"1");
        } else if (!wcscmp(a, L"--cc") && i + 1 < argc) {
            SetEnvironmentVariableW(L"COMPYLER_CC", argv[++i]);
        } else if (!wcscmp(a, L"--why") && i + 1 < argc) {
            PRUNE_WHY = cpy_w_to_utf8(argv[++i], -1);
        } else if (!wcscmp(a, L"--prune-lazy")) {
            PRUNE = 1;
            cpy_scan_set_lazy(1);
        } else if (!wcscmp(a, L"--hidden-import") && i + 1 < argc) {
            char *m = cpy_w_to_utf8(argv[++i], -1);
            cpy_set_add(&HIDDEN, m);
        } else if (!wcscmp(a, L"--exclude") && i + 1 < argc) {
            char *m = cpy_w_to_utf8(argv[++i], -1);
            cpy_set_add(&EXCLUDES, m);
        } else if (!wcscmp(a, L"--add-data") && i + 1 < argc) {
            if (ndata < 64) datas[ndata++] = argv[++i];
        } else if (!wcscmp(a, L"--no-compress")) {
            compress = 0;
        } else if (!wcscmp(a, L"--upx")) {
            upx = 1;
        } else if (!wcscmp(a, L"--run")) {
            run_after = 1;
        } else if (!wcscmp(a, L"--compress") && i + 1 < argc) {
            const wchar_t *lv = argv[++i];
            if (!wcscmp(lv, L"none")) { compress = 0; algo = CPY_ALGO_STORE; }
            else if (!wcscmp(lv, L"fast")) { compress = 1; algo = CPY_ALGO_FAST; }
            else if (!wcscmp(lv, L"max")) { compress = 1; algo = CPY_ALGO_MAX; }
            else usage();
        } else if (!wcscmp(a, L"--no-strip")) {
            STRIP = 0;
        } else if (!wcscmp(a, L"--strip-tests")) {
            DROP_TESTS = 1;
        } else if (!wcscmp(a, L"--keep-build")) {
            KEEP_BUILD = 1;
        } else if (!wcscmp(a, L"--no-native")) {
            NATIVE = 0;
        } else if (!wcscmp(a, L"--no-default-excludes")) {
            NO_DEFAULT_EX = 1;
        } else if (!wcscmp(a, L"-j") && i + 1 < argc) {
            jobs = _wtoi(argv[++i]);
        } else if (!wcscmp(a, L"-v")) {
            VERBOSE = 1;
        } else {
            usage();
        }
    }
    if (!script) usage();

    {
        wchar_t full[MAX_PATH * 4];
        if (!GetFullPathNameW(script, MAX_PATH * 4, full, NULL)) diew(L"bad path", script);
        if (!cpy_file_exists(full)) diew(L"no such file:", full);
        script = cpy_wdup(full);
    }
    APP_DIR = dir_of(script);

    {
        wchar_t *base = wcsrchr(script, L'\\');
        wchar_t stem[256];
        wchar_t *dot;
        wcsncpy(stem, base ? base + 1 : script, 255);
        stem[255] = 0;
        dot = wcsrchr(stem, L'.');
        if (dot) *dot = 0;
        if (!app_name) app_name = cpy_w_to_utf8(stem, -1);
        if (!out) {
            wchar_t o[MAX_PATH * 4];
            _snwprintf(o, MAX_PATH * 4 - 1, L"%s\\%s.exe", APP_DIR, stem);
            o[MAX_PATH * 4 - 1] = 0;
            out = cpy_wdup(o);
        } else {
            wchar_t full[MAX_PATH * 4];
            if (GetFullPathNameW(out, MAX_PATH * 4, full, NULL)) out = cpy_wdup(full);
        }
        _snprintf(entry_dest, sizeof(entry_dest) - 1, "app\\%s.pyc", app_name);
        entry_dest[sizeof(entry_dest) - 1] = 0;
    }

    if (!NO_DEFAULT_EX)
        for (i = 0; DEFAULT_EXCLUDES[i]; i++)
            if (!cpy_set_has(&HIDDEN, DEFAULT_EXCLUDES[i])) cpy_set_add(&EXCLUDES, DEFAULT_EXCLUDES[i]);

    sweep_temp();
    setup_target(py_opt);
    start_python();

    if (NATIVE) {
        NC = nc_open(&PY, VERBOSE);
        if (!nc_ready(NC)) {
            if (VERBOSE)
                fprintf(stderr, "compyler: native compilation unavailable for python %d.%d\n",
                        PY.version / 100, PY.version % 100);
            NATIVE = 0;
        } else if (!verify_toolchain()) {
            if (!TOOLCHAIN_WHY)
                fputs("compyler: c toolchain did not reproduce interpreter semantics, "
                      "building bytecode only\n", stderr);
            else
                fputs("compyler: building bytecode only\n", stderr);
            NATIVE = 0;
        }
    }

    if (VERBOSE) {
        fwprintf(stderr, L"compyler: target %s (%d.%d)\n", PY_DLL_PATH, PY.version / 100, PY.version % 100);
        fwprintf(stderr, L"compyler: entry  %s\n", script);
    }

    handle_py(script, entry_dest, 1);
    cpy_set_add(&SEEN_MOD, app_name);

    for (i = 0; BASE_MODULES[i]; i++) queue_push(BASE_MODULES[i]);
    {
        int b;
        for (b = 0; b < HIDDEN.nb; b++) {
            cpy_snode *n;
            for (n = HIDDEN.b[b]; n; n = n->next) {
                char top[256];
                char *dot;
                strncpy(top, n->s, 255);
                top[255] = 0;
                dot = strchr(top, '.');
                if (dot) *dot = 0;
                queue_push(top);
            }
        }
    }

    while (QN) {
        char *name = QUEUE[--QN];
        collect_module(name);
        free(name);
    }
    why_report();

    for (i = 0; i < ndata; i++) add_data_spec(datas[i]);

    handle_native(PY_DLL_PATH, PY_DLL_NAME);
    {
        collect_root_dlls(PY_DLL_NAME);
    }

    {
        char pth_name[128];
        char pth_body[8192];
        char *dot;
        size_t off = 0;
        int k;
        strncpy(pth_name, PY_DLL_NAME, sizeof(pth_name) - 1);
        pth_name[sizeof(pth_name) - 1] = 0;
        dot = strrchr(pth_name, '.');
        if (dot) strcpy(dot, "._pth");
        for (k = 0; k < NPTH; k++)
            off += (size_t)_snprintf(pth_body + off, sizeof(pth_body) - 1 - off, "%s\n", PTHLINES[k]);
        pth_body[off] = 0;
        cpy_items_add_blob(&ITEMS, pth_name, (unsigned char *)cpy_adup(pth_body), off);
    }

    if (NATIVE && NC && nc_count(NC) > 0) {
        wchar_t tmp[MAX_PATH * 2], work[MAX_PATH * 2], wdll[64];
        wchar_t *csrc, *pyd, *rt_src, *rt_dst, *sd, self[MAX_PATH * 2];
        GetTempPathW(MAX_PATH * 2, tmp);
        _snwprintf(work, MAX_PATH * 2 - 1, L"%scompyler-%lu", tmp, (unsigned long)GetCurrentProcessId());
        work[MAX_PATH * 2 - 1] = 0;
        cpy_rmtree(work);
        cpy_mkdirs(work);
        csrc = cpy_wjoin(work, L"_compyler_native.c");
        pyd = cpy_wjoin(work, L"_compyler_native.pyd");
        GetModuleFileNameW(NULL, self, MAX_PATH * 2);
        sd = dir_of(self);
        rt_src = cpy_wjoin(sd, L"cpyrt.h");
        rt_dst = cpy_wjoin(work, L"cpyrt.h");
        free(sd);
        {
            wchar_t *w = cpy_utf8_to_w(PY_DLL_NAME, -1);
            wcsncpy(wdll, w, 63);
            wdll[63] = 0;
            free(w);
        }
        if (!cpy_file_exists(rt_src)) {
            fwprintf(stderr, L"compyler: cpyrt.h not found next to compyler.exe: %s\n", rt_src);
        } else if (!nc_write(NC, csrc)) {
            fputs("compyler: could not write generated source\n", stderr);
        } else {
            wchar_t *cached;
            int have = 0;
            CopyFileW(rt_src, rt_dst, FALSE);
            if (KEEP_BUILD) fwprintf(stderr, L"compyler: build dir %s\n", work);
            cached = nc_cache_path(csrc, rt_src);
            if (cached && cpy_file_exists(cached) && CopyFileW(cached, pyd, FALSE)) {
                have = 1;
                if (VERBOSE) fputs("compyler: reused cached native module\n", stderr);
            }
            if (!have && nc_build_pyd(csrc, pyd, PY_BASE, wdll, work, VERBOSE)) {
                have = 1;
                if (cached) CopyFileW(pyd, cached, FALSE);
            }
            free(cached);
            if (have) {
                cpy_items_add_file(&ITEMS, "app\\_compyler_native.pyd", pyd);
                NC_FUNCS = nc_count(NC);
                NC_WORK = cpy_wdup(work);
            } else {
                revert_native();
            }
        }
        free(csrc); free(pyd); free(rt_src); free(rt_dst);
    }

    build_config();
    {
        char *hook = build_hook_source(APP_USES_MP &&
                                       cpy_set_has(&SEEN_MOD, "multiprocessing") &&
                                       !cpy_set_has(&MISSING, "multiprocessing"));
        if (hook) {
            add_generated("app\\" CPY_HOOK_MODULE ".pyc", hook);
            free(hook);
            HAS_HOOK = 1;
        }
    }

    if (MISSING.count) {
        int b, shown = 0;
        fprintf(stderr, "compyler: %d import(s) not resolved:", MISSING.count);
        for (b = 0; b < MISSING.nb && shown < 24; b++) {
            cpy_snode *n;
            for (n = MISSING.b[b]; n && shown < 24; n = n->next) {
                fprintf(stderr, " %s", n->s);
                shown++;
            }
        }
        fputs(MISSING.count > 24 ? " ...\n" : "\n", stderr);
    }

    memset(&po, 0, sizeof(po));
    po.out = out;
    po.app_name = app_name;
    po.dll_name = PY_DLL_NAME;
    po.entry_path = entry_dest;
    po.icon = icon;
    po.onedir = onedir;
    po.windowed = windowed;
    po.compress = compress;
    po.algo = algo;
    po.upx = upx;
    po.jobs = jobs > 0 ? jobs : cpy_cpu_count() * 2;
    po.verbose = VERBOSE;
    po.has_hook = HAS_HOOK;
    po.cfg = CFG_BLOB;
    po.cfg_len = CFG_LEN;
    {
        wchar_t self[MAX_PATH * 2];
        wchar_t *sd;
        GetModuleFileNameW(NULL, self, MAX_PATH * 2);
        sd = dir_of(self);
        po.stub = cpy_wjoin(sd, L"stub.exe");
        free(sd);
        if (!cpy_file_exists(po.stub)) diew(L"stub.exe not found next to compyler.exe:", po.stub);
    }

    if (!cpy_pack(&ITEMS, &po)) die("packaging failed");
    if (SIZE_REPORT) size_report();
    if (NC_WORK && !KEEP_BUILD) cpy_rmtree(NC_WORK);

    QueryPerformanceCounter(&t1);
    {
        double ms = (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)freq.QuadPart;
        fwprintf(stderr, L"compyler: %s (%.1f MB)\n", out,
                 (double)exe_size(out) / 1048576.0);
        fprintf(stderr, "compyler: %d modules, %d files, %d compiled, %d cached, %.2fs\n",
                SEEN_MOD.count, ITEMS.n, N_COMPILED, N_REUSED, ms / 1000.0);
        if (NATIVE && NC)
            fprintf(stderr, "compyler: %d function(s) native, %d left as bytecode%s%s\n",
                    NC_FUNCS, nc_skipped(NC),
                    nc_reject_summary(NC)[0] ? ", first reason: " : "",
                    nc_reject_summary(NC));
    }
    if (run_after) return launch(out);
    return 0;
}
