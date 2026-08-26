#include "comp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned hashs(const char *s)
{
    unsigned h = 2166136261u;
    while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
    return h;
}

void cpy_set_init(cpy_set *s, int nb)
{
    s->nb = nb;
    s->count = 0;
    s->b = (cpy_snode **)cpy_xmalloc(sizeof(cpy_snode *) * (size_t)nb);
    memset(s->b, 0, sizeof(cpy_snode *) * (size_t)nb);
}

int cpy_set_has(cpy_set *s, const char *k)
{
    cpy_snode *n;
    if (!s->b) return 0;
    for (n = s->b[hashs(k) % (unsigned)s->nb]; n; n = n->next)
        if (!strcmp(n->s, k)) return 1;
    return 0;
}

int cpy_set_add(cpy_set *s, const char *k)
{
    unsigned i;
    cpy_snode *n;
    if (!s->b) cpy_set_init(s, 1024);
    i = hashs(k) % (unsigned)s->nb;
    for (n = s->b[i]; n; n = n->next)
        if (!strcmp(n->s, k)) return 0;
    n = (cpy_snode *)cpy_xmalloc(sizeof(cpy_snode) + strlen(k));
    strcpy(n->s, k);
    n->next = s->b[i];
    s->b[i] = n;
    s->count++;
    return 1;
}

void cpy_items_init(cpy_itemlist *l)
{
    l->n = 0;
    l->cap = 512;
    l->v = (cpy_item *)cpy_xmalloc(sizeof(cpy_item) * (size_t)l->cap);
    cpy_set_init(&l->seen, 8192);
}

static cpy_item *item_new(cpy_itemlist *l, const char *dest)
{
    cpy_item *it;
    if (!cpy_set_add(&l->seen, dest)) return NULL;
    if (l->n == l->cap) {
        l->cap *= 2;
        l->v = (cpy_item *)cpy_xrealloc(l->v, sizeof(cpy_item) * (size_t)l->cap);
    }
    it = &l->v[l->n++];
    memset(it, 0, sizeof(*it));
    it->dest = cpy_adup(dest);
    return it;
}

int cpy_items_add_blob(cpy_itemlist *l, const char *dest, unsigned char *data, size_t len)
{
    cpy_item *it = item_new(l, dest);
    if (!it) return 0;
    it->blob = data;
    it->blob_len = len;
    return 1;
}

int cpy_items_add_file(cpy_itemlist *l, const char *dest, const wchar_t *src)
{
    cpy_item *it = item_new(l, dest);
    if (!it) return 0;
    it->src = cpy_wdup(src);
    return 1;
}

int cpy_items_replace_blob(cpy_itemlist *l, const char *dest, unsigned char *data, size_t len)
{
    int i;
    for (i = 0; i < l->n; i++) {
        if (strcmp(l->v[i].dest, dest)) continue;
        free(l->v[i].blob);
        free(l->v[i].src);
        l->v[i].src = NULL;
        l->v[i].blob = data;
        l->v[i].blob_len = len;
        return 1;
    }
    return 0;
}

typedef struct {
    cpy_itemlist *l;
    volatile LONG failed;
} pack_ctx;

static void pack_job(void *vctx, int index, int worker)
{
    pack_ctx *c = (pack_ctx *)vctx;
    cpy_item *it = &c->l->v[index];
    unsigned char *raw;
    size_t len;

    (void)worker;
    if (it->blob) {
        raw = it->blob;
        len = it->blob_len;
    } else {
        raw = cpy_read_file(it->src, &len);
        if (!raw) { InterlockedExchange(&c->failed, 1); return; }
    }
    it->usize = len;
    it->crc = cpy_crc32(raw, len);
    it->cbuf = raw;
    it->csize = len;
    it->blob = NULL;
}

typedef struct {
    cpy_itemlist   *l;
    const int      *order;
    cpy_arc_group  *g;
    unsigned char **out;
    int             algo;
    int             compress;
    cpy_codec       codec[64];
    int             up[64];
} group_ctx;

