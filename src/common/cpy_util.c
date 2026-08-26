#include "cpy_util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void *cpy_xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) { fputs("compyler: out of memory\n", stderr); ExitProcess(3); }
    return p;
}

void *cpy_xrealloc(void *p, size_t n)
{
    void *q = realloc(p, n ? n : 1);
    if (!q) { fputs("compyler: out of memory\n", stderr); ExitProcess(3); }
    return q;
}

wchar_t *cpy_wdup(const wchar_t *s)
{
    size_t n = wcslen(s) + 1;
    wchar_t *d = (wchar_t *)cpy_xmalloc(n * sizeof(wchar_t));
    memcpy(d, s, n * sizeof(wchar_t));
    return d;
}

char *cpy_adup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *d = (char *)cpy_xmalloc(n);
    memcpy(d, s, n);
    return d;
}

wchar_t *cpy_utf8_to_w(const char *s, int len)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s, len, NULL, 0);
    wchar_t *w = (wchar_t *)cpy_xmalloc((size_t)(n + 1) * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, s, len, w, n);
    w[n] = 0;
    return w;
}

char *cpy_w_to_utf8(const wchar_t *s, int len)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, s, len, NULL, 0, NULL, NULL);
    char *a = (char *)cpy_xmalloc((size_t)n + 1);
    WideCharToMultiByte(CP_UTF8, 0, s, len, a, n, NULL, NULL);
    a[n] = 0;
    return a;
}

void cpy_wnorm(wchar_t *s)
{
    for (; *s; s++)
        if (*s == L'/') *s = L'\\';
}

wchar_t *cpy_wjoin(const wchar_t *a, const wchar_t *b)
{
    size_t la = wcslen(a), lb = wcslen(b);
    int sep = (la && a[la - 1] != L'\\' && a[la - 1] != L'/' && lb && b[0] != L'\\');
    wchar_t *r = (wchar_t *)cpy_xmalloc((la + lb + 2) * sizeof(wchar_t));
    memcpy(r, a, la * sizeof(wchar_t));
    if (sep) r[la++] = L'\\';
    memcpy(r + la, b, (lb + 1) * sizeof(wchar_t));
    cpy_wnorm(r);
    return r;
}

static wchar_t *cpy_wlong(const wchar_t *p)
{
    size_t n = wcslen(p);
    wchar_t *r;
    if (n < 4 || p[1] != L':') return cpy_wdup(p);
    r = (wchar_t *)cpy_xmalloc((n + 8) * sizeof(wchar_t));
    r[0] = L'\\'; r[1] = L'\\'; r[2] = L'?'; r[3] = L'\\';
    memcpy(r + 4, p, (n + 1) * sizeof(wchar_t));
    return r;
}

