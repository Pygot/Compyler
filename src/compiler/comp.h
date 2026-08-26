#ifndef CPY_COMP_H
#define CPY_COMP_H

#include "../common/cpy_util.h"
#include "../common/cpy_arc.h"
#include "../common/cpy_pyapi.h"

typedef struct cpy_snode {
    struct cpy_snode *next;
    char            s[1];
} cpy_snode;

typedef struct {
    cpy_snode **b;
    int       nb;
    int       count;
} cpy_set;

void  cpy_set_init(cpy_set *s, int nb);
int   cpy_set_add(cpy_set *s, const char *k);
int   cpy_set_has(cpy_set *s, const char *k);

typedef struct {
    char          *dest;
    wchar_t       *src;
    unsigned char *blob;
    size_t         blob_len;
    unsigned char *cbuf;
    size_t         csize;
    size_t         usize;
    uint32_t       flags;
    uint32_t       crc;
} cpy_item;

typedef struct {
    cpy_item *v;
    int       n;
    int       cap;
    cpy_set    seen;
} cpy_itemlist;

void cpy_items_init(cpy_itemlist *l);
int  cpy_items_add_blob(cpy_itemlist *l, const char *dest, unsigned char *data, size_t len);
int  cpy_items_add_file(cpy_itemlist *l, const char *dest, const wchar_t *src);
int  cpy_items_replace_blob(cpy_itemlist *l, const char *dest, unsigned char *data, size_t len);

typedef struct {
    wchar_t *out;
    char    *app_name;
    char    *dll_name;
    char    *entry_path;
    wchar_t *stub;
    wchar_t *icon;
    const char *cfg;
    size_t   cfg_len;
    int      onedir;
    int      windowed;
    int      compress;
    int      algo;
    int      upx;
    int      jobs;
    int      verbose;
    int      has_hook;
} cpy_packopt;

int cpy_pack(cpy_itemlist *l, cpy_packopt *o);

#endif