static void group_job(void *vctx, int index, int worker)
{
    group_ctx *c = (group_ctx *)vctx;
    cpy_arc_group *g = &c->g[index];
    unsigned char *blob;
    size_t off = 0, cap, got;
    unsigned q;

    blob = (unsigned char *)cpy_xmalloc(g->usize ? (size_t)g->usize : 1);
    for (q = 0; q < g->count; q++) {
        cpy_item *it = &c->l->v[c->order[g->first + q]];
        memcpy(blob + off, it->cbuf, it->usize);
        off += it->usize;
    }

    g->csize = g->usize;
    g->flags = 0;
    c->out[index] = blob;
    if (!c->compress || g->usize <= 96) return;

    if (!c->up[worker]) {
        if (cpy_codec_open(&c->codec[worker], 1, c->algo)) c->up[worker] = 1;
        else c->up[worker] = -1;
    }
    if (c->up[worker] != 1) return;

    cap = (size_t)g->usize + (size_t)g->usize / 8 + 1024;
    {
        unsigned char *cb = (unsigned char *)cpy_xmalloc(cap);
        got = cpy_codec_compress(&c->codec[worker], blob, (size_t)g->usize, cb, cap);
        if (got && got < (size_t)g->usize) {
            free(blob);
            c->out[index] = cb;
            g->csize = got;
            g->flags = CPY_EF_COMPRESSED;
        } else {
            free(cb);
        }
    }
}

static const char *item_ext(const char *d)
{
    const char *dot = strrchr(d, '.');
    const char *sl = strrchr(d, '\\');
    if (!dot || (sl && dot < sl)) return "";
    return dot;
}

static cpy_itemlist *SORT_L;

static int order_cmp(const void *a, const void *b)
{
    const cpy_item *x = &SORT_L->v[*(const int *)a];
    const cpy_item *y = &SORT_L->v[*(const int *)b];
    int r = _stricmp(item_ext(x->dest), item_ext(y->dest));
    if (r) return r;
    return _stricmp(x->dest, y->dest);
}

#pragma pack(push, 2)
typedef struct { WORD res; WORD type; WORD count; } ico_dir;
typedef struct { BYTE w, h, cc, r; WORD planes, bits; DWORD bytes; DWORD off; } ico_ent;
typedef struct { BYTE w, h, cc, r; WORD planes, bits; DWORD bytes; WORD id; } grp_ent;
#pragma pack(pop)

static int set_icon(const wchar_t *exe, const wchar_t *ico)
{
    size_t len = 0;
    unsigned char *d = cpy_read_file(ico, &len);
    ico_dir *dir;
    ico_ent *ents;
    unsigned char *grp;
    HANDLE up;
    int i, n, ok = 1;

    if (!d || len < sizeof(ico_dir)) { free(d); return 0; }
    dir = (ico_dir *)d;
    if (dir->type != 1 || !dir->count) { free(d); return 0; }
    n = dir->count;
    if (len < sizeof(ico_dir) + (size_t)n * sizeof(ico_ent)) { free(d); return 0; }
    ents = (ico_ent *)(d + sizeof(ico_dir));

    grp = (unsigned char *)cpy_xmalloc(sizeof(ico_dir) + (size_t)n * sizeof(grp_ent));
    memcpy(grp, d, sizeof(ico_dir));
    for (i = 0; i < n; i++) {
        grp_ent *g = (grp_ent *)(grp + sizeof(ico_dir) + (size_t)i * sizeof(grp_ent));
        g->w = ents[i].w; g->h = ents[i].h; g->cc = ents[i].cc; g->r = ents[i].r;
        g->planes = ents[i].planes; g->bits = ents[i].bits; g->bytes = ents[i].bytes;
        g->id = (WORD)(i + 1);
    }

    up = BeginUpdateResourceW(exe, FALSE);
    if (!up) { free(d); free(grp); return 0; }
    for (i = 0; i < n; i++) {
        if ((size_t)ents[i].off + ents[i].bytes > len) { ok = 0; break; }
        if (!UpdateResourceW(up, (LPCWSTR)RT_ICON, MAKEINTRESOURCEW(i + 1), 0,
                             d + ents[i].off, ents[i].bytes)) { ok = 0; break; }
    }
    if (ok)
        ok = UpdateResourceW(up, (LPCWSTR)RT_GROUP_ICON, MAKEINTRESOURCEW(1), 0,
                             grp, (DWORD)(sizeof(ico_dir) + (size_t)n * sizeof(grp_ent))) ? 1 : 0;
    EndUpdateResourceW(up, ok ? FALSE : TRUE);
    free(d);
    free(grp);
    return ok;
}

