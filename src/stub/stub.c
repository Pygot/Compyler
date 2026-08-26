#include "../common/cpy_util.h"
#include "../common/cpy_arc.h"
#include "../common/cpy_pyapi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psapi.h>

static int TRACE_ON = -1;
static LARGE_INTEGER TRACE_T0, TRACE_FREQ;

static void trace_mark(const char *what)
{
    LARGE_INTEGER now;
    PROCESS_MEMORY_COUNTERS pmc;
    if (TRACE_ON < 0) {
        wchar_t v[8];
        TRACE_ON = GetEnvironmentVariableW(L"COMPYLER_TRACE", v, 8) ? 1 : 0;
        QueryPerformanceFrequency(&TRACE_FREQ);
        QueryPerformanceCounter(&TRACE_T0);
    }
    if (!TRACE_ON) return;
    QueryPerformanceCounter(&now);
    memset(&pmc, 0, sizeof(pmc));
    pmc.cb = sizeof(pmc);
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
    fprintf(stderr, "compyler trace: %-22s %7.1f ms  ws %6.2f MB\n", what,
            (double)(now.QuadPart - TRACE_T0.QuadPart) * 1000.0 / (double)TRACE_FREQ.QuadPart,
            (double)pmc.WorkingSetSize / 1048576.0);
}

static void fatal(const wchar_t *msg)
{
    fwprintf(stderr, L"compyler: %s\n", msg);
    if (!GetConsoleWindow())
        MessageBoxW(NULL, msg, L"Application error", MB_ICONERROR | MB_OK);
    ExitProcess(2);
}

static void fatal2(const wchar_t *msg, const wchar_t *detail)
{
    wchar_t buf[1200];
    _snwprintf(buf, 1199, L"%s\n%s", msg, detail ? detail : L"");
    buf[1199] = 0;
    fatal(buf);
}

typedef struct {
    const unsigned char *arc;
    const cpy_arc_entry *ents;
    const cpy_arc_group *groups;
    const char          *names;
    const wchar_t       *dest;
    int                  count;
    int                  algo;
    volatile LONG        failed;
    cpy_codec            codec[64];
    int                  codec_up[64];
    unsigned char       *buf[64];
    size_t               cap[64];
} unpack_ctx;

static void unpack_job(void *vctx, int index, int worker)
{
    unpack_ctx *c = (unpack_ctx *)vctx;
    const cpy_arc_group *g = &c->groups[index];
    const unsigned char *src = c->arc + g->data_off;
    const unsigned char *plain;
    unsigned q;

    if (c->failed) return;

    if (g->flags & CPY_EF_COMPRESSED) {
        size_t got = 0;
        if (!c->codec_up[worker]) {
            if (!cpy_codec_open(&c->codec[worker], 0, c->algo)) { InterlockedExchange(&c->failed, 1); return; }
            c->codec_up[worker] = 1;
        }
        if (c->cap[worker] < (size_t)g->usize) {
            c->buf[worker] = (unsigned char *)cpy_xrealloc(c->buf[worker], (size_t)g->usize + 64);
            c->cap[worker] = (size_t)g->usize + 64;
        }
        if (!cpy_codec_decompress(&c->codec[worker], src, (size_t)g->csize,
                                  c->buf[worker], c->cap[worker], &got) ||
            got != (size_t)g->usize) {
            InterlockedExchange(&c->failed, 1);
            return;
        }
        plain = c->buf[worker];
    } else {
        plain = src;
    }

    for (q = 0; q < g->count; q++) {
        const cpy_arc_entry *e = &c->ents[g->first + q];
        wchar_t *name = cpy_utf8_to_w(c->names + e->name_off, (int)e->name_len);
        wchar_t *path = cpy_wjoin(c->dest, name);
        int ok;
        free(name);
        cpy_mkdirs_for(path);
        ok = cpy_write_file(path, plain + e->goff, (size_t)e->usize);
        free(path);
        if (!ok) { InterlockedExchange(&c->failed, 1); return; }
    }
}

