#ifndef CPY_SCAN_H
#define CPY_SCAN_H

#include <stddef.h>

typedef void (*cpy_import_cb)(void *ud, const char *name, size_t len,
                              size_t full, int level);

void cpy_scan_imports(const char *src, size_t len, cpy_import_cb cb, void *ud);
void cpy_scan_set_lazy(int v);
int  cpy_scan_get_lazy(void);

#endif