static void patch_subsystem(unsigned char *d, size_t len, int gui)
{
    uint32_t pe;
    uint16_t magic;
    if (len < 0x40 || d[0] != 'M' || d[1] != 'Z') return;
    pe = (uint32_t)d[0x3C] | ((uint32_t)d[0x3D] << 8) | ((uint32_t)d[0x3E] << 16) | ((uint32_t)d[0x3F] << 24);
    if ((size_t)pe + 0x80 > len || memcmp(d + pe, "PE\0\0", 4)) return;
    magic = (uint16_t)(d[pe + 24] | (d[pe + 25] << 8));
    if (magic != 0x20b) return;
    d[pe + 24 + 68] = (unsigned char)(gui ? 2 : 3);
    d[pe + 24 + 69] = 0;
}

static wchar_t *find_upx(void)
{
    static wchar_t found[MAX_PATH * 2];
    wchar_t self[MAX_PATH * 2];
    wchar_t *sd, *cand;

    if (SearchPathW(NULL, L"upx.exe", NULL, MAX_PATH * 2, found, NULL)) return found;
    if (!GetModuleFileNameW(NULL, self, MAX_PATH * 2)) return NULL;
    sd = cpy_wdup(self);
    { wchar_t *s = wcsrchr(sd, L'\\'); if (s) *s = 0; }

    {
        int lvl;
        wchar_t *base = cpy_wdup(sd);
        for (lvl = 0; lvl < 3; lvl++) {
            wchar_t *up;
            cand = cpy_wjoin(base, L"upx.exe");
            if (cpy_file_exists(cand)) {
                wcsncpy(found, cand, MAX_PATH * 2 - 1);
                found[MAX_PATH * 2 - 1] = 0;
                free(cand);
                free(base);
                free(sd);
                return found;
            }
            free(cand);
            {
                wchar_t *pat = cpy_wjoin(base, L"upx*");
                WIN32_FIND_DATAW fd;
                HANDLE h = FindFirstFileW(pat, &fd);
                free(pat);
                if (h != INVALID_HANDLE_VALUE) {
                    do {
                        wchar_t *dir, *exe;
                        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
                        if (fd.cFileName[0] == L'.') continue;
                        dir = cpy_wjoin(base, fd.cFileName);
                        exe = cpy_wjoin(dir, L"upx.exe");
                        free(dir);
                        if (cpy_file_exists(exe)) {
                            wcsncpy(found, exe, MAX_PATH * 2 - 1);
                            found[MAX_PATH * 2 - 1] = 0;
                            free(exe);
                            FindClose(h);
                            free(base);
                            free(sd);
                            return found;
                        }
                        free(exe);
                    } while (FindNextFileW(h, &fd));
                    FindClose(h);
                }
            }
            up = cpy_wdup(base);
            { wchar_t *s = wcsrchr(up, L'\\'); if (!s) { free(up); break; } *s = 0; }
            free(base);
            base = up;
        }
        free(base);
    }
    free(sd);
    return NULL;
}