static void prune_stale(const wchar_t *cache_root, const char *app_name, const wchar_t *keep)
{
    wchar_t *pat = cpy_wjoin(cache_root, L"*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    wchar_t *wapp = cpy_utf8_to_w(app_name, -1);
    size_t applen = wcslen(wapp);
    free(pat);
    if (h == INVALID_HANDLE_VALUE) { free(wapp); return; }
    do {
        wchar_t *full, *doomed;
        wchar_t tag[MAX_PATH + 64];
        int stale;
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (fd.cFileName[0] == L'.') continue;
        stale = !_wcsnicmp(fd.cFileName, wapp, applen) && fd.cFileName[applen] == L'-';
        if (!stale) {
            size_t l = wcslen(fd.cFileName);
            stale = l > 5 && !_wcsicmp(fd.cFileName + l - 5, L".dead");
        }
        if (!stale) {
            FILETIME now;
            ULARGE_INTEGER tnow, t;
            GetSystemTimeAsFileTime(&now);
            tnow.LowPart = now.dwLowDateTime;
            tnow.HighPart = now.dwHighDateTime;
            t.LowPart = fd.ftLastWriteTime.dwLowDateTime;
            t.HighPart = fd.ftLastWriteTime.dwHighDateTime;
            if (tnow.QuadPart > t.QuadPart &&
                tnow.QuadPart - t.QuadPart > 25920000000000ULL)
                stale = 1;
        }
        if (!stale) continue;
        full = cpy_wjoin(cache_root, fd.cFileName);
        if (!_wcsicmp(full, keep)) { free(full); continue; }
        _snwprintf(tag, MAX_PATH + 63, L"%s.%lu.dead", fd.cFileName, (unsigned long)GetTickCount());
        tag[MAX_PATH + 63] = 0;
        doomed = cpy_wjoin(cache_root, tag);
        if (MoveFileExW(full, doomed, 0)) cpy_rmtree(doomed);
        free(doomed);
        free(full);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    free(wapp);
}

static void read_tail(const wchar_t *exe, cpy_footer *foot, cpy_arc_header *hdr)
{
    HANDLE f;
    LARGE_INTEGER sz, pos;
    DWORD got = 0;

    f = CreateFileW(exe, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) fatal(L"cannot open own image");
    if (!GetFileSizeEx(f, &sz) || sz.QuadPart < (LONGLONG)sizeof(cpy_footer)) {
        CloseHandle(f);
        fatal(L"image too small");
    }
    pos.QuadPart = sz.QuadPart - (LONGLONG)sizeof(cpy_footer);
    SetFilePointerEx(f, pos, NULL, FILE_BEGIN);
    if (!ReadFile(f, foot, (DWORD)sizeof(cpy_footer), &got, NULL) || got != sizeof(cpy_footer)) {
        CloseHandle(f);
        fatal(L"cannot read payload footer");
    }
    if (memcmp(foot->magic, CPY_FOOTER_MAGIC, 8)) {
        CloseHandle(f);
        fatal(L"no payload attached to this executable");
    }
    if (foot->arc_off + foot->arc_size > (uint64_t)sz.QuadPart) {
        CloseHandle(f);
        fatal(L"payload is truncated");
    }
    pos.QuadPart = (LONGLONG)foot->arc_off;
    SetFilePointerEx(f, pos, NULL, FILE_BEGIN);
    if (!ReadFile(f, hdr, (DWORD)sizeof(cpy_arc_header), &got, NULL) || got != sizeof(cpy_arc_header)) {
        CloseHandle(f);
        fatal(L"cannot read payload header");
    }
    CloseHandle(f);
    if (memcmp(hdr->magic, CPY_ARC_MAGIC, 8)) fatal(L"payload header is corrupt");
}

static const unsigned char *map_arc(const wchar_t *exe)
{
    HANDLE f, m;
    const unsigned char *view;
    f = CreateFileW(exe, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) fatal(L"cannot open own image");
    m = CreateFileMappingW(f, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!m) { CloseHandle(f); fatal(L"cannot map own image"); }
    view = (const unsigned char *)MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(m);
    CloseHandle(f);
    if (!view) fatal(L"cannot view own image");
    return view;
}

typedef PVOID (WINAPI *add_dll_dir_fn)(PCWSTR);
typedef BOOL (WINAPI *set_dll_dirs_fn)(DWORD);

static void apply_config(cpy_py *py, const char *cfg, size_t len, const wchar_t *root)
{
    add_dll_dir_fn addr = NULL;
    set_dll_dirs_fn setd = NULL;
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    PyObj path = NULL;
    size_t i = 0;

    if (k32) {
        addr = (add_dll_dir_fn)GetProcAddress(k32, "AddDllDirectory");
        setd = (set_dll_dirs_fn)GetProcAddress(k32, "SetDefaultDllDirectories");
    }
    if (setd) setd(0x00001000);

    path = py->PySys_GetObject("path");

    while (i < len) {
        char kind = cfg[i];
        const char *text = cfg + i + 1;
        size_t tl = strlen(text);
        i += tl + 2;
        if (kind == CPY_CFG_ENV) {
            const char *eq = strchr(text, '=');
            if (eq) {
                wchar_t *name = cpy_utf8_to_w(text, (int)(eq - text));
                wchar_t *rel = cpy_utf8_to_w(eq + 1, -1);
                wchar_t *full = cpy_wjoin(root, rel);
                SetEnvironmentVariableW(name, full);
                free(name); free(rel); free(full);
            }
            continue;
        }
        {
            wchar_t *rel = cpy_utf8_to_w(text, (int)tl);
            wchar_t *full = cpy_wjoin(root, rel);
            if (kind == CPY_CFG_DLL) {
                if (addr && cpy_dir_exists(full)) addr(full);
            } else if (kind == CPY_CFG_PATH && path) {
                if (cpy_dir_exists(full)) {
                    PyObj str = py->PyUnicode_FromWideChar(full, (cpy_ssize)wcslen(full));
                    cpy_ssize n = py->PyList_Size(path), q;
                    int seen = 0;
                    char *b = cpy_w_to_utf8(full, -1);
                    for (q = 0; q < n; q++) {
                        PyObj e = py->PyList_GetItem(path, q);
                        const char *a = py->PyUnicode_AsUTF8(e);
                        if (a && b && !_stricmp(a, b)) { seen = 1; break; }
                    }
                    free(b);
                    if (!seen) py->PyList_Append(path, str);
                    py->Py_DecRef(str);
                }
            }
            free(rel);
            free(full);
        }
    }

    SetEnvironmentVariableW(L"PYTHONHOME", NULL);
    SetEnvironmentVariableW(L"PYTHONPATH", NULL);
    SetEnvironmentVariableW(L"PYTHONDONTWRITEBYTECODE", NULL);
    SetEnvironmentVariableW(L"PYTHONNOUSERSITE", NULL);
}

int wmain(int argc, wchar_t **argv)
{
    wchar_t exe[MAX_PATH * 4];
    wchar_t *exe_dir, *root = NULL, *dll_path, *entry_path;
    const unsigned char *view = NULL, *arc = NULL;
    cpy_footer foot;
    cpy_arc_header hdrbuf;
    const cpy_arc_header *hdr = &hdrbuf;
    cpy_py py;
    PyObj main_mod, main_dict, code, res, list;
    unsigned char *pyc;
    size_t pyc_len = 0;
    int i, rc = 0, onedir;

    if (!GetModuleFileNameW(NULL, exe, MAX_PATH * 4)) fatal(L"cannot locate own image");
    exe_dir = cpy_wdup(exe);
    { wchar_t *s = wcsrchr(exe_dir, L'\\'); if (s) *s = 0; }

    trace_mark("start");
    read_tail(exe, &foot, &hdrbuf);
    trace_mark("read tail");
    onedir = (hdr->flags & CPY_HF_ONEDIR) ? 1 : 0;

    if (onedir) {
        root = cpy_wjoin(exe_dir, L"_internal");
        if (!cpy_dir_exists(root)) fatal2(L"runtime directory is missing:", root);
    } else {
        wchar_t cache_root[MAX_PATH * 2];
        wchar_t leaf[128];
        wchar_t *over = NULL;
        DWORD n = GetEnvironmentVariableW(L"COMPYLER_CACHE_DIR", cache_root, MAX_PATH * 2);
        if (!n || n >= MAX_PATH * 2) {
            n = GetEnvironmentVariableW(L"LOCALAPPDATA", cache_root, MAX_PATH * 2);
            if (!n || n >= MAX_PATH * 2) {
                n = GetTempPathW(MAX_PATH * 2, cache_root);
                if (!n) fatal(L"cannot resolve a cache location");
            }
            over = cpy_wjoin(cache_root, L"Compyler");
        } else {
            over = cpy_wdup(cache_root);
        }
        {
            wchar_t *wapp = cpy_utf8_to_w(hdr->app_name, -1);
            _snwprintf(leaf, 127, L"%s-%016llx", wapp, (unsigned long long)foot.payload_id);
            leaf[127] = 0;
            free(wapp);
        }
        root = cpy_wjoin(over, leaf);

        {
            wchar_t *ready = cpy_wjoin(root, L".ready");
            if (!cpy_file_exists(ready)) {
                wchar_t tmp[160];
                wchar_t *stage;
                unpack_ctx ctx;
                _snwprintf(tmp, 159, L"%s.%lu.tmp", leaf, (unsigned long)GetCurrentProcessId());
                tmp[159] = 0;
                stage = cpy_wjoin(over, tmp);
                cpy_rmtree(stage);
                if (!cpy_mkdirs(stage)) fatal2(L"cannot create runtime cache:", stage);

                view = map_arc(exe);
                arc = view + foot.arc_off;
                memset(&ctx, 0, sizeof(ctx));
                ctx.arc = arc;
                ctx.ents = (const cpy_arc_entry *)(arc + hdr->table_off);
                ctx.groups = (const cpy_arc_group *)(arc + hdr->group_off);
                ctx.names = (const char *)(arc + hdr->name_pool_off);
                ctx.dest = stage;
                ctx.count = (int)hdr->group_count;
                ctx.algo = (int)hdr->algo;
                {
                    int gw = (int)cpy_cpu_count();
                    if (gw > 4) gw = 4;
                    if (gw < 1) gw = 1;
                    cpy_parallel(ctx.count, gw, unpack_job, &ctx);
                }
                for (i = 0; i < 64; i++) {
                    if (ctx.codec_up[i]) cpy_codec_close(&ctx.codec[i]);
                    free(ctx.buf[i]);
                }
                if (ctx.failed) { cpy_rmtree(stage); fatal(L"payload extraction failed"); }
                {
                    char stamp[32];
                    wchar_t *sready = cpy_wjoin(stage, L".ready");
                    int k = _snprintf(stamp, 31, "%016llx\n", (unsigned long long)foot.payload_id);
                    cpy_write_file(sready, stamp, (size_t)(k > 0 ? k : 0));
                    free(sready);
                }
                if (cpy_dir_exists(root) && !cpy_file_exists(ready)) cpy_rmtree(root);
                if (!MoveFileExW(stage, root, 0)) {
                    if (cpy_file_exists(ready)) {
                        cpy_rmtree(stage);
                    } else {
                        cpy_rmtree(root);
                        if (!MoveFileExW(stage, root, 0)) {
                            free(root);
                            root = cpy_wdup(stage);
                        }
                    }
                }
                prune_stale(over, hdr->app_name, root);
                free(stage);
            }
            free(ready);
        }
        free(over);
    }

    {
        wchar_t *wdll = cpy_utf8_to_w(hdr->dll_name, -1);
        dll_path = cpy_wjoin(root, wdll);
        free(wdll);
    }
    {
        wchar_t *wentry = cpy_utf8_to_w(hdr->entry_path, -1);
        entry_path = cpy_wjoin(root, wentry);
        free(wentry);
    }

    SetEnvironmentVariableW(L"PYTHONHOME", root);
    {
        wchar_t *p1 = cpy_wjoin(root, L"app");
        wchar_t *p2 = cpy_wjoin(root, L"Lib");
        wchar_t *p3 = cpy_wjoin(root, L"DLLs");
        wchar_t *p4 = cpy_wjoin(root, L"Lib\\site-packages");
        size_t need = wcslen(p1) + wcslen(p2) + wcslen(p3) + wcslen(p4) + 8;
        wchar_t *pp = (wchar_t *)cpy_xmalloc(need * sizeof(wchar_t));
        _snwprintf(pp, need - 1, L"%s;%s;%s;%s", p1, p2, p3, p4);
        pp[need - 1] = 0;
        SetEnvironmentVariableW(L"PYTHONPATH", pp);
        free(pp); free(p1); free(p2); free(p3); free(p4);
    }
    SetEnvironmentVariableW(L"PYTHONDONTWRITEBYTECODE", L"1");
    SetEnvironmentVariableW(L"PYTHONNOUSERSITE", L"1");

    trace_mark("cache ready");
    if (!cpy_py_load(&py, dll_path)) fatal2(L"cannot load the python runtime:", dll_path);
    trace_mark("python dll loaded");

    py.Py_InitializeEx(1);
    trace_mark("interpreter init");
    if (!py.Py_IsInitialized()) fatal(L"python runtime failed to initialize");

    list = py.PyList_New(0);
    {
        PyObj s = py.PyUnicode_FromWideChar(exe, (cpy_ssize)wcslen(exe));
        py.PyList_Append(list, s);
        py.Py_DecRef(s);
    }
    for (i = 1; i < argc; i++) {
        PyObj s = py.PyUnicode_FromWideChar(argv[i], (cpy_ssize)wcslen(argv[i]));
        py.PyList_Append(list, s);
        py.Py_DecRef(s);
    }
    py.PySys_SetObject("argv", list);
    py.Py_DecRef(list);

    {
        PyObj t = py.PyBool_FromLong(1);
        PyObj r = py.PyUnicode_FromWideChar(root, (cpy_ssize)wcslen(root));
        PyObj sys = py.PyImport_ImportModule("sys");
        py.PyObject_SetAttrString(sys, "frozen", t);
        py.PyObject_SetAttrString(sys, "_MEIPASS", r);
        py.PyObject_SetAttrString(sys, "_COMPYLER_ROOT", r);
        py.Py_DecRef(t);
        py.Py_DecRef(r);
        py.Py_DecRef(sys);
    }

    {
        const unsigned char *cfgview = NULL;
        if (hdr->cfg_len) {
            cfgview = view ? view : map_arc(exe);
            apply_config(&py, (const char *)(cfgview + foot.arc_off + hdr->cfg_off),
                         (size_t)hdr->cfg_len, root);
        } else {
            apply_config(&py, NULL, 0, root);
        }
        if (cfgview && cfgview != view) UnmapViewOfFile((LPCVOID)cfgview);
    }
    trace_mark("sys configured");
    if (hdr->flags & CPY_HF_HAS_HOOK) {
        PyObj hook = py.PyImport_ImportModule(CPY_HOOK_MODULE);
        if (!hook) py.PyErr_Print();
        else py.Py_DecRef(hook);
    }

    trace_mark("hook imported");
    pyc = cpy_read_file(entry_path, &pyc_len);
    if (!pyc || pyc_len <= 16) fatal2(L"entry module is missing:", entry_path);

    code = py.PyMarshal_ReadObjectFromString((const char *)pyc + 16, (cpy_ssize)(pyc_len - 16));
    if (!code) { py.PyErr_Print(); fatal(L"entry module is corrupt"); }

    main_mod = py.PyImport_AddModule("__main__");
    main_dict = py.PyModule_GetDict(main_mod);
    {
        PyObj f = py.PyUnicode_FromWideChar(entry_path, (cpy_ssize)wcslen(entry_path));
        PyObj n = py.PyUnicode_FromString("__main__");
        py.PyDict_SetItemString(main_dict, "__file__", f);
        py.PyDict_SetItemString(main_dict, "__name__", n);
        py.Py_DecRef(f);
        py.Py_DecRef(n);
    }

    trace_mark("about to run");
    res = py.PyEval_EvalCode(code, main_dict, main_dict);
    trace_mark("app finished");
    if (!res) {
        py.PyErr_Print();
        rc = 1;
    } else {
        py.Py_DecRef(res);
    }
    py.Py_DecRef(code);

    py.Py_FinalizeEx();
    return rc;
}
