#include "scan.h"
#include <string.h>

#define IS_ID0(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z') || (c) == '_' || (unsigned char)(c) >= 0x80)
#define IS_ID(c)  (IS_ID0(c) || ((c) >= '0' && (c) <= '9'))

static size_t skip_string(const char *s, size_t n, size_t i)
{
    char q = s[i];
    int triple = 0;
    if (i + 2 < n && s[i + 1] == q && s[i + 2] == q) triple = 1;
    i += triple ? 3 : 1;
    while (i < n) {
        if (s[i] == '\\') { i += (i + 2 < n && s[i + 1] == '\r' && s[i + 2] == '\n') ? 3 : 2; continue; }
        if (s[i] == q) {
            if (!triple) return i + 1;
            if (i + 2 < n && s[i + 1] == q && s[i + 2] == q) return i + 3;
        }
        if (!triple && (s[i] == '\n' || s[i] == '\r')) return i;
        i++;
    }
    return n;
}

static int is_str_prefix(const char *s, size_t n)
{
    size_t k;
    if (n > 3) return 0;
    for (k = 0; k < n; k++) {
        char c = s[k] | 0x20;
        if (c != 'r' && c != 'b' && c != 'u' && c != 'f') return 0;
    }
    return 1;
}

static size_t skip_gap(const char *s, size_t n, size_t i, int cross_lines)
{
    for (;;) {
        while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\f')) i++;
        if (i < n && s[i] == '\\' && i + 1 < n && (s[i + 1] == '\n' || s[i + 1] == '\r')) {
            i += 2;
            if (i < n && s[i] == '\n' && s[i - 1] == '\r') i++;
            continue;
        }
        if (cross_lines && i < n && (s[i] == '\n' || s[i] == '\r' || s[i] == '#')) {
            if (s[i] == '#') { while (i < n && s[i] != '\n') i++; continue; }
            i++;
            continue;
        }
        return i;
    }
}

static size_t read_dotted(const char *s, size_t n, size_t i, size_t *first_len)
{
    size_t start = i;
    if (i >= n || !IS_ID0(s[i])) { *first_len = 0; return i; }
    while (i < n && IS_ID(s[i])) i++;
    *first_len = i - start;
    for (;;) {
        size_t j = skip_gap(s, n, i, 0);
        if (j < n && s[j] == '.' && j + 1 < n && IS_ID0(s[j + 1])) {
            j++;
            while (j < n && IS_ID(s[j])) j++;
            i = j;
        } else {
            return i;
        }
    }
}

static size_t parse_import(const char *s, size_t n, size_t i, cpy_import_cb cb, void *ud)
{
    for (;;) {
        size_t first = 0, start;
        i = skip_gap(s, n, i, 0);
        if (i >= n || !IS_ID0(s[i])) return i;
        start = i;
        i = read_dotted(s, n, i, &first);
        if (cb && first) cb(ud, s + start, first, i - start, 0);
        i = skip_gap(s, n, i, 0);
        if (i + 1 < n && s[i] == 'a' && s[i + 1] == 's' && (i + 2 >= n || !IS_ID(s[i + 2]))) {
            i = skip_gap(s, n, i + 2, 0);
            while (i < n && IS_ID(s[i])) i++;
            i = skip_gap(s, n, i, 0);
        }
        if (i < n && s[i] == ',') { i++; continue; }
        return i;
    }
}

static size_t parse_from(const char *s, size_t n, size_t i, cpy_import_cb cb, void *ud)
{
    int level = 0, paren = 0;
    size_t first = 0, start;

    i = skip_gap(s, n, i, 0);
    while (i < n && s[i] == '.') { level++; i++; i = skip_gap(s, n, i, 0); }
    {
        size_t k = i;
        while (k < n && IS_ID(s[k])) k++;
        if (k - i == 6 && !memcmp(s + i, "import", 6)) {
            if (level <= 0) return i;
            if (cb) cb(ud, "", 0, 0, level);
        } else if (i < n && IS_ID0(s[i])) {
            start = i;
            i = read_dotted(s, n, i, &first);
            if (cb && first) cb(ud, s + start, first, i - start, level);
        } else if (level > 0) {
            if (cb) cb(ud, "", 0, 0, level);
        } else {
            return i;
        }
    }

    i = skip_gap(s, n, i, 0);
    if (i + 6 > n || memcmp(s + i, "import", 6) || (i + 6 < n && IS_ID(s[i + 6]))) return i;
    i += 6;

    for (;;) {
        size_t ms;
        i = skip_gap(s, n, i, paren);
        if (i < n && s[i] == '(' && !paren) { paren = 1; i++; continue; }
        if (i < n && s[i] == ')' && paren) { i++; break; }
        if (i < n && s[i] == '*') { i++; break; }
        if (i >= n || !IS_ID0(s[i])) break;
        ms = i;
        while (i < n && IS_ID(s[i])) i++;
        if (cb) cb(ud, s + ms, i - ms, i - ms, -2);
        i = skip_gap(s, n, i, paren);
        if (i + 2 <= n && s[i] == 'a' && s[i + 1] == 's' && (i + 2 >= n || !IS_ID(s[i + 2]))) {
            i = skip_gap(s, n, i + 2, paren);
            while (i < n && IS_ID(s[i])) i++;
            i = skip_gap(s, n, i, paren);
        }
        if (i < n && s[i] == ',') { i++; continue; }
        if (i < n && s[i] == ')' && paren) { i++; break; }
        break;
    }
    return i;
}