static int run_quiet(const wchar_t *cmdline)
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD code = 1;
    wchar_t *line = cpy_wdup(cmdline);
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    if (CreateProcessW(NULL, line, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    free(line);
    return code == 0;
}

static void upx_file(const wchar_t *path, int verbose)
{
    wchar_t *upx = find_upx();
    wchar_t *cmd;
    size_t before = 0, after = 0;
    unsigned char *d;

    if (!upx) {
        fputs("compyler: upx not found on PATH or next to compyler.exe, skipping\n", stderr);
        return;
    }
    d = cpy_read_file(path, &before);
    free(d);
    {
        size_t cap = wcslen(upx) + wcslen(path) + 64;
        cmd = (wchar_t *)cpy_xmalloc(cap * sizeof(wchar_t));
        _snwprintf(cmd, cap - 1, L"\"%s\" --best --lzma -q \"%s\"", upx, path);
        cmd[cap - 1] = 0;
    }
    if (run_quiet(cmd)) {
        d = cpy_read_file(path, &after);
        free(d);
        if (verbose || after)
            fprintf(stderr, "compyler: upx loader %llu -> %llu bytes\n",
                    (unsigned long long)before, (unsigned long long)after);
    } else {
        fputs("compyler: upx failed, keeping uncompressed loader\n", stderr);
    }
    free(cmd);
}

static unsigned char *read_file_retry(const wchar_t *path, size_t *len)
{
    int i;
    unsigned char *d = NULL;
    for (i = 0; i < 40; i++) {
        d = cpy_read_file(path, len);
        if (d) return d;
        Sleep(50);
    }
    return NULL;
}

static HANDLE create_retry(const wchar_t *path)
{
    int i;
    HANDLE h = INVALID_HANDLE_VALUE;
    for (i = 0; i < 40; i++) {
        h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) return h;
        {
            DWORD e = GetLastError();
            if (e != ERROR_SHARING_VIOLATION && e != ERROR_LOCK_VIOLATION &&
                e != ERROR_ACCESS_DENIED && e != ERROR_USER_MAPPED_FILE)
                return INVALID_HANDLE_VALUE;
        }
        Sleep(50);
    }
    return INVALID_HANDLE_VALUE;
}

static int write_all(HANDLE h, const void *data, size_t len)
{
    const unsigned char *p = (const unsigned char *)data;
    size_t off = 0;
    while (off < len) {
        DWORD chunk = (DWORD)((len - off > 0x4000000) ? 0x4000000 : (len - off)), put = 0;
        if (!WriteFile(h, p + off, chunk, &put, NULL) || !put) return 0;
        off += put;
    }
    return 1;
}