int cpy_file_exists(const wchar_t *p)
{
    wchar_t *l = cpy_wlong(p);
    DWORD a = GetFileAttributesW(l);
    free(l);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

int cpy_dir_exists(const wchar_t *p)
{
    wchar_t *l = cpy_wlong(p);
    DWORD a = GetFileAttributesW(l);
    free(l);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

int cpy_mkdirs(const wchar_t *p)
{
    wchar_t *l = cpy_wlong(p);
    size_t i, n = wcslen(l), start = 0;
    DWORD a;

    if (n > 6 && l[0] == L'\\' && l[1] == L'\\' && l[2] == L'?') start = 7;
    else if (n > 2 && l[1] == L':') start = 3;
    else start = 1;

    for (i = start; i <= n; i++) {
        if (l[i] == L'\\' || l[i] == 0) {
            wchar_t save = l[i];
            l[i] = 0;
            CreateDirectoryW(l, NULL);
            l[i] = save;
            if (!save) break;
        }
    }
    a = GetFileAttributesW(l);
    free(l);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

int cpy_mkdirs_for(const wchar_t *file)
{
    wchar_t *d = cpy_wdup(file);
    wchar_t *s = wcsrchr(d, L'\\');
    int rc = 1;
    if (s) { *s = 0; rc = cpy_mkdirs(d); }
    free(d);
    return rc;
}

unsigned char *cpy_read_file(const wchar_t *p, size_t *out_len)
{
    wchar_t *l = cpy_wlong(p);
    HANDLE h = CreateFileW(l, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    LARGE_INTEGER sz;
    unsigned char *buf;
    DWORD got;
    size_t off = 0;
    free(l);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart > 0x7FFFFFFF) { CloseHandle(h); return NULL; }
    buf = (unsigned char *)cpy_xmalloc((size_t)sz.QuadPart + 1);
    while (off < (size_t)sz.QuadPart) {
        if (!ReadFile(h, buf + off, (DWORD)((size_t)sz.QuadPart - off), &got, NULL) || !got) break;
        off += got;
    }
    CloseHandle(h);
    buf[off] = 0;
    if (out_len) *out_len = off;
    return buf;
}

int cpy_write_file(const wchar_t *p, const void *data, size_t len)
{
    wchar_t *l = cpy_wlong(p);
    HANDLE h = CreateFileW(l, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    const unsigned char *d = (const unsigned char *)data;
    size_t off = 0;
    free(l);
    if (h == INVALID_HANDLE_VALUE) return 0;
    while (off < len) {
        DWORD chunk = (DWORD)((len - off > 0x4000000) ? 0x4000000 : (len - off)), put = 0;
        if (!WriteFile(h, d + off, chunk, &put, NULL) || !put) { CloseHandle(h); return 0; }
        off += put;
    }
    CloseHandle(h);
    return 1;
}

int cpy_rmtree(const wchar_t *p)
{
    wchar_t *pat = cpy_wjoin(p, L"*");
    wchar_t *lp = cpy_wlong(pat);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(lp, &fd);
    int rc;
    free(lp);
    free(pat);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            wchar_t *child;
            if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
            child = cpy_wjoin(p, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                cpy_rmtree(child);
            } else {
                wchar_t *lc = cpy_wlong(child);
                SetFileAttributesW(lc, FILE_ATTRIBUTE_NORMAL);
                DeleteFileW(lc);
                free(lc);
            }
            free(child);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    {
        wchar_t *l = cpy_wlong(p);
        rc = RemoveDirectoryW(l) ? 1 : 0;
        free(l);
    }
    return rc;
}

uint64_t cpy_fnv1a(const void *data, size_t len, uint64_t seed)
{
    const unsigned char *p = (const unsigned char *)data;
    uint64_t h = seed ? seed : 1469598103934665603ULL;
    size_t i;
    for (i = 0; i < len; i++) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static uint32_t crc_tab[256];
static volatile LONG crc_ready = 0;

uint32_t cpy_crc32(const void *data, size_t len)
{
    const unsigned char *p = (const unsigned char *)data;
    uint32_t c = 0xFFFFFFFFu;
    size_t i;
    if (!crc_ready) {
        uint32_t n, k, v;
        for (n = 0; n < 256; n++) {
            v = n;
            for (k = 0; k < 8; k++) v = (v & 1) ? (0xEDB88320u ^ (v >> 1)) : (v >> 1);
            crc_tab[n] = v;
        }
        InterlockedExchange(&crc_ready, 1);
    }
    for (i = 0; i < len; i++) c = crc_tab[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

typedef BOOL (WINAPI *fn_create_t)(DWORD, void *, void **);
typedef BOOL (WINAPI *fn_codec_t)(void *, const void *, SIZE_T, void *, SIZE_T, SIZE_T *);
typedef BOOL (WINAPI *fn_close_t)(void *);

int cpy_codec_open(cpy_codec *c, int for_compress, int algo)
{
    memset(c, 0, sizeof(*c));
    c->lib = LoadLibraryW(L"cabinet.dll");
    if (!c->lib) return 0;
    if (for_compress) {
        fn_create_t cr = (fn_create_t)(void *)GetProcAddress(c->lib, "CreateCompressor");
        c->fn_compress = (void *)GetProcAddress(c->lib, "Compress");
        c->fn_close_c  = (void *)GetProcAddress(c->lib, "CloseCompressor");
        if (!cr || !c->fn_compress || !cr((DWORD)algo, NULL, &c->hcomp)) { c->hcomp = NULL; return 0; }
    } else {
        fn_create_t cr = (fn_create_t)(void *)GetProcAddress(c->lib, "CreateDecompressor");
        c->fn_decompress = (void *)GetProcAddress(c->lib, "Decompress");
        c->fn_close_d    = (void *)GetProcAddress(c->lib, "CloseDecompressor");
        if (!cr || !c->fn_decompress || !cr((DWORD)algo, NULL, &c->hdecomp)) { c->hdecomp = NULL; return 0; }
    }
    return 1;
}

void cpy_codec_close(cpy_codec *c)
{
    if (c->hcomp && c->fn_close_c) ((fn_close_t)c->fn_close_c)(c->hcomp);
    if (c->hdecomp && c->fn_close_d) ((fn_close_t)c->fn_close_d)(c->hdecomp);
    c->hcomp = NULL;
    c->hdecomp = NULL;
}

size_t cpy_codec_compress(cpy_codec *c, const void *src, size_t slen, void *dst, size_t dcap)
{
    SIZE_T out = 0;
    if (!c->hcomp) return 0;
    if (!((fn_codec_t)c->fn_compress)(c->hcomp, src, slen, dst, dcap, &out)) return 0;
    return (size_t)out;
}

int cpy_codec_decompress(cpy_codec *c, const void *src, size_t slen, void *dst, size_t dcap, size_t *out)
{
    SIZE_T got = 0;
    if (!c->hdecomp) return 0;
    if (!((fn_codec_t)c->fn_decompress)(c->hdecomp, src, slen, dst, dcap, &got)) return 0;
    if (out) *out = (size_t)got;
    return 1;
}

typedef struct {
    volatile LONG next;
    volatile LONG worker_seq;
    int           count;
    cpy_job_fn    fn;
    void         *ctx;
} cpy_pool;

static DWORD WINAPI cpy_worker(LPVOID arg)
{
    cpy_pool *p = (cpy_pool *)arg;
    int me = (int)InterlockedIncrement(&p->worker_seq) - 1;
    for (;;) {
        int i = (int)InterlockedIncrement(&p->next) - 1;
        if (i >= p->count) break;
        p->fn(p->ctx, i, me);
    }
    return 0;
}

int cpy_cpu_count(void)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwNumberOfProcessors > 0 ? (int)si.dwNumberOfProcessors : 1;
}

void cpy_parallel(int count, int workers, cpy_job_fn fn, void *ctx)
{
    cpy_pool pool;
    HANDLE th[64];
    int i, n = 0;
    if (count <= 0) return;
    if (workers < 1) workers = 1;
    if (workers > 64) workers = 64;
    if (workers > count) workers = count;
    pool.next = 0;
    pool.worker_seq = 0;
    pool.count = count;
    pool.fn = fn;
    pool.ctx = ctx;
    for (i = 1; i < workers; i++) {
        th[n] = CreateThread(NULL, 0, cpy_worker, &pool, 0, NULL);
        if (th[n]) n++;
    }
    cpy_worker(&pool);
    if (n) WaitForMultipleObjects((DWORD)n, th, TRUE, INFINITE);
    for (i = 0; i < n; i++) CloseHandle(th[i]);
}