static void parse_dynamic(const char *s, size_t n, size_t i, cpy_import_cb cb, int weak, void *ud)
{
    size_t j = skip_gap(s, n, i, 1);
    size_t k, e;
    if (j >= n || s[j] != '(') return;
    j = skip_gap(s, n, j + 1, 1);
    if (j < n && IS_ID0(s[j]) && j + 1 < n && (s[j + 1] == '"' || s[j + 1] == '\'') && is_str_prefix(s + j, 1)) j++;
    if (j >= n || (s[j] != '"' && s[j] != '\'')) {
        if (cb && !weak) cb(ud, "", 0, 0, -1);
        return;
    }
    if (j + 2 < n && s[j + 1] == s[j] && s[j + 2] == s[j]) return;
    k = j + 1;
    e = k;
    while (e < n && s[e] != s[j] && s[e] != '\n' && s[e] != '\\') e++;
    if (e >= n || s[e] != s[j] || e == k) return;
    if (s[k] == '.') return;
    {
        size_t len = 0, t;
        int partial = 0;
        while (len < e - k && IS_ID(s[k + len])) len++;
        if (s[e - 1] == 0x2e) partial = 1;
        t = skip_gap(s, n, e + 1, 1);
        if (t < n && (s[t] == '+' || s[t] == '%')) partial = 1;
        if (partial) {
            if (cb) cb(ud, s + k, len, e - k, -1);
        } else if (cb && !weak && len && (len == e - k || s[k + len] == 0x2e)) {
            cb(ud, s + k, len, e - k, 0);
        }
    }
}

static int CPY_SCAN_LAZY;

void cpy_scan_set_lazy(int v)
{
    CPY_SCAN_LAZY = v;
}

int cpy_scan_get_lazy(void)
{
    return CPY_SCAN_LAZY;
}

void cpy_scan_imports(const char *s, size_t n, cpy_import_cb cb, void *ud)
{
    size_t i = 0;
    int depth = 0, stmt = 1, bol = 1, col = 0, dead = -1, was_def = 0, fun = -1;
    cpy_import_cb ecb;

    if (n >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) i = 3;

    while (i < n) {
        char c = s[i];
        if (c == '\n' || c == '\r') { i++; if (!depth) { stmt = 1; bol = 1; col = 0; } continue; }
        if (c == ' ' || c == '\t' || c == '\f') { if (bol) col += (c == '\t') ? 8 : 1; i++; continue; }
        if (bol && !depth) {
            bol = 0;
            if (dead >= 0 && col <= dead) dead = -1;
            if (fun >= 0 && col <= fun) fun = -1;
        }
        ecb = (dead >= 0 || (fun >= 0 && CPY_SCAN_LAZY)) ? NULL : cb;
        if (c == '#') { while (i < n && s[i] != '\n') i++; continue; }
        if (c == '\\' && i + 1 < n && (s[i + 1] == '\n' || s[i + 1] == '\r')) { i += 2; if (i < n && s[i - 1] == '\r' && s[i] == '\n') i++; continue; }
        if (c == '(' || c == '[' || c == '{') { depth++; i++; stmt = 0; continue; }
        if (c == ')' || c == ']' || c == '}') { if (depth) depth--; i++; stmt = 0; continue; }
        if (c == ';') { i++; stmt = 1; continue; }
        if (c == ':' && !depth) { i++; stmt = 1; continue; }
        if (c == '"' || c == '\'') { i = skip_string(s, n, i); stmt = 0; continue; }
        if (IS_ID0(c)) {
            size_t start = i, len;
            while (i < n && IS_ID(s[i])) i++;
            len = i - start;
            if (i < n && (s[i] == '"' || s[i] == '\'') && is_str_prefix(s + start, len)) {
                i = skip_string(s, n, i);
                stmt = 0;
                continue;
            }
            if (stmt && len == 2 && !memcmp(s + start, "if", 2) && !depth) {
                size_t j = skip_gap(s, n, i, 0);
                if (j + 8 <= n && !memcmp(s + j, "__name__", 8) && (j + 8 >= n || !IS_ID(s[j + 8])))
                    dead = col;
                if (j + 7 <= n && !memcmp(s + j, "typing", 6) && s[j + 6] == '.')
                    j = skip_gap(s, n, j + 7, 0);
                if (j + 13 <= n && !memcmp(s + j, "TYPE_CHECKING", 13) &&
                    (j + 13 >= n || !IS_ID(s[j + 13])))
                    dead = col;
                stmt = 0;
                continue;
            }
            if (stmt && len == 5 && !memcmp(s + start, "async", 5) && !depth) {
                continue;
            }
            if (stmt && len == 6 && !memcmp(s + start, "import", 6)) {
                i = parse_import(s, n, i, ecb, ud);
                stmt = 0;
                continue;
            }
            if (stmt && len == 4 && !memcmp(s + start, "from", 4)) {
                i = parse_from(s, n, i, ecb, ud);
                stmt = 0;
                continue;
            }
            if (stmt && len == 3 && !memcmp(s + start, "def", 3)) {
                was_def = 1;
                if (fun < 0) fun = col;
                stmt = 0;
                continue;
            }
            if (((len == 10 && !memcmp(s + start, "__import__", 10)) ||
                 (len == 13 && !memcmp(s + start, "import_module", 13))) && !was_def) {
                parse_dynamic(s, n, i, dead >= 0 ? NULL : cb,
                              fun >= 0 && CPY_SCAN_LAZY, ud);
            }
            was_def = 0;
            stmt = 0;
            continue;
        }
        i++;
        stmt = 0;
    }
}