int cpy_pack(cpy_itemlist *l, cpy_packopt *o)
{
    pack_ctx ctx;
    group_ctx gctx;
    cpy_arc_header hdr;
    cpy_arc_entry *tab;
    cpy_arc_group *grp = NULL;
    unsigned char **gbuf = NULL;
    int *order = NULL;
    cpy_footer foot;
    unsigned char *stub;
    size_t stub_len = 0, pool_cap, pool_len = 0, data_off;
    char *pool;
    HANDLE h;
    uint64_t id = 1469598103934665603ULL, total = 0, packed = 0;
    int i, real = l->n, ngroups = 0;

    stub = cpy_read_file(o->stub, &stub_len);
    if (!stub) {
        fwprintf(stderr, L"compyler: cannot read stub image: %s\n", o->stub);
        return 0;
    }
    patch_subsystem(stub, stub_len, o->windowed);

    if (o->onedir) real = 0;

    memset(&ctx, 0, sizeof(ctx));
    ctx.l = l;
    cpy_parallel(l->n, o->jobs, pack_job, &ctx);
    if (ctx.failed) {
        fputs("compyler: failed to read a payload file\n", stderr);
        return 0;
    }

    pool_cap = 64;
    for (i = 0; i < real; i++) pool_cap += strlen(l->v[i].dest) + 1;
    pool = (char *)cpy_xmalloc(pool_cap);
    tab = (cpy_arc_entry *)cpy_xmalloc(sizeof(cpy_arc_entry) * (size_t)(real ? real : 1));

    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.magic, CPY_ARC_MAGIC, 8);
    hdr.version = CPY_ARC_VERSION;
    hdr.flags = (o->onedir ? CPY_HF_ONEDIR : 0u) | (o->has_hook ? CPY_HF_HAS_HOOK : 0u);
    hdr.entry_count = (uint32_t)real;
    hdr.algo = (uint32_t)o->algo;
    strncpy(hdr.dll_name, o->dll_name, sizeof(hdr.dll_name) - 1);
    strncpy(hdr.entry_path, o->entry_path, sizeof(hdr.entry_path) - 1);
    strncpy(hdr.app_name, o->app_name, sizeof(hdr.app_name) - 1);

    if (real) {
        uint64_t target;
        int gi;
        order = (int *)cpy_xmalloc(sizeof(int) * (size_t)real);
        for (i = 0; i < real; i++) { order[i] = i; total += l->v[i].usize; }
        SORT_L = l;
        qsort(order, (size_t)real, sizeof(int), order_cmp);

        target = CPY_GROUP_TARGET;
        if (o->compress) {
            while (target && total / target > CPY_GROUP_MAX) target *= 2;
        } else {
            target = total + 1;
        }
        grp = (cpy_arc_group *)cpy_xmalloc(sizeof(cpy_arc_group) * (size_t)(CPY_GROUP_MAX + 4));
        memset(grp, 0, sizeof(cpy_arc_group) * (size_t)(CPY_GROUP_MAX + 4));
        ngroups = 0;
        grp[0].first = 0;
        for (i = 0; i < real; i++) {
            cpy_item *it = &l->v[order[i]];
            if (grp[ngroups].usize && grp[ngroups].usize + it->usize > target &&
                ngroups + 1 < CPY_GROUP_MAX + 4) {
                ngroups++;
                grp[ngroups].first = (uint32_t)i;
            }
            grp[ngroups].usize += it->usize;
            grp[ngroups].count++;
        }
        ngroups++;

        gbuf = (unsigned char **)cpy_xmalloc(sizeof(unsigned char *) * (size_t)ngroups);
        memset(gbuf, 0, sizeof(unsigned char *) * (size_t)ngroups);
        memset(&gctx, 0, sizeof(gctx));
        gctx.l = l;
        gctx.order = order;
        gctx.g = grp;
        gctx.out = gbuf;
        gctx.algo = o->algo;
        gctx.compress = o->compress;
        cpy_parallel(ngroups, o->jobs, group_job, &gctx);
        for (i = 0; i < 64; i++) if (gctx.up[i] == 1) cpy_codec_close(&gctx.codec[i]);

        for (gi = 0; gi < ngroups; gi++) {
            uint64_t goff = 0;
            unsigned q;
            for (q = 0; q < grp[gi].count; q++) {
                int idx = order[grp[gi].first + q];
                cpy_item *it = &l->v[idx];
                int e = (int)(grp[gi].first + q);
                size_t nl = strlen(it->dest);
                tab[e].name_off = (uint32_t)pool_len;
                tab[e].name_len = (uint32_t)nl;
                memcpy(pool + pool_len, it->dest, nl);
                pool_len += nl;
                pool[pool_len++] = 0;
                tab[e].group = (uint32_t)gi;
                tab[e].goff = goff;
                tab[e].usize = (uint32_t)it->usize;
                tab[e].flags = 0;
                tab[e].crc = it->crc;
                goff += it->usize;
                id = cpy_fnv1a(it->dest, nl, id);
                id = cpy_fnv1a(&it->crc, sizeof(it->crc), id);
            }
        }
    }
    hdr.group_count = (uint32_t)ngroups;
    hdr.table_off = sizeof(hdr);
    hdr.group_off = hdr.table_off + sizeof(cpy_arc_entry) * (uint64_t)real;
    hdr.name_pool_off = hdr.group_off + sizeof(cpy_arc_group) * (uint64_t)ngroups;
    hdr.cfg_off = hdr.name_pool_off + pool_len;
    hdr.cfg_len = (uint32_t)o->cfg_len;

    id = cpy_fnv1a(hdr.entry_path, strlen(hdr.entry_path), id);
    id = cpy_fnv1a(hdr.dll_name, strlen(hdr.dll_name), id);

    data_off = (size_t)hdr.cfg_off + o->cfg_len;
    for (i = 0; i < ngroups; i++) {
        grp[i].data_off = data_off;
        data_off += (size_t)grp[i].csize;
        packed += grp[i].csize;
    }

    if (o->onedir) {
        wchar_t *outdir = cpy_wdup(o->out);
        wchar_t *slash = wcsrchr(outdir, L'\\');
        wchar_t *root;
        if (slash) *slash = 0; else wcscpy(outdir, L".");
        root = cpy_wjoin(outdir, L"_internal");
        for (i = 0; i < l->n; i++) {
            cpy_item *it = &l->v[i];
            wchar_t *wd = cpy_utf8_to_w(it->dest, -1);
            wchar_t *dst = cpy_wjoin(root, wd);
            cpy_mkdirs_for(dst);
            if (!cpy_write_file(dst, it->cbuf, it->usize)) {
                fwprintf(stderr, L"compyler: cannot write %s\n", dst);
                return 0;
            }
            free(wd);
            free(dst);
        }
        free(root);
        free(outdir);
    }

    {
        int wi, ok = 0;
        for (wi = 0; wi < 40; wi++) {
            if (cpy_write_file(o->out, stub, stub_len)) { ok = 1; break; }
            Sleep(50);
        }
        if (!ok) {
            fwprintf(stderr, L"compyler: cannot write output: %s (error %lu, is it still running?)\n",
                     o->out, (unsigned long)GetLastError());
            return 0;
        }
    }
    if (o->icon && !set_icon(o->out, o->icon))
        fputs("compyler: warning: could not apply icon\n", stderr);
    if (o->upx) upx_file(o->out, o->verbose);

    {
        size_t cur = 0;
        unsigned char *img = read_file_retry(o->out, &cur);
        if (!img) {
            fwprintf(stderr, L"compyler: cannot reopen %s after post-processing (error %lu)\n",
                     o->out, (unsigned long)GetLastError());
            return 0;
        }
        h = create_retry(o->out);
        if (h == INVALID_HANDLE_VALUE) {
            fwprintf(stderr, L"compyler: cannot rewrite %s, it is locked by another process (error %lu)\n",
                     o->out, (unsigned long)GetLastError());
            free(img);
            return 0;
        }
        if (!write_all(h, img, cur)) { CloseHandle(h); free(img); return 0; }
        foot.arc_off = cur;
        free(img);
    }

    if (!write_all(h, &hdr, sizeof(hdr)) ||
        !write_all(h, tab, sizeof(cpy_arc_entry) * (size_t)real)) { CloseHandle(h); return 0; }
    if (ngroups && !write_all(h, grp, sizeof(cpy_arc_group) * (size_t)ngroups)) { CloseHandle(h); return 0; }
    if (!write_all(h, pool, pool_len)) { CloseHandle(h); return 0; }
    if (o->cfg_len && !write_all(h, o->cfg, o->cfg_len)) { CloseHandle(h); return 0; }
    for (i = 0; i < ngroups; i++) {
        if (!write_all(h, gbuf[i], (size_t)grp[i].csize)) { CloseHandle(h); return 0; }
    }

    memcpy(foot.magic, CPY_FOOTER_MAGIC, 8);
    foot.arc_size = data_off;
    foot.payload_id = id;
    if (!write_all(h, &foot, sizeof(foot))) { CloseHandle(h); return 0; }
    CloseHandle(h);

    if (o->verbose) {
        fprintf(stderr, "compyler: archive %d files in %d group(s), %.1f MB raw, %.1f MB packed\n",
                real, ngroups, (double)total / 1048576.0, (double)packed / 1048576.0);
    }
    return 1;
}
