#ifndef CPY_PE_H
#define CPY_PE_H

#include <stddef.h>

typedef void (*cpy_dll_cb)(void *ud, const char *dll_name);

int cpy_pe_imports(const unsigned char *data, size_t len, cpy_dll_cb cb, void *ud);

#endif
