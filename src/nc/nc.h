#ifndef CPY_NC_H
#define CPY_NC_H

#include "../common/cpy_pyapi.h"
#include "../common/cpy_util.h"

typedef struct nc_ctx nc_ctx;

nc_ctx *nc_open(cpy_py *py, int verbose);
int     nc_ready(nc_ctx *c);
int     nc_count(nc_ctx *c);
int     nc_skipped(nc_ctx *c);

PyObj   nc_transform(nc_ctx *c, PyObj code, const char *modname);

int     nc_write(nc_ctx *c, const wchar_t *path);

const char *nc_reject_summary(nc_ctx *c);

int nc_add_selftest(nc_ctx *c, PyObj *orig, PyObj *xform);

int nc_backend_desc(char *out, size_t cap);
int nc_build_pyd(const wchar_t *csrc, const wchar_t *outpyd, const wchar_t *py_base,
                 const wchar_t *dll_name, const wchar_t *work, int verbose);

#endif
