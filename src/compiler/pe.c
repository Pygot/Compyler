#include "pe.h"
#include <string.h>
#include <stdint.h>

typedef struct {
    uint32_t va;
    uint32_t vsize;
    uint32_t raw;
    uint32_t rsize;
} pe_sec;

static size_t rva_to_off(const pe_sec *secs, int n, uint32_t rva)
{
    int i;
    for (i = 0; i < n; i++) {
        uint32_t hi = secs[i].vsize > secs[i].rsize ? secs[i].vsize : secs[i].rsize;
        if (rva >= secs[i].va && rva < secs[i].va + hi) {
            uint32_t d = rva - secs[i].va;
            if (d >= secs[i].rsize) return 0;
            return (size_t)secs[i].raw + d;
        }
    }
    return 0;
}

static uint16_t rd16(const unsigned char *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void emit_name(const unsigned char *d, size_t len, size_t off, cpy_dll_cb cb, void *ud)
{
    size_t e = off;
    char buf[260];
    size_t n;
    if (!off || off >= len) return;
    while (e < len && d[e]) e++;
    n = e - off;
    if (!n || n >= sizeof(buf)) return;
    memcpy(buf, d + off, n);
    buf[n] = 0;
    cb(ud, buf);
}

int cpy_pe_imports(const unsigned char *d, size_t len, cpy_dll_cb cb, void *ud)
{
    uint32_t pe_off, ddir_off, nsec_off;
    uint16_t nsec, opt_size, magic;
    const unsigned char *opt;
    pe_sec secs[96];
    int i, ndir;
    uint32_t imp_rva = 0, dly_rva = 0;

    if (len < 0x40 || d[0] != 'M' || d[1] != 'Z') return 0;
    pe_off = rd32(d + 0x3C);
    if ((size_t)pe_off + 0x78 > len) return 0;
    if (memcmp(d + pe_off, "PE\0\0", 4)) return 0;

    nsec = rd16(d + pe_off + 6);
    opt_size = rd16(d + pe_off + 20);
    opt = d + pe_off + 24;
    if ((size_t)(opt - d) + opt_size > len) return 0;
    magic = rd16(opt);
    if (magic == 0x20b) ddir_off = 112;
    else if (magic == 0x10b) ddir_off = 96;
    else return 0;
    ndir = (int)rd32(opt + ddir_off - 4);
    if (ndir > 16) ndir = 16;
    if (ndir > 1) imp_rva = rd32(opt + ddir_off + 1 * 8);
    if (ndir > 13) dly_rva = rd32(opt + ddir_off + 13 * 8);
    if (!imp_rva && !dly_rva) return 1;

    if (nsec > 96) nsec = 96;
    nsec_off = (uint32_t)(opt - d) + opt_size;
    for (i = 0; i < nsec; i++) {
        const unsigned char *s = d + nsec_off + (size_t)i * 40;
        if ((size_t)(nsec_off + (size_t)(i + 1) * 40) > len) { nsec = (uint16_t)i; break; }
        secs[i].vsize = rd32(s + 8);
        secs[i].va    = rd32(s + 12);
        secs[i].rsize = rd32(s + 16);
        secs[i].raw   = rd32(s + 20);
    }

    if (imp_rva) {
        size_t off = rva_to_off(secs, nsec, imp_rva);
        while (off && off + 20 <= len) {
            uint32_t name_rva = rd32(d + off + 12);
            if (!rd32(d + off) && !name_rva && !rd32(d + off + 16)) break;
            if (name_rva) emit_name(d, len, rva_to_off(secs, nsec, name_rva), cb, ud);
            off += 20;
        }
    }
    if (dly_rva) {
        size_t off = rva_to_off(secs, nsec, dly_rva);
        while (off && off + 32 <= len) {
            uint32_t attrs = rd32(d + off);
            uint32_t name_rva = rd32(d + off + 4);
            if (!name_rva) break;
            if (attrs & 1) emit_name(d, len, rva_to_off(secs, nsec, name_rva), cb, ud);
            off += 32;
        }
    }
    return 1;
}
