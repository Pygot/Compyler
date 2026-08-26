#include "nc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static wchar_t *find_vcvars(void)
{
    static wchar_t found[MAX_PATH * 2];
    static int done = 0;
    WIN32_FIND_DATAW fd;

    if (done) return found[0] ? found : NULL;
    done = 1;
    found[0] = 0;

    {
        const wchar_t *roots[2];
        wchar_t a[MAX_PATH * 2], b[MAX_PATH * 2];
        int r;
        if (!GetEnvironmentVariableW(L"ProgramFiles", a, MAX_PATH * 2)) a[0] = 0;
        if (!GetEnvironmentVariableW(L"ProgramFiles(x86)", b, MAX_PATH * 2)) b[0] = 0;
        roots[0] = a;
        roots[1] = b;
        for (r = 0; r < 2; r++) {
            wchar_t pat[MAX_PATH * 2];
            HANDLE h;
            if (!roots[r][0]) continue;
            _snwprintf(pat, MAX_PATH * 2 - 1, L"%s\\Microsoft Visual Studio\\*", roots[r]);
            pat[MAX_PATH * 2 - 1] = 0;
            h = FindFirstFileW(pat, &fd);
            if (h == INVALID_HANDLE_VALUE) continue;
            do {
                wchar_t pat2[MAX_PATH * 2];
                WIN32_FIND_DATAW fd2;
                HANDLE h2;
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
                if (fd.cFileName[0] == L'.') continue;
                _snwprintf(pat2, MAX_PATH * 2 - 1, L"%s\\Microsoft Visual Studio\\%s\\*",
                           roots[r], fd.cFileName);
                pat2[MAX_PATH * 2 - 1] = 0;
                h2 = FindFirstFileW(pat2, &fd2);
                if (h2 == INVALID_HANDLE_VALUE) continue;
                do {
                    wchar_t cand[MAX_PATH * 2];
                    if (!(fd2.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
                    if (fd2.cFileName[0] == L'.') continue;
                    _snwprintf(cand, MAX_PATH * 2 - 1,
                               L"%s\\Microsoft Visual Studio\\%s\\%s\\VC\\Auxiliary\\Build\\vcvars64.bat",
                               roots[r], fd.cFileName, fd2.cFileName);
                    cand[MAX_PATH * 2 - 1] = 0;
                    if (cpy_file_exists(cand)) wcscpy(found, cand);
                } while (FindNextFileW(h2, &fd2));
                FindClose(h2);
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
    }
    return found[0] ? found : NULL;
}

static wchar_t *find_clang(void)
{
    static wchar_t found[MAX_PATH * 2];
    static int done = 0;
    wchar_t v[MAX_PATH * 2];

    if (done) return found[0] ? found : NULL;
    done = 1;
    found[0] = 0;

    if (GetEnvironmentVariableW(L"COMPYLER_CC", v, MAX_PATH * 2)) {
        if (!wcscmp(v, L"cl")) return NULL;
        if (wcscmp(v, L"clang")) {
            if (cpy_file_exists(v)) { wcscpy(found, v); return found; }
            return NULL;
        }
    }
    {
        wchar_t pf[MAX_PATH * 2];
        if (GetEnvironmentVariableW(L"ProgramFiles", pf, MAX_PATH * 2)) {
            _snwprintf(v, MAX_PATH * 2 - 1, L"%s\\LLVM\\bin\\clang-cl.exe", pf);
            v[MAX_PATH * 2 - 1] = 0;
            if (cpy_file_exists(v)) { wcscpy(found, v); return found; }
        }
    }
    {
        wchar_t *vc = find_vcvars();
        if (vc) {
            wchar_t *p;
            wcscpy(v, vc);
            p = wcsstr(v, L"\\Auxiliary\\Build\\vcvars64.bat");
            if (p) {
                wcscpy(p, L"\\Tools\\Llvm\\x64\\bin\\clang-cl.exe");
                if (cpy_file_exists(v)) { wcscpy(found, v); return found; }
            }
        }
    }
    if (SearchPathW(NULL, L"clang-cl.exe", NULL, MAX_PATH * 2, v, NULL) &&
        cpy_file_exists(v)) {
        wcscpy(found, v);
        return found;
    }
    {
        const wchar_t *cands[2] = { L"C:\\msys64\\mingw64\\bin\\clang-cl.exe",
                                    L"C:\\msys64\\clang64\\bin\\clang-cl.exe" };
        int i;
        for (i = 0; i < 2; i++)
            if (cpy_file_exists(cands[i])) { wcscpy(found, cands[i]); return found; }
    }
    return NULL;
}

int nc_backend_desc(char *out, size_t cap)
{
    wchar_t *c = find_clang();
    if (!c) {
        strncpy(out, "cl", cap - 1);
        out[cap - 1] = 0;
        return 0;
    }
    {
        char *u = cpy_w_to_utf8(c, -1);
        WIN32_FILE_ATTRIBUTE_DATA fa;
        unsigned long long ts = 0;
        if (GetFileAttributesExW(c, GetFileExInfoStandard, &fa))
            ts = ((unsigned long long)fa.ftLastWriteTime.dwHighDateTime << 32) |
                 fa.ftLastWriteTime.dwLowDateTime;
        _snprintf(out, cap - 1, "clang:%s:%llx", u, ts);
        out[cap - 1] = 0;
        free(u);
    }
    return 1;
}

static int run(const wchar_t *cmd, const wchar_t *logfile, const wchar_t *cwd)
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    SECURITY_ATTRIBUTES sa;
    HANDLE log;
    DWORD code = 1;
    wchar_t *line = cpy_wdup(cmd);

    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    log = CreateFileW(logfile, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS,
                      FILE_ATTRIBUTE_NORMAL, NULL);

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = log;
    si.hStdError = log;
    si.hStdInput = NULL;
    memset(&pi, 0, sizeof(pi));

    if (CreateProcessW(NULL, line, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, cwd, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
    free(line);
    return code == 0;
}

int nc_build_pyd(const wchar_t *csrc, const wchar_t *outpyd, const wchar_t *py_base,
                 const wchar_t *dll_name, const wchar_t *work, int verbose)
{
    wchar_t *vc = find_vcvars();
    wchar_t *bat = cpy_wjoin(work, L"build.bat");
    wchar_t *log = cpy_wjoin(work, L"build.log");
    wchar_t *inc = cpy_wjoin(py_base, L"include");
    wchar_t *libs = cpy_wjoin(py_base, L"libs");
    wchar_t libname[64];
    wchar_t *cmd;
    char *script;
    size_t cap = 8192;
    int rc;

    {
        size_t n = wcslen(dll_name);
        if (n > 63) n = 63;
        wcsncpy(libname, dll_name, 63);
        libname[63] = 0;
        if (n > 4) wcscpy(libname + n - 4, L".lib");
    }

    if (!vc) {
        fputs("compyler: no visual studio toolchain found, native compilation disabled\n", stderr);
        return 0;
    }

    {
        wchar_t *clang = find_clang();
        char *a_vc = cpy_w_to_utf8(vc, -1);
        char *a_src = cpy_w_to_utf8(csrc, -1);
        char *a_out = cpy_w_to_utf8(outpyd, -1);
        char *a_inc = cpy_w_to_utf8(inc, -1);
        char *a_libs = cpy_w_to_utf8(libs, -1);
        char *a_lib = cpy_w_to_utf8(libname, -1);
        char *a_work = cpy_w_to_utf8(work, -1);
        char ccline[2200];
        int attempt;

        script = (char *)cpy_xmalloc(cap);
        rc = 0;
        for (attempt = 0; attempt < 2; attempt++) {
            if (clang && attempt == 0) {
                char *a_cc = cpy_w_to_utf8(clang, -1);
                _snprintf(ccline, sizeof(ccline) - 1,
                          "\"%s\" /nologo /c /O2 /Oi /Ot /GS- /fp:precise /MD /DNDEBUG /W3 /clang:-O3"
                          " /clang:-falign-functions=32 /clang:-falign-loops=32"
                          " /clang:-mllvm /clang:-inline-threshold=1000",
                          a_cc);
                ccline[sizeof(ccline) - 1] = 0;
                free(a_cc);
            } else {
                strcpy(ccline,
                       "cl /nologo /c /O2 /Ob3 /Oi /Ot /GS- /fp:precise /d2SSAOptimizer- /MD /DNDEBUG /W3 /we4013 /we4047 /we4020");
                if (!clang) attempt = 1;
            }
            if (verbose)
                fprintf(stderr, "compyler: cc backend %s\n",
                        clang && attempt == 0 ? "clang-cl" : "cl");
            _snprintf(script, cap - 1,
                      "@echo off\r\n"
                      "call \"%s\" >nul\r\n"
                      "%s "
                      "/I\"%s\" /I\"%s\" /Fo\"%s\\gen.obj\" \"%s\"\r\n"
                      "if errorlevel 1 exit /b 1\r\n"
                      "link /nologo /DLL /INCREMENTAL:NO /OPT:REF /OPT:ICF "
                      "/LIBPATH:\"%s\" \"%s\" \"%s\\gen.obj\" /OUT:\"%s\"\r\n",
                      a_vc, ccline, a_inc, a_work, a_work, a_src, a_libs, a_lib, a_work, a_out);
            script[cap - 1] = 0;
            cpy_write_file(bat, script, strlen(script));

            {
                size_t n = wcslen(bat) + 64;
                cmd = (wchar_t *)cpy_xmalloc(n * sizeof(wchar_t));
                _snwprintf(cmd, n - 1, L"cmd.exe /c \"\"%s\"\"", bat);
                cmd[n - 1] = 0;
            }
            rc = run(cmd, log, work);
            free(cmd);
            if (rc && cpy_file_exists(outpyd)) break;
            rc = 0;
            if (clang && attempt == 0) {
                if (verbose)
                    fputs("compyler: clang-cl failed, retrying with cl\n", stderr);
                DeleteFileW(outpyd);
                continue;
            }
            {
                size_t len = 0;
                unsigned char *d = cpy_read_file(log, &len);
                fputs("compyler: native compilation failed\n", stderr);
                if (d && verbose) fwrite(d, 1, len > 4000 ? 4000 : len, stderr);
                free(d);
            }
            break;
        }
        free(script);
        free(a_vc); free(a_src); free(a_out); free(a_inc); free(a_libs); free(a_lib); free(a_work);
    }
    free(bat); free(log); free(inc); free(libs);
    return rc;
}
