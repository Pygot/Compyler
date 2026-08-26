#ifndef CPY_UTIL_H
#define CPY_UTIL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>

void  *cpy_xmalloc(size_t n);
void  *cpy_xrealloc(void *p, size_t n);
wchar_t *cpy_wdup(const wchar_t *s);
char    *cpy_adup(const char *s);

wchar_t *cpy_utf8_to_w(const char *s, int len);
char    *cpy_w_to_utf8(const wchar_t *s, int len);

wchar_t *cpy_wjoin(const wchar_t *a, const wchar_t *b);
void     cpy_wnorm(wchar_t *s);
int      cpy_file_exists(const wchar_t *p);
int      cpy_dir_exists(const wchar_t *p);
int      cpy_mkdirs(const wchar_t *p);
int      cpy_mkdirs_for(const wchar_t *file);
unsigned char *cpy_read_file(const wchar_t *p, size_t *out_len);
int      cpy_write_file(const wchar_t *p, const void *data, size_t len);
int      cpy_rmtree(const wchar_t *p);
uint64_t cpy_fnv1a(const void *data, size_t len, uint64_t seed);
uint32_t cpy_crc32(const void *data, size_t len);

typedef struct {
    HMODULE  lib;
    void    *hcomp;
    void    *hdecomp;
    void    *fn_compress;
    void    *fn_decompress;
    void    *fn_close_c;
    void    *fn_close_d;
} cpy_codec;

int    cpy_codec_open(cpy_codec *c, int for_compress, int algo);
void   cpy_codec_close(cpy_codec *c);
size_t cpy_codec_compress(cpy_codec *c, const void *src, size_t slen, void *dst, size_t dcap);
int    cpy_codec_decompress(cpy_codec *c, const void *src, size_t slen, void *dst, size_t dcap, size_t *out);

typedef void (*cpy_job_fn)(void *ctx, int index, int worker);
void cpy_parallel(int count, int workers, cpy_job_fn fn, void *ctx);
int  cpy_cpu_count(void);

#endif
