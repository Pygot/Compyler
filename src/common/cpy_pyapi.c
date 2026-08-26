#include "cpy_pyapi.h"
#include "cpy_util.h"
#include <string.h>

#define CPY_BIND_REQ(ret, name, args)                                          \
    py->name = (ret (*) args)(void *)GetProcAddress(dll, #name);               \
    if (!py->name) return 0;

#define CPY_BIND_OPT(ret, name, args)                                          \
    py->name = (ret (*) args)(void *)GetProcAddress(dll, #name);

int cpy_py_bind(cpy_py *py, HMODULE dll)
{
    wchar_t path[MAX_PATH * 2];
    DWORD n;
    CPY_PY_REQUIRED(CPY_BIND_REQ)
    CPY_PY_OPTIONAL(CPY_BIND_OPT)
    py->dll_name[0] = 0;
    py->version = 0;
    n = GetModuleFileNameW(dll, path, MAX_PATH * 2);
    if (n && n < MAX_PATH * 2) {
        wchar_t *base = wcsrchr(path, L'\\');
        base = base ? base + 1 : path;
        wcsncpy(py->dll_name, base, 63);
        py->dll_name[63] = 0;
        if (!_wcsnicmp(base, L"python", 6)) {
            int major = 0, minor = 0;
            const wchar_t *d = base + 6;
            if (*d >= L'0' && *d <= L'9') { major = *d - L'0'; d++; }
            while (*d >= L'0' && *d <= L'9') { minor = minor * 10 + (*d - L'0'); d++; }
            py->version = major * 100 + minor;
        }
    }
    return 1;
}

int cpy_py_load(cpy_py *py, const wchar_t *dll_path)
{
    HMODULE dll;
    wchar_t dir[MAX_PATH * 2];
    wchar_t *slash;
    memset(py, 0, sizeof(*py));
    wcsncpy(dir, dll_path, MAX_PATH * 2 - 1);
    dir[MAX_PATH * 2 - 1] = 0;
    slash = wcsrchr(dir, L'\\');
    if (slash) {
        *slash = 0;
        SetDllDirectoryW(dir);
    }
    dll = LoadLibraryExW(dll_path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!dll) return 0;
    return cpy_py_bind(py, dll);
}
