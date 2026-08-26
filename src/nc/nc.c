#include "nc.h"
#include "typed.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

enum {
    NCO_BAD = 0, NCO_NOP, NCO_RESUME, NCO_PUSH_NULL, NCO_POP_TOP, NCO_COPY, NCO_SWAP,
    NCO_LOAD_CONST, NCO_RETURN_VALUE, NCO_RETURN_CONST,
    NCO_LOAD_FAST, NCO_STORE_FAST, NCO_DELETE_FAST,
    NCO_LOAD_GLOBAL, NCO_STORE_GLOBAL,
    NCO_BINARY_OP, NCO_COMPARE_OP, NCO_IS_OP, NCO_CONTAINS_OP,
    NCO_UNARY_NEGATIVE, NCO_UNARY_NOT, NCO_UNARY_INVERT,
    NCO_BINARY_SUBSCR, NCO_STORE_SUBSCR,
    NCO_BUILD_LIST, NCO_BUILD_TUPLE, NCO_LIST_APPEND,
    NCO_GET_ITER, NCO_FOR_ITER, NCO_END_FOR,
    NCO_JUMP, NCO_POP_JUMP_IF_FALSE, NCO_POP_JUMP_IF_TRUE,
    NCO_POP_JUMP_IF_NONE, NCO_POP_JUMP_IF_NOT_NONE,
    NCO_CALL, NCO_PRECALL, NCO_LOAD_ATTR, NCO_STORE_ATTR,
    NCO_UNPACK_SEQUENCE,
    NCO_LOAD_FAST2, NCO_STORE_LOAD_FAST, NCO_STORE_FAST2, NCO_TO_BOOL,
    NCO_BUILD_MAP, NCO_BUILD_CONST_KEY_MAP, NCO_MAP_ADD,
    NCO_BINARY_SLICE, NCO_STORE_SLICE,
    NCO_FORMAT_VALUE, NCO_BUILD_STRING,
    NCO_CONVERT_VALUE, NCO_FORMAT_SIMPLE, NCO_FORMAT_WITH_SPEC
};

typedef struct { const char *name; int op; } opent;

static const opent OPTAB[] = {
    { "NOP", NCO_NOP }, { "CACHE", NCO_NOP }, { "RESUME", NCO_RESUME },
    { "PUSH_NULL", NCO_PUSH_NULL }, { "POP_TOP", NCO_POP_TOP },
    { "COPY", NCO_COPY }, { "SWAP", NCO_SWAP },
    { "LOAD_CONST", NCO_LOAD_CONST }, { "RETURN_VALUE", NCO_RETURN_VALUE },
    { "RETURN_CONST", NCO_RETURN_CONST },
    { "LOAD_FAST", NCO_LOAD_FAST }, { "LOAD_FAST_CHECK", NCO_LOAD_FAST },
    { "LOAD_FAST_BORROW", NCO_LOAD_FAST },
    { "STORE_FAST", NCO_STORE_FAST }, { "DELETE_FAST", NCO_DELETE_FAST },
    { "LOAD_GLOBAL", NCO_LOAD_GLOBAL }, { "STORE_GLOBAL", NCO_STORE_GLOBAL },
    { "BINARY_OP", NCO_BINARY_OP }, { "COMPARE_OP", NCO_COMPARE_OP },
    { "IS_OP", NCO_IS_OP }, { "CONTAINS_OP", NCO_CONTAINS_OP },
    { "UNARY_NEGATIVE", NCO_UNARY_NEGATIVE }, { "UNARY_NOT", NCO_UNARY_NOT },
    { "UNARY_INVERT", NCO_UNARY_INVERT },
    { "BINARY_SUBSCR", NCO_BINARY_SUBSCR }, { "STORE_SUBSCR", NCO_STORE_SUBSCR },
    { "BUILD_LIST", NCO_BUILD_LIST }, { "BUILD_TUPLE", NCO_BUILD_TUPLE },
    { "LIST_APPEND", NCO_LIST_APPEND },
    { "GET_ITER", NCO_GET_ITER }, { "FOR_ITER", NCO_FOR_ITER }, { "END_FOR", NCO_END_FOR },
    { "JUMP_FORWARD", NCO_JUMP }, { "JUMP_BACKWARD", NCO_JUMP },
    { "JUMP_BACKWARD_NO_INTERRUPT", NCO_JUMP },
    { "POP_JUMP_IF_FALSE", NCO_POP_JUMP_IF_FALSE },
    { "POP_JUMP_FORWARD_IF_FALSE", NCO_POP_JUMP_IF_FALSE },
    { "POP_JUMP_BACKWARD_IF_FALSE", NCO_POP_JUMP_IF_FALSE },
    { "POP_JUMP_IF_TRUE", NCO_POP_JUMP_IF_TRUE },
    { "POP_JUMP_FORWARD_IF_TRUE", NCO_POP_JUMP_IF_TRUE },
    { "POP_JUMP_BACKWARD_IF_TRUE", NCO_POP_JUMP_IF_TRUE },
    { "POP_JUMP_IF_NONE", NCO_POP_JUMP_IF_NONE },
    { "POP_JUMP_IF_NOT_NONE", NCO_POP_JUMP_IF_NOT_NONE },
    { "CALL", NCO_CALL }, { "PRECALL", NCO_PRECALL },
    { "LOAD_ATTR", NCO_LOAD_ATTR }, { "LOAD_METHOD", NCO_LOAD_ATTR },
    { "STORE_ATTR", NCO_STORE_ATTR },
    { "UNPACK_SEQUENCE", NCO_UNPACK_SEQUENCE },
    { "LOAD_FAST_LOAD_FAST", NCO_LOAD_FAST2 },
    { "STORE_FAST_LOAD_FAST", NCO_STORE_LOAD_FAST },
    { "STORE_FAST_STORE_FAST", NCO_STORE_FAST2 },
    { "TO_BOOL", NCO_TO_BOOL },
    { "BUILD_MAP", NCO_BUILD_MAP },
    { "BUILD_CONST_KEY_MAP", NCO_BUILD_CONST_KEY_MAP },
    { "MAP_ADD", NCO_MAP_ADD },
    { "BINARY_SLICE", NCO_BINARY_SLICE }, { "STORE_SLICE", NCO_STORE_SLICE },
    { "FORMAT_VALUE", NCO_FORMAT_VALUE }, { "BUILD_STRING", NCO_BUILD_STRING },
    { "CONVERT_VALUE", NCO_CONVERT_VALUE }, { "FORMAT_SIMPLE", NCO_FORMAT_SIMPLE },
    { "FORMAT_WITH_SPEC", NCO_FORMAT_WITH_SPEC },
    { NULL, 0 }
};

typedef struct {
    char *p;
    size_t n, cap;
} buf;

static void bput(buf *b, const char *s, size_t n)
{
    if (b->n + n + 1 > b->cap) {
        while (b->n + n + 1 > b->cap) b->cap = b->cap ? b->cap * 2 : 8192;
        b->p = (char *)cpy_xrealloc(b->p, b->cap);
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = 0;
}

static void bpf(buf *b, const char *fmt, ...)
{
    char tmp[4096];
    int n;
    va_list ap;
    va_start(ap, fmt);
    n = _vsnprintf(tmp, sizeof(tmp) - 1, fmt, ap);
    va_end(ap);
    tmp[sizeof(tmp) - 1] = 0;
    if (n < 0) n = (int)strlen(tmp);
    bput(b, tmp, (size_t)n);
}

struct nc_ctx {
    cpy_py *py;
    int     verbose;
    int     ready;
    int     version;
    PyObj   get_instructions;
    PyObj   stack_effect;
    PyObj   codetype;
    PyObj   t_int, t_float, t_bool, t_str;
    PyObj   v_none, v_true, v_false;
    PyObj   consts;
    PyObj   names;
    buf     code;
    buf     tab;
    int     nfun;
    int     nskip;
    char    reject[512];
    struct { PyObj code; int idx; char qual[192]; } pend[2048];
    int     npend;
    struct { char name[96]; int idx; int argc; } fnmap[512];
    int     nfnmap;
    struct { char name[96]; int idx; int np; signed char par[14]; signed char ret; } tyreg[256];
    int     ntyreg;
};

#define PY (c->py)

static PyObj getattr_(nc_ctx *c, PyObj o, const char *n)
{
    PyObj r = PY->PyObject_GetAttrString(o, n);
    if (!r) PY->PyErr_Clear();
    return r;
}

static long as_long(nc_ctx *c, PyObj o, long dflt)
{
    long v;
    if (!o) return dflt;
    v = PY->PyLong_AsLong(o);
    if (v == -1 && PY->PyErr_Occurred()) { PY->PyErr_Clear(); return dflt; }
    return v;
}

static void note(nc_ctx *c, const char *what)
{
    if (!c->reject[0]) _snprintf(c->reject, sizeof(c->reject) - 1, "%s", what);
    c->nskip++;
}

static int op_of(const char *name)
{
    int i;
    for (i = 0; OPTAB[i].name; i++)
        if (!strcmp(OPTAB[i].name, name)) return OPTAB[i].op;
    return NCO_BAD;
}

nc_ctx *nc_open(cpy_py *py, int verbose)
{
    nc_ctx *c = (nc_ctx *)cpy_xmalloc(sizeof(nc_ctx));
    PyObj dis, builtins;
    memset(c, 0, sizeof(*c));
    c->py = py;
    c->verbose = verbose;
    c->version = py->version;
    if (py->version < 311 || py->version > 313) return c;

    dis = py->PyImport_ImportModule("dis");
    builtins = py->PyImport_ImportModule("builtins");
    if (!dis || !builtins) { py->PyErr_Clear(); return c; }
    c->get_instructions = getattr_(c, dis, "get_instructions");
    c->stack_effect = getattr_(c, dis, "stack_effect");
    c->t_int = getattr_(c, builtins, "int");
    c->t_float = getattr_(c, builtins, "float");
    c->t_bool = getattr_(c, builtins, "bool");
    c->t_str = getattr_(c, builtins, "str");
    c->v_none = getattr_(c, builtins, "None");
    c->v_true = getattr_(c, builtins, "True");
    c->v_false = getattr_(c, builtins, "False");
    if (!c->get_instructions || !c->stack_effect || !c->t_int || !c->v_none) return c;
    c->consts = py->PyList_New(0);
    c->names = py->PyList_New(0);
    c->ready = 1;
    return c;
}

int nc_ready(nc_ctx *c)   { return c->ready; }
int nc_count(nc_ctx *c)   { return c->nfun; }
int nc_skipped(nc_ctx *c) { return c->nskip; }
const char *nc_reject_summary(nc_ctx *c) { return c->reject; }

static int pool_add(nc_ctx *c, PyObj list, PyObj o)
{
    cpy_ssize n = PY->PyList_Size(list), i;
    for (i = 0; i < n; i++)
        if (PY->PyList_GetItem(list, i) == o) return (int)i;
    PY->PyList_Append(list, o);
    return (int)n;
}

typedef struct {
    int   opc;
    int   arg;
    int   off;
    int   target;
    int   depth;
    int   eff;
    int   effj;
    int   jump;
    int   label;
    PyObj argval;
    PyObj argrepr;
} ninst;

typedef struct {
    nc_ctx *c;
    buf    *b;
    ninst  *ins;
    int     n;
    int     nlocals;
    int     stacksize;
    int     argcount;
    int     maxerr;
    int     uses_globals;
    int     uses_iter;
    int     nglob;
    int     nattr;
    int    *origin;
    int    *bid;
} fnc;

static int stack_eff(nc_ctx *c, int opnum, PyObj arg, int jump)
{
    PyObj args = PY->PyTuple_New(2), kw = PY->PyDict_New(), r;
    long v;
    PY->Py_IncRef(arg ? arg : c->v_none);
    PY->PyTuple_SetItem(args, 0, PY->PyLong_FromLong(opnum));
    PY->PyTuple_SetItem(args, 1, arg ? arg : c->v_none);
    if (jump >= 0)
        PY->PyDict_SetItemString(kw, "jump", jump ? c->v_true : c->v_false);
    r = PY->PyObject_Call(c->stack_effect, args, kw);
    PY->Py_DecRef(args);
    PY->Py_DecRef(kw);
    if (!r) { PY->PyErr_Clear(); return -999; }
    v = as_long(c, r, -999);
    PY->Py_DecRef(r);
    return (int)v;
}

static int decode(nc_ctx *c, PyObj code, ninst **out)
{
    PyObj it, item, args, list;
    ninst *ins = NULL;
    int n = 0, cap = 0, i;
    int *depth_at;
    int codelen = 0;

    args = PY->PyTuple_New(1);
    PY->Py_IncRef(code);
    PY->PyTuple_SetItem(args, 0, code);
    list = PY->PyObject_Call(c->get_instructions, args, NULL);
    PY->Py_DecRef(args);
    if (!list) { PY->PyErr_Clear(); return -1; }
    it = PY->PyObject_GetIter(list);
    PY->Py_DecRef(list);
    if (!it) { PY->PyErr_Clear(); return -1; }

    while ((item = PY->PyIter_Next(it)) != NULL) {
        PyObj a_name = getattr_(c, item, "opname");
        PyObj a_op = getattr_(c, item, "opcode");
        PyObj a_arg = getattr_(c, item, "arg");
        PyObj a_val = getattr_(c, item, "argval");
        PyObj a_off = getattr_(c, item, "offset");
        PyObj a_tgt = getattr_(c, item, "is_jump_target");
        PyObj a_rep = getattr_(c, item, "argrepr");
        const char *nm = a_name ? PY->PyUnicode_AsUTF8(a_name) : NULL;
        ninst *k;

        if (n == cap) {
            cap = cap ? cap * 2 : 64;
            ins = (ninst *)cpy_xrealloc(ins, sizeof(ninst) * (size_t)cap);
        }
        k = &ins[n++];
        memset(k, 0, sizeof(*k));
        k->opc = nm ? op_of(nm) : NCO_BAD;
        k->arg = (int)as_long(c, a_arg, -1);
        k->off = (int)as_long(c, a_off, 0);
        k->argval = a_val;
        k->argrepr = a_rep;
        k->label = a_tgt && PY->PyObject_IsTrue(a_tgt) == 1;
        k->target = -1;
        if (k->off + 2 > codelen) codelen = k->off + 2;

        if (k->opc == NCO_BAD) {
            if (nm) {
                char m[128];
                _snprintf(m, sizeof(m) - 1, "opcode %s", nm);
                m[sizeof(m) - 1] = 0;
                note(c, m);
            }
            PY->Py_DecRef(item);
            PY->Py_DecRef(it);
            free(ins);
            return -1;
        }
        switch (k->opc) {
        case NCO_JUMP: case NCO_FOR_ITER:
        case NCO_POP_JUMP_IF_FALSE: case NCO_POP_JUMP_IF_TRUE:
        case NCO_POP_JUMP_IF_NONE: case NCO_POP_JUMP_IF_NOT_NONE:
            k->jump = 1;
            k->target = (int)as_long(c, a_val, -1);
            if (k->target < 0) { PY->Py_DecRef(item); PY->Py_DecRef(it); free(ins); return -1; }
            break;
        default: break;
        }
        {
            int opnum = (int)as_long(c, a_op, -1);
            k->eff = stack_eff(c, opnum, a_arg, k->jump ? 0 : -1);
            k->effj = k->jump ? stack_eff(c, opnum, a_arg, 1) : 0;
            if (k->eff == -999 || k->effj == -999) {
                PY->Py_DecRef(item);
                PY->Py_DecRef(it);
                free(ins);
                note(c, "stack effect");
                return -1;
            }
        }
        PY->Py_DecRef(item);
    }
    PY->Py_DecRef(it);
    PY->PyErr_Clear();

    depth_at = (int *)cpy_xmalloc(sizeof(int) * (size_t)(codelen + 4));
    for (i = 0; i < codelen + 4; i++) depth_at[i] = -1;
    depth_at[0] = 0;
    {
        int cur = 0;
        for (i = 0; i < n; i++) {
            ninst *k = &ins[i];
            if (depth_at[k->off] >= 0) cur = depth_at[k->off];
            else depth_at[k->off] = cur;
            if (cur < 0) cur = 0;
            k->depth = cur;
            if (k->jump && k->target >= 0 && k->target < codelen + 4) {
                int d2 = cur + k->effj;
                if (depth_at[k->target] < 0) depth_at[k->target] = d2;
            }
            if (k->opc == NCO_JUMP || k->opc == NCO_RETURN_VALUE || k->opc == NCO_RETURN_CONST)
                cur = -1;
            else
                cur = cur + k->eff;
        }
    }
    free(depth_at);

    for (i = 0; i < n; i++) {
        if (ins[i].jump && ins[i].target >= 0) {
            int j;
            for (j = 0; j < n; j++)
                if (ins[j].off == ins[i].target) { ins[j].label = 1; break; }
        }
    }
    *out = ins;
    return n;
}

static void use_err(fnc *f, int d)
{
    if (d < 0) d = 0;
    if (d > f->maxerr) f->maxerr = d;
}

static void emit_const(fnc *f, PyObj o, int slot)
{
    nc_ctx *c = f->c;
    buf *b = f->b;
    if (o == c->v_none) { bpf(b, "  s%d = cv_obj(Py_None); Py_INCREF(Py_None);\n", slot); return; }
    if (o == c->v_true) { bpf(b, "  s%d = cv_obj(Py_True); Py_INCREF(Py_True);\n", slot); return; }
    if (o == c->v_false) { bpf(b, "  s%d = cv_obj(Py_False); Py_INCREF(Py_False);\n", slot); return; }
    if (PY->PyObject_IsInstance(o, c->t_bool) != 1) {
        if (PY->PyObject_IsInstance(o, c->t_int) == 1) {
            long long v = PY->PyLong_AsLongLong(o);
            if (!(v == -1 && PY->PyErr_Occurred())) {
                bpf(b, "  s%d = cv_int(%lldLL);\n", slot, v);
                return;
            }
            PY->PyErr_Clear();
        } else if (PY->PyObject_IsInstance(o, c->t_float) == 1) {
            double d = PY->PyFloat_AsDouble(o);
            if (!PY->PyErr_Occurred()) {
                bpf(b, "  s%d = cv_flt(%.17g);\n", slot, d);
                return;
            }
            PY->PyErr_Clear();
        }
    }
    bpf(b, "  s%d = cv_norm(K(%d));\n", slot, pool_add(c, c->consts, o));
}

static int call_base(nc_ctx *c, int depth, int arg)
{
    return c->version < 312 ? depth - 2 : depth - arg - 2;
}

static const char *binop_call(const char *sym, int *ip, const char **bitop)
{
    size_t n = strlen(sym);
    static char base[8];
    *ip = 0;
    *bitop = NULL;
    if (n >= 2 && sym[n - 1] == '=' && strcmp(sym, "==") && strcmp(sym, "<=") &&
        strcmp(sym, ">=") && strcmp(sym, "!=")) {
        *ip = 1;
        n--;
    }
    if (n >= sizeof(base)) return NULL;
    memcpy(base, sym, n);
    base[n] = 0;
    if (!strcmp(base, "+"))  return "cv_add";
    if (!strcmp(base, "-"))  return "cv_sub";
    if (!strcmp(base, "*"))  return "cv_mul";
    if (!strcmp(base, "/"))  return "cv_tdiv";
    if (!strcmp(base, "//")) return "cv_fdiv";
    if (!strcmp(base, "%"))  return "cv_mod";
    if (!strcmp(base, "**")) return "cv_pow";
    if (!strcmp(base, "&"))  { *bitop = "CPY_OP_AND";  return "cv_bitop"; }
    if (!strcmp(base, "|"))  { *bitop = "CPY_OP_OR";   return "cv_bitop"; }
    if (!strcmp(base, "^"))  { *bitop = "CPY_OP_XOR";  return "cv_bitop"; }
    if (!strcmp(base, "<<")) { *bitop = "CPY_OP_SHL";  return "cv_bitop"; }
    if (!strcmp(base, ">>")) { *bitop = "CPY_OP_SHR";  return "cv_bitop"; }
    if (!strcmp(base, "@"))  { *bitop = "CPY_OP_MATM"; return "cv_bitop"; }
    return NULL;
}

static const char *cmp_name(const char *sym)
{
    if (!strcmp(sym, "<"))  return "Py_LT";
    if (!strcmp(sym, "<=")) return "Py_LE";
    if (!strcmp(sym, "==")) return "Py_EQ";
    if (!strcmp(sym, "!=")) return "Py_NE";
    if (!strcmp(sym, ">"))  return "Py_GT";
    if (!strcmp(sym, ">=")) return "Py_GE";
    return NULL;
}

static const char *simple_const(fnc *f, PyObj o, char *buf, size_t n)
{
    nc_ctx *c = f->c;
    if (PY->PyObject_IsInstance(o, c->t_bool) == 1) return NULL;
    if (PY->PyObject_IsInstance(o, c->t_int) == 1) {
        long long v = PY->PyLong_AsLongLong(o);
        if (v == -1 && PY->PyErr_Occurred()) { PY->PyErr_Clear(); return NULL; }
        _snprintf(buf, n - 1, "cv_int(%lldLL)", v);
        buf[n - 1] = 0;
        return buf;
    }
    if (PY->PyObject_IsInstance(o, c->t_float) == 1) {
        double d = PY->PyFloat_AsDouble(o);
        if (PY->PyErr_Occurred()) { PY->PyErr_Clear(); return NULL; }
        _snprintf(buf, n - 1, "cv_flt(%.17g)", d);
        buf[n - 1] = 0;
        return buf;
    }
    return NULL;
}

static const char *operand_of(fnc *f, ninst *k, char *buf, size_t n)
{
    if (k->label) return NULL;
    if (k->opc == NCO_LOAD_FAST) {
        _snprintf(buf, n - 1, "l%d", k->arg);
        buf[n - 1] = 0;
        return buf;
    }
    if (k->opc == NCO_LOAD_CONST) return simple_const(f, k->argval, buf, n);
    return NULL;
}

static int fold_ops(fnc *f, int i, char *ba, size_t na, char *bb, size_t nb,
                    const char **oa, const char **ob)
{
    ninst *k = &f->ins[i];

    if (k->opc == NCO_LOAD_FAST2) {
        _snprintf(ba, na - 1, "l%d", k->arg >> 4);
        ba[na - 1] = 0;
        _snprintf(bb, nb - 1, "l%d", k->arg & 15);
        bb[nb - 1] = 0;
        *oa = ba;
        *ob = bb;
        return 1;
    }
    if (i + 1 >= f->n) return 0;
    *oa = operand_of(f, k, ba, na);
    if (!*oa) return 0;
    *ob = operand_of(f, &f->ins[i + 1], bb, nb);
    if (!*ob) return 0;
    return 2;
}

static int fold_pair(fnc *f, int i, int d)
{
    nc_ctx *c = f->c;
    buf *b = f->b;
    char ba[64], bb[64];
    const char *oa = NULL, *ob = NULL;
    ninst *op;
    int nload = fold_ops(f, i, ba, sizeof(ba), bb, sizeof(bb), &oa, &ob);

    if (!nload) return 0;
    if (i + nload >= f->n) return 0;
    op = &f->ins[i + nload];
    if (op->label) return 0;

    if (op->opc == NCO_BINARY_OP) {
        int ip = 0;
        const char *fn = NULL, *bit = NULL;
        if (op->argrepr) {
            const char *sy = PY->PyUnicode_AsUTF8(op->argrepr);
            if (sy) fn = binop_call(sy, &ip, &bit);
        }
        if (!fn) return 0;
        {
            ninst *st = (i + nload + 1 < f->n) ? &f->ins[i + nload + 1] : NULL;
            int direct = st && !st->label && st->opc == NCO_STORE_FAST;
            bpf(b, "  { cv r; if (%s(&r, %s, %s, ", fn, oa, ob);
            if (bit) bpf(b, "%s, ", bit);
            bpf(b, "%d)) goto E%d;\n", ip, d);
            if (direct)
                bpf(b, "    cv_clear(&l%d); l%d = r; }\n", st->arg, st->arg);
            else
                bpf(b, "    s%d = r; }\n", d);
            use_err(f, d);
            return direct ? nload + 1 : nload;
        }
    }

    if (op->opc == NCO_BINARY_SUBSCR) {
        ninst *st = (i + nload + 1 < f->n) ? &f->ins[i + nload + 1] : NULL;
        int direct = st && !st->label && st->opc == NCO_STORE_FAST;
        bpf(b, "  { cv r; if (cv_getitem(&r, %s, %s)) goto E%d;\n", oa, ob, d);
        if (direct)
            bpf(b, "    cv_clear(&l%d); l%d = r; }\n", st->arg, st->arg);
        else
            bpf(b, "    s%d = r; }\n", d);
        use_err(f, d);
        return direct ? nload + 1 : nload;
    }

    if (op->opc == NCO_IS_OP) {
        bpf(b, "  s%d = cv_bool(cv_is(%s, %s) %s);\n", d, oa, ob, op->arg ? "== 0" : "!= 0");
        return nload;
    }

    if (op->opc == NCO_COMPARE_OP) {
        const char *sy = op->argval ? PY->PyUnicode_AsUTF8(op->argval) : NULL;
        const char *cn = sy ? cmp_name(sy) : NULL;
        ninst *j;
        if (!cn) return 0;
        j = (i + nload + 1 < f->n) ? &f->ins[i + nload + 1] : NULL;
        if (j && !j->label &&
            (j->opc == NCO_POP_JUMP_IF_FALSE || j->opc == NCO_POP_JUMP_IF_TRUE)) {
            bpf(b, "  { int t = cv_cmp_br(%s, %s, %s);\n", oa, ob, cn);
            bpf(b, "    if (t < 0) goto E%d;\n", d);
            bpf(b, "    if (%st) goto L%d; }\n",
                j->opc == NCO_POP_JUMP_IF_FALSE ? "!" : "", j->target);
            use_err(f, d);
            return nload + 1;
        }
        bpf(b, "  { cv r; if (cv_cmp(&r, %s, %s, %s)) goto E%d; s%d = r; }\n",
            oa, ob, cn, d, d);
        use_err(f, d);
        return nload;
    }
    return 0;
}

static int fold_store_subscr(fnc *f, int i, int d)
{
    buf *b = f->b;
    char bv[64], bc[64], bk[64];
    const char *ov, *oc = NULL, *ok = NULL;
    int nload;

    ov = operand_of(f, &f->ins[i], bv, sizeof(bv));
    if (!ov) return 0;
    nload = fold_ops(f, i + 1, bc, sizeof(bc), bk, sizeof(bk), &oc, &ok);
    if (!nload) return 0;
    if (i + 1 + nload >= f->n) return 0;
    if (f->ins[i + 1 + nload].opc != NCO_STORE_SUBSCR) return 0;
    if (f->ins[i + 1 + nload].label) return 0;
    if (f->ins[i + 1].label) return 0;
    bpf(b, "  if (cv_setitem(%s, %s, %s)) goto E%d;\n", oc, ok, ov, d);
    use_err(f, d);
    return 1 + nload;
}

static int emit_body(fnc *f)
{
    nc_ctx *c = f->c;
    buf *b = f->b;
    int i;

    for (i = 0; i < f->n; i++) {
        ninst *k = &f->ins[i];
        int d = k->depth;
        int lo = d + (k->eff < 0 ? k->eff : 0);
        int q, call_src = -1, call_bid = -1;

        if (k->label) {
            for (q = 0; q < f->stacksize; q++) { f->origin[q] = -1; f->bid[q] = -1; }
            bpf(b, " L%d:\n", k->off);
        }

        if (k->opc == NCO_CALL) {
            int cbase = call_base(c, d, k->arg);
            int cb = c->version >= 313 ? cbase : cbase + 1;
            if (cb >= 0 && cb < f->stacksize) { call_src = f->origin[cb]; call_bid = f->bid[cb]; }
        }
        if (lo < 0) lo = 0;
        for (q = lo; q < f->stacksize; q++) { f->origin[q] = -1; f->bid[q] = -1; }

        if (k->opc == NCO_LOAD_FAST || k->opc == NCO_LOAD_CONST ||
            k->opc == NCO_LOAD_FAST2) {
            int used = fold_pair(f, i, d);
            if (used) { i += used; continue; }
        }
        if (k->opc == NCO_LOAD_FAST || k->opc == NCO_LOAD_CONST) {
            int used = fold_store_subscr(f, i, d);
            if (used) { i += used; continue; }
        }

        switch (k->opc) {
        case NCO_NOP:
        case NCO_RESUME:
        case NCO_PRECALL:
            break;

        case NCO_PUSH_NULL:
            bpf(b, "  s%d = CV_NIL;\n", d);
            break;

        case NCO_POP_TOP:
            bpf(b, "  cv_clear(&s%d);\n", d - 1);
            break;

        case NCO_COPY:
            bpf(b, "  s%d = s%d; cv_hold(s%d);\n", d, d - k->arg, d);
            break;

        case NCO_SWAP:
            bpf(b, "  { cv t = s%d; s%d = s%d; s%d = t; }\n",
                d - 1, d - 1, d - k->arg, d - k->arg);
            break;

        case NCO_LOAD_CONST:
            emit_const(f, k->argval, d);
            break;

        case NCO_LOAD_FAST:
            bpf(b, "  s%d = l%d; cv_hold(s%d);\n", d, k->arg, d);
            break;

        case NCO_LOAD_FAST2:
            bpf(b, "  s%d = l%d; cv_hold(s%d);\n", d, k->arg >> 4, d);
            bpf(b, "  s%d = l%d; cv_hold(s%d);\n", d + 1, k->arg & 15, d + 1);
            break;

        case NCO_STORE_LOAD_FAST:
            bpf(b, "  cv_clear(&l%d); l%d = s%d; s%d = CV_NIL;\n",
                k->arg >> 4, k->arg >> 4, d - 1, d - 1);
            bpf(b, "  s%d = l%d; cv_hold(s%d);\n", d - 1, k->arg & 15, d - 1);
            break;

        case NCO_STORE_FAST2:
            bpf(b, "  cv_clear(&l%d); l%d = s%d; s%d = CV_NIL;\n",
                k->arg >> 4, k->arg >> 4, d - 1, d - 1);
            bpf(b, "  cv_clear(&l%d); l%d = s%d; s%d = CV_NIL;\n",
                k->arg & 15, k->arg & 15, d - 2, d - 2);
            break;

        case NCO_TO_BOOL: {
            bpf(b, "  { int tb = cv_truth(s%d);\n", d - 1);
            bpf(b, "    if (tb < 0) goto E%d;\n", d);
            bpf(b, "    cv_clear(&s%d);\n    s%d = cv_bool(tb); }\n", d - 1, d - 1);
            use_err(f, d);
            break;
        }

        case NCO_STORE_FAST:
            bpf(b, "  cv_clear(&l%d); l%d = s%d; s%d = CV_NIL;\n",
                k->arg, k->arg, d - 1, d - 1);
            break;

        case NCO_DELETE_FAST:
            bpf(b, "  cv_clear(&l%d);\n", k->arg);
            break;

        case NCO_LOAD_GLOBAL: {
            int idx = pool_add(c, c->names, k->argval);
            int slot = d;
            f->uses_globals = 1;
            if (k->eff == 2) {
                if (c->version >= 313) bpf(b, "  s%d = CV_NIL;\n", d + 1);
                else { bpf(b, "  s%d = CV_NIL;\n", d); slot = d + 1; }
            }
            bpf(b, "  if (cv_global(&s%d, G, B, N(%d), &CPYG[%d])) goto E%d;\n",
                slot, idx, f->nglob++, d);
            if (slot < f->stacksize) {
                static const char *BNAMES[] = { "len", "ord", "abs", "chr", "float", "int", NULL };
                const char *nm = PY->PyUnicode_AsUTF8(k->argval);
                int m;
                f->origin[slot] = -1;
                f->bid[slot] = -1;
                for (m = 0; nm && m < c->nfnmap; m++)
                    if (!strcmp(c->fnmap[m].name, nm)) { f->origin[slot] = m; break; }
                for (m = 0; nm && BNAMES[m]; m++)
                    if (!strcmp(BNAMES[m], nm)) { f->bid[slot] = m; break; }
            }
            use_err(f, d);
            break;
        }

        case NCO_STORE_GLOBAL: {
            int idx = pool_add(c, c->names, k->argval);
            f->uses_globals = 1;
            bpf(b, "  { PyObject *o = cv_box(s%d); if (!o) goto E%d;\n", d - 1, d);
            bpf(b, "    if (PyDict_SetItem(G, N(%d), o) < 0) { Py_DECREF(o); goto E%d; }\n", idx, d);
            bpf(b, "    Py_DECREF(o); cv_clear(&s%d); }\n", d - 1);
            use_err(f, d);
            break;
        }

        case NCO_BINARY_OP: {
            int ip = 0;
            const char *opn = NULL, *bit = NULL;
            if (k->argrepr) {
                const char *s = PY->PyUnicode_AsUTF8(k->argrepr);
                if (s) opn = binop_call(s, &ip, &bit);
            }
            if (!opn) { note(c, "binary op"); return 0; }
            if (bit)
                bpf(b, "  { cv r; if (%s(&r, s%d, s%d, %s, %d)) goto E%d;\n",
                    opn, d - 2, d - 1, bit, ip, d);
            else
                bpf(b, "  { cv r; if (%s(&r, s%d, s%d, %d)) goto E%d;\n",
                    opn, d - 2, d - 1, ip, d);
            bpf(b, "    cv_clear(&s%d); cv_clear(&s%d); s%d = r; }\n", d - 2, d - 1, d - 2);
            use_err(f, d);
            break;
        }

        case NCO_COMPARE_OP: {
            const char *s = k->argval ? PY->PyUnicode_AsUTF8(k->argval) : NULL;
            const char *cn = s ? cmp_name(s) : NULL;
            int fuse = 0;
            if (!cn) { note(c, "compare op"); return 0; }
            if (i + 1 < f->n && !f->ins[i + 1].label &&
                (f->ins[i + 1].opc == NCO_POP_JUMP_IF_FALSE || f->ins[i + 1].opc == NCO_POP_JUMP_IF_TRUE))
                fuse = 1;
            if (fuse) {
                ninst *j = &f->ins[i + 1];
                bpf(b, "  { int t = cv_cmp_br(s%d, s%d, %s);\n", d - 2, d - 1, cn);
                bpf(b, "    cv_clear(&s%d); cv_clear(&s%d);\n", d - 2, d - 1);
                bpf(b, "    if (t < 0) goto E%d;\n", d - 2);
                bpf(b, "    if (%st) goto L%d; }\n",
                    j->opc == NCO_POP_JUMP_IF_FALSE ? "!" : "", j->target);
                use_err(f, d);
                i++;
            } else {
                bpf(b, "  { cv r; if (cv_cmp(&r, s%d, s%d, %s)) goto E%d;\n", d - 2, d - 1, cn, d);
                bpf(b, "    cv_clear(&s%d); cv_clear(&s%d); s%d = r; }\n", d - 2, d - 1, d - 2);
                use_err(f, d);
            }
            break;
        }

        case NCO_IS_OP:
            bpf(b, "  { int t = cv_is(s%d, s%d);\n", d - 2, d - 1);
            bpf(b, "    cv_clear(&s%d); cv_clear(&s%d);\n", d - 2, d - 1);
            bpf(b, "    if (%d) t = !t;\n", k->arg);
            bpf(b, "    s%d = cv_bool(t); }\n", d - 2);
            break;

        case NCO_CONTAINS_OP:
            bpf(b, "  { cv r; if (cv_contains(&r, s%d, s%d, %d)) goto E%d;\n",
                d - 2, d - 1, k->arg, d);
            bpf(b, "    cv_clear(&s%d); cv_clear(&s%d); s%d = r; }\n", d - 2, d - 1, d - 2);
            use_err(f, d);
            break;

        case NCO_UNARY_NEGATIVE:
            bpf(b, "  { cv r; if (cv_neg(&r, s%d)) goto E%d; cv_clear(&s%d); s%d = r; }\n",
                d - 1, d, d - 1, d - 1);
            use_err(f, d);
            break;

        case NCO_UNARY_INVERT:
            bpf(b, "  { cv r; if (cv_invert(&r, s%d)) goto E%d; cv_clear(&s%d); s%d = r; }\n",
                d - 1, d, d - 1, d - 1);
            use_err(f, d);
            break;

        case NCO_UNARY_NOT:
            bpf(b, "  { int t = cv_truth(s%d); if (t < 0) goto E%d;\n", d - 1, d);
            bpf(b, "    cv_clear(&s%d);\n", d - 1);
            bpf(b, "    s%d = cv_bool(!t); }\n", d - 1);
            use_err(f, d);
            break;

        case NCO_BINARY_SUBSCR:
            bpf(b, "  { cv r; if (cv_getitem(&r, s%d, s%d)) goto E%d;\n", d - 2, d - 1, d);
            bpf(b, "    cv_clear(&s%d); cv_clear(&s%d); s%d = r; }\n", d - 2, d - 1, d - 2);
            use_err(f, d);
            break;

        case NCO_STORE_SUBSCR:
            bpf(b, "  if (cv_setitem(s%d, s%d, s%d)) goto E%d;\n", d - 2, d - 1, d - 3, d);
            bpf(b, "  cv_clear(&s%d); cv_clear(&s%d); cv_clear(&s%d);\n", d - 1, d - 2, d - 3);
            use_err(f, d);
            break;

        case NCO_BUILD_LIST:
        case NCO_BUILD_TUPLE: {
            int q, base = d - k->arg;
            const char *fn = k->opc == NCO_BUILD_LIST ? "cv_build_list" : "cv_build_tuple";
            bpf(b, "  { cv A[%d]; cv r;\n", k->arg > 0 ? k->arg : 1);
            for (q = 0; q < k->arg; q++) bpf(b, "    A[%d] = s%d;\n", q, base + q);
            bpf(b, "    if (%s(&r, A, %d)) goto E%d;\n", fn, k->arg, d);
            for (q = 0; q < k->arg; q++) bpf(b, "    cv_clear(&s%d);\n", base + q);
            bpf(b, "    s%d = r; }\n", base);
            use_err(f, d);
            break;
        }

        case NCO_LIST_APPEND:
            bpf(b, "  if (cv_list_append(s%d, s%d)) goto E%d;\n", d - 1 - k->arg, d - 1, d);
            bpf(b, "  cv_clear(&s%d);\n", d - 1);
            use_err(f, d);
            break;

        case NCO_GET_ITER:
            f->uses_iter = 1;
            bpf(b, "  if (cv_iter_init(&it%d, s%d)) goto E%d;\n", d - 1, d - 1, d);
            bpf(b, "  cv_clear(&s%d);\n", d - 1);
            use_err(f, d);
            break;

        case NCO_FOR_ITER:
            bpf(b, "  { cv v; int rc = cv_iter_next(&it%d, &v);\n", d - 1);
            bpf(b, "    if (rc < 0) goto E%d;\n", d);
            bpf(b, "    if (!rc) { cv_iter_clear(&it%d); s%d = CV_NIL; goto L%d; }\n",
                d - 1, d - 1, k->target);
            bpf(b, "    s%d = v; }\n", d);
            use_err(f, d);
            break;

        case NCO_END_FOR: {
            int q, pops = -k->eff;
            for (q = 0; q < pops; q++) bpf(b, "  cv_clear(&s%d);\n", d - 1 - q);
            break;
        }

        case NCO_JUMP:
            bpf(b, "  goto L%d;\n", k->target);
            break;

        case NCO_POP_JUMP_IF_FALSE:
        case NCO_POP_JUMP_IF_TRUE:
            bpf(b, "  { int t = cv_truth(s%d); cv_clear(&s%d);\n", d - 1, d - 1);
            bpf(b, "    if (t < 0) goto E%d;\n", d - 1);
            bpf(b, "    if (%st) goto L%d; }\n",
                k->opc == NCO_POP_JUMP_IF_FALSE ? "!" : "", k->target);
            use_err(f, d - 1);
            break;

        case NCO_POP_JUMP_IF_NONE:
        case NCO_POP_JUMP_IF_NOT_NONE:
            bpf(b, "  { int t = cv_is_none(s%d);\n", d - 1);
            bpf(b, "    cv_clear(&s%d);\n", d - 1);
            bpf(b, "    if (%st) goto L%d; }\n",
                k->opc == NCO_POP_JUMP_IF_NONE ? "" : "!", k->target);
            break;

        case NCO_LOAD_ATTR: {
            int idx = pool_add(c, c->names, k->argval);
            int ac = f->nattr++;
            if (k->eff == 1) {
                bpf(b, "  { cv r; if (cv_getattr_c(&r, s%d, N(%d), &CPYA[%d])) goto E%d;\n", d - 1, idx, ac, d);
                if (c->version >= 313)
                    bpf(b, "    cv_clear(&s%d); s%d = r; s%d = CV_NIL; }\n", d - 1, d - 1, d);
                else
                    bpf(b, "    cv_clear(&s%d); s%d = CV_NIL; s%d = r; }\n", d - 1, d - 1, d);
            } else {
                bpf(b, "  { cv r; if (cv_getattr_c(&r, s%d, N(%d), &CPYA[%d])) goto E%d;\n", d - 1, idx, ac, d);
                bpf(b, "    cv_clear(&s%d); s%d = r; }\n", d - 1, d - 1);
            }
            use_err(f, d);
            break;
        }

        case NCO_STORE_ATTR: {
            int idx = pool_add(c, c->names, k->argval);
            bpf(b, "  if (cv_setattr(s%d, N(%d), s%d)) goto E%d;\n", d - 1, idx, d - 2, d);
            bpf(b, "  cv_clear(&s%d); cv_clear(&s%d);\n", d - 1, d - 2);
            use_err(f, d);
            break;
        }

        case NCO_UNPACK_SEQUENCE:
            bpf(b, "  { cv tmp[%d]; if (cv_unpack(s%d, tmp, %d)) goto E%d;\n",
                k->arg, d - 1, k->arg, d);
            bpf(b, "    cv_clear(&s%d);\n", d - 1);
            {
                int q;
                for (q = 0; q < k->arg; q++) bpf(b, "    s%d = tmp[%d];\n", d - 1 + q, q);
            }
            bpf(b, "  }\n");
            use_err(f, d);
            break;

        case NCO_CALL: {
            int nargs = k->arg;
            int base = call_base(c, d, nargs);
            int q, direct = -1;
            if (base < 0) { note(c, "call shape"); return 0; }
            if (nargs > 11) { note(c, "call arity"); return 0; }
            if (call_src >= 0 && c->fnmap[call_src].argc == nargs)
                direct = c->fnmap[call_src].idx;
            bpf(b, "  { cv A[%d]; cv r; int rc;\n", nargs > 0 ? nargs : 1);
            for (q = 0; q < nargs; q++) bpf(b, "    A[%d] = s%d;\n", q, base + 2 + q);
            {
                int cs = c->version >= 313 ? base : base + 1;
                int ns = c->version >= 313 ? base + 1 : base;
                if (call_bid >= 0 && nargs == 1) {
                    bpf(b, "    rc = cv_bcall(&r, s%d, A, 1, %d);\n", cs, call_bid);
                    bpf(b, "    if (rc > 0) {\n");
                }
                if (direct >= 0) {
                    bpf(b, "    if (cpy_same_fn(s%d, cpy_tc[%d], G) && s%d.t == CPY_T_NIL)\n",
                        cs, direct, ns);
                    bpf(b, "      rc = cpyf_%d_core(G, A, %d, &r);\n", direct, nargs);
                    bpf(b, "    else if (s%d.t == CPY_T_NIL) rc = cv_call(&r, s%d, A, %d);\n",
                        ns, cs, nargs);
                } else {
                    bpf(b, "    if (s%d.t == CPY_T_NIL) rc = cv_call(&r, s%d, A, %d);\n",
                        ns, cs, nargs);
                }
                bpf(b, "    else rc = cv_call_bound(&r, s%d, s%d, A, %d);\n",
                    cs, ns, nargs);
            }
            if (call_bid >= 0 && nargs == 1) bpf(b, "    }\n");
            bpf(b, "    if (rc) goto E%d;\n", d);
            for (q = 0; q < nargs + 2; q++) bpf(b, "    cv_clear(&s%d);\n", base + q);
            bpf(b, "    s%d = r; }\n", base);
            use_err(f, d);
            break;
        }

        case NCO_RETURN_VALUE:
            bpf(b, "  *cpy_out = s%d; s%d = CV_NIL; cpy_rc = 0; goto DONE;\n", d - 1, d - 1);
            break;

        case NCO_RETURN_CONST:
            emit_const(f, k->argval, d);
            bpf(b, "  *cpy_out = s%d; s%d = CV_NIL; cpy_rc = 0; goto DONE;\n", d, d);
            break;

        case NCO_CONVERT_VALUE:
        case NCO_FORMAT_VALUE:
        case NCO_FORMAT_SIMPLE:
        case NCO_FORMAT_WITH_SPEC: {
            int conv = 0, hs = 0, fmt = 1, vi;
            if (k->opc == NCO_CONVERT_VALUE) { conv = k->arg; fmt = 0; }
            else if (k->opc == NCO_FORMAT_WITH_SPEC) hs = 1;
            else if (k->opc == NCO_FORMAT_VALUE) { conv = k->arg & 3; hs = (k->arg & 4) ? 1 : 0; }
            vi = d - 1 - hs;
            if (vi < 0) { note(c, "fstring depth"); return 0; }
            bpf(b, "  { cv r;\n");
            if (conv) {
                bpf(b, "    if (cv_conv(&r, s%d, %d)) goto E%d;\n", vi, conv, d);
                bpf(b, "    cv_clear(&s%d);\n    s%d = r;\n", vi, vi);
            }
            if (fmt) {
                if (hs) bpf(b, "    if (cv_format(&r, s%d, &s%d)) goto E%d;\n", vi, d - 1, d);
                else    bpf(b, "    if (cv_format(&r, s%d, 0)) goto E%d;\n", vi, d);
                bpf(b, "    cv_clear(&s%d);\n", vi);
                if (hs) bpf(b, "    cv_clear(&s%d);\n", d - 1);
                bpf(b, "    s%d = r;\n", vi);
            }
            bpf(b, "  }\n");
            use_err(f, d);
            break;
        }

        case NCO_BUILD_MAP: {
            int q, base = d - k->arg * 2;
            if (k->arg < 0 || base < 0) { note(c, "map arity"); return 0; }
            bpf(b, "  { cv A[%d]; cv r;\n", k->arg > 0 ? k->arg * 2 : 1);
            for (q = 0; q < k->arg * 2; q++) bpf(b, "    A[%d] = s%d;\n", q, base + q);
            bpf(b, "    if (cv_build_map(&r, A, %d)) goto E%d;\n", k->arg, d);
            for (q = 0; q < k->arg * 2; q++) bpf(b, "    cv_clear(&s%d);\n", base + q);
            bpf(b, "    s%d = r; }\n", base);
            use_err(f, d);
            break;
        }

        case NCO_BUILD_CONST_KEY_MAP: {
            int q, base = d - k->arg - 1;
            if (k->arg < 0 || base < 0) { note(c, "map arity"); return 0; }
            bpf(b, "  { cv A[%d]; cv r;\n", k->arg > 0 ? k->arg : 1);
            for (q = 0; q < k->arg; q++) bpf(b, "    A[%d] = s%d;\n", q, base + q);
            bpf(b, "    if (cv_const_key_map(&r, A, s%d, %d)) goto E%d;\n", d - 1, k->arg, d);
            for (q = 0; q < k->arg + 1; q++) bpf(b, "    cv_clear(&s%d);\n", base + q);
            bpf(b, "    s%d = r; }\n", base);
            use_err(f, d);
            break;
        }

        case NCO_MAP_ADD: {
            int di = d - 2 - k->arg;
            if (di < 0) { note(c, "map add depth"); return 0; }
            bpf(b, "  if (cv_map_add(s%d, s%d, s%d)) goto E%d;\n", di, d - 2, d - 1, d);
            bpf(b, "  cv_clear(&s%d);\n  cv_clear(&s%d);\n", d - 2, d - 1);
            use_err(f, d);
            break;
        }

        case NCO_BINARY_SLICE: {
            int base = d - 3;
            if (base < 0) { note(c, "slice depth"); return 0; }
            bpf(b, "  { cv r; if (cv_slice(&r, s%d, s%d, s%d)) goto E%d;\n",
                base, base + 1, base + 2, d);
            bpf(b, "    cv_clear(&s%d);\n    cv_clear(&s%d);\n    cv_clear(&s%d);\n",
                base, base + 1, base + 2);
            bpf(b, "    s%d = r; }\n", base);
            use_err(f, d);
            break;
        }

        case NCO_STORE_SLICE: {
            int base = d - 4;
            if (base < 0) { note(c, "slice depth"); return 0; }
            bpf(b, "  if (cv_store_slice(s%d, s%d, s%d, s%d)) goto E%d;\n",
                base + 1, base + 2, base + 3, base, d);
            bpf(b, "  cv_clear(&s%d);\n  cv_clear(&s%d);\n  cv_clear(&s%d);\n  cv_clear(&s%d);\n",
                base, base + 1, base + 2, base + 3);
            use_err(f, d);
            break;
        }

        case NCO_BUILD_STRING: {
            int q, base = d - k->arg;
            if (k->arg <= 0 || base < 0) { note(c, "fstring arity"); return 0; }
            bpf(b, "  { cv A[%d]; cv r;\n", k->arg);
            for (q = 0; q < k->arg; q++) bpf(b, "    A[%d] = s%d;\n", q, base + q);
            bpf(b, "    if (cv_build_string(&r, A, %d)) goto E%d;\n", k->arg, d);
            for (q = 0; q < k->arg; q++) bpf(b, "    cv_clear(&s%d);\n", base + q);
            bpf(b, "    s%d = r; }\n", base);
            use_err(f, d);
            break;
        }

        default:
            note(c, "opcode");
            return 0;
        }
    }
    return 1;
}

typedef struct {
    int     la;
    int     lb;
    int     aconst;
    int     bconst;
    int64_t ka;
    int64_t kb;
    int     cmp;
} ty_brfent;

typedef struct {
    nc_ctx      *c;
    ninst       *ins;
    int          n;
    int          nlocals, stacksize, argcount, width;
    int         *brf;
    ty_brfent    brfs[48];
    int          nbrf;
    int          contract;
    int          cselfok;
    char         cidx[14];
    char         ccmp[14];
    int          g_done[24];
    int          g_kind[24];
    char        *stk;
    signed char *nar;
    int         *asrc;
    int         *ralias;
    int          ac1;
    int          ac2;
    int          acadd;
    int          acret;
    signed char *snk;
    char        *dfr;
    int          ndfr;
    signed char *tin;
    signed char *ltype;
    char        *skip;
    int         *rng_at;
    int         *math_at;
    ty_math      mth[TY_MAXMATH];
    int          nmath;
    ty_acr       acr[TY_MAXARR];
    int          nacr;
    int          nmap;
    int         *map_create;
    int          nlol;
    int         *lol_create;
    int         *lol_app;
    int         *lol_app_row;
    int         *lol_sub;
    signed char  lol_elem[80];
    signed char  is_lol[80];
    int          site_of[80];
    int          pure;
    int          wrap;
    int         *bnd;
    int          g_head[24];
    int          g_arr[24];
    int          g_n[24];
    int          ng;
    int         *arr_create;
    int         *sib;
    int          nsib;
    int64_t     *tlo;
    int64_t     *thi;
    struct { int reg; int gname; } sibs[TY_MAXSIB];
    int          ety;
    char         cand[14];
    ty_rng       rng[TY_MAXRNG];
    int          nrng;
    int          rettype;
    int          retguess;
    const char  *selfname;
    char        *selfcall;
    int          nself;
    int          selfname_idx;
} tinf;

#define IV_MIN INT64_MIN
#define IV_MAX INT64_MAX

static int iv_addo(int64_t a, int64_t b, int64_t *r)
{
    if (b > 0 && a > IV_MAX - b) return 1;
    if (b < 0 && a < IV_MIN - b) return 1;
    *r = a + b;
    return 0;
}

static int iv_mulo(int64_t a, int64_t b, int64_t *r)
{
    if (a == 0 || b == 0) { *r = 0; return 0; }
    if (a == IV_MIN || b == IV_MIN) return 1;
    {
        int64_t x = a * b;
        if (x / b != a) return 1;
        *r = x;
        return 0;
    }
}

static int64_t iv_sat_add(int64_t a, int64_t b)
{
    int64_t r;
    if (iv_addo(a, b, &r)) return b > 0 ? IV_MAX : IV_MIN;
    return r;
}

static void iv_add(int64_t l1, int64_t h1, int64_t l2, int64_t h2,
                   int64_t *lo, int64_t *hi)
{
    *lo = iv_sat_add(l1, l2);
    *hi = iv_sat_add(h1, h2);
}

static void iv_sub(int64_t l1, int64_t h1, int64_t l2, int64_t h2,
                   int64_t *lo, int64_t *hi)
{
    int64_t nl2, nh2;
    if (h2 == IV_MIN || l2 == IV_MIN) { *lo = IV_MIN; *hi = IV_MAX; return; }
    nl2 = -h2;
    nh2 = -l2;
    iv_add(l1, h1, nl2, nh2, lo, hi);
}

static int64_t iv_sat_mul(int64_t a, int64_t b)
{
    int64_t r;
    if (!iv_mulo(a, b, &r)) return r;
    return ((a > 0) == (b > 0)) ? IV_MAX : IV_MIN;
}

static void iv_mul(int64_t l1, int64_t h1, int64_t l2, int64_t h2,
                   int64_t *lo, int64_t *hi)
{
    int64_t p[4];
    int i;
    p[0] = iv_sat_mul(l1, l2);
    p[1] = iv_sat_mul(l1, h2);
    p[2] = iv_sat_mul(h1, l2);
    p[3] = iv_sat_mul(h1, h2);
    *lo = p[0];
    *hi = p[0];
    for (i = 1; i < 4; i++) {
        if (p[i] < *lo) *lo = p[i];
        if (p[i] > *hi) *hi = p[i];
    }
}

static int iv_bounded(int64_t lo, int64_t hi)
{
    return lo != IV_MIN && hi != IV_MAX;
}

static int ty_join(int a, int b)
{
    if (a == TY_UNSET) return b;
    if (b == TY_UNSET) return a;
    if (a == b) return a;
    return TY_BAD;
}

static int ty_num(int t) { return t == TY_INT || t == TY_FLT || t == TY_BOOL; }

static int ty_of_const(tinf *t, PyObj o)
{
    nc_ctx *c = t->c;
    if (PY->PyObject_IsInstance(o, c->t_bool) == 1) return TY_BAD;
    if (PY->PyObject_IsInstance(o, c->t_int) == 1) {
        long long v = PY->PyLong_AsLongLong(o);
        if (v == -1 && PY->PyErr_Occurred()) { PY->PyErr_Clear(); return TY_BAD; }
        return TY_INT;
    }
    if (PY->PyObject_IsInstance(o, c->t_float) == 1) {
        PY->PyFloat_AsDouble(o);
        if (PY->PyErr_Occurred()) { PY->PyErr_Clear(); return TY_BAD; }
        return TY_FLT;
    }
    return TY_BAD;
}

static int ty_binop(const char *sym, int a, int b, int *ip)
{
    const char *bit = NULL;
    const char *fn = binop_call(sym, ip, &bit);
    if (!fn) return TY_BAD;
    if (!ty_num(a) || !ty_num(b)) return TY_BAD;
    if (!strcmp(fn, "cv_tdiv")) return TY_FLT;
    if (!strcmp(fn, "cv_pow")) return TY_BAD;
    if (bit) {
        if (!strcmp(bit, "CPY_OP_MATM")) return TY_BAD;
        if (a == TY_FLT || b == TY_FLT) return TY_BAD;
        return TY_INT;
    }
    if (a == TY_FLT || b == TY_FLT) return TY_FLT;
    return TY_INT;
}

typedef struct { const char *name; int arity; const char *fn; const char *dom; } mathent;

static const mathent MATHFN[] = {
    { "sqrt",     1, "sqrt",     "A0 < 0.0" },
    { "sin",      1, "sin",      NULL },
    { "cos",      1, "cos",      NULL },
    { "tan",      1, "tan",      NULL },
    { "asin",     1, "asin",     "A0 < -1.0 || A0 > 1.0" },
    { "acos",     1, "acos",     "A0 < -1.0 || A0 > 1.0" },
    { "atan",     1, "atan",     NULL },
    { "sinh",     1, "sinh",     NULL },
    { "cosh",     1, "cosh",     NULL },
    { "tanh",     1, "tanh",     NULL },
    { "exp",      1, "exp",      NULL },
    { "expm1",    1, "expm1",    NULL },
    { "log",      1, "log",      "A0 <= 0.0" },
    { "log2",     1, "log2",     "A0 <= 0.0" },
    { "log10",    1, "log10",    "A0 <= 0.0" },
    { "log1p",    1, "log1p",    "A0 <= -1.0" },
    { "fabs",     1, "fabs",     NULL },
    { "degrees",  1, "cpy_deg",  NULL },
    { "radians",  1, "cpy_rad",  NULL },
    { "atan2",    2, "atan2",    NULL },
    { "hypot",    2, "hypot",    NULL },
    { "fmod",     2, "fmod",     "A1 == 0.0" },
    { "copysign", 2, "copysign", NULL },
    { NULL, 0, NULL, NULL }
};

static const char *BUILTFN[] = { "float", "int", "abs", "len", "ord", "min", "max", NULL };

static int ty_find_call(tinf *t, int from, int base, int arity)
{
    int j;
    for (j = from; j < t->n; j++) {
        ninst *a = &t->ins[j];
        if (a->label) return -1;
        if (a->opc == NCO_CALL) {
            if (a->arg != arity) return -1;
            if (call_base(t->c, a->depth, a->arg) != base) return -1;
            return j;
        }
        if (a->opc == NCO_JUMP || a->opc == NCO_RETURN_VALUE ||
            a->opc == NCO_RETURN_CONST || a->opc == NCO_FOR_ITER) return -1;
        if (a->depth < base) return -1;
    }
    return -1;
}

static void ty_scan_builtins(tinf *t)
{
    nc_ctx *c = t->c;
    int i;
    for (i = 0; i < t->n; i++) {
        ninst *k = &t->ins[i];
        const char *nm;
        int j, bi;
        if (k->opc != NCO_LOAD_GLOBAL || !k->argval) continue;
        if (t->skip[i]) continue;
        nm = PY->PyUnicode_AsUTF8(k->argval);
        if (!nm) continue;
        for (bi = 0; BUILTFN[bi]; bi++) if (!strcmp(BUILTFN[bi], nm)) break;
        if (!BUILTFN[bi]) continue;
        j = ty_find_call(t, i + 1, k->depth, bi >= 5 ? 2 : 1);
        if (j < 0 || t->math_at[j] >= 0 || t->nmath >= TY_MAXMATH) continue;
        {
            ty_math *m = &t->mth[t->nmath];
            m->lg = i;
            m->attr = -1;
            m->call = j;
            m->nargs = bi >= 5 ? 2 : 1;
            m->fn = -1;
            m->kind = bi == 0 ? TY_K_FLOAT : (bi == 1 ? TY_K_INT :
                      (bi == 2 ? TY_K_ABS : (bi == 3 ? TY_K_LEN :
                      (bi == 4 ? TY_K_ORD : (bi == 5 ? TY_K_MIN : TY_K_MAX)))));
            t->math_at[j] = t->nmath;
            t->skip[i] = 1;
            t->nmath++;
        }
    }
}

static void ty_scan_math(tinf *t)
{
    nc_ctx *c = t->c;
    int i;
    for (i = 0; i < t->n; i++) {
        ninst *k = &t->ins[i];
        const char *nm, *an;
        int j, fi;
        if (k->opc != NCO_LOAD_GLOBAL || !k->argval) continue;
        if (t->skip[i]) continue;
        nm = PY->PyUnicode_AsUTF8(k->argval);
        if (!nm || strcmp(nm, "math")) continue;
        if (i + 1 >= t->n) continue;
        if (t->ins[i + 1].opc != NCO_LOAD_ATTR || !t->ins[i + 1].argval) continue;
        if (t->ins[i + 1].label) continue;
        an = PY->PyUnicode_AsUTF8(t->ins[i + 1].argval);
        if (!an) continue;
        for (fi = 0; MATHFN[fi].name; fi++) if (!strcmp(MATHFN[fi].name, an)) break;
        if (!MATHFN[fi].name) continue;
        j = ty_find_call(t, i + 2, k->depth, MATHFN[fi].arity);
        if (j < 0 || t->math_at[j] >= 0 || t->nmath >= TY_MAXMATH) continue;
        {
            ty_math *m = &t->mth[t->nmath];
            m->lg = i;
            m->attr = i + 1;
            m->call = j;
            m->nargs = MATHFN[fi].arity;
            m->fn = fi;
            m->kind = TY_K_MATH;
            t->math_at[j] = t->nmath;
            t->skip[i] = 1;
            t->skip[i + 1] = 1;
            t->nmath++;
        }
    }
}

static int ty_str_const_ok(nc_ctx *c, PyObj o)
{
    const char *u;
    size_t n, q;
    if (PY->PyObject_IsInstance(o, c->t_str) != 1) return 0;
    u = PY->PyUnicode_AsUTF8(o);
    if (!u) { PY->PyErr_Clear(); return 0; }
    n = strlen(u);
    if (n > 63) return 0;
    for (q = 0; q < n; q++)
        if ((unsigned char)u[q] >= 128) return 0;
    return 1;
}

static int ty_arrty(int e) { return e == TY_INT ? TY_ARRI : TY_ARRF; }
static int ty_elem(int a) { return a == TY_ARRI ? TY_INT : TY_FLT; }
static int ty_is_arr(int a) { return a == TY_ARRI || a == TY_ARRF; }

static void ty_scan_params(tinf *t)
{
    int i;
    memset(t->cand, 0, sizeof(t->cand));
    for (i = 0; i < t->n; i++) {
        ninst *k = &t->ins[i];
        int p = -1;
        if (k->opc == NCO_LOAD_FAST && i + 2 < t->n &&
            (t->ins[i + 2].opc == NCO_BINARY_SUBSCR ||
             t->ins[i + 2].opc == NCO_STORE_SUBSCR))
            p = k->arg;
        if (k->opc == NCO_LOAD_FAST2 && i + 1 < t->n &&
            (t->ins[i + 1].opc == NCO_BINARY_SUBSCR ||
             t->ins[i + 1].opc == NCO_STORE_SUBSCR))
            p = k->arg >> 4;
        if (p >= 0 && p < t->argcount && p < 14) t->cand[p] = 1;
    }
    for (i = 0; i < t->n; i++) {
        if (t->math_at[i] < 0) continue;
        if (t->mth[t->math_at[i]].kind != TY_K_LEN) continue;
        {
            int j = i - 1;
            while (j >= 0 && (t->ins[j].opc == NCO_PRECALL)) j--;
            if (j >= 0 && t->ins[j].opc == NCO_LOAD_FAST &&
                t->ins[j].arg < t->argcount && t->ins[j].arg < 14)
                t->cand[t->ins[j].arg] = 1;
        }
    }
}

static void ty_scan_lol(tinf *t)
{
    nc_ctx *c = t->c;
    int i;
    for (i = 0; i < t->nacr; i++) {
        int loc = t->ins[t->acr[i].store_i].arg;
        if (loc < 80) t->site_of[loc] = i;
    }
    for (i = 0; i + 1 < t->n; i++) {
        if (t->ins[i].opc != NCO_BUILD_LIST || t->ins[i].arg != 0) continue;
        if (t->ins[i + 1].opc != NCO_STORE_FAST) continue;
        if (t->ins[i + 1].label || t->skip[i]) continue;
        if (t->nlol >= 16) continue;
        if (t->ins[i + 1].arg >= 80) continue;
        t->skip[i] = 1;
        t->lol_create[i + 1] = t->nlol;
        t->is_lol[t->ins[i + 1].arg] = 1;
        t->nlol++;
    }
    if (!t->nlol) return;
    for (i = 1; i < t->n; i++) {
        ninst *k = &t->ins[i];
        const char *nm;
        int j, lolloc, rowloc;
        if (k->opc != NCO_LOAD_ATTR || !k->argval || t->skip[i]) continue;
        nm = PY->PyUnicode_AsUTF8(k->argval);
        if (!nm || strcmp(nm, "append")) continue;
        if (t->ins[i - 1].opc != NCO_LOAD_FAST || k->label) continue;
        lolloc = t->ins[i - 1].arg;
        if (lolloc >= 80 || !t->is_lol[lolloc]) continue;
        j = i + 1;
        if (j < t->n && t->ins[j].opc == NCO_PUSH_NULL && !t->ins[j].label) j++;
        if (j >= t->n || t->ins[j].opc != NCO_LOAD_FAST || t->ins[j].label) continue;
        rowloc = t->ins[j].arg;
        j++;
        while (j < t->n && t->ins[j].opc == NCO_PRECALL && !t->ins[j].label) j++;
        if (j >= t->n || t->ins[j].opc != NCO_CALL || t->ins[j].arg != 1 ||
            t->ins[j].label) continue;
        if (rowloc >= 80 || t->site_of[rowloc] < 0) continue;
        {
            int e = t->acr[t->site_of[rowloc]].elem;
            if (t->lol_elem[lolloc] && t->lol_elem[lolloc] != e) continue;
            t->lol_elem[lolloc] = (signed char)e;
        }
        {
            int q;
            for (q = i - 1; q < j; q++) t->skip[q] = 1;
        }
        t->lol_app[j] = lolloc;
        t->lol_app_row[j] = rowloc;
    }
    for (i = 1; i < t->n; i++) {
        ninst *k = &t->ins[i];
        int lolloc = -1;
        if (t->skip[i]) continue;
        if (k->opc == NCO_BINARY_SUBSCR && i >= 2 &&
            t->ins[i - 2].opc == NCO_LOAD_FAST && !t->skip[i - 2] &&
            !t->ins[i - 1].label && !k->label &&
            t->ins[i - 2].arg < 80 && t->is_lol[t->ins[i - 2].arg])
            lolloc = t->ins[i - 2].arg;
        if (k->opc == NCO_BINARY_SUBSCR && i >= 1 &&
            t->ins[i - 1].opc == NCO_LOAD_FAST2 && !t->skip[i - 1] &&
            !k->label &&
            (t->ins[i - 1].arg >> 4) < 80 && t->is_lol[t->ins[i - 1].arg >> 4])
            lolloc = t->ins[i - 1].arg >> 4;
        if (lolloc >= 0 && t->lol_elem[lolloc])
            t->lol_sub[i] = lolloc;
    }
}

static void ty_scan_maps(tinf *t)
{
    int i;
    for (i = 0; i + 1 < t->n; i++) {
        if (t->ins[i].opc != NCO_BUILD_MAP || t->ins[i].arg != 0) continue;
        if (t->ins[i + 1].opc != NCO_STORE_FAST) continue;
        if (t->ins[i + 1].label) continue;
        if (t->skip[i]) continue;
        if (t->nmap >= TY_MAXARR) continue;
        t->skip[i] = 1;
        t->map_create[i + 1] = t->nmap;
        t->nmap++;
    }
}

static void ty_scan_arrays(tinf *t)
{
    nc_ctx *c = t->c;
    int i;
    for (i = 0; i + 4 < t->n; i++) {
        ninst *k = &t->ins[i];
        int ce;
        if (k->opc != NCO_LOAD_CONST) continue;
        if (t->skip[i]) continue;
        if (t->ins[i + 1].opc != NCO_BUILD_LIST || t->ins[i + 1].arg != 1) continue;
        if (t->ins[i + 2].opc != NCO_LOAD_FAST && t->ins[i + 2].opc != NCO_LOAD_CONST) continue;
        if (t->ins[i + 3].opc != NCO_BINARY_OP) continue;
        if (t->ins[i + 4].opc != NCO_STORE_FAST) continue;
        if (t->ins[i + 1].label || t->ins[i + 2].label || t->ins[i + 3].label ||
            t->ins[i + 4].label) continue;
        {
            const char *sy = t->ins[i + 3].argrepr ?
                PY->PyUnicode_AsUTF8(t->ins[i + 3].argrepr) : NULL;
            if (!sy || strcmp(sy, "*")) continue;
        }
        ce = ty_of_const(t, k->argval);
        if (ce != TY_INT && ce != TY_FLT) continue;
        if (t->ins[i + 2].opc == NCO_LOAD_CONST &&
            ty_of_const(t, t->ins[i + 2].argval) != TY_INT) continue;
        if (t->nacr >= TY_MAXARR) continue;
        {
            ty_acr *a = &t->acr[t->nacr];
            a->store_i = i + 4;
            a->elem = ce;
            a->len_i = i + 2;
            a->iv = 0;
            a->dv = 0.0;
            if (ce == TY_INT) a->iv = PY->PyLong_AsLongLong(k->argval);
            else a->dv = PY->PyFloat_AsDouble(k->argval);
            if (PY->PyErr_Occurred()) { PY->PyErr_Clear(); continue; }
            t->skip[i] = 1;
            t->skip[i + 1] = 1;
            t->skip[i + 2] = 1;
            t->skip[i + 3] = 1;
            t->arr_create[i + 4] = t->nacr;
            t->nacr++;
        }
        i += 4;
    }
}

static void ty_scan_sibs(tinf *t)
{
    nc_ctx *c = t->c;
    int i;
    for (i = 0; i < t->n; i++) {
        ninst *k = &t->ins[i];
        int base, j;
        if (k->opc != NCO_CALL) continue;
        if (t->selfcall[i] || t->math_at[i] >= 0 || t->skip[i]) continue;
        base = call_base(c, k->depth, k->arg);
        if (base < 0) continue;
        for (j = i - 1; j >= 0; j--) {
            ninst *p = &t->ins[j];
            if (p->depth != base || p->eff <= 0) continue;
            if (p->opc == NCO_LOAD_GLOBAL && p->eff == 2 && p->argval) {
                const char *nm = PY->PyUnicode_AsUTF8(p->argval);
                int rg;
                if (!nm) break;
                if (t->selfname && !strcmp(nm, t->selfname)) break;
                for (rg = 0; rg < c->ntyreg; rg++)
                    if (c->tyreg[rg].np == k->arg &&
                        !strcmp(c->tyreg[rg].name, nm)) break;
                if (rg == c->ntyreg) break;
                {
                    int sl, have = -1;
                    for (sl = 0; sl < t->nsib; sl++)
                        if (t->sibs[sl].reg == rg) have = sl;
                    if (have < 0) {
                        if (t->nsib >= TY_MAXSIB) break;
                        t->sibs[t->nsib].reg = rg;
                        t->sibs[t->nsib].gname = pool_add(c, c->names, p->argval);
                        t->nsib++;
                    }
                }
                t->sib[i] = rg;
                t->skip[j] = 1;
            }
            break;
        }
    }
}

static void ty_scan_ranges(tinf *t)
{
    nc_ctx *c = t->c;
    int i;
    for (i = 0; i < t->n; i++) {
        ninst *k = &t->ins[i];
        const char *nm;
        int j, na = 0, ai[3];
        if (k->opc != NCO_LOAD_GLOBAL || !k->argval) continue;
        nm = PY->PyUnicode_AsUTF8(k->argval);
        if (!nm || strcmp(nm, "range")) continue;
        j = i + 1;
        while (j < t->n && na < 3) {
            ninst *a = &t->ins[j];
            if (a->label) break;
            if (a->opc == NCO_LOAD_FAST) { ai[na++] = j; j++; continue; }
            if (a->opc == NCO_LOAD_CONST && ty_of_const(t, a->argval) == TY_INT) {
                ai[na++] = j; j++; continue;
            }
            break;
        }
        if (na < 1 || j + 2 >= t->n) continue;
        if (t->ins[j].opc != NCO_CALL || t->ins[j].arg != na) continue;
        if (t->ins[j + 1].opc != NCO_GET_ITER) continue;
        if (t->ins[j + 2].opc != NCO_FOR_ITER) continue;
        if (t->nrng >= TY_MAXRNG) continue;
        {
            ty_rng *r = &t->rng[t->nrng];
            int q;
            r->lg = i;
            r->nargs = na;
            for (q = 0; q < na; q++) r->argi[q] = ai[q];
            r->foriter = j + 2;
            t->rng_at[j + 2] = t->nrng;
            for (q = i; q <= j + 1; q++) t->skip[q] = 1;
            t->nrng++;
        }
    }
}

static void ty_scan_self(tinf *t)
{
    nc_ctx *c = t->c;
    int i;
    if (!t->selfname) return;
    for (i = 0; i < t->n; i++) {
        ninst *k = &t->ins[i];
        int base, j;
        if (k->opc != NCO_CALL) continue;
        if (k->arg != t->argcount) continue;
        base = call_base(c, k->depth, k->arg);
        if (base < 0) continue;
        for (j = i - 1; j >= 0; j--) {
            ninst *p = &t->ins[j];
            if (p->depth != base || p->eff <= 0) continue;
            if (p->opc == NCO_LOAD_GLOBAL && p->eff == 2 && p->argval) {
                const char *nm = PY->PyUnicode_AsUTF8(p->argval);
                if (nm && !strcmp(nm, t->selfname)) {
                    t->selfcall[i] = 1;
                    t->skip[j] = 1;
                    t->selfname_idx = pool_add(c, c->names, p->argval);
                    t->nself++;
                }
            }
            break;
        }
    }
}

static int ty_slot_ty(tinf *t, int i, int slot);
static int ty_loc_ty(tinf *t, int loc);

static int ty_brf_cmpcode(const char *sy)
{
    if (!strcmp(sy, "<"))  return 0;
    if (!strcmp(sy, "<=")) return 1;
    if (!strcmp(sy, "==")) return 2;
    if (!strcmp(sy, "!=")) return 3;
    if (!strcmp(sy, ">"))  return 4;
    if (!strcmp(sy, ">=")) return 5;
    return -1;
}

static void ty_scan_branches(tinf *t)
{
    nc_ctx *c = t->c;
    int h;
    for (h = 0; h < t->n; h++) {
        int la = -1, lb = -1, aconst = 0, bconst = 0, cmprow = -1, pjrow, cm;
        int64_t ka = 0, kb = 0;
        ninst *k = &t->ins[h];
        const char *sy;
        if (t->skip[h]) continue;
        if (k->opc == NCO_LOAD_FAST2) {
            la = k->arg >> 4;
            lb = k->arg & 15;
            cmprow = h + 1;
        } else if (k->opc == NCO_LOAD_FAST && h + 1 < t->n &&
                   !t->ins[h + 1].label && !t->skip[h + 1]) {
            la = k->arg;
            if (t->ins[h + 1].opc == NCO_LOAD_FAST) {
                lb = t->ins[h + 1].arg;
                cmprow = h + 2;
            } else if (t->ins[h + 1].opc == NCO_LOAD_CONST &&
                       ty_of_const(t, t->ins[h + 1].argval) == TY_INT) {
                long long v = PY->PyLong_AsLongLong(t->ins[h + 1].argval);
                if (v == -1 && PY->PyErr_Occurred()) { PY->PyErr_Clear(); continue; }
                bconst = 1;
                kb = v;
                cmprow = h + 2;
            } else continue;
        } else if (k->opc == NCO_LOAD_CONST &&
                   ty_of_const(t, k->argval) == TY_INT && h + 1 < t->n &&
                   !t->ins[h + 1].label && !t->skip[h + 1] &&
                   t->ins[h + 1].opc == NCO_LOAD_FAST) {
            long long v = PY->PyLong_AsLongLong(k->argval);
            if (v == -1 && PY->PyErr_Occurred()) { PY->PyErr_Clear(); continue; }
            aconst = 1;
            ka = v;
            lb = t->ins[h + 1].arg;
            cmprow = h + 2;
        } else continue;
        if (!aconst && !bconst && la == lb) continue;
        if (cmprow >= t->n || t->ins[cmprow].label || t->skip[cmprow]) continue;
        if (t->ins[cmprow].opc != NCO_COMPARE_OP || !t->ins[cmprow].argval) continue;
        sy = PY->PyUnicode_AsUTF8(t->ins[cmprow].argval);
        cm = sy ? ty_brf_cmpcode(sy) : -1;
        if (cm < 0) continue;
        pjrow = cmprow + 1;
        if (pjrow < t->n && t->ins[pjrow].opc == NCO_TO_BOOL &&
            !t->ins[pjrow].label && !t->skip[pjrow]) pjrow++;
        if (pjrow >= t->n || t->ins[pjrow].label || t->skip[pjrow]) continue;
        if (t->ins[pjrow].opc != NCO_POP_JUMP_IF_FALSE &&
            t->ins[pjrow].opc != NCO_POP_JUMP_IF_TRUE) continue;
        if (!aconst && (la >= t->nlocals || ty_loc_ty(t, la) != TY_INT)) continue;
        if (!bconst && (lb >= t->nlocals || ty_loc_ty(t, lb) != TY_INT)) continue;
        if (t->brf[pjrow] >= 0 || t->nbrf >= 48) continue;
        t->brfs[t->nbrf].la = la;
        t->brfs[t->nbrf].lb = lb;
        t->brfs[t->nbrf].aconst = aconst;
        t->brfs[t->nbrf].bconst = bconst;
        t->brfs[t->nbrf].ka = ka;
        t->brfs[t->nbrf].kb = kb;
        t->brfs[t->nbrf].cmp = cm;
        t->brf[pjrow] = t->nbrf;
        t->nbrf++;
    }
}

static void iv_refine1(int cmp, int64_t *alo, int64_t *ahi, int64_t *blo, int64_t *bhi)
{
    switch (cmp) {
    case 0:
        if (*bhi == IV_MIN) { *alo = IV_MAX; *ahi = IV_MIN; break; }
        if (*bhi != IV_MAX && *bhi - 1 < *ahi) *ahi = *bhi - 1;
        if (*alo == IV_MAX) { *blo = IV_MAX; *bhi = IV_MIN; break; }
        if (*alo != IV_MIN && *alo + 1 > *blo) *blo = *alo + 1;
        break;
    case 1:
        if (*bhi < *ahi) *ahi = *bhi;
        if (*alo > *blo) *blo = *alo;
        break;
    case 2: {
        int64_t lo = *alo > *blo ? *alo : *blo;
        int64_t hi = *ahi < *bhi ? *ahi : *bhi;
        *alo = lo; *ahi = hi;
        *blo = lo; *bhi = hi;
        break;
    }
    case 3:
        break;
    case 4:
        if (*blo == IV_MAX) { *alo = IV_MAX; *ahi = IV_MIN; break; }
        if (*blo != IV_MIN && *blo + 1 > *alo) *alo = *blo + 1;
        if (*ahi == IV_MIN) { *blo = IV_MAX; *bhi = IV_MIN; break; }
        if (*ahi != IV_MAX && *ahi - 1 < *bhi) *bhi = *ahi - 1;
        break;
    case 5:
        if (*blo > *alo) *alo = *blo;
        if (*ahi < *bhi) *bhi = *ahi;
        break;
    }
}

static void ty_brf_apply(tinf *t, int bx, int truth, int64_t *clo, int64_t *chi)
{
    ty_brfent *bf = &t->brfs[bx];
    int cmp = truth ? bf->cmp : 5 - bf->cmp;
    int64_t alo, ahi, blo, bhi;
    if (bf->aconst) { alo = bf->ka; ahi = bf->ka; }
    else { alo = clo[bf->la]; ahi = chi[bf->la]; if (alo > ahi) return; }
    if (bf->bconst) { blo = bf->kb; bhi = bf->kb; }
    else { blo = clo[bf->lb]; bhi = chi[bf->lb]; if (blo > bhi) return; }
    iv_refine1(cmp, &alo, &ahi, &blo, &bhi);
    if (!bf->aconst) { clo[bf->la] = alo; chi[bf->la] = ahi; }
    if (!bf->bconst) { clo[bf->lb] = blo; chi[bf->lb] = bhi; }
}

static void ty_ivpass(tinf *t)
{
    nc_ctx *c = t->c;
    int64_t *clo, *chi;
    int pass, i, q, changed;
    int W = t->width;

    clo = (int64_t *)cpy_xmalloc(sizeof(int64_t) * (size_t)W);
    chi = (int64_t *)cpy_xmalloc(sizeof(int64_t) * (size_t)W);
    for (i = 0; i < t->n * W; i++) { t->tlo[i] = IV_MAX; t->thi[i] = IV_MIN; }
    for (i = 0; i < t->argcount; i++) { t->tlo[i] = IV_MIN; t->thi[i] = IV_MAX; }
    if (t->contract)
        for (i = 0; i < t->argcount && i < 14; i++) {
            if (t->tin[i] != TY_INT) continue;
            if (t->cidx[i] || t->ccmp[i]) {
                t->tlo[i] = 0;
                t->thi[i] = (int64_t)1 << 31;
            }
        }

    for (pass = 0; pass < 12; pass++) {
        changed = 0;
        for (q = 0; q < W; q++) { clo[q] = IV_MAX; chi[q] = IV_MIN; }
        for (i = 0; i < t->n; i++) {
            ninst *k = &t->ins[i];
            int64_t *rlo = t->tlo + (size_t)i * W;
            int64_t *rhi = t->thi + (size_t)i * W;
            int d = k->depth;

            for (q = 0; q < W; q++) {
                int64_t jl, jh;
                if (clo[q] > chi[q]) { jl = rlo[q]; jh = rhi[q]; }
                else if (rlo[q] > rhi[q]) { jl = clo[q]; jh = chi[q]; }
                else {
                    jl = rlo[q] < clo[q] ? rlo[q] : clo[q];
                    jh = rhi[q] > chi[q] ? rhi[q] : chi[q];
                    if (pass >= 3) {
                        if (jl != rlo[q]) jl = IV_MIN;
                        if (jh != rhi[q]) jh = IV_MAX;
                    }
                }
                if (jl != rlo[q] || jh != rhi[q]) {
                    rlo[q] = jl;
                    rhi[q] = jh;
                    changed = 1;
                }
            }
            memcpy(clo, rlo, sizeof(int64_t) * (size_t)W);
            memcpy(chi, rhi, sizeof(int64_t) * (size_t)W);

            if (t->skip[i]) continue;

            switch (k->opc) {
            case NCO_LOAD_CONST:
                if (ty_slot_ty(t, i + 1 < t->n ? i + 1 : i, d) == TY_INT) {
                    long long v = PY->PyLong_AsLongLong(k->argval);
                    if (v == -1 && PY->PyErr_Occurred()) PY->PyErr_Clear();
                    else { clo[t->nlocals + d] = v; chi[t->nlocals + d] = v; }
                }
                break;
            case NCO_LOAD_FAST:
                clo[t->nlocals + d] = clo[k->arg];
                chi[t->nlocals + d] = chi[k->arg];
                break;
            case NCO_LOAD_FAST2:
                clo[t->nlocals + d] = clo[k->arg >> 4];
                chi[t->nlocals + d] = chi[k->arg >> 4];
                clo[t->nlocals + d + 1] = clo[k->arg & 15];
                chi[t->nlocals + d + 1] = chi[k->arg & 15];
                break;
            case NCO_STORE_FAST:
                if (t->arr_create[i] >= 0 || t->map_create[i] >= 0) break;
                clo[k->arg] = clo[t->nlocals + d - 1];
                chi[k->arg] = chi[t->nlocals + d - 1];
                break;
            case NCO_STORE_LOAD_FAST:
                clo[k->arg >> 4] = clo[t->nlocals + d - 1];
                chi[k->arg >> 4] = chi[t->nlocals + d - 1];
                clo[t->nlocals + d - 1] = clo[k->arg & 15];
                chi[t->nlocals + d - 1] = chi[k->arg & 15];
                break;
            case NCO_STORE_FAST2:
                clo[k->arg >> 4] = clo[t->nlocals + d - 1];
                chi[k->arg >> 4] = chi[t->nlocals + d - 1];
                clo[k->arg & 15] = clo[t->nlocals + d - 2];
                chi[k->arg & 15] = chi[t->nlocals + d - 2];
                break;
            case NCO_COMPARE_OP: case NCO_IS_OP: case NCO_CONTAINS_OP:
            case NCO_UNARY_NOT: case NCO_TO_BOOL:
                clo[t->nlocals + d - (k->opc == NCO_COMPARE_OP ? 2 : 1)] = 0;
                chi[t->nlocals + d - (k->opc == NCO_COMPARE_OP ? 2 : 1)] = 1;
                break;
            case NCO_COPY:
                clo[t->nlocals + d] = clo[t->nlocals + d - k->arg];
                chi[t->nlocals + d] = chi[t->nlocals + d - k->arg];
                break;
            case NCO_SWAP: {
                int64_t wl = clo[t->nlocals + d - 1], wh = chi[t->nlocals + d - 1];
                clo[t->nlocals + d - 1] = clo[t->nlocals + d - k->arg];
                chi[t->nlocals + d - 1] = chi[t->nlocals + d - k->arg];
                clo[t->nlocals + d - k->arg] = wl;
                chi[t->nlocals + d - k->arg] = wh;
                break;
            }
            case NCO_UNARY_NEGATIVE: {
                int64_t l = clo[t->nlocals + d - 1], h = chi[t->nlocals + d - 1];
                if (l != IV_MIN && h != IV_MIN) {
                    clo[t->nlocals + d - 1] = -h;
                    chi[t->nlocals + d - 1] = -l;
                } else {
                    clo[t->nlocals + d - 1] = IV_MIN;
                    chi[t->nlocals + d - 1] = IV_MAX;
                }
                break;
            }
            case NCO_BINARY_SUBSCR: {
                int at = ty_slot_ty(t, i, d - 2);
                if (at == TY_STR) { clo[t->nlocals + d - 2] = 0; chi[t->nlocals + d - 2] = 127; }
                else { clo[t->nlocals + d - 2] = IV_MIN; chi[t->nlocals + d - 2] = IV_MAX; }
                break;
            }
            case NCO_BINARY_OP: {
                int ip2 = 0;
                const char *bit = NULL;
                const char *sy = PY->PyUnicode_AsUTF8(k->argrepr);
                const char *fn = sy ? binop_call(sy, &ip2, &bit) : NULL;
                int64_t l1 = clo[t->nlocals + d - 2], h1 = chi[t->nlocals + d - 2];
                int64_t l2 = clo[t->nlocals + d - 1], h2 = chi[t->nlocals + d - 1];
                int64_t rl = IV_MIN, rh = IV_MAX;
                int tr = ty_slot_ty(t, i + 1 < t->n ? i + 1 : i, d - 2);
                if (tr == TY_INT && fn) {
                    if (!strcmp(fn, "cv_add")) iv_add(l1, h1, l2, h2, &rl, &rh);
                    else if (!strcmp(fn, "cv_sub")) iv_sub(l1, h1, l2, h2, &rl, &rh);
                    else if (!strcmp(fn, "cv_mul")) iv_mul(l1, h1, l2, h2, &rl, &rh);
                    else if (!strcmp(fn, "cv_mod")) {
                        if (l2 >= 1 && h2 != IV_MAX) { rl = 0; rh = h2 - 1; }
                    }
                    else if (!strcmp(fn, "cv_fdiv")) {
                        if (l1 >= 0 && l2 >= 1) { rl = 0; rh = h1; }
                    }
                    else if (bit && !strcmp(bit, "CPY_OP_AND")) {
                        if (l2 >= 0) { rl = 0; rh = h2; }
                        else if (l1 >= 0) { rl = 0; rh = h1; }
                    }
                    else if (bit && (!strcmp(bit, "CPY_OP_OR") || !strcmp(bit, "CPY_OP_XOR"))) {
                        if (l1 >= 0 && l2 >= 0 && h1 != IV_MAX && h2 != IV_MAX) {
                            uint64_t m = (uint64_t)(h1 | h2);
                            uint64_t p = 1;
                            while (p <= m && p < ((uint64_t)1 << 62)) p <<= 1;
                            rl = 0;
                            rh = (int64_t)(p - 1);
                        }
                    }
                    else if (bit && !strcmp(bit, "CPY_OP_SHR")) {
                        if (l2 >= 0 && h2 <= 62 && l2 == h2) { rl = l1 >> l2; rh = h1 >> l2; }
                    }
                }
                clo[t->nlocals + d - 2] = rl;
                chi[t->nlocals + d - 2] = rh;
                break;
            }
            case NCO_CALL: {
                int base = call_base(c, d, k->arg);
                if (t->math_at[i] >= 0) {
                    ty_math *m = &t->mth[t->math_at[i]];
                    if (m->kind == TY_K_LEN && ty_slot_ty(t, i, base + 2) == TY_STR) {
                        clo[t->nlocals + base] = 0;
                        chi[t->nlocals + base] = 63;
                        break;
                    }
                    if (m->kind == TY_K_LEN) {
                        clo[t->nlocals + base] = 0;
                        chi[t->nlocals + base] = (int64_t)1 << 47;
                        break;
                    }
                    if (m->kind == TY_K_ORD) {
                        clo[t->nlocals + base] = 0;
                        chi[t->nlocals + base] = 127;
                        break;
                    }
                    if ((m->kind == TY_K_MIN || m->kind == TY_K_MAX) &&
                        ty_slot_ty(t, i, base + 2) == TY_INT &&
                        ty_slot_ty(t, i, base + 3) == TY_INT) {
                        int64_t l1 = clo[t->nlocals + base + 2];
                        int64_t h1 = chi[t->nlocals + base + 2];
                        int64_t l2 = clo[t->nlocals + base + 3];
                        int64_t h2 = chi[t->nlocals + base + 3];
                        if (l1 <= h1 && l2 <= h2) {
                            if (m->kind == TY_K_MIN) {
                                clo[t->nlocals + base] = l1 < l2 ? l1 : l2;
                                chi[t->nlocals + base] = h1 < h2 ? h1 : h2;
                            } else {
                                clo[t->nlocals + base] = l1 > l2 ? l1 : l2;
                                chi[t->nlocals + base] = h1 > h2 ? h1 : h2;
                            }
                        } else {
                            clo[t->nlocals + base] = IV_MIN;
                            chi[t->nlocals + base] = IV_MAX;
                        }
                        break;
                    }
                }
                clo[t->nlocals + base] = IV_MIN;
                chi[t->nlocals + base] = IV_MAX;
                break;
            }
            case NCO_FOR_ITER: {
                int r2 = t->rng_at[i];
                int64_t vlo = IV_MIN, vhi = IV_MAX;
                if (r2 >= 0) {
                    ty_rng *rg = &t->rng[r2];
                    int64_t alo[3], ahi[3];
                    int q2, okargs = 1;
                    for (q2 = 0; q2 < rg->nargs; q2++) {
                        ninst *ar = &t->ins[rg->argi[q2]];
                        if (ar->opc == NCO_LOAD_CONST) {
                            long long v = PY->PyLong_AsLongLong(ar->argval);
                            if (v == -1 && PY->PyErr_Occurred()) {
                                PY->PyErr_Clear();
                                okargs = 0;
                                break;
                            }
                            alo[q2] = v;
                            ahi[q2] = v;
                        } else {
                            int64_t *rl2 = t->tlo + (size_t)rg->argi[q2] * W;
                            int64_t *rh2 = t->thi + (size_t)rg->argi[q2] * W;
                            alo[q2] = rl2[ar->arg];
                            ahi[q2] = rh2[ar->arg];
                            if (alo[q2] > ahi[q2]) {
                                alo[q2] = IV_MIN;
                                ahi[q2] = IV_MAX;
                            }
                        }
                    }
                    if (okargs) {
                        if (rg->nargs == 1) {
                            vlo = 0;
                            vhi = ahi[0] == IV_MAX ? IV_MAX : ahi[0] - 1;
                            if (vhi < 0) vhi = 0;
                        } else {
                            int64_t slo = 1, shi = 1;
                            if (rg->nargs == 3) { slo = alo[2]; shi = ahi[2]; }
                            if (slo >= 1) {
                                vlo = alo[0];
                                vhi = ahi[1] == IV_MAX ? IV_MAX : ahi[1] - 1;
                                if (vlo != IV_MIN && vhi < vlo) vhi = vlo;
                            } else if (shi <= -1 && shi != IV_MIN) {
                                vhi = ahi[0];
                                vlo = alo[1] == IV_MIN ? IV_MIN : alo[1] + 1;
                                if (vhi != IV_MAX && vlo > vhi) vlo = vhi;
                            }
                        }
                    }
                }
                clo[t->nlocals + d] = vlo;
                chi[t->nlocals + d] = vhi;
                break;
            }
            default:
                if (k->eff > 0) {
                    for (q = 0; q < k->eff; q++) {
                        clo[t->nlocals + d + q] = IV_MIN;
                        chi[t->nlocals + d + q] = IV_MAX;
                    }
                } else if (k->eff < 0) {
                    clo[t->nlocals + d + k->eff] = IV_MIN;
                    chi[t->nlocals + d + k->eff] = IV_MAX;
                }
                break;
            }

            if (k->jump && k->target >= 0) {
                int tj;
                int bx = t->brf ? t->brf[i] : -1;
                int64_t sva0 = 0, sva1 = 0, svb0 = 0, svb1 = 0;
                int saved = 0;
                if (bx >= 0) {
                    ty_brfent *bf = &t->brfs[bx];
                    if (!bf->aconst) { sva0 = clo[bf->la]; sva1 = chi[bf->la]; }
                    if (!bf->bconst) { svb0 = clo[bf->lb]; svb1 = chi[bf->lb]; }
                    ty_brf_apply(t, bx,
                                 k->opc == NCO_POP_JUMP_IF_TRUE ? 1 : 0, clo, chi);
                    saved = 1;
                }
                for (tj = 0; tj < t->n; tj++) {
                    if (t->ins[tj].off != k->target) continue;
                    {
                        int64_t *ts = t->tlo + (size_t)tj * W;
                        int64_t *th = t->thi + (size_t)tj * W;
                        for (q = 0; q < W; q++) {
                            int64_t jl, jh;
                            if (clo[q] > chi[q]) { jl = ts[q]; jh = th[q]; }
                            else if (ts[q] > th[q]) { jl = clo[q]; jh = chi[q]; }
                            else {
                                jl = ts[q] < clo[q] ? ts[q] : clo[q];
                                jh = th[q] > chi[q] ? th[q] : chi[q];
                                if (pass >= 3) {
                                    if (jl != ts[q]) jl = IV_MIN;
                                    if (jh != th[q]) jh = IV_MAX;
                                }
                            }
                            if (jl != ts[q] || jh != th[q]) {
                                ts[q] = jl;
                                th[q] = jh;
                                changed = 1;
                            }
                        }
                    }
                    break;
                }
                if (saved) {
                    ty_brfent *bf = &t->brfs[bx];
                    if (!bf->aconst) { clo[bf->la] = sva0; chi[bf->la] = sva1; }
                    if (!bf->bconst) { clo[bf->lb] = svb0; chi[bf->lb] = svb1; }
                    ty_brf_apply(t, bx,
                                 k->opc == NCO_POP_JUMP_IF_TRUE ? 0 : 1, clo, chi);
                }
            }
            if (k->opc == NCO_JUMP || k->opc == NCO_RETURN_VALUE || k->opc == NCO_RETURN_CONST) {
                for (q = 0; q < W; q++) { clo[q] = IV_MAX; chi[q] = IV_MIN; }
            }
        }
        if (!changed) break;
    }
    free(clo);
    free(chi);
}

static int ty_iv_elide(tinf *t, int i, int slot1, int slot2, int kind)
{
    int64_t *rlo = t->tlo + (size_t)i * t->width;
    int64_t *rhi = t->thi + (size_t)i * t->width;
    int64_t l1 = rlo[t->nlocals + slot1], h1 = rhi[t->nlocals + slot1];
    int64_t l2 = rlo[t->nlocals + slot2], h2 = rhi[t->nlocals + slot2];
    int64_t rl, rh;
    if (l1 > h1 || l2 > h2) return 0;
    if (!iv_bounded(l1, h1) || !iv_bounded(l2, h2)) return 0;
    if (kind == 0) iv_add(l1, h1, l2, h2, &rl, &rh);
    else if (kind == 1) iv_sub(l1, h1, l2, h2, &rl, &rh);
    else iv_mul(l1, h1, l2, h2, &rl, &rh);
    return iv_bounded(rl, rh);
}

static int ty_supported(tinf *t, ninst *k, int idx)
{
    nc_ctx *c = t->c;
    switch (k->opc) {
    case NCO_NOP: case NCO_RESUME: case NCO_POP_TOP: case NCO_END_FOR:
    case NCO_LOAD_FAST: case NCO_STORE_FAST:
    case NCO_LOAD_FAST2: case NCO_STORE_LOAD_FAST: case NCO_STORE_FAST2:
    case NCO_TO_BOOL: case NCO_COPY: case NCO_SWAP:
    case NCO_FORMAT_VALUE: case NCO_FORMAT_SIMPLE: case NCO_BUILD_STRING:
    case NCO_BINARY_SUBSCR: case NCO_STORE_SUBSCR:
    case NCO_JUMP: case NCO_POP_JUMP_IF_FALSE: case NCO_POP_JUMP_IF_TRUE:
    case NCO_RETURN_VALUE: case NCO_COMPARE_OP:
    case NCO_UNARY_NEGATIVE: case NCO_UNARY_NOT:
        return 1;
    case NCO_LOAD_CONST:
        return ty_of_const(t, k->argval) != TY_BAD ||
               ty_str_const_ok(c, k->argval);
    case NCO_RETURN_CONST:
        return k->argval == c->v_none || ty_of_const(t, k->argval) != TY_BAD;
    case NCO_BINARY_OP: {
        int ip = 0;
        const char *bit = NULL, *sy, *fn;
        if (!k->argrepr) return 0;
        sy = PY->PyUnicode_AsUTF8(k->argrepr);
        if (!sy) return 0;
        fn = binop_call(sy, &ip, &bit);
        return fn && strcmp(fn, "cv_pow");
    }
    case NCO_FOR_ITER:
        return t->rng_at[idx] >= 0;
    case NCO_CALL:
        return t->selfcall[idx] || t->skip[idx] || t->math_at[idx] >= 0 ||
               t->sib[idx] >= 0 || t->lol_app[idx] >= 0;
    default:
        return t->skip[idx];
    }
}

static int ty_rej(signed char *cur, int row, int line)
{
    free(cur);
    if (getenv("CPY_TY_DBG"))
        fprintf(stderr, "tyrej line %d row %d\n", line, row);
    return 0;
}

static int ty_infer(tinf *t, int paramty)
{
    nc_ctx *c = t->c;
    int pass, i, changed;
    signed char *cur = (signed char *)cpy_xmalloc((size_t)t->width);

    for (i = 0; i < t->n * t->width; i++) t->tin[i] = TY_UNSET;
    for (i = 0; i < t->argcount; i++)
        t->tin[i] = (signed char)((i < 14 && t->cand[i]) ? ty_arrty(t->ety) : paramty);
    t->rettype = TY_UNSET;

    for (i = 0; i < t->n; i++) {
        if (!ty_supported(t, &t->ins[i], i)) return ty_rej(cur, i, __LINE__);
        if (t->ins[i].depth + 1 >= t->stacksize) return ty_rej(cur, i, __LINE__);
    }

    for (pass = 0; pass < 24; pass++) {
        changed = 0;
        memcpy(cur, t->tin, (size_t)t->width);
        for (i = 0; i < t->n; i++) {
            ninst *k = &t->ins[i];
            signed char *st = t->tin + (size_t)i * t->width;
            int d = k->depth, q;

            for (q = 0; q < t->width; q++) {
                int j = ty_join(st[q], cur[q]);
                if (j != st[q]) { st[q] = (signed char)j; changed = 1; }
            }
            memcpy(cur, st, (size_t)t->width);

            if (t->skip[i]) continue;

            switch (k->opc) {
            case NCO_NOP: case NCO_RESUME: case NCO_END_FOR: case NCO_JUMP: break;
            case NCO_POP_TOP: break;
            case NCO_LOAD_FAST:
                if (cur[k->arg] == TY_UNSET || cur[k->arg] == TY_BAD) return ty_rej(cur, i, __LINE__);
                cur[t->nlocals + d] = cur[k->arg];
                break;
            case NCO_LOAD_FAST2: {
                int i1 = k->arg >> 4, i2 = k->arg & 15;
                if (cur[i1] == TY_UNSET || cur[i1] == TY_BAD) return ty_rej(cur, i, __LINE__);
                if (cur[i2] == TY_UNSET || cur[i2] == TY_BAD) return ty_rej(cur, i, __LINE__);
                cur[t->nlocals + d] = cur[i1];
                cur[t->nlocals + d + 1] = cur[i2];
                break;
            }
            case NCO_STORE_LOAD_FAST: {
                int i1 = k->arg >> 4, i2 = k->arg & 15;
                if (ty_is_arr(cur[t->nlocals + d - 1])) return ty_rej(cur, i, __LINE__);
                cur[i1] = cur[t->nlocals + d - 1];
                if (cur[i1] == TY_BAD || cur[i1] == TY_UNSET) return ty_rej(cur, i, __LINE__);
                if (cur[i2] == TY_UNSET || cur[i2] == TY_BAD) return ty_rej(cur, i, __LINE__);
                cur[t->nlocals + d - 1] = cur[i2];
                break;
            }
            case NCO_STORE_FAST2: {
                int i1 = k->arg >> 4, i2 = k->arg & 15;
                if (ty_is_arr(cur[t->nlocals + d - 1]) ||
                    ty_is_arr(cur[t->nlocals + d - 2])) return ty_rej(cur, i, __LINE__);
                cur[i1] = cur[t->nlocals + d - 1];
                cur[i2] = cur[t->nlocals + d - 2];
                if (cur[i1] == TY_BAD || cur[i1] == TY_UNSET) return ty_rej(cur, i, __LINE__);
                if (cur[i2] == TY_BAD || cur[i2] == TY_UNSET) return ty_rej(cur, i, __LINE__);
                break;
            }
            case NCO_TO_BOOL:
                if (!ty_num(cur[t->nlocals + d - 1])) return ty_rej(cur, i, __LINE__);
                cur[t->nlocals + d - 1] = TY_BOOL;
                break;
            case NCO_COPY: {
                int cs = t->nlocals + d - k->arg;
                if (k->arg < 1 || d - k->arg < 0) return ty_rej(cur, i, __LINE__);
                if (cur[cs] == TY_UNSET || cur[cs] == TY_BAD) return ty_rej(cur, i, __LINE__);
                cur[t->nlocals + d] = cur[cs];
                break;
            }
            case NCO_SWAP: {
                int w1 = t->nlocals + d - 1, w2 = t->nlocals + d - k->arg;
                signed char tw;
                if (k->arg < 2 || d - k->arg < 0) return ty_rej(cur, i, __LINE__);
                if (cur[w1] == TY_UNSET || cur[w1] == TY_BAD) return ty_rej(cur, i, __LINE__);
                if (cur[w2] == TY_UNSET || cur[w2] == TY_BAD) return ty_rej(cur, i, __LINE__);
                tw = cur[w1];
                cur[w1] = cur[w2];
                cur[w2] = tw;
                break;
            }
            case NCO_STORE_FAST:
                if (cur[t->nlocals + d - 1] == TY_CHR ||
                    cur[t->nlocals + d - 1] == TY_LOL) return ty_rej(cur, i, __LINE__);
                if (t->lol_create[i] >= 0) {
                    if (cur[k->arg] != TY_UNSET && cur[k->arg] != TY_LOL) return ty_rej(cur, i, __LINE__);
                    cur[k->arg] = TY_LOL;
                    break;
                }
                if (t->map_create[i] >= 0) {
                    if (cur[k->arg] != TY_UNSET && cur[k->arg] != TY_MAPI) return ty_rej(cur, i, __LINE__);
                    cur[k->arg] = TY_MAPI;
                    break;
                }
                if (cur[t->nlocals + d - 1] == TY_MAPI) return ty_rej(cur, i, __LINE__);
                if (t->arr_create[i] >= 0) {
                    ty_acr *ac = &t->acr[t->arr_create[i]];
                    int at = ty_arrty(ac->elem);
                    if (t->ins[ac->len_i].opc == NCO_LOAD_FAST &&
                        cur[t->ins[ac->len_i].arg] != TY_INT) return ty_rej(cur, i, __LINE__);
                    if (cur[k->arg] != TY_UNSET && cur[k->arg] != at) return ty_rej(cur, i, __LINE__);
                    cur[k->arg] = (signed char)at;
                    break;
                }
                if (ty_is_arr(cur[t->nlocals + d - 1])) {
                    int at3 = cur[t->nlocals + d - 1];
                    if (!t->ralias[i]) return ty_rej(cur, i, __LINE__);
                    if (cur[k->arg] != TY_UNSET && cur[k->arg] != at3) return ty_rej(cur, i, __LINE__);
                    cur[k->arg] = (signed char)at3;
                    break;
                }
                cur[k->arg] = cur[t->nlocals + d - 1];
                if (cur[k->arg] == TY_BAD || cur[k->arg] == TY_UNSET) return ty_rej(cur, i, __LINE__);
                break;
            case NCO_BINARY_SUBSCR: {
                int at = cur[t->nlocals + d - 2];
                if (cur[t->nlocals + d - 1] != TY_INT) return ty_rej(cur, i, __LINE__);
                if (at == TY_LOL) {
                    if (t->lol_sub[i] < 0) return ty_rej(cur, i, __LINE__);
                    cur[t->nlocals + d - 2] =
                        (signed char)ty_arrty(t->lol_elem[t->lol_sub[i]]);
                    break;
                }
                if (at == TY_MAPI) {
                    cur[t->nlocals + d - 2] = TY_INT;
                    break;
                }
                if (at == TY_STR) {
                    cur[t->nlocals + d - 2] = TY_CHR;
                    break;
                }
                if (!ty_is_arr(at)) return ty_rej(cur, i, __LINE__);
                cur[t->nlocals + d - 2] = (signed char)ty_elem(at);
                break;
            }
            case NCO_STORE_SUBSCR: {
                int at = cur[t->nlocals + d - 2];
                if (cur[t->nlocals + d - 1] != TY_INT) return ty_rej(cur, i, __LINE__);
                if (at == TY_MAPI) {
                    if (cur[t->nlocals + d - 3] != TY_INT) return ty_rej(cur, i, __LINE__);
                    break;
                }
                if (!ty_is_arr(at)) return ty_rej(cur, i, __LINE__);
                if (cur[t->nlocals + d - 3] != ty_elem(at)) return ty_rej(cur, i, __LINE__);
                break;
            }
            case NCO_LOAD_CONST: {
                int ct = ty_of_const(t, k->argval);
                if (ct == TY_BAD && ty_str_const_ok(c, k->argval)) ct = TY_STR;
                cur[t->nlocals + d] = (signed char)ct;
                break;
            }
            case NCO_FORMAT_VALUE:
            case NCO_FORMAT_SIMPLE: {
                if (k->opc == NCO_FORMAT_VALUE && k->arg != 0) return ty_rej(cur, i, __LINE__);
                if (cur[t->nlocals + d - 1] != TY_INT) return ty_rej(cur, i, __LINE__);
                cur[t->nlocals + d - 1] = TY_STR;
                break;
            }
            case NCO_BUILD_STRING: {
                int q2;
                if (k->arg < 1) return ty_rej(cur, i, __LINE__);
                for (q2 = 0; q2 < k->arg; q2++)
                    if (cur[t->nlocals + d - k->arg + q2] != TY_STR) return ty_rej(cur, i, __LINE__);
                cur[t->nlocals + d - k->arg] = TY_STR;
                break;
            }
            case NCO_BINARY_OP: {
                int ip = 0;
                const char *sy = PY->PyUnicode_AsUTF8(k->argrepr);
                int r = ty_binop(sy, cur[t->nlocals + d - 2], cur[t->nlocals + d - 1], &ip);
                if (r == TY_BAD) return ty_rej(cur, i, __LINE__);
                cur[t->nlocals + d - 2] = (signed char)r;
                break;
            }
            case NCO_COMPARE_OP:
                if (!ty_num(cur[t->nlocals + d - 2]) || !ty_num(cur[t->nlocals + d - 1])) return ty_rej(cur, i, __LINE__);
                cur[t->nlocals + d - 2] = TY_BOOL;
                break;
            case NCO_UNARY_NEGATIVE:
                if (!ty_num(cur[t->nlocals + d - 1])) return ty_rej(cur, i, __LINE__);
                if (cur[t->nlocals + d - 1] == TY_BOOL) cur[t->nlocals + d - 1] = TY_INT;
                break;
            case NCO_UNARY_NOT:
                if (!ty_num(cur[t->nlocals + d - 1])) return ty_rej(cur, i, __LINE__);
                cur[t->nlocals + d - 1] = TY_BOOL;
                break;
            case NCO_FOR_ITER:
                cur[t->nlocals + d] = TY_INT;
                break;
            case NCO_CALL: {
                int base = call_base(c, d, k->arg), q2;
                if (t->lol_app[i] >= 0) {
                    if (cur[t->lol_app[i]] != TY_LOL) return ty_rej(cur, i, __LINE__);
                    if (!ty_is_arr(cur[t->lol_app_row[i]])) return ty_rej(cur, i, __LINE__);
                    break;
                }
                if (t->sib[i] >= 0 && t->math_at[i] < 0) {
                    for (q2 = 0; q2 < k->arg; q2++)
                        if (cur[t->nlocals + base + 2 + q2] !=
                            c->tyreg[t->sib[i]].par[q2]) return ty_rej(cur, i, __LINE__);
                    cur[t->nlocals + base] = c->tyreg[t->sib[i]].ret;
                    break;
                }
                if (t->math_at[i] >= 0) {
                    ty_math *m = &t->mth[t->math_at[i]];
                    int aty = TY_UNSET;
                    if (m->kind == TY_K_LEN) {
                        int lt = cur[t->nlocals + base + 2];
                        if (!ty_is_arr(lt) && lt != TY_STR && lt != TY_MAPI &&
                            lt != TY_LOL) return ty_rej(cur, i, __LINE__);
                        cur[t->nlocals + base] = TY_INT;
                        break;
                    }
                    if (m->kind == TY_K_ORD) {
                        if (cur[t->nlocals + base + 2] != TY_CHR) return ty_rej(cur, i, __LINE__);
                        cur[t->nlocals + base] = TY_INT;
                        break;
                    }
                    if (m->kind == TY_K_MIN || m->kind == TY_K_MAX) {
                        int a1 = cur[t->nlocals + base + 2];
                        int a2 = cur[t->nlocals + base + 3];
                        if (!((a1 == TY_INT && a2 == TY_INT) ||
                              (a1 == TY_FLT && a2 == TY_FLT)))
                            return ty_rej(cur, i, __LINE__);
                        cur[t->nlocals + base] = (signed char)a1;
                        break;
                    }
                    for (q2 = 0; q2 < m->nargs; q2++) {
                        int at = cur[t->nlocals + base + 2 + q2];
                        if (!ty_num(at)) return ty_rej(cur, i, __LINE__);
                        if (q2 == 0) aty = at == TY_BOOL ? TY_INT : at;
                    }
                    if (m->kind == TY_K_MATH || m->kind == TY_K_FLOAT)
                        cur[t->nlocals + base] = TY_FLT;
                    else if (m->kind == TY_K_INT)
                        cur[t->nlocals + base] = TY_INT;
                    else
                        cur[t->nlocals + base] = (signed char)aty;
                    break;
                }
                for (q2 = 0; q2 < k->arg; q2++)
                    if (cur[t->nlocals + base + 2 + q2] != t->tin[q2]) return ty_rej(cur, i, __LINE__);
                cur[t->nlocals + base] = (signed char)t->retguess;
                break;
            }
            case NCO_POP_JUMP_IF_FALSE: case NCO_POP_JUMP_IF_TRUE:
                if (!ty_num(cur[t->nlocals + d - 1])) return ty_rej(cur, i, __LINE__);
                break;
            case NCO_RETURN_VALUE:
                if (ty_is_arr(cur[t->nlocals + d - 1]) ||
                    cur[t->nlocals + d - 1] == TY_STR ||
                    cur[t->nlocals + d - 1] == TY_CHR ||
                    cur[t->nlocals + d - 1] == TY_MAPI ||
                    cur[t->nlocals + d - 1] == TY_LOL) return ty_rej(cur, i, __LINE__);
                t->rettype = ty_join(t->rettype, cur[t->nlocals + d - 1]);
                if (t->rettype == TY_BAD) return ty_rej(cur, i, __LINE__);
                break;
            case NCO_RETURN_CONST:
                if (k->argval == c->v_none)
                    t->rettype = ty_join(t->rettype, TY_NONE);
                else
                    t->rettype = ty_join(t->rettype, ty_of_const(t, k->argval));
                if (t->rettype == TY_BAD) return ty_rej(cur, i, __LINE__);
                break;
            default:
                free(cur);
                return 0;
            }

            if (k->jump && k->target >= 0) {
                int tj;
                for (tj = 0; tj < t->n; tj++) {
                    if (t->ins[tj].off != k->target) continue;
                    {
                        signed char *ts = t->tin + (size_t)tj * t->width;
                        for (q = 0; q < t->width; q++) {
                            int j2 = ty_join(ts[q], cur[q]);
                            if (j2 != ts[q]) { ts[q] = (signed char)j2; changed = 1; }
                        }
                    }
                    break;
                }
            }
            if (k->opc == NCO_JUMP || k->opc == NCO_RETURN_VALUE || k->opc == NCO_RETURN_CONST)
                memset(cur, TY_UNSET, (size_t)t->width);
        }
        if (!changed) break;
    }
    free(cur);

    for (i = 0; i < t->nlocals; i++) t->ltype[i] = TY_UNSET;
    for (i = 0; i < t->n; i++) {
        signed char *st = t->tin + (size_t)i * t->width;
        int q;
        for (q = 0; q < t->nlocals; q++) {
            int j = ty_join(t->ltype[q], st[q]);
            if (j == TY_BAD) return 0;
            t->ltype[q] = (signed char)j;
        }
    }
    for (i = 0; i < t->argcount; i++)
        if (t->ltype[i] != t->tin[i]) return 0;
    if (t->rettype == TY_UNSET || t->rettype == TY_BAD) return 0;
    return 1;
}

static const char *ty_cdecl(int t)
{
    if (t == TY_NONE) return "int64_t";
    if (t == TY_FLT) return "double";
    if (t == TY_BOOL) return "int";
    return "int64_t";
}

static const char *ty_slot(char *buf, size_t n, tinf *t, int i, int slot)
{
    signed char *st = t->tin + (size_t)i * t->width;
    int ty = st[t->nlocals + slot];
    const char *p = ty == TY_FLT ? "sd" : (ty == TY_BOOL ? "sb" : "si");
    _snprintf(buf, n - 1, "%s%d", p, slot);
    buf[n - 1] = 0;
    return buf;
}

static int ty_slot_ty(tinf *t, int i, int slot)
{
    signed char *st = t->tin + (size_t)i * t->width;
    return st[t->nlocals + slot];
}

static void ty_as_dbl(buf *b, const char *e, int ty)
{
    if (ty == TY_FLT) bpf(b, "%s", e);
    else bpf(b, "(double)%s", e);
}

static const char *ty_operand(char *buf, size_t n, tinf *t, ninst *k, int *ty)
{
    if (k->opc == NCO_LOAD_FAST) {
        _snprintf(buf, n - 1, "v%d", k->arg);
        buf[n - 1] = 0;
        *ty = t->ltype[k->arg];
        return buf;
    }
    if (k->opc == NCO_LOAD_CONST || k->opc == NCO_RETURN_CONST) {
        nc_ctx *c = t->c;
        int cty = ty_of_const(t, k->argval);
        if (cty == TY_BAD) return NULL;
        *ty = cty;
        if (cty == TY_INT) {
            long long v = PY->PyLong_AsLongLong(k->argval);
            _snprintf(buf, n - 1, "%lldLL", v);
        } else {
            double d = PY->PyFloat_AsDouble(k->argval);
            _snprintf(buf, n - 1, "%.17g", d);
        }
        buf[n - 1] = 0;
        return buf;
    }
    return NULL;
}

static void ty_load_local(tinf *t, buf *b, int inext, int slot, int local)
{
    char nb[64];
    int lt = t->ltype[local];
    if (ty_is_arr(lt)) {
        bpf(b, "  sa%d = (void*)v%d_p; sl%d = v%d_n;\n", slot, local, slot, local);
    } else if (lt == TY_STR) {
        bpf(b, "  memcpy(sc%d, vc%d, 64); scn%d = vcn%d;\n", slot, local, slot, local);
    } else if (lt == TY_MAPI) {
        bpf(b, "  sm%d = vm%d;\n", slot, local);
    } else if (lt == TY_LOL) {
        bpf(b, "  sll%d = vl%d;\n", slot, local);
    } else {
        bpf(b, "  %s = v%d;\n", ty_slot(nb, sizeof(nb), t, inext, slot), local);
    }
}

static void ty_store_local(tinf *t, buf *b, int i, int slot, int local)
{
    char nb[64];
    int st = ty_slot_ty(t, i, slot);
    if (ty_is_arr(st)) {
        bpf(b, "  v%d_p = (%s)sa%d; v%d_n = sl%d;\n",
            local, st == TY_ARRI ? "int64_t*" : "double*", slot, local, slot);
    } else if (st == TY_STR) {
        bpf(b, "  memcpy(vc%d, sc%d, 64); vcn%d = scn%d;\n", local, slot, local, slot);
    } else {
        bpf(b, "  v%d = %s;\n", local, ty_slot(nb, sizeof(nb), t, i, slot));
    }
}

static long long ty_klit(const char *k)
{
    char *end;
    long long v;
    if (!k) return -1;
    v = _strtoi64(k, &end, 10);
    if (end == k) return -1;
    if (strcmp(end, "LL")) return -1;
    return v;
}

static int ty_pow2(long long v)
{
    if (v <= 0) return -1;
    if (v & (v - 1)) return -1;
    {
        int k = 0;
        while (v > 1) { v >>= 1; k++; }
        return k;
    }
}

static int ty_stores_loc(ninst *k, int loc)
{
    switch (k->opc) {
    case NCO_STORE_FAST:
    case NCO_DELETE_FAST:
        return k->arg == loc;
    case NCO_STORE_FAST2:
        return (k->arg >> 4) == loc || (k->arg & 15) == loc;
    case NCO_STORE_LOAD_FAST:
        return (k->arg >> 4) == loc;
    default:
        return 0;
    }
}

static int ty_jump_target(ninst *k)
{
    switch (k->opc) {
    case NCO_JUMP:
    case NCO_POP_JUMP_IF_FALSE:
    case NCO_POP_JUMP_IF_TRUE:
    case NCO_POP_JUMP_IF_NONE:
    case NCO_POP_JUMP_IF_NOT_NONE:
    case NCO_FOR_ITER:
        return k->target;
    default:
        return -1;
    }
}

static int ty_row_of(tinf *t, int off)
{
    int i;
    for (i = 0; i < t->n; i++)
        if (t->ins[i].off == off) return i;
    return -1;
}

static int ty_loc_ty(tinf *t, int loc)
{
    if (loc < t->argcount) return t->tin[loc];
    return t->ltype[loc];
}

static void ty_scan_cwant(tinf *t)
{
    int i;
    memset(t->cidx, 0, sizeof(t->cidx));
    memset(t->ccmp, 0, sizeof(t->ccmp));
    for (i = 0; i < t->n; i++) {
        ninst *k = &t->ins[i];
        int nx = (i + 1 < t->n) ? t->ins[i + 1].opc : NCO_NOP;
        if (k->opc == NCO_LOAD_FAST2 &&
            (nx == NCO_BINARY_SUBSCR || nx == NCO_STORE_SUBSCR)) {
            int p = k->arg & 15;
            if (p < t->argcount && p < 14) t->cidx[p] = 1;
        }
        if (k->opc == NCO_LOAD_FAST &&
            (nx == NCO_BINARY_SUBSCR || nx == NCO_STORE_SUBSCR)) {
            if (k->arg < t->argcount && k->arg < 14) t->cidx[k->arg] = 1;
        }
        if (k->opc == NCO_LOAD_FAST2 && nx == NCO_COMPARE_OP) {
            int p1 = k->arg >> 4, p2 = k->arg & 15;
            if (p1 < t->argcount && p1 < 14) t->ccmp[p1] = 1;
            if (p2 < t->argcount && p2 < 14) t->ccmp[p2] = 1;
        }
        if (k->opc == NCO_LOAD_FAST &&
            (nx == NCO_COMPARE_OP ||
             (nx == NCO_LOAD_FAST && i + 2 < t->n &&
              t->ins[i + 2].opc == NCO_COMPARE_OP))) {
            if (k->arg < t->argcount && k->arg < 14) t->ccmp[k->arg] = 1;
            if (nx == NCO_LOAD_FAST && t->ins[i + 1].arg < t->argcount &&
                t->ins[i + 1].arg < 14) t->ccmp[t->ins[i + 1].arg] = 1;
        }
        if (k->opc == NCO_LOAD_FAST && nx == NCO_LOAD_CONST &&
            i + 2 < t->n && t->ins[i + 2].opc == NCO_COMPARE_OP) {
            if (k->arg < t->argcount && k->arg < 14) t->ccmp[k->arg] = 1;
        }
    }
    for (i = 0; i < 14; i++)
        if (t->cidx[i]) t->ccmp[i] = 0;
    if (getenv("CPY_NOCCMP")) memset(t->ccmp, 0, sizeof(t->ccmp));
}

static void ty_scan_ralias(tinf *t)
{
    int i, firstal = -1;
    for (i = 0; i < t->n; i++) t->ralias[i] = 0;
    if (!t->nlol) return;
    for (i = 1; i < t->n; i++) {
        if (t->ins[i].opc != NCO_STORE_FAST || t->ins[i].label) continue;
        if (t->skip[i] || t->skip[i - 1]) continue;
        if (t->lol_create[i] >= 0 || t->arr_create[i] >= 0 ||
            t->map_create[i] >= 0) continue;
        if (t->ins[i - 1].opc != NCO_BINARY_SUBSCR || t->lol_sub[i - 1] < 0)
            continue;
        t->ralias[i] = 1;
        if (firstal < 0) firstal = i;
    }
    if (firstal < 0) return;
    for (i = 0; i < t->n; i++) {
        int q;
        if (t->lol_create[i] < 0) continue;
        if (i > firstal) goto killall;
        for (q = 0; q < t->n; q++) {
            int tg2 = ty_jump_target(&t->ins[q]);
            if (tg2 < 0) continue;
            if (ty_row_of(t, tg2) <= i && q >= i) goto killall;
        }
    }
    return;
killall:
    for (i = 0; i < t->n; i++) t->ralias[i] = 0;
}

static void ty_scan_guard(tinf *t)
{
    nc_ctx *c = t->c;
    int h;
    int dbg = getenv("CPY_BND_DBG") != NULL;
    if (t->wrap) return;
    for (h = 0; h < t->n; h++) {
        int I, N, cmprow, pjrow, xrow, r, q, s, ok, tg, inv, rs0, fence;
        ninst *k = &t->ins[h];
        inv = -1;
        if (k->opc == NCO_LOAD_FAST2 && h + 2 < t->n &&
            !t->ins[h + 1].label && !t->ins[h + 2].label &&
            t->ins[h + 1].opc == NCO_COMPARE_OP) {
            I = k->arg >> 4;
            N = k->arg & 15;
            cmprow = h + 1;
            pjrow = h + 2;
        } else if (k->opc == NCO_LOAD_FAST && h + 3 < t->n &&
                   !t->ins[h + 1].label && !t->ins[h + 2].label &&
                   !t->ins[h + 3].label &&
                   t->ins[h + 1].opc == NCO_LOAD_FAST &&
                   t->ins[h + 2].opc == NCO_COMPARE_OP) {
            I = k->arg;
            N = t->ins[h + 1].arg;
            cmprow = h + 2;
            pjrow = h + 3;
        } else if (k->opc == NCO_FOR_ITER && t->rng_at[h] >= 0 &&
                   h > 0 && h + 1 < t->n &&
                   t->ins[h + 1].opc == NCO_STORE_FAST &&
                   !t->ins[h + 1].label && !t->skip[h + 1]) {
            ty_rng *rg2 = &t->rng[t->rng_at[h]];
            int stepok = rg2->nargs <= 2;
            if (rg2->nargs == 3) {
                ninst *sa2 = &t->ins[rg2->argi[2]];
                if (sa2->opc == NCO_LOAD_CONST &&
                    ty_of_const(t, sa2->argval) == TY_INT) {
                    long long sv = PY->PyLong_AsLongLong(sa2->argval);
                    if (sv == -1 && PY->PyErr_Occurred()) PY->PyErr_Clear();
                    else if (sv > 0) stepok = 1;
                }
            }
            if (!stepok) continue;
            I = t->ins[h + 1].arg;
            if (I >= t->nlocals) continue;
            N = -1;
            cmprow = -1;
            pjrow = h;
            inv = 2;
        } else continue;
        if (inv != 2) {
            if (t->ins[pjrow].opc == NCO_TO_BOOL && pjrow + 1 < t->n &&
                !t->ins[pjrow + 1].label)
                pjrow++;
            if (I == N || I >= t->nlocals || N >= t->nlocals) continue;
            if (ty_loc_ty(t, I) != TY_INT || ty_loc_ty(t, N) != TY_INT) continue;
            {
                const char *sy = t->ins[cmprow].argval ?
                    PY->PyUnicode_AsUTF8(t->ins[cmprow].argval) : NULL;
                const char *cn = sy ? cmp_name(sy) : NULL;
                int pj = t->ins[pjrow].opc;
                if (!cn) continue;
                if ((pj == NCO_POP_JUMP_IF_FALSE && !strcmp(cn, "Py_LT")) ||
                    (pj == NCO_POP_JUMP_IF_TRUE && !strcmp(cn, "Py_GE")))
                    inv = 0;
                else if ((pj == NCO_POP_JUMP_IF_FALSE && !strcmp(cn, "Py_GE")) ||
                         (pj == NCO_POP_JUMP_IF_TRUE && !strcmp(cn, "Py_LT")))
                    inv = 1;
                else
                    continue;
            }
        }
        if (dbg) fprintf(stderr, "bnd: h=%d I=%d N=%d cmp=%d pj=%d inv=%d\n", h, I, N, cmprow, pjrow, inv);
        tg = ty_jump_target(&t->ins[pjrow]);
        if (inv == 0) {
            xrow = ty_row_of(t, tg);
            if (xrow <= pjrow) continue;
            rs0 = pjrow + 1;
        } else if (inv == 1) {
            int L0 = ty_row_of(t, tg), bq;
            if (L0 <= pjrow) continue;
            for (bq = L0; bq < t->n; bq++)
                if (t->ins[bq].opc == NCO_JUMP &&
                    ty_row_of(t, t->ins[bq].target) <= h) break;
            if (bq >= t->n) continue;
            rs0 = L0;
            xrow = bq + 1;
        } else {
            xrow = ty_row_of(t, tg);
            rs0 = h + 2;
            if (xrow <= rs0) continue;
        }
        fence = rs0 - 1;
        if (dbg) fprintf(stderr, "bnd: rs0=%d xrow=%d\n", rs0, xrow);
        ok = 1;
        for (r = rs0; r < xrow && ok; r++) {
            if (!t->ins[r].label) continue;
            for (q = 0; q < t->n; q++) {
                int tg2 = ty_jump_target(&t->ins[q]);
                if (tg2 == t->ins[r].off && (q < h || q >= xrow)) { ok = 0; break; }
            }
        }
        if (k->label) {
            for (q = 0; q < t->n && ok; q++) {
                int tg2 = ty_jump_target(&t->ins[q]);
                if (tg2 == k->off && (q < h || q >= xrow)) ok = 0;
            }
        }
        for (r = rs0; r < xrow && ok; r++)
            if (ty_stores_loc(&t->ins[r], N)) ok = 0;
        if (dbg) fprintf(stderr, "bnd: region ok=%d\n", ok);
        if (!ok) continue;
        for (s = rs0; s < xrow; s++) {
            ninst *ks = &t->ins[s];
            int A, idx, prevrow, at2, gq, have;
            int64_t *xlo, *xhi;
            if (ks->opc != NCO_BINARY_SUBSCR && ks->opc != NCO_STORE_SUBSCR)
                continue;
            if (t->skip[s] || ks->label) continue;
            if (s >= 1 && t->ins[s - 1].opc == NCO_LOAD_FAST2 &&
                !t->ins[s - 1].label && !t->skip[s - 1]) {
                A = t->ins[s - 1].arg >> 4;
                idx = t->ins[s - 1].arg & 15;
                prevrow = s - 1;
            } else if (s >= 2 && t->ins[s - 1].opc == NCO_LOAD_FAST &&
                       !t->ins[s - 1].label && !t->skip[s - 1] &&
                       t->ins[s - 2].opc == NCO_LOAD_FAST &&
                       !t->ins[s - 2].label && !t->skip[s - 2]) {
                A = t->ins[s - 2].arg;
                idx = t->ins[s - 1].arg;
                prevrow = s - 2;
            } else continue;
            if (dbg) fprintf(stderr, "bnd: s=%d A=%d idx=%d\n", s, A, idx);
            if (idx != I || A == I || A == N || prevrow < rs0) continue;
            at2 = ty_loc_ty(t, A);
            if (dbg) fprintf(stderr, "bnd: s=%d at2=%d\n", s, at2);
            if (at2 != TY_ARRI && at2 != TY_ARRF &&
                !(at2 == TY_LOL && ks->opc == NCO_BINARY_SUBSCR &&
                  t->lol_sub[s] >= 0))
                continue;
            ok = 1;
            for (r = rs0; r <= s && ok; r++)
                if (ty_stores_loc(&t->ins[r], I)) ok = 0;
            for (r = rs0; r < xrow && ok; r++) {
                if (ty_stores_loc(&t->ins[r], A)) ok = 0;
                if (t->lol_app[r] == A) ok = 0;
            }
            for (q = rs0; q < xrow && ok; q++) {
                int tg2 = ty_jump_target(&t->ins[q]);
                int L, recheck = 0, hi2;
                if (tg2 < 0) continue;
                L = ty_row_of(t, tg2);
                if (L < rs0 || L > q) continue;
                if (L > s || q < s) continue;
                {
                    int cr = -1, pj2 = 0;
                    if (t->ins[q].opc == NCO_POP_JUMP_IF_TRUE) {
                        cr = q - 1;
                        pj2 = NCO_POP_JUMP_IF_TRUE;
                    } else if (t->ins[q].opc == NCO_JUMP && !t->ins[q].label &&
                               q >= rs0 + 2 &&
                               (t->ins[q - 1].opc == NCO_POP_JUMP_IF_FALSE ||
                                t->ins[q - 1].opc == NCO_POP_JUMP_IF_TRUE)) {
                        cr = q - 2;
                        pj2 = t->ins[q - 1].opc;
                    }
                    if (cr > fence && t->ins[cr].opc == NCO_TO_BOOL) cr--;
                    if (cr > fence && t->ins[cr].opc == NCO_COMPARE_OP) {
                        const char *sy2 = t->ins[cr].argval ?
                            PY->PyUnicode_AsUTF8(t->ins[cr].argval) : NULL;
                        const char *cn2 = sy2 ? cmp_name(sy2) : NULL;
                        int dirok = cn2 &&
                            ((pj2 == NCO_POP_JUMP_IF_TRUE && t->ins[q].opc != NCO_JUMP &&
                              !strcmp(cn2, "Py_LT")) ||
                             (t->ins[q].opc == NCO_JUMP &&
                              ((pj2 == NCO_POP_JUMP_IF_FALSE && !strcmp(cn2, "Py_LT")) ||
                               (pj2 == NCO_POP_JUMP_IF_TRUE && !strcmp(cn2, "Py_GE")))));
                        {
                            int lr;
                            for (lr = cr; lr < q && dirok; lr++)
                                if (t->ins[lr].label) dirok = 0;
                        }
                        if (dirok) {
                            if (cr - 1 > fence &&
                                t->ins[cr - 1].opc == NCO_LOAD_FAST2 &&
                                (t->ins[cr - 1].arg >> 4) == I &&
                                (t->ins[cr - 1].arg & 15) == N)
                                recheck = 1;
                            else if (cr - 2 > fence &&
                                     t->ins[cr - 1].opc == NCO_LOAD_FAST &&
                                     t->ins[cr - 1].arg == N &&
                                     t->ins[cr - 2].opc == NCO_LOAD_FAST &&
                                     t->ins[cr - 2].arg == I)
                                recheck = 1;
                        }
                    }
                }
                hi2 = recheck ? s : q;
                for (r = L; r <= hi2; r++)
                    if (ty_stores_loc(&t->ins[r], I)) { ok = 0; break; }
            }
            if (dbg) fprintf(stderr, "bnd: s=%d stores ok=%d\n", s, ok);
            if (!ok) continue;
            xlo = t->tlo + (size_t)s * t->width;
            xhi = t->thi + (size_t)s * t->width;
            if (dbg) fprintf(stderr, "bnd: s=%d lo=%lld\n", s, (long long)xlo[I]);
            if (inv != 2 && (xlo[I] < 0 || xlo[I] > xhi[I])) continue;
            if (A >= t->argcount) {
                have = 0;
                for (r = 0; r < h; r++) {
                    if ((t->arr_create[r] >= 0 || t->lol_create[r] >= 0 ||
                         t->ralias[r]) &&
                        t->ins[r].opc == NCO_STORE_FAST &&
                        t->ins[r].arg == A)
                        have = 1;
                }
                if (!have) continue;
            }
            {
                int ghead = inv == 2 ? h - 1 : h;
                int gn2 = inv == 2 ? t->rng_at[h] : N;
                int gk2 = inv == 2 ? 1 : 0;
                have = 0;
                for (gq = 0; gq < t->ng; gq++)
                    if (t->g_head[gq] == ghead && t->g_arr[gq] == A &&
                        t->g_n[gq] == gn2 && t->g_kind[gq] == gk2)
                        have = 1;
                if (!have) {
                    if (t->ng >= 24) continue;
                    t->g_head[t->ng] = ghead;
                    t->g_arr[t->ng] = A;
                    t->g_n[t->ng] = gn2;
                    t->g_kind[t->ng] = gk2;
                    t->ng++;
                }
            }
            t->bnd[s] = 1;
        }
    }
}

static int ty_count_elide(tinf *t)
{
    nc_ctx *c = t->c;
    int i, cnt = 0;
    for (i = 0; i < t->n; i++) {
        ninst *k = &t->ins[i];
        int ip = 0, d = k->depth;
        const char *sy, *bit = NULL, *fn;
        if (t->bnd[i]) cnt++;
        if (k->opc != NCO_BINARY_OP || t->skip[i]) continue;
        sy = k->argrepr ? PY->PyUnicode_AsUTF8(k->argrepr) : NULL;
        fn = sy ? binop_call(sy, &ip, &bit) : NULL;
        if (!fn || bit) continue;
        if (ty_slot_ty(t, i + 1 < t->n ? i + 1 : i, d - 2) != TY_INT) continue;
        if (!strcmp(fn, "cv_add")) {
            if (ty_iv_elide(t, i, d - 2, d - 1, 0)) cnt++;
        } else if (!strcmp(fn, "cv_sub")) {
            if (ty_iv_elide(t, i, d - 2, d - 1, 1)) cnt++;
        } else if (!strcmp(fn, "cv_mul")) {
            if (ty_iv_elide(t, i, d - 2, d - 1, 2)) cnt++;
        } else if (!strcmp(fn, "cv_mod")) {
            int64_t *xlo = t->tlo + (size_t)i * t->width;
            int64_t *xhi = t->thi + (size_t)i * t->width;
            int64_t dl1 = xlo[t->nlocals + d - 2];
            int64_t dl2 = xlo[t->nlocals + d - 1];
            if (xlo[t->nlocals + d - 2] > xhi[t->nlocals + d - 2]) dl1 = IV_MIN;
            if (xlo[t->nlocals + d - 1] > xhi[t->nlocals + d - 1]) dl2 = IV_MIN;
            if (dl1 >= 0 && dl2 >= 1) cnt++;
        }
    }
    return cnt;
}

static void ty_contract_selfok(tinf *t)
{
    nc_ctx *c = t->c;
    int i, q;
    t->cselfok = 1;
    if (!t->contract) return;
    for (i = 0; i < t->n; i++) {
        ninst *k = &t->ins[i];
        int base;
        if (k->opc != NCO_CALL || !t->selfcall[i]) continue;
        base = call_base(c, k->depth, k->arg);
        for (q = 0; q < k->arg && q < 14; q++) {
            int64_t lo, hi, nlo, nhi;
            if (t->tin[q] != TY_INT) continue;
            if (t->cidx[q] || t->ccmp[q]) { nlo = 0; nhi = (int64_t)1 << 31; }
            else continue;
            lo = t->tlo[(size_t)i * t->width + t->nlocals + base + 2 + q];
            hi = t->thi[(size_t)i * t->width + t->nlocals + base + 2 + q];
            if (lo > hi) continue;
            if (lo < nlo || hi > nhi) t->cselfok = 0;
        }
    }
}

static void ty_scan_narrow(tinf *t)
{
    int i, s, L;
    for (i = 0; i < t->nlocals; i++) t->nar[i] = 0;
    for (i = 0; i < t->n; i++) t->asrc[i] = -1;
    if (t->nsib || t->nself) return;
    for (s = 0; s < t->n; s++) {
        ninst *ks = &t->ins[s];
        int at, src = -2;
        if (ks->opc != NCO_BINARY_SUBSCR && ks->opc != NCO_STORE_SUBSCR) continue;
        if (t->skip[s]) continue;
        at = ty_slot_ty(t, s, ks->depth - 2);
        if (at != TY_ARRI && at != TY_ARRF) continue;
        if (!ks->label) {
            if (s >= 1 && t->ins[s - 1].opc == NCO_LOAD_FAST2 &&
                !t->ins[s - 1].label && !t->skip[s - 1])
                src = t->ins[s - 1].arg >> 4;
            else if (s >= 2 && !t->ins[s - 1].label && !t->skip[s - 1] &&
                     (t->ins[s - 1].opc == NCO_LOAD_FAST ||
                      t->ins[s - 1].opc == NCO_LOAD_CONST) &&
                     t->ins[s - 2].opc == NCO_LOAD_FAST && !t->skip[s - 2])
                src = t->ins[s - 2].arg;
            if (src >= 0 && (src >= t->nlocals || ty_loc_ty(t, src) != at))
                src = -2;
        }
        t->asrc[s] = src;
    }
    for (L = t->argcount; L < t->nlocals; L++) {
        int have = 0, r;
        if (ty_loc_ty(t, L) != TY_ARRI) continue;
        for (r = 0; r < t->n; r++) {
            if (t->arr_create[r] >= 0 && t->ins[r].opc == NCO_STORE_FAST &&
                t->ins[r].arg == L) {
                ty_acr *ac = &t->acr[t->arr_create[r]];
                if (ac->elem != TY_INT || ac->iv < -128 || ac->iv > 127) {
                    have = -1;
                    break;
                }
                have = 1;
            }
            if (t->lol_app_row[r] == L) { have = -1; break; }
        }
        if (have == 1) t->nar[L] = 1;
    }
    for (s = 0; s < t->n; s++) {
        ninst *ks = &t->ins[s];
        if (ks->opc != NCO_BINARY_SUBSCR && ks->opc != NCO_STORE_SUBSCR) continue;
        if (t->skip[s]) continue;
        if (t->asrc[s] == -2 && ty_slot_ty(t, s, ks->depth - 2) == TY_ARRI) {
            for (L = 0; L < t->nlocals; L++) t->nar[L] = 0;
            return;
        }
        if (t->asrc[s] >= 0 && ks->opc == NCO_STORE_SUBSCR && t->nar[t->asrc[s]]) {
            int64_t *xlo = t->tlo + (size_t)s * t->width;
            int64_t *xhi = t->thi + (size_t)s * t->width;
            int64_t lo = xlo[t->nlocals + ks->depth - 3];
            int64_t hi = xhi[t->nlocals + ks->depth - 3];
            if (lo <= hi && (lo < -128 || hi > 127)) t->nar[t->asrc[s]] = 0;
        }
    }
}

static int ty_sink_chain(tinf *t, int j0, int cur, char *dfr)
{
    nc_ctx *c = t->c;
    int j;
    for (j = j0; j < t->n; j++) {
        ninst *k = &t->ins[j];
        int D = k->depth;
        if (k->label || k->jump) return 0;
        if (t->skip[j]) return 0;
        switch (k->opc) {
        case NCO_LOAD_FAST: case NCO_LOAD_CONST: case NCO_LOAD_FAST2:
        case NCO_NOP: case NCO_RESUME:
            continue;
        case NCO_BINARY_OP: {
            const char *sy;
            const char *bit = NULL;
            const char *fn;
            int ip = 0, rt;
            if (D - 2 > cur) continue;
            if (D - 1 < cur) return 0;
            sy = k->argrepr ? PY->PyUnicode_AsUTF8(k->argrepr) : NULL;
            fn = sy ? binop_call(sy, &ip, &bit) : NULL;
            if (!fn) return 0;
            rt = ty_slot_ty(t, j + 1 < t->n ? j + 1 : j, D - 2);
            if (rt != TY_INT) return 0;
            if (bit) {
                if (strcmp(bit, "CPY_OP_AND") && strcmp(bit, "CPY_OP_OR") &&
                    strcmp(bit, "CPY_OP_XOR")) return 0;
            } else {
                if (strcmp(fn, "cv_add") && strcmp(fn, "cv_sub") &&
                    strcmp(fn, "cv_mul")) return 0;
                if (dfr) dfr[j] = 1;
            }
            cur = D - 2;
            continue;
        }
        case NCO_BINARY_SUBSCR:
            if (D - 2 > cur) continue;
            return 0;
        case NCO_STORE_SUBSCR:
            if (D - 3 > cur) continue;
            return 0;
        case NCO_COMPARE_OP: case NCO_IS_OP: case NCO_CONTAINS_OP:
            if (D - 2 > cur) continue;
            return 0;
        case NCO_TO_BOOL: case NCO_UNARY_NOT:
            if (D - 1 > cur) continue;
            return 0;
        case NCO_UNARY_NEGATIVE: case NCO_UNARY_INVERT:
            if (D - 1 >= cur) continue;
            return 0;
        case NCO_POP_TOP:
            if (D - 1 == cur) return 1;
            if (D - 1 > cur) continue;
            return 0;
        case NCO_CALL: {
            int cb = call_base(c, D, k->arg);
            if (cb > cur) continue;
            return 0;
        }
        case NCO_STORE_FAST:
            if (D - 1 == cur) {
                if (k->arg >= t->nlocals) return 0;
                if (t->snk[k->arg]) return 1;
                return 0;
            }
            if (D - 1 > cur) continue;
            return 0;
        case NCO_STORE_LOAD_FAST:
            if (D - 1 == cur) {
                if ((k->arg >> 4) >= t->nlocals) return 0;
                if (t->snk[k->arg >> 4]) return 1;
                return 0;
            }
            if (D - 1 > cur) continue;
            return 0;
        case NCO_STORE_FAST2:
            if (D - 1 == cur) {
                if ((k->arg >> 4) >= t->nlocals) return 0;
                if (t->snk[k->arg >> 4]) return 1;
                return 0;
            }
            if (D - 2 == cur) {
                if ((k->arg & 15) >= t->nlocals) return 0;
                if (t->snk[k->arg & 15]) return 1;
                return 0;
            }
            if (D - 2 > cur) continue;
            return 0;
        case NCO_RETURN_VALUE:
            if (D - 1 == cur) return 1;
            return 0;
        default:
            return 0;
        }
    }
    return 0;
}

static void ty_scan_sink(tinf *t)
{
    int i, L, changed, pass;
    memset(t->dfr, 0, (size_t)t->n);
    t->ndfr = 0;
    for (L = 0; L < t->nlocals; L++)
        t->snk[L] = (signed char)(ty_loc_ty(t, L) == TY_INT);
    for (i = 0; i < t->n; i++) {
        if (!t->skip[i]) continue;
        if (t->ins[i].opc == NCO_LOAD_FAST) {
            if (t->ins[i].arg < t->nlocals) t->snk[t->ins[i].arg] = 0;
        } else if (t->ins[i].opc == NCO_LOAD_FAST2) {
            if ((t->ins[i].arg >> 4) < t->nlocals) t->snk[t->ins[i].arg >> 4] = 0;
            if ((t->ins[i].arg & 15) < t->nlocals) t->snk[t->ins[i].arg & 15] = 0;
        }
    }
    for (pass = 0; pass < 16; pass++) {
        changed = 0;
        for (i = 0; i < t->n; i++) {
            ninst *k = &t->ins[i];
            if (t->skip[i]) continue;
            if (k->opc == NCO_LOAD_FAST) {
                L = k->arg;
                if (L >= t->nlocals || !t->snk[L]) continue;
                if (!ty_sink_chain(t, i + 1, k->depth, NULL)) {
                    t->snk[L] = 0;
                    changed = 1;
                }
            } else if (k->opc == NCO_LOAD_FAST2) {
                int l1 = k->arg >> 4, l2 = k->arg & 15;
                if (l1 < t->nlocals && t->snk[l1] &&
                    !ty_sink_chain(t, i + 1, k->depth, NULL)) {
                    t->snk[l1] = 0;
                    changed = 1;
                }
                if (l2 < t->nlocals && t->snk[l2] &&
                    !ty_sink_chain(t, i + 1, k->depth + 1, NULL)) {
                    t->snk[l2] = 0;
                    changed = 1;
                }
            }
        }
        if (!changed) break;
    }
    for (i = 0; i < t->n; i++) {
        ninst *k = &t->ins[i];
        if (t->skip[i]) continue;
        if (k->opc == NCO_LOAD_FAST) {
            L = k->arg;
            if (L < t->nlocals && t->snk[L])
                ty_sink_chain(t, i + 1, k->depth, t->dfr);
        } else if (k->opc == NCO_LOAD_FAST2) {
            int l1 = k->arg >> 4, l2 = k->arg & 15;
            if (l1 < t->nlocals && t->snk[l1])
                ty_sink_chain(t, i + 1, k->depth, t->dfr);
            if (l2 < t->nlocals && t->snk[l2])
                ty_sink_chain(t, i + 1, k->depth + 1, t->dfr);
        }
    }
    for (i = 0; i < t->n; i++)
        if (t->dfr[i]) t->ndfr++;
}

static void ty_scan_sticky(tinf *t)
{
    nc_ctx *c = t->c;
    int i;
    for (i = t->n - 1; i >= 0; i--) {
        ninst *k = &t->ins[i];
        int ip = 0, d = k->depth, j, dj;
        const char *sy, *bit = NULL, *fn;
        if (k->opc != NCO_BINARY_OP || t->skip[i]) continue;
        sy = k->argrepr ? PY->PyUnicode_AsUTF8(k->argrepr) : NULL;
        fn = sy ? binop_call(sy, &ip, &bit) : NULL;
        if (!fn || bit) continue;
        if (strcmp(fn, "cv_add") && strcmp(fn, "cv_sub") && strcmp(fn, "cv_mul"))
            continue;
        if (i + 1 >= t->n) continue;
        if (ty_slot_ty(t, i + 1, d - 2) != TY_INT) continue;
        j = i + 1;
        while (j < t->n && !t->ins[j].label &&
               (t->ins[j].opc == NCO_LOAD_CONST ||
                t->ins[j].opc == NCO_LOAD_FAST) && !t->skip[j]) j++;
        if (j >= t->n || t->ins[j].label || t->skip[j]) continue;
        dj = t->ins[j].depth;
        if (t->ins[j].opc == NCO_RETURN_VALUE && j == i + 1 && dj == d - 1) {
            t->stk[i] = 1;
            continue;
        }
        if (t->ins[j].opc == NCO_BINARY_OP && t->stk[j] &&
            (dj == d || (j == i + 1 && dj == d - 1)))
            t->stk[i] = 1;
    }
}

static void ty_slot_copy(tinf *t, buf *b, int inext, int dst, int src)
{
    int ty = ty_slot_ty(t, inext, dst);
    char b1[64], b2[64];
    if (ty_is_arr(ty))
        bpf(b, "  sa%d = sa%d; sl%d = sl%d;\n", dst, src, dst, src);
    else if (ty == TY_STR)
        bpf(b, "  memcpy(sc%d, sc%d, 64); scn%d = scn%d;\n", dst, src, dst, src);
    else if (ty == TY_MAPI)
        bpf(b, "  sm%d = sm%d;\n", dst, src);
    else if (ty == TY_LOL)
        bpf(b, "  sll%d = sll%d;\n", dst, src);
    else
        bpf(b, "  %s = %s;\n", ty_slot(b1, sizeof(b1), t, inext, dst),
            ty_slot(b2, sizeof(b2), t, inext, src));
}

static void ty_swap_save(tinf *t, buf *b, int i, int slot, int tag)
{
    int ty = ty_slot_ty(t, i, slot);
    char nb[64];
    if (ty_is_arr(ty))
        bpf(b, "    void *tp%d = sa%d; Py_ssize_t tn%d = sl%d;\n",
            tag, slot, tag, slot);
    else if (ty == TY_STR)
        bpf(b, "    char tc%d[64]; int tcn%d = scn%d; memcpy(tc%d, sc%d, 64);\n",
            tag, tag, slot, tag, slot);
    else if (ty == TY_MAPI)
        bpf(b, "    cpy_mii *tm%d = sm%d;\n", tag, slot);
    else if (ty == TY_LOL)
        bpf(b, "    cpy_lol *tl%d = sll%d;\n", tag, slot);
    else
        bpf(b, "    %s tv%d = %s;\n", ty_cdecl(ty), tag,
            ty_slot(nb, sizeof(nb), t, i, slot));
}

static void ty_swap_restore(tinf *t, buf *b, int inext, int slot, int srcty, int tag)
{
    char nb[64];
    if (ty_is_arr(srcty))
        bpf(b, "    sa%d = tp%d; sl%d = tn%d;\n", slot, tag, slot, tag);
    else if (srcty == TY_STR)
        bpf(b, "    memcpy(sc%d, tc%d, 64); scn%d = tcn%d;\n",
            slot, tag, slot, tag);
    else if (srcty == TY_MAPI)
        bpf(b, "    sm%d = tm%d;\n", slot, tag);
    else if (srcty == TY_LOL)
        bpf(b, "    sll%d = tl%d;\n", slot, tag);
    else
        bpf(b, "    %s = tv%d;\n", ty_slot(nb, sizeof(nb), t, inext, slot), tag);
}

static void ty_scan_accrec(tinf *t)
{
    nc_ctx *c = t->c;
    int i, j, q, base1, base2;
    t->ac1 = -1;
    t->ac2 = -1;
    t->acadd = -1;
    t->acret = -1;
    if (t->nself != 2) return;
    if (t->rettype != TY_INT) return;
    for (q = 0; q < t->argcount; q++)
        if (t->tin[q] != TY_INT) return;
    for (i = 0; i < t->n; i++)
        if (t->ins[i].opc == NCO_CALL && t->selfcall[i]) break;
    if (i >= t->n) return;
    base1 = call_base(c, t->ins[i].depth, t->ins[i].arg);
    if (base1 < 0 || t->ins[i].arg != t->argcount) return;
    for (j = i + 1; j < t->n; j++) {
        ninst *k = &t->ins[j];
        if (k->label) return;
        if (k->opc == NCO_CALL && t->selfcall[j]) break;
        if (t->skip[j]) continue;
        if (k->jump) return;
        if (k->opc != NCO_LOAD_FAST && k->opc != NCO_LOAD_FAST2 &&
            k->opc != NCO_LOAD_CONST && k->opc != NCO_BINARY_OP &&
            k->opc != NCO_UNARY_NEGATIVE && k->opc != NCO_NOP &&
            k->opc != NCO_PUSH_NULL && k->opc != NCO_COPY) return;
    }
    if (j >= t->n) return;
    base2 = call_base(c, t->ins[j].depth, t->ins[j].arg);
    if (base2 != base1 + 1 || t->ins[j].arg != t->argcount) return;
    if (j + 2 >= t->n) return;
    if (t->ins[j + 1].label || t->ins[j + 2].label) return;
    if (t->ins[j + 1].opc != NCO_BINARY_OP || !t->stk[j + 1]) return;
    if (t->ins[j + 1].depth != base1 + 2) return;
    {
        const char *sy = t->ins[j + 1].argrepr ?
            PY->PyUnicode_AsUTF8(t->ins[j + 1].argrepr) : NULL;
        int ip = 0;
        const char *bit = NULL;
        const char *fn = sy ? binop_call(sy, &ip, &bit) : NULL;
        if (!fn || bit || strcmp(fn, "cv_add")) return;
    }
    if (t->ins[j + 2].opc != NCO_RETURN_VALUE) return;
    t->ac1 = i;
    t->ac2 = j;
    t->acadd = j + 1;
    t->acret = j + 2;
}

static int ty_emit_fn(tinf *t, int index, char **kc)
{
    nc_ctx *c = t->c;
    buf *b = &c->code;
    int i, q, gslot = 0;
    for (q = 0; q < t->stacksize; q++) { free(kc[q]); kc[q] = NULL; }
    if (t->pure) {
        if (t->pure >= 2) bpf(b, "static int cpy_ofl_%d;\n", index);
        bpf(b, "static %s cpyf_%d_pure(PyObject *G", ty_cdecl(t->rettype), index);
        for (i = 0; i < t->argcount; i++)
            bpf(b, ", %s v%d", ty_cdecl(t->tin[i]), i);
        bpf(b, ")\n{\n");
    } else {
        bpf(b, "static int cpyf_%d_fast(PyObject *G, ", index);
        for (i = 0; i < t->argcount; i++) {
            int pt = t->tin[i];
            if (pt == TY_ARRI) bpf(b, "int64_t *v%d_p, Py_ssize_t v%d_n, ", i, i);
            else if (pt == TY_ARRF) bpf(b, "double *v%d_p, Py_ssize_t v%d_n, ", i, i);
            else bpf(b, "%s v%d, ", ty_cdecl(pt), i);
        }
        bpf(b, "%s *cpy_r, int *cpy_wf)\n{\n", ty_cdecl(t->rettype));
        bpf(b, "  int64_t cpy_dummy = 0;\n  (void)cpy_wf; (void)cpy_dummy;\n");
        if (t->ndfr) bpf(b, "  int cpy_dof = 0;\n");
    }
    for (i = 0; i < t->nacr; i++) bpf(b, "  void *cpy_ar%d = 0;\n", i);
    for (i = 0; i < t->nmap; i++) bpf(b, "  cpy_mii cpy_mp%d; memset(&cpy_mp%d, 0, sizeof(cpy_mp%d));\n", i, i, i);
    for (i = 0; i < t->nlol; i++) bpf(b, "  cpy_lol cpy_ll%d; memset(&cpy_ll%d, 0, sizeof(cpy_ll%d));\n", i, i, i);

    for (i = t->argcount; i < t->nlocals; i++) {
        int lt = t->ltype[i];
        if (lt == TY_UNSET) continue;
        if (lt == TY_ARRI)
            bpf(b, "  %s *v%d_p = 0; Py_ssize_t v%d_n = 0;\n",
                t->nar[i] ? "int8_t" : "int64_t", i, i);
        else if (lt == TY_ARRF) bpf(b, "  double *v%d_p = 0; Py_ssize_t v%d_n = 0;\n", i, i);
        else if (lt == TY_STR) bpf(b, "  char vc%d[64]; int vcn%d = 0; (void)vc%d; (void)vcn%d;\n", i, i, i, i);
        else if (lt == TY_MAPI) bpf(b, "  cpy_mii *vm%d = 0; (void)vm%d;\n", i, i);
        else if (lt == TY_LOL) bpf(b, "  cpy_lol *vl%d = 0; (void)vl%d;\n", i, i);
        else bpf(b, "  %s v%d = 0;\n", ty_cdecl(lt), i);
    }
    for (i = 0; i < t->stacksize; i++) {
        int needi = 0, needf = 0, needb = 0, needa = 0, needs = 0, needm = 0, needl = 0;
        int r2;
        for (r2 = 0; r2 < t->n; r2++) {
            int tt = t->tin[(size_t)r2 * t->width + t->nlocals + i];
            if (tt == TY_INT || tt == TY_CHR || tt == TY_NONE) needi = 1;
            else if (tt == TY_FLT) needf = 1;
            else if (tt == TY_BOOL) needb = 1;
            else if (tt == TY_ARRI || tt == TY_ARRF) needa = 1;
            else if (tt == TY_STR) needs = 1;
            else if (tt == TY_MAPI) needm = 1;
            else if (tt == TY_LOL) needl = 1;
        }
        needi = needi || t->nself || t->nsib;
        if (needi) bpf(b, "  int64_t si%d = 0; (void)si%d;\n", i, i);
        if (needf) bpf(b, "  double sd%d = 0; (void)sd%d;\n", i, i);
        if (needb) bpf(b, "  int sb%d = 0; (void)sb%d;\n", i, i);
        if (needa) bpf(b, "  void *sa%d = 0; Py_ssize_t sl%d = 0; (void)sa%d; (void)sl%d;\n", i, i, i, i);
        if (needs) bpf(b, "  char sc%d[64]; int scn%d = 0; (void)scn%d;\n", i, i, i);
        if (needm) bpf(b, "  cpy_mii *sm%d = 0; (void)sm%d;\n", i, i);
        if (needl) bpf(b, "  cpy_lol *sll%d = 0; (void)sll%d;\n", i, i);
    }
    for (i = 0; i < t->nrng; i++)
        bpf(b, "  int64_t rs%d = 0, re%d = 0, rp%d = 1;\n", i, i, i);
    if (t->pure == 3) {
        bpf(b, "  int64_t cpy_acc = 0;\n");
        for (i = 0; i < t->argcount; i++)
            bpf(b, "  int64_t cpy_ta%d = 0;\n", i);
    }
    if (t->nrng || t->nmath || t->nsib) bpf(b, "  PyObject *B = PyEval_GetBuiltins();\n");
    bpf(b, "  (void)G;\n");
    if (t->pure == 3) bpf(b, " CPY_TRE:;\n");
    if (!t->pure && t->contract)
        for (i = 0; i < t->argcount && i < 14; i++) {
            if (t->tin[i] != TY_INT) continue;
            if (t->cidx[i] || t->ccmp[i])
                bpf(b, "  if ((uint64_t)v%d > (1ULL << 31)) goto TYB1;\n", i);
        }
    if (!t->pure) {
        int gq2;
        for (gq2 = 0; gq2 < t->ng; gq2++) {
            int al = t->g_arr[gq2], nl2 = t->g_n[gq2], r2, st = 0;
            t->g_done[gq2] = 0;
            if (t->g_kind[gq2] == 1) continue;
            if (al >= t->argcount || nl2 >= t->argcount) continue;
            for (r2 = 0; r2 < t->n; r2++)
                if (ty_stores_loc(&t->ins[r2], al) ||
                    ty_stores_loc(&t->ins[r2], nl2)) st = 1;
            if (st) continue;
            if (ty_loc_ty(t, al) == TY_LOL)
                bpf(b, "  if (!vl%d || (uint64_t)v%d > (uint64_t)vl%d->n) goto TYB1;\n",
                    al, nl2, al);
            else
                bpf(b, "  if ((uint64_t)v%d > (uint64_t)v%d_n) goto TYB1;\n",
                    nl2, al);
            t->g_done[gq2] = 1;
        }
    }
    for (i = 0; i < t->nmath; i++) {
        ty_math *m = &t->mth[i];
        int gi = pool_add(c, c->names, t->ins[m->lg].argval);
        bpf(b, "  { cv g; PyObject *go;\n");
        bpf(b, "    if (cv_global(&g, G, B, N(%d), &CPYT[%d])) goto TYB2;\n", gi, t->nrng + 1 + i);
        bpf(b, "    go = (g.t == CPY_T_OBJ) ? cv_o(g) : (PyObject *)0; cv_clear(&g);\n");
        if (m->kind == TY_K_MATH) {
            int ni = pool_add(c, c->names, t->ins[m->attr].argval);
            bpf(b, "    if (!cpy_intr_ok(go, N(%d))) goto TYB1; }\n", ni);
        } else if (m->kind == TY_K_FLOAT) {
            bpf(b, "    if (go != (PyObject *)&PyFloat_Type) goto TYB1; }\n");
        } else if (m->kind == TY_K_INT) {
            bpf(b, "    if (go != (PyObject *)&PyLong_Type) goto TYB1; }\n");
        } else {
            int ni = pool_add(c, c->names, t->ins[m->lg].argval);
            bpf(b, "    if (!cpy_builtin_ok(go, N(%d))) goto TYB1; }\n", ni);
        }
    }

    for (i = 0; i < t->nsib; i++) {
        int rg = t->sibs[i].reg;
        bpf(b, "  { cv g; int sok;\n");
        bpf(b, "    if (cv_global(&g, G, B, N(%d), &CPYT[%d])) goto TYB2;\n",
            t->sibs[i].gname, t->nrng + 1 + t->nmath + i);
        bpf(b, "    sok = cpy_sib_ok(g, &cpy_tc[%d], G, \"__cpy_%d\");\n",
            c->tyreg[rg].idx, c->tyreg[rg].idx);
        bpf(b, "    cv_clear(&g);\n");
        bpf(b, "    if (!sok) goto TYB1; }\n");
    }

    for (i = 0; i < t->n; i++) {
        ninst *k = &t->ins[i];
        int d = k->depth;
        char ba[64], bb[64], bd[64];

        {
            int gq;
            for (gq = 0; gq < t->ng; gq++) {
                int al, nl2;
                if (t->g_head[gq] != i) continue;
                if (t->g_done[gq]) continue;
                al = t->g_arr[gq];
                nl2 = t->g_n[gq];
                if (t->g_kind[gq] == 1) {
                    if (ty_loc_ty(t, al) == TY_LOL)
                        bpf(b, "  if (rs%d < 0 || !vl%d || re%d > (int64_t)vl%d->n) goto TYB1;\n",
                            nl2, al, nl2, al);
                    else
                        bpf(b, "  if (rs%d < 0 || re%d > (int64_t)v%d_n) goto TYB1;\n",
                            nl2, nl2, al);
                    continue;
                }
                if (ty_loc_ty(t, al) == TY_LOL)
                    bpf(b, "  if (!vl%d || (uint64_t)v%d > (uint64_t)vl%d->n) goto TYB1;\n",
                        al, nl2, al);
                else
                    bpf(b, "  if ((uint64_t)v%d > (uint64_t)v%d_n) goto TYB1;\n",
                        nl2, al);
            }
        }
        if (k->label) {
            for (q = 0; q < t->stacksize; q++) { free(kc[q]); kc[q] = NULL; }
            bpf(b, " T%d:\n", k->off);
        }
        if (t->skip[i]) {
            if (k->opc == NCO_LOAD_GLOBAL) {
                int r;
                for (r = 0; r < t->nrng; r++) if (t->rng[r].lg == i) break;
                if (r < t->nrng) {
                    ty_rng *rg = &t->rng[r];
                    int idx = pool_add(c, c->names, k->argval);
                    bpf(b, "  { cv g; PyObject *go;\n");
                    bpf(b, "    if (cv_global(&g, G, B, N(%d), &CPYT[%d])) goto TYB2;\n", idx, gslot++);
                    bpf(b, "    go = (g.t == CPY_T_OBJ) ? cv_o(g) : (PyObject *)0; cv_clear(&g);\n");
                    bpf(b, "    if (go != (PyObject *)&PyRange_Type) goto TYB1; }\n");
                    {
                        int ty0 = 0;
                        const char *a0 = ty_operand(ba, sizeof(ba), t, &t->ins[rg->argi[0]], &ty0);
                        if (rg->nargs == 1) {
                            bpf(b, "  rs%d = 0; re%d = %s; rp%d = 1;\n", r, r, a0, r);
                        } else {
                            int ty1 = 0;
                            const char *a1 = ty_operand(bb, sizeof(bb), t, &t->ins[rg->argi[1]], &ty1);
                            bpf(b, "  rs%d = %s; re%d = %s; rp%d = ", r, a0, r, a1, r);
                            if (rg->nargs == 3) {
                                int ty2 = 0;
                                const char *a2 = ty_operand(bd, sizeof(bd), t, &t->ins[rg->argi[2]], &ty2);
                                bpf(b, "%s;\n", a2);
                            } else {
                                bpf(b, "1;\n");
                            }
                            bpf(b, "  if (rp%d == 0) goto TYB1;\n", r);
                        }
                    }
                }
            }
            continue;
        }

        switch (k->opc) {
        case NCO_NOP: case NCO_RESUME: case NCO_END_FOR: case NCO_POP_TOP:
            break;

        case NCO_LOAD_FAST:
            ty_load_local(t, b, i + 1 < t->n ? i + 1 : i, d, k->arg);
            break;

        case NCO_LOAD_FAST2: {
            int nx = i + 1 < t->n ? i + 1 : i;
            ty_load_local(t, b, nx, d, k->arg >> 4);
            ty_load_local(t, b, nx, d + 1, k->arg & 15);
            break;
        }

        case NCO_STORE_LOAD_FAST: {
            int nx = i + 1 < t->n ? i + 1 : i;
            ty_store_local(t, b, i, d - 1, k->arg >> 4);
            ty_load_local(t, b, nx, d - 1, k->arg & 15);
            break;
        }

        case NCO_STORE_FAST2:
            ty_store_local(t, b, i, d - 1, k->arg >> 4);
            ty_store_local(t, b, i, d - 2, k->arg & 15);
            break;

        case NCO_TO_BOOL: {
            int ta = ty_slot_ty(t, i, d - 1);
            const char *A = ty_slot(ba, sizeof(ba), t, i, d - 1);
            char rbuf[64];
            strcpy(rbuf, ty_slot(bb, sizeof(bb), t, i + 1 < t->n ? i + 1 : i, d - 1));
            bpf(b, "  %s = (%s %s);\n", rbuf, A, ta == TY_FLT ? "!= 0.0" : "!= 0");
            break;
        }

        case NCO_COPY:
            ty_slot_copy(t, b, i + 1 < t->n ? i + 1 : i, d, d - k->arg);
            break;

        case NCO_SWAP: {
            int w1 = d - 1, w2 = d - k->arg;
            int t1y = ty_slot_ty(t, i, w1), t2y = ty_slot_ty(t, i, w2);
            int nx = i + 1 < t->n ? i + 1 : i;
            bpf(b, "  {\n");
            ty_swap_save(t, b, i, w1, 1);
            ty_swap_save(t, b, i, w2, 2);
            ty_swap_restore(t, b, nx, w1, t2y, 2);
            ty_swap_restore(t, b, nx, w2, t1y, 1);
            bpf(b, "  }\n");
            break;
        }

        case NCO_STORE_FAST:
            if (t->lol_create[i] >= 0) {
                int ln2 = t->lol_create[i];
                bpf(b, "  cpy_lol_free(&cpy_ll%d);\n", ln2);
                bpf(b, "  vl%d = &cpy_ll%d;\n", k->arg, ln2);
                break;
            }
            if (t->map_create[i] >= 0) {
                int mn = t->map_create[i];
                bpf(b, "  cpy_mii_free(&cpy_mp%d);\n", mn);
                bpf(b, "  vm%d = &cpy_mp%d;\n", k->arg, mn);
                break;
            }
            if (t->arr_create[i] >= 0) {
                int an = t->arr_create[i];
                ty_acr *ac = &t->acr[an];
                char lb[64];
                const char *lenex;
                if (t->ins[ac->len_i].opc == NCO_LOAD_FAST) {
                    _snprintf(lb, sizeof(lb) - 1, "v%d", t->ins[ac->len_i].arg);
                    lb[sizeof(lb) - 1] = 0;
                    lenex = lb;
                } else {
                    long long lv = PY->PyLong_AsLongLong(t->ins[ac->len_i].argval);
                    if (lv == -1 && PY->PyErr_Occurred()) { PY->PyErr_Clear(); return 0; }
                    _snprintf(lb, sizeof(lb) - 1, "%lldLL", lv);
                    lb[sizeof(lb) - 1] = 0;
                    lenex = lb;
                }
                bpf(b, "  { int64_t cl = %s;\n", lenex);
                bpf(b, "    if (cl < 0) cl = 0;\n");
                bpf(b, "    if (cl > 268435455) goto TYB1;\n");
                bpf(b, "    free(cpy_ar%d);\n", an);
                if (ac->elem == TY_INT && t->nar[k->arg]) {
                    bpf(b, "    cpy_ar%d = malloc((size_t)(cl ? cl : 1));\n", an);
                    bpf(b, "    if (!cpy_ar%d) goto TYB1;\n", an);
                    bpf(b, "    v%d_p = (int8_t*)cpy_ar%d; v%d_n = cl;\n",
                        k->arg, an, k->arg);
                    bpf(b, "    memset(cpy_ar%d, %d, (size_t)cl);\n",
                        an, (int)(ac->iv & 0xFF));
                    bpf(b, "  }\n");
                    break;
                }
                bpf(b, "    cpy_ar%d = malloc((size_t)(cl ? cl : 1) * 8);\n", an);
                bpf(b, "    if (!cpy_ar%d) goto TYB1;\n", an);
                if (ac->elem == TY_INT) {
                    bpf(b, "    v%d_p = (int64_t*)cpy_ar%d; v%d_n = cl;\n",
                        k->arg, an, k->arg);
                    if (ac->iv == 0)
                        bpf(b, "    memset(cpy_ar%d, 0, (size_t)cl * 8);\n", an);
                    else
                        bpf(b, "    { int64_t cf; for (cf = 0; cf < cl; cf++) v%d_p[cf] = %lldLL; }\n",
                            k->arg, ac->iv);
                } else {
                    bpf(b, "    v%d_p = (double*)cpy_ar%d; v%d_n = cl;\n",
                        k->arg, an, k->arg);
                    if (ac->dv == 0.0)
                        bpf(b, "    memset(cpy_ar%d, 0, (size_t)cl * 8);\n", an);
                    else
                        bpf(b, "    { int64_t cf; for (cf = 0; cf < cl; cf++) v%d_p[cf] = %.17g; }\n",
                            k->arg, ac->dv);
                }
                bpf(b, "  }\n");
                break;
            }
            ty_store_local(t, b, i, d - 1, k->arg);
            break;

        case NCO_LOAD_CONST: {
            int cty = 0;
            const char *e;
            if (ty_slot_ty(t, i + 1 < t->n ? i + 1 : i, d) == TY_STR) {
                const char *u = PY->PyUnicode_AsUTF8(k->argval);
                size_t ul, q2;
                if (!u) { PY->PyErr_Clear(); return 0; }
                ul = strlen(u);
                if (ul > 63) return 0;
                bpf(b, "  { static const char kstr[] = {");
                for (q2 = 0; q2 < ul; q2++) bpf(b, "%d,", (int)(unsigned char)u[q2]);
                bpf(b, "0};\n");
                bpf(b, "    memcpy(sc%d, kstr, %d); scn%d = %d; }\n",
                    d, (int)ul + 1, d, (int)ul);
                break;
            }
            e = ty_operand(ba, sizeof(ba), t, k, &cty);
            if (!e) return 0;
            bpf(b, "  %s = %s;\n", ty_slot(bb, sizeof(bb), t, i + 1 < t->n ? i + 1 : i, d), e);
            if (d < t->stacksize) {
                free(kc[d]);
                kc[d] = (cty == TY_INT) ? cpy_adup(e) : NULL;
            }
            break;
        }

        case NCO_BINARY_OP: {
            int ip = 0;
            const char *bit = NULL;
            const char *sy = PY->PyUnicode_AsUTF8(k->argrepr);
            const char *fn = binop_call(sy, &ip, &bit);
            int ta = ty_slot_ty(t, i, d - 2), tb = ty_slot_ty(t, i, d - 1);
            int tr = ty_binop(sy, ta, tb, &ip);
            if (t->pure == 3 && i == t->acadd) break;
            const char *A = ty_slot(ba, sizeof(ba), t, i, d - 2);
            const char *Bo = ty_slot(bb, sizeof(bb), t, i, d - 1);
            const char *R = ty_slot(bd, sizeof(bd), t, i + 1 < t->n ? i + 1 : i, d - 2);
            char rbuf[64];
            strcpy(rbuf, R);
            if (tr == TY_INT) {
                const char *ka = (d - 2 < t->stacksize) ? kc[d - 2] : NULL;
                const char *kb = (d - 1 < t->stacksize) ? kc[d - 1] : NULL;
                if (!strcmp(fn, "cv_add")) {
                    if (ty_iv_elide(t, i, d - 2, d - 1, 0) || (t->pure && t->wrap))
                        bpf(b, "  %s = %s + %s;\n", rbuf, A, kb ? kb : Bo);
                    else if (t->pure >= 2 && t->stk[i])
                        bpf(b, "  if (cpy_addo(%s, %s, &%s)) cpy_ofl_%d = 1;\n", A, Bo, rbuf, index);
                    else if (!t->pure && t->dfr[i])
                        bpf(b, "  cpy_dof |= cpy_addo(%s, %s, &%s);\n", A, Bo, rbuf);
                    else if (kb) bpf(b, "  if (cpy_addo_k(%s, %s, &%s)) goto TYB1;\n", A, kb, rbuf);
                    else if (ka) bpf(b, "  if (cpy_addo_k(%s, %s, &%s)) goto TYB1;\n", Bo, ka, rbuf);
                    else bpf(b, "  if (cpy_addo(%s, %s, &%s)) goto TYB1;\n", A, Bo, rbuf);
                }
                else if (!strcmp(fn, "cv_sub")) {
                    if (ty_iv_elide(t, i, d - 2, d - 1, 1) || (t->pure && t->wrap))
                        bpf(b, "  %s = %s - %s;\n", rbuf, A, kb ? kb : Bo);
                    else if (t->pure >= 2 && t->stk[i])
                        bpf(b, "  if (cpy_subo(%s, %s, &%s)) cpy_ofl_%d = 1;\n", A, Bo, rbuf, index);
                    else if (!t->pure && t->dfr[i])
                        bpf(b, "  cpy_dof |= cpy_subo(%s, %s, &%s);\n", A, Bo, rbuf);
                    else if (kb) bpf(b, "  if (cpy_subo_k(%s, %s, &%s)) goto TYB1;\n", A, kb, rbuf);
                    else if (ka) bpf(b, "  if (cpy_ksub_o(%s, %s, &%s)) goto TYB1;\n", ka, Bo, rbuf);
                    else bpf(b, "  if (cpy_subo(%s, %s, &%s)) goto TYB1;\n", A, Bo, rbuf);
                }
                else if (!strcmp(fn, "cv_mul")) {
                    if (ty_iv_elide(t, i, d - 2, d - 1, 2) || (t->pure && t->wrap))
                        bpf(b, "  %s = %s * %s;\n", rbuf, A, kb ? kb : Bo);
                    else if (t->pure >= 2 && t->stk[i])
                        bpf(b, "  if (cpy_mulo(%s, %s, &%s)) cpy_ofl_%d = 1;\n", A, Bo, rbuf, index);
                    else if (!t->pure && t->dfr[i])
                        bpf(b, "  cpy_dof |= cpy_mulo(%s, %s, &%s);\n", A, Bo, rbuf);
                    else if (kb) bpf(b, "  if (cpy_mulo_k(%s, %s, &%s)) goto TYB1;\n", A, kb, rbuf);
                    else if (ka) bpf(b, "  if (cpy_mulo_k(%s, %s, &%s)) goto TYB1;\n", Bo, ka, rbuf);
                    else bpf(b, "  if (cpy_mulo(%s, %s, &%s)) goto TYB1;\n", A, Bo, rbuf);
                }
                else if (!strcmp(fn, "cv_fdiv")) {
                    int p2 = ty_pow2(ty_klit(kb));
                    if (p2 >= 0)
                        bpf(b, "  %s = %s >> %d;\n", rbuf, A, p2);
                    else
                        bpf(b, "  if (cpy_ifdiv(%s, %s, &%s)) goto TYB1;\n", A, kb ? kb : Bo, rbuf);
                }
                else if (!strcmp(fn, "cv_mod")) {
                    long long kv = ty_klit(kb);
                    int p2 = ty_pow2(kv);
                    int64_t *xlo = t->tlo + (size_t)i * t->width;
                    int64_t *xhi = t->thi + (size_t)i * t->width;
                    int64_t dl1 = xlo[t->nlocals + d - 2];
                    int64_t dl2 = xlo[t->nlocals + d - 1];
                    if (xlo[t->nlocals + d - 2] > xhi[t->nlocals + d - 2]) dl1 = IV_MIN;
                    if (xlo[t->nlocals + d - 1] > xhi[t->nlocals + d - 1]) dl2 = IV_MIN;
                    if (p2 >= 0)
                        bpf(b, "  %s = %s & %lldLL;\n", rbuf, A, kv - 1);
                    else if (kv > 0 || dl2 >= 1) {
                        if (dl1 >= 0)
                            bpf(b, "  %s = %s %% %s;\n", rbuf, A, kb ? kb : Bo);
                        else {
                            bpf(b, "  { int64_t mq = %s %% %s;\n", A, kb ? kb : Bo);
                            bpf(b, "    if (mq < 0) mq += %s;\n", kb ? kb : Bo);
                            bpf(b, "    %s = mq; }\n", rbuf);
                        }
                    }
                    else
                        bpf(b, "  if (cpy_imod(%s, %s, &%s)) goto TYB1;\n", A, kb ? kb : Bo, rbuf);
                }
                else if (bit && !strcmp(bit, "CPY_OP_AND")) bpf(b, "  %s = %s & %s;\n", rbuf, A, kb ? kb : Bo);
                else if (bit && !strcmp(bit, "CPY_OP_OR")) bpf(b, "  %s = %s | %s;\n", rbuf, A, kb ? kb : Bo);
                else if (bit && !strcmp(bit, "CPY_OP_XOR")) bpf(b, "  %s = %s ^ %s;\n", rbuf, A, kb ? kb : Bo);
                else if (bit && !strcmp(bit, "CPY_OP_SHL"))
                    bpf(b, "  if (cpy_ishl(%s, %s, &%s)) goto TYB1;\n", A, kb ? kb : Bo, rbuf);
                else if (bit && !strcmp(bit, "CPY_OP_SHR"))
                    bpf(b, "  if (cpy_ishr(%s, %s, &%s)) goto TYB1;\n", A, kb ? kb : Bo, rbuf);
                else { return 0; }
            } else if (tr == TY_FLT) {
                const char *o = !strcmp(fn, "cv_add") ? "+" : !strcmp(fn, "cv_sub") ? "-" :
                                !strcmp(fn, "cv_mul") ? "*" : !strcmp(fn, "cv_tdiv") ? "/" : NULL;
                if (!o) return 0;
                if (!strcmp(fn, "cv_tdiv")) {
                    bpf(b, "  if (");
                    ty_as_dbl(b, Bo, tb);
                    bpf(b, " == 0.0) goto TYB1;\n");
                }
                bpf(b, "  %s = ", rbuf);
                ty_as_dbl(b, A, ta);
                bpf(b, " %s ", o);
                ty_as_dbl(b, Bo, tb);
                bpf(b, ";\n");
            } else { return 0; }
            break;
        }

        case NCO_COMPARE_OP: {
            const char *sy = k->argval ? PY->PyUnicode_AsUTF8(k->argval) : NULL;
            const char *cn = sy ? cmp_name(sy) : NULL;
            const char *o;
            int ta = ty_slot_ty(t, i, d - 2), tb = ty_slot_ty(t, i, d - 1);
            const char *A = ty_slot(ba, sizeof(ba), t, i, d - 2);
            const char *Bo = ty_slot(bb, sizeof(bb), t, i, d - 1);
            const char *kb2 = (d - 1 < t->stacksize) ? kc[d - 1] : NULL;
            char rbuf[64];
            if (kb2 && ta != TY_FLT && tb != TY_FLT) Bo = kb2;
            strcpy(rbuf, ty_slot(bd, sizeof(bd), t, i + 1 < t->n ? i + 1 : i, d - 2));
            if (!cn) { return 0; }
            o = !strcmp(cn, "Py_LT") ? "<" : !strcmp(cn, "Py_LE") ? "<=" :
                !strcmp(cn, "Py_EQ") ? "==" : !strcmp(cn, "Py_NE") ? "!=" :
                !strcmp(cn, "Py_GT") ? ">" : ">=";
            bpf(b, "  %s = (", rbuf);
            if (ta == TY_FLT || tb == TY_FLT) {
                ty_as_dbl(b, A, ta);
                bpf(b, " %s ", o);
                ty_as_dbl(b, Bo, tb);
            } else {
                bpf(b, "%s %s %s", A, o, Bo);
            }
            bpf(b, ");\n");
            break;
        }

        case NCO_UNARY_NEGATIVE: {
            int ta = ty_slot_ty(t, i, d - 1);
            const char *A = ty_slot(ba, sizeof(ba), t, i, d - 1);
            char rbuf[64];
            strcpy(rbuf, ty_slot(bb, sizeof(bb), t, i + 1 < t->n ? i + 1 : i, d - 1));
            if (ta == TY_FLT) bpf(b, "  %s = -%s;\n", rbuf, A);
            else bpf(b, "  if (%s == INT64_MIN) goto TYB1;\n  %s = -%s;\n", A, rbuf, A);
            break;
        }

        case NCO_UNARY_NOT: {
            int ta = ty_slot_ty(t, i, d - 1);
            const char *A = ty_slot(ba, sizeof(ba), t, i, d - 1);
            char rbuf[64];
            strcpy(rbuf, ty_slot(bb, sizeof(bb), t, i + 1 < t->n ? i + 1 : i, d - 1));
            bpf(b, "  %s = (%s %s);\n", rbuf, A, ta == TY_FLT ? "== 0.0" : "== 0");
            break;
        }

        case NCO_BINARY_SUBSCR: {
            int at = ty_slot_ty(t, i, d - 2);
            char rbuf[64];
            const char *ety;
            if (at == TY_LOL) {
                bpf(b, "  { int64_t cix = %s;\n", ty_slot(bb, sizeof(bb), t, i, d - 1));
                if (!t->bnd[i]) {
                    bpf(b, "    if ((uint64_t)cix >= (uint64_t)sll%d->n) {\n", d - 2);
                    bpf(b, "      cix += sll%d->n;\n", d - 2);
                    bpf(b, "      if ((uint64_t)cix >= (uint64_t)sll%d->n) goto TYB1;\n", d - 2);
                    bpf(b, "    }\n");
                }
                bpf(b, "    sa%d = sll%d->rows[cix]; sl%d = sll%d->lens[cix]; }\n",
                    d - 2, d - 2, d - 2, d - 2);
                break;
            }
            if (at == TY_MAPI) {
                strcpy(rbuf, ty_slot(ba, sizeof(ba), t, i + 1 < t->n ? i + 1 : i, d - 2));
                bpf(b, "  if (cpy_mii_get(sm%d, %s, &%s)) goto TYB1;\n",
                    d - 2, ty_slot(bb, sizeof(bb), t, i, d - 1), rbuf);
                break;
            }
            if (at == TY_STR) {
                strcpy(rbuf, ty_slot(ba, sizeof(ba), t, i + 1 < t->n ? i + 1 : i, d - 2));
                bpf(b, "  { int64_t cix = %s;\n", ty_slot(bb, sizeof(bb), t, i, d - 1));
                bpf(b, "    if ((uint64_t)cix >= (uint64_t)scn%d) {\n", d - 2);
                bpf(b, "      cix += scn%d;\n", d - 2);
                bpf(b, "      if ((uint64_t)cix >= (uint64_t)scn%d) goto TYB1;\n", d - 2);
                bpf(b, "    }\n");
                bpf(b, "    %s = (int64_t)(unsigned char)sc%d[cix]; }\n", rbuf, d - 2);
                break;
            }
            ety = at == TY_ARRI ? "int64_t" : "double";
            if (at == TY_ARRI && t->asrc[i] >= 0 && t->nar[t->asrc[i]])
                ety = "int8_t";
            strcpy(rbuf, ty_slot(ba, sizeof(ba), t, i + 1 < t->n ? i + 1 : i, d - 2));
            bpf(b, "  { int64_t cix = %s;\n", ty_slot(bb, sizeof(bb), t, i, d - 1));
            if (!t->bnd[i]) {
                bpf(b, "    if ((uint64_t)cix >= (uint64_t)sl%d) {\n", d - 2);
                bpf(b, "      cix += sl%d;\n", d - 2);
                bpf(b, "      if ((uint64_t)cix >= (uint64_t)sl%d) goto TYB1;\n", d - 2);
                bpf(b, "    }\n");
            }
            bpf(b, "    %s = ((%s*)sa%d)[cix]; }\n", rbuf, ety, d - 2);
            break;
        }

        case NCO_STORE_SUBSCR: {
            int at = ty_slot_ty(t, i, d - 2);
            const char *ety;
            if (at == TY_MAPI) {
                bpf(b, "  if (cpy_mii_set(sm%d, %s, %s)) goto TYB1;\n",
                    d - 2, ty_slot(bb, sizeof(bb), t, i, d - 1),
                    ty_slot(ba, sizeof(ba), t, i, d - 3));
                break;
            }
            ety = at == TY_ARRI ? "int64_t" : "double";
            if (at == TY_ARRI && t->asrc[i] >= 0 && t->nar[t->asrc[i]]) {
                bpf(b, "  { int64_t cix = %s;\n", ty_slot(bb, sizeof(bb), t, i, d - 1));
                if (!t->bnd[i]) {
                    bpf(b, "    if ((uint64_t)cix >= (uint64_t)sl%d) {\n", d - 2);
                    bpf(b, "      cix += sl%d;\n", d - 2);
                    bpf(b, "      if ((uint64_t)cix >= (uint64_t)sl%d) goto TYB1;\n", d - 2);
                    bpf(b, "    }\n");
                }
                bpf(b, "    ((int8_t*)sa%d)[cix] = (int8_t)(%s);\n", d - 2,
                    ty_slot(ba, sizeof(ba), t, i, d - 3));
                bpf(b, "    *cpy_wf = 1; }\n");
                break;
            }
            bpf(b, "  { int64_t cix = %s;\n", ty_slot(bb, sizeof(bb), t, i, d - 1));
            if (!t->bnd[i]) {
                bpf(b, "    if ((uint64_t)cix >= (uint64_t)sl%d) {\n", d - 2);
                bpf(b, "      cix += sl%d;\n", d - 2);
                bpf(b, "      if ((uint64_t)cix >= (uint64_t)sl%d) goto TYB1;\n", d - 2);
                bpf(b, "    }\n");
            }
            bpf(b, "    ((%s*)sa%d)[cix] = %s;\n", ety, d - 2,
                ty_slot(ba, sizeof(ba), t, i, d - 3));
            bpf(b, "    *cpy_wf = 1; }\n");
            break;
        }

        case NCO_FORMAT_VALUE:
        case NCO_FORMAT_SIMPLE: {
            char vb[64];
            bpf(b, "  scn%d = cpy_i64s(sc%d, %s);\n",
                d - 1, d - 1, ty_slot(vb, sizeof(vb), t, i, d - 1));
            break;
        }

        case NCO_BUILD_STRING: {
            int q2, base2 = d - k->arg;
            bpf(b, "  { int tt = 0;\n");
            for (q2 = 0; q2 < k->arg; q2++)
                bpf(b, "    tt += scn%d;\n", base2 + q2);
            bpf(b, "    if (tt > 63) goto TYB1;\n");
            bpf(b, "    tt = 0;\n");
            for (q2 = 0; q2 < k->arg; q2++) {
                if (base2 + q2 == base2)
                    bpf(b, "    tt = scn%d;\n", base2);
                else {
                    bpf(b, "    memcpy(sc%d + tt, sc%d, (size_t)scn%d);\n",
                        base2, base2 + q2, base2 + q2);
                    bpf(b, "    tt += scn%d;\n", base2 + q2);
                }
            }
            bpf(b, "    scn%d = tt; }\n", base2);
            break;
        }

        case NCO_FOR_ITER: {
            int r = t->rng_at[i];
            char rbuf[64];
            strcpy(rbuf, ty_slot(ba, sizeof(ba), t, i + 1 < t->n ? i + 1 : i, d));
            bpf(b, "  if (rp%d > 0 ? rs%d >= re%d : rs%d <= re%d) goto T%d;\n",
                r, r, r, r, r, k->target);
            bpf(b, "  %s = rs%d; rs%d += rp%d;\n", rbuf, r, r, r);
            break;
        }

        case NCO_CALL: {
            int base = call_base(c, d, k->arg), q2;
            char rbuf[64];
            strcpy(rbuf, ty_slot(ba, sizeof(ba), t, i + 1 < t->n ? i + 1 : i, base));
            if (t->math_at[i] >= 0) {
                ty_math *m = &t->mth[t->math_at[i]];
                char a0[64], a1[64];
                int ty0 = ty_slot_ty(t, i, base + 2);
                const char *o0 = ty_slot(a0, sizeof(a0), t, i, base + 2);
                if (m->kind == TY_K_LEN) {
                    int lt2 = ty_slot_ty(t, i, base + 2);
                    if (lt2 == TY_STR)
                        bpf(b, "  %s = scn%d;\n", rbuf, base + 2);
                    else if (lt2 == TY_MAPI)
                        bpf(b, "  %s = sm%d->n;\n", rbuf, base + 2);
                    else if (lt2 == TY_LOL)
                        bpf(b, "  %s = sll%d->n;\n", rbuf, base + 2);
                    else
                        bpf(b, "  %s = sl%d;\n", rbuf, base + 2);
                    break;
                }
                if (m->kind == TY_K_ORD) {
                    bpf(b, "  %s = %s;\n", rbuf,
                        ty_slot(a0, sizeof(a0), t, i, base + 2));
                    break;
                }
                if (m->kind == TY_K_MIN || m->kind == TY_K_MAX) {
                    const char *o1m = ty_slot(a1, sizeof(a1), t, i, base + 3);
                    bpf(b, "  %s = (%s %s %s) ? %s : %s;\n", rbuf,
                        o1m, m->kind == TY_K_MIN ? "<" : ">", o0, o1m, o0);
                    break;
                }
                if (m->kind == TY_K_MATH) {
                    const mathent *me = &MATHFN[m->fn];
                    const char *o1 = me->arity == 2
                        ? ty_slot(a1, sizeof(a1), t, i, base + 3) : NULL;
                    bpf(b, "  { double A0 = (double)(%s);\n", o0);
                    if (me->arity == 2) bpf(b, "    double A1 = (double)(%s);\n", o1);
                    bpf(b, "    double AR;\n");
                    if (ty0 == TY_FLT) bpf(b, "    if (!cpy_fin(A0)) goto TYB1;\n");
                    if (me->arity == 2 && ty_slot_ty(t, i, base + 3) == TY_FLT)
                        bpf(b, "    if (!cpy_fin(A1)) goto TYB1;\n");
                    if (me->dom) bpf(b, "    if (%s) goto TYB1;\n", me->dom);
                    if (me->arity == 2) bpf(b, "    AR = %s(A0, A1);\n", me->fn);
                    else bpf(b, "    AR = %s(A0);\n", me->fn);
                    bpf(b, "    if (!cpy_fin(AR)) goto TYB1;\n");
                    bpf(b, "    %s = AR; }\n", rbuf);
                } else if (m->kind == TY_K_FLOAT) {
                    if (ty0 == TY_FLT) {
                        bpf(b, "  { double AR = (double)(%s);\n", o0);
                        bpf(b, "    if (!cpy_fin(AR)) goto TYB1;\n");
                        bpf(b, "    %s = AR; }\n", rbuf);
                    } else {
                        bpf(b, "  %s = (double)(%s);\n", rbuf, o0);
                    }
                } else if (m->kind == TY_K_INT) {
                    if (ty0 == TY_FLT) {
                        bpf(b, "  { double A0 = (%s);\n", o0);
                        bpf(b, "    if (!cpy_fin(A0) || A0 >= 9223372036854775808.0"
                               " || A0 <= -9223372036854775809.0) goto TYB1;\n");
                        bpf(b, "    %s = (int64_t)A0; }\n", rbuf);
                    } else {
                        bpf(b, "  %s = (int64_t)(%s);\n", rbuf, o0);
                    }
                } else {
                    if (ty0 == TY_FLT) {
                        bpf(b, "  { double A0 = (%s);\n", o0);
                        bpf(b, "    if (!cpy_fin(A0)) goto TYB1;\n");
                        bpf(b, "    %s = fabs(A0); }\n", rbuf);
                    } else {
                        bpf(b, "  { int64_t A0 = (int64_t)(%s);\n", o0);
                        bpf(b, "    if (A0 == INT64_MIN) goto TYB1;\n");
                        bpf(b, "    %s = A0 < 0 ? -A0 : A0; }\n", rbuf);
                    }
                }
                break;
            }
            if (t->lol_app[i] >= 0) {
                int lolloc = t->lol_app[i];
                int rowloc = t->lol_app_row[i];
                int site = t->site_of[rowloc];
                bpf(b, "  if (cpy_lol_push(vl%d, (void*)v%d_p, v%d_n, cpy_ar%d != 0)) goto TYB1;\n",
                    lolloc, rowloc, rowloc, site);
                bpf(b, "  cpy_ar%d = 0;\n", site);
                break;
            }
            {
                int callee = index;
                int cret = t->retguess;
                if (t->pure == 3 && i == t->ac1) {
                    for (q2 = 0; q2 < k->arg; q2++)
                        bpf(b, "  cpy_ta%d = %s;\n", q2,
                            ty_slot(bb, sizeof(bb), t, i, base + 2 + q2));
                    break;
                }
                if (t->pure == 3 && i == t->ac2) {
                    bpf(b, "  if (cpy_addo(cpy_acc, cpyf_%d_pure(G", index);
                    for (q2 = 0; q2 < k->arg; q2++)
                        bpf(b, ", %s", ty_slot(bb, sizeof(bb), t, i, base + 2 + q2));
                    bpf(b, "), &cpy_acc)) cpy_ofl_%d = 1;\n", index);
                    break;
                }
                if (t->pure && t->sib[i] < 0) {
                    bpf(b, "  %s = cpyf_%d_pure(G", rbuf, index);
                    for (q2 = 0; q2 < k->arg; q2++)
                        bpf(b, ", %s", ty_slot(bb, sizeof(bb), t, i, base + 2 + q2));
                    bpf(b, ");\n");
                    break;
                }
                if (t->sib[i] >= 0) {
                    callee = c->tyreg[t->sib[i]].idx;
                    cret = c->tyreg[t->sib[i]].ret;
                }
                bpf(b, "  if (cpyf_%d_fast(G", callee);
                for (q2 = 0; q2 < k->arg; q2++) {
                    int at = ty_slot_ty(t, i, base + 2 + q2);
                    if (at == TY_ARRI || at == TY_ARRF) {
                        bpf(b, ", (%s)sa%d, sl%d",
                            at == TY_ARRI ? "int64_t*" : "double*",
                            base + 2 + q2, base + 2 + q2);
                    } else {
                        bpf(b, ", %s", ty_slot(bb, sizeof(bb), t, i, base + 2 + q2));
                    }
                }
                if (cret == TY_NONE)
                    bpf(b, ", &cpy_dummy, cpy_wf)) goto TYB1;\n");
                else
                    bpf(b, ", &%s, cpy_wf)) goto TYB1;\n", rbuf);
            }
            break;
        }

        case NCO_JUMP:
            bpf(b, "  goto T%d;\n", k->target);
            break;

        case NCO_POP_JUMP_IF_FALSE:
        case NCO_POP_JUMP_IF_TRUE: {
            int ta = ty_slot_ty(t, i, d - 1);
            const char *A = ty_slot(ba, sizeof(ba), t, i, d - 1);
            const char *neg = k->opc == NCO_POP_JUMP_IF_FALSE ? "!" : "";
            if (ta == TY_FLT) bpf(b, "  if (%s(%s != 0.0)) goto T%d;\n", neg, A, k->target);
            else bpf(b, "  if (%s(%s != 0)) goto T%d;\n", neg, A, k->target);
            break;
        }

        case NCO_RETURN_VALUE: {
            int ta = ty_slot_ty(t, i, d - 1);
            const char *A = ty_slot(ba, sizeof(ba), t, i, d - 1);
            if (t->pure == 3) {
                if (i == t->acret) {
                    int q3;
                    for (q3 = 0; q3 < t->argcount; q3++)
                        bpf(b, "  v%d = cpy_ta%d;\n", q3, q3);
                    bpf(b, "  goto CPY_TRE;\n");
                } else {
                    bpf(b, "  if (cpy_addo(cpy_acc, %s, &cpy_acc)) cpy_ofl_%d = 1;\n",
                        A, index);
                    bpf(b, "  return cpy_acc;\n");
                }
                break;
            }
            if (t->pure) {
                if (t->rettype == TY_FLT && ta != TY_FLT) bpf(b, "  return (double)%s;\n", A);
                else bpf(b, "  return %s;\n", A);
                break;
            }
            if (t->ndfr) bpf(b, "  if (cpy_dof) goto TYB1;\n");
            if (t->rettype == TY_FLT && ta != TY_FLT) bpf(b, "  *cpy_r = (double)%s;\n", A);
            else bpf(b, "  *cpy_r = %s;\n", A);
            bpf(b, "  goto TYB0;\n");
            break;
        }

        case NCO_RETURN_CONST: {
            int cty = 0;
            const char *e;
            if (t->rettype == TY_NONE) {
                if (t->ndfr) bpf(b, "  if (cpy_dof) goto TYB1;\n");
                bpf(b, "  *cpy_r = 0;\n  goto TYB0;\n");
                break;
            }
            e = ty_operand(ba, sizeof(ba), t, k, &cty);
            if (!e) return 0;
            if (t->pure == 3) {
                bpf(b, "  if (cpy_addo(cpy_acc, %s, &cpy_acc)) cpy_ofl_%d = 1;\n",
                    e, index);
                bpf(b, "  return cpy_acc;\n");
                break;
            }
            if (t->pure) {
                if (t->rettype == TY_FLT && cty != TY_FLT) bpf(b, "  return (double)(%s);\n", e);
                else bpf(b, "  return %s;\n", e);
                break;
            }
            if (t->ndfr) bpf(b, "  if (cpy_dof) goto TYB1;\n");
            if (t->rettype == TY_FLT && cty != TY_FLT) bpf(b, "  *cpy_r = (double)(%s);\n", e);
            else bpf(b, "  *cpy_r = %s;\n", e);
            bpf(b, "  goto TYB0;\n");
            break;
        }

        default:
            { return 0; }
        }
        {
            int lo;
            if (k->opc == NCO_LOAD_CONST) lo = d + 1;
            else if (k->opc == NCO_SWAP) lo = d - k->arg;
            else lo = d + k->eff - 1;
            if (lo < 0) lo = 0;
            for (q = lo; q < t->stacksize; q++) { free(kc[q]); kc[q] = NULL; }
        }
    }
    if (t->pure) {
        bpf(b, "  return 0;\n}\n");
        goto pdone;
    }
    bpf(b, "  goto TYB1;\n");
    bpf(b, " TYB0:\n");
    for (i = 0; i < t->nacr; i++) bpf(b, "  free(cpy_ar%d);\n", i);
    for (i = 0; i < t->nmap; i++) bpf(b, "  cpy_mii_free(&cpy_mp%d);\n", i);
    for (i = 0; i < t->nlol; i++) bpf(b, "  cpy_lol_free(&cpy_ll%d);\n", i);
    bpf(b, "  return 0;\n");
    bpf(b, " TYB1:\n");
    for (i = 0; i < t->nacr; i++) bpf(b, "  free(cpy_ar%d);\n", i);
    for (i = 0; i < t->nmap; i++) bpf(b, "  cpy_mii_free(&cpy_mp%d);\n", i);
    for (i = 0; i < t->nlol; i++) bpf(b, "  cpy_lol_free(&cpy_ll%d);\n", i);
    bpf(b, "  return 1;\n");
    bpf(b, " TYB2:\n");
    for (i = 0; i < t->nacr; i++) bpf(b, "  free(cpy_ar%d);\n", i);
    for (i = 0; i < t->nmap; i++) bpf(b, "  cpy_mii_free(&cpy_mp%d);\n", i);
    for (i = 0; i < t->nlol; i++) bpf(b, "  cpy_lol_free(&cpy_ll%d);\n", i);
    bpf(b, "  return 2;\n}\n");
 pdone:
    return 1;
}

static int ty_emit(tinf *t, int index)
{
    nc_ctx *c = t->c;
    buf *b = &c->code;
    int i;
    char **kc = (char **)cpy_xmalloc(sizeof(char *) * (size_t)t->stacksize);
    for (i = 0; i < t->stacksize; i++) kc[i] = NULL;

    if (t->nrng || t->nself || t->nmath || t->nsib) {
        bpf(b, "static cpy_gc CPYT_%d[%d];\n", index,
            t->nrng + t->nmath + t->nsib + 2);
        bpf(b, "#define CPYT CPYT_%d\n", index);
    }
    for (i = 0; i < t->nsib; i++) {
        int rg = t->sibs[i].reg;
        int p2;
        bpf(b, "static int cpyf_%d_fast(PyObject *", c->tyreg[rg].idx);
        for (p2 = 0; p2 < c->tyreg[rg].np; p2++) {
            int pt = c->tyreg[rg].par[p2];
            if (pt == TY_ARRI) bpf(b, ", int64_t *, Py_ssize_t");
            else if (pt == TY_ARRF) bpf(b, ", double *, Py_ssize_t");
            else bpf(b, ", %s", ty_cdecl(pt));
        }
        bpf(b, ", %s *, int *);\n", ty_cdecl(c->tyreg[rg].ret));
    }
    if (t->nself) {
        bpf(b, "static int cpyf_%d_selfok(PyObject *G)\n{\n", index);
        bpf(b, "  PyObject *B = PyEval_GetBuiltins();\n  cv g; int ok;\n");
        bpf(b, "  if (cv_global(&g, G, B, N(%d), &CPYT[%d])) { PyErr_Clear(); return 0; }\n",
            t->selfname_idx, t->nrng);
        bpf(b, "  ok = cpy_same_fn(g, cpy_tc[%d], G);\n  cv_clear(&g);\n  return ok;\n}\n", index);
    }
    {
        int pure_ok = 0, pure_mode = 0;
        size_t pstart = b->n;
        if (t->nself && !t->nacr && !t->nmap && !t->nlol && !t->nsib &&
            !t->nmath && !t->nrng && (!t->contract || t->cselfok) &&
            (t->rettype == TY_INT || t->rettype == TY_FLT)) {
            int pi, okp = 1;
            for (pi = 0; pi < t->argcount; pi++)
                if (t->tin[pi] != TY_INT && t->tin[pi] != TY_FLT) okp = 0;
            if (okp) {
                int mi;
                static const int MORDER[3] = { 1, 3, 2 };
                if (t->stk) {
                    memset(t->stk, 0, (size_t)t->n);
                    ty_scan_sticky(t);
                }
                ty_scan_accrec(t);
                for (mi = 0; mi < 3 && !pure_ok; mi++) {
                    int mode = MORDER[mi];
                    if (!t->stk) break;
                    if (mode == 3 && t->acret < 0) continue;
                    if (mode >= 2) {
                        int si2, any = 0;
                        for (si2 = 0; si2 < t->n; si2++) if (t->stk[si2]) any = 1;
                        if (!any) continue;
                    }
                    t->pure = mode;
                    if (!ty_emit_fn(t, index, kc)) { t->pure = 0; goto tyfail; }
                    {
                        size_t sp;
                        pure_ok = 1;
                        for (sp = pstart; sp + 3 <= b->n; sp++)
                            if (b->p[sp] == 'T' && b->p[sp + 1] == 'Y' &&
                                b->p[sp + 2] == 'B') {
                                pure_ok = 0;
                                break;
                            }
                    }
                    if (pure_ok) pure_mode = mode;
                    else {
                        b->n = pstart;
                        if (b->p) b->p[pstart] = 0;
                    }
                    t->pure = 0;
                }
            }
        }
        if (pure_ok) {
            bpf(b, "static int cpyf_%d_fast(PyObject *G, ", index);
            for (i = 0; i < t->argcount; i++)
                bpf(b, "%s v%d, ", ty_cdecl(t->tin[i]), i);
            bpf(b, "%s *cpy_r, int *cpy_wf)\n{\n", ty_cdecl(t->rettype));
            if (t->contract)
                for (i = 0; i < t->argcount && i < 14; i++) {
                    if (t->tin[i] != TY_INT) continue;
                    if (t->cidx[i] || t->ccmp[i])
                        bpf(b, "  if ((uint64_t)v%d > (1ULL << 31)) return 1;\n", i);
                }
            if (pure_mode >= 2) {
                bpf(b, "  cpy_ofl_%d = 0;\n", index);
                bpf(b, "  (void)cpy_wf;\n  *cpy_r = cpyf_%d_pure(G", index);
                for (i = 0; i < t->argcount; i++) bpf(b, ", v%d", i);
                bpf(b, ");\n  return cpy_ofl_%d ? 1 : 0;\n}\n", index);
            } else {
                bpf(b, "  (void)cpy_wf;\n  *cpy_r = cpyf_%d_pure(G", index);
                for (i = 0; i < t->argcount; i++) bpf(b, ", v%d", i);
                bpf(b, ");\n  return 0;\n}\n");
            }
        } else {
            if (!ty_emit_fn(t, index, kc)) goto tyfail;
        }
    }
    for (i = 0; i < t->stacksize; i++) free(kc[i]);
    free(kc);
    if (t->nrng || t->nself || t->nmath || t->nsib) bpf(b, "#undef CPYT\n");
    bpf(b, "\n");
    return 1;
tyfail:
    for (i = 0; i < t->stacksize; i++) free(kc[i]);
    free(kc);
    return 0;
}

static int compile_fn(nc_ctx *c, PyObj code, int index, const char *qual, int trial)
{
    ninst *ins = NULL;
    fnc f;
    buf body;
    int n, i, ok;
    long nlocals, stacksize, argcount;
    size_t save_code = c->code.n, save_tab = c->tab.n;
    int ty_ok = 0, ty_param = 0, ty_ret = 0, ty_self = 0, ty_hasarr = 0;
    int f_argc_probe = 0;
    signed char tysig[14];
    const char *selfname = strrchr(qual, '.');
    selfname = selfname ? selfname + 1 : qual;

    n = decode(c, code, &ins);
    if (n < 0) return 0;

    {
        PyObj a;
        a = getattr_(c, code, "co_nlocals");   nlocals = as_long(c, a, -1);
        a = getattr_(c, code, "co_stacksize"); stacksize = as_long(c, a, -1);
        a = getattr_(c, code, "co_argcount");  argcount = as_long(c, a, -1);
    }
    if (nlocals < 0 || stacksize < 0 || argcount < 0) { free(ins); return 0; }
    f_argc_probe = (int)argcount;
    memset(tysig, 0, sizeof(tysig));

    ty_ok = 0;
    if (argcount <= 8 && nlocals > 0) {
        static const int PTY[5] = { TY_INT, TY_INT, TY_INT, TY_INT, TY_FLT };
        static const int ETY[5] = { TY_INT, TY_INT, TY_FLT, TY_FLT, TY_FLT };
        static const int RTY[5] = { TY_INT, TY_FLT, TY_FLT, TY_INT, TY_FLT };
        tinf t;
        int attempt;
        for (attempt = 0; attempt < 5 && !ty_ok; attempt++) {
            int pty = PTY[attempt];
            memset(&t, 0, sizeof(t));
            t.c = c;
            t.ins = ins;
            t.n = n;
            t.nlocals = (int)nlocals;
            t.stacksize = (int)stacksize + 4;
            t.argcount = (int)argcount;
            t.width = t.nlocals + t.stacksize;
            t.tin = (signed char *)cpy_xmalloc((size_t)n * t.width);
            t.ltype = (signed char *)cpy_xmalloc((size_t)t.nlocals);
            t.skip = (char *)cpy_xmalloc((size_t)n);
            t.rng_at = (int *)cpy_xmalloc(sizeof(int) * (size_t)n);
            t.math_at = (int *)cpy_xmalloc(sizeof(int) * (size_t)n);
            t.selfcall = (char *)cpy_xmalloc((size_t)n);
            t.arr_create = (int *)cpy_xmalloc(sizeof(int) * (size_t)n);
            t.map_create = (int *)cpy_xmalloc(sizeof(int) * (size_t)n);
            t.lol_create = (int *)cpy_xmalloc(sizeof(int) * (size_t)n);
            t.lol_app = (int *)cpy_xmalloc(sizeof(int) * (size_t)n);
            t.lol_app_row = (int *)cpy_xmalloc(sizeof(int) * (size_t)n);
            t.lol_sub = (int *)cpy_xmalloc(sizeof(int) * (size_t)n);
            t.sib = (int *)cpy_xmalloc(sizeof(int) * (size_t)n);
            t.stk = (char *)cpy_xmalloc((size_t)n);
            t.ralias = (int *)cpy_xmalloc(sizeof(int) * (size_t)n);
            memset(t.stk, 0, (size_t)n);
            memset(t.skip, 0, (size_t)n);
            memset(t.selfcall, 0, (size_t)n);
            for (i = 0; i < n; i++) t.rng_at[i] = -1;
            for (i = 0; i < n; i++) t.math_at[i] = -1;
            for (i = 0; i < n; i++) t.arr_create[i] = -1;
            for (i = 0; i < n; i++) t.map_create[i] = -1;
            for (i = 0; i < n; i++) t.lol_create[i] = -1;
            for (i = 0; i < n; i++) t.lol_app[i] = -1;
            for (i = 0; i < n; i++) t.lol_app_row[i] = -1;
            for (i = 0; i < n; i++) t.lol_sub[i] = -1;
            t.nmap = 0;
            t.nlol = 0;
            memset(t.lol_elem, 0, sizeof(t.lol_elem));
            memset(t.is_lol, 0, sizeof(t.is_lol));
            memset(t.site_of, -1, sizeof(t.site_of));
            for (i = 0; i < n; i++) t.sib[i] = -1;
            t.nmath = 0;
            t.nacr = 0;
            t.nsib = 0;
            t.ety = ETY[attempt];
            t.retguess = RTY[attempt];
            t.selfname = selfname;
            {
                wchar_t wv[8];
                t.wrap = GetEnvironmentVariableW(L"COMPYLER_UNSAFE", wv, 8) ? 1 : 0;
            }
            ty_scan_arrays(&t);
            ty_scan_maps(&t);
            ty_scan_lol(&t);
            ty_scan_ranges(&t);
            ty_scan_math(&t);
            ty_scan_builtins(&t);
            ty_scan_params(&t);
            ty_scan_sibs(&t);
            {
                int anycand = 0, ci;
                for (ci = 0; ci < t.argcount && ci < 14; ci++)
                    if (t.cand[ci]) anycand = 1;
                if (!anycand && ETY[attempt] != PTY[attempt]) {
                    free(t.tin); free(t.ltype); free(t.skip);
                    free(t.rng_at); free(t.math_at); free(t.selfcall);
                    free(t.arr_create); free(t.sib); free(t.stk);
                    free(t.map_create); free(t.lol_create); free(t.lol_app);
                    free(t.lol_app_row); free(t.lol_sub); free(t.ralias);
                    continue;
                }
            }
            ty_scan_ralias(&t);
            ty_scan_self(&t);
            if (ty_infer(&t, pty) && (!t.nself || t.rettype == t.retguess)) {
                size_t mark = c->code.n;
                t.tlo = (int64_t *)cpy_xmalloc(sizeof(int64_t) * (size_t)n * t.width);
                t.thi = (int64_t *)cpy_xmalloc(sizeof(int64_t) * (size_t)n * t.width);
                t.brf = (int *)cpy_xmalloc(sizeof(int) * (size_t)n);
                t.bnd = (int *)cpy_xmalloc(sizeof(int) * (size_t)n);
                for (i = 0; i < n; i++) t.brf[i] = -1;
                t.nbrf = 0;
                ty_scan_branches(&t);
                t.contract = 0;
                t.cselfok = 1;
                ty_scan_cwant(&t);
                {
                    int pi2, want = 0;
                    for (pi2 = 0; pi2 < t.argcount && pi2 < 14; pi2++)
                        if ((t.cidx[pi2] || t.ccmp[pi2]) && t.tin[pi2] == TY_INT)
                            want = 1;
                    if (want) {
                        int ca, cb;
                        ty_ivpass(&t);
                        memset(t.bnd, 0, sizeof(int) * (size_t)n);
                        t.ng = 0;
                        ty_scan_guard(&t);
                        ca = ty_count_elide(&t);
                        t.contract = 1;
                        ty_ivpass(&t);
                        memset(t.bnd, 0, sizeof(int) * (size_t)n);
                        t.ng = 0;
                        ty_scan_guard(&t);
                        cb = ty_count_elide(&t);
                        if (cb <= ca) {
                            t.contract = 0;
                            ty_ivpass(&t);
                            memset(t.bnd, 0, sizeof(int) * (size_t)n);
                            t.ng = 0;
                            ty_scan_guard(&t);
                        } else {
                            ty_contract_selfok(&t);
                        }
                    } else {
                        ty_ivpass(&t);
                        memset(t.bnd, 0, sizeof(int) * (size_t)n);
                        t.ng = 0;
                        ty_scan_guard(&t);
                    }
                }
                t.nar = (signed char *)cpy_xmalloc((size_t)(t.nlocals ? t.nlocals : 1));
                t.asrc = (int *)cpy_xmalloc(sizeof(int) * (size_t)n);
                ty_scan_narrow(&t);
                t.snk = (signed char *)cpy_xmalloc((size_t)(t.nlocals ? t.nlocals : 1));
                t.dfr = (char *)cpy_xmalloc((size_t)n);
                ty_scan_sink(&t);
                if (ty_emit(&t, index)) {
                    ty_ok = 1;
                    ty_param = pty;
                    ty_ret = t.rettype;
                    for (i = 0; i < f_argc_probe && i < 14; i++) tysig[i] = t.tin[i];
                    ty_hasarr = 0;
                    for (i = 0; i < f_argc_probe && i < 14; i++)
                        if (tysig[i] == TY_ARRI || tysig[i] == TY_ARRF) ty_hasarr = 1;
                } else {
                    c->code.n = mark;
                    if (c->code.p) c->code.p[mark] = 0;
                }
            }
            if (ty_ok) ty_self = t.nself;
            free(t.tlo);
            free(t.thi);
            free(t.bnd);
            free(t.brf);
            free(t.nar);
            free(t.asrc);
            free(t.snk);
            free(t.dfr);
            free(t.tin);
            free(t.ltype);
            free(t.skip);
            free(t.rng_at);
            free(t.math_at);
            free(t.selfcall);
            free(t.arr_create);
            free(t.map_create);
            free(t.lol_create);
            free(t.lol_app);
            free(t.lol_app_row);
            free(t.lol_sub);
            free(t.sib);
            free(t.stk);
            free(t.ralias);
        }
    }

    memset(&body, 0, sizeof(body));
    memset(&f, 0, sizeof(f));
    f.c = c;
    f.b = &body;
    f.ins = ins;
    f.n = n;
    f.nlocals = (int)nlocals;
    f.stacksize = (int)stacksize + 4;
    f.argcount = (int)argcount;
    f.origin = (int *)cpy_xmalloc(sizeof(int) * (size_t)(f.stacksize + 4));
    f.bid = (int *)cpy_xmalloc(sizeof(int) * (size_t)(f.stacksize + 4));
    for (i = 0; i < f.stacksize + 4; i++) { f.origin[i] = -1; f.bid[i] = -1; }

    ok = emit_body(&f);
    free(ins);
    if (!ok) { free(body.p); free(f.origin); free(f.bid); return 0; }

    if (f.nglob)
        bpf(&c->code, "static cpy_gc CPYG_%d[%d];\n#define CPYG CPYG_%d\n", index, f.nglob, index);
    if (f.nattr)
        bpf(&c->code, "static cpy_ac CPYA_%d[%d];\n#define CPYA CPYA_%d\n", index, f.nattr, index);
    if (ty_ok && ty_hasarr)
        bpf(&c->code, "static int cpy_strikes_%d;\n", index);
    bpf(&c->code, "static int cpyf_%d_core(PyObject *G, cv *cpy_a, int cpy_na, cv *cpy_out)\n{\n", index);

    for (i = 0; i < f.nlocals; i++) bpf(&c->code, "  cv l%d;\n", i);
    for (i = 0; i < f.stacksize; i++) bpf(&c->code, "  cv s%d;\n", i);
    if (f.uses_iter)
        for (i = 0; i < f.stacksize; i++) bpf(&c->code, "  cv_iter it%d;\n", i);
    bpf(&c->code, "  int cpy_rc = -1;\n");
    if (f.uses_globals)
        bpf(&c->code, "  PyObject *B = PyEval_GetBuiltins();\n  if (!G) G = PyEval_GetGlobals();\n");
    else
        bpf(&c->code, "  (void)G;\n");
    bpf(&c->code, "  (void)cpy_na;\n");
    if (ty_ok) {
        bpf(&c->code, "  if (");
        for (i = 0; i < f.argcount; i++) {
            if (tysig[i] == TY_ARRI || tysig[i] == TY_ARRF)
                bpf(&c->code, "%s(cpy_a[%d].t == CPY_T_OBJ && PyList_CheckExact(cv_o(cpy_a[%d])))",
                    i ? " && " : "", i, i);
            else
                bpf(&c->code, "%scpy_a[%d].t == %s", i ? " && " : "", i,
                    tysig[i] == TY_FLT ? "CPY_T_FLT" : "CPY_T_INT");
        }
        if (!f.argcount) bpf(&c->code, "1");
        if (ty_self)
            bpf(&c->code, "%scpyf_%d_selfok(G ? G : PyEval_GetGlobals())",
                f.argcount ? " && " : "", index);
        bpf(&c->code, ") {\n");
        bpf(&c->code, "    %s fr; int cpy_wr = 0; int fs = 1; int cpy_ok = 1;\n", ty_cdecl(ty_ret));
        if (ty_hasarr)
            bpf(&c->code, "    if (cpy_strikes_%d > 2) cpy_ok = 0;\n", index);
        if (ty_hasarr) {
            for (i = 0; i < f.argcount; i++) {
                if (tysig[i] != TY_ARRI && tysig[i] != TY_ARRF) continue;
                bpf(&c->code, "    %s *ab%d = 0; Py_ssize_t an%d = 0; int ash%d = 0;\n",
                    tysig[i] == TY_ARRI ? "int64_t" : "double", i, i, i);
                bpf(&c->code, "    PyObject *aL%d = cv_o(cpy_a[%d]);\n", i, i);
            }
            for (i = 0; i < f.argcount; i++) {
                int j;
                if (tysig[i] != TY_ARRI && tysig[i] != TY_ARRF) continue;
                for (j = 0; j < i; j++) {
                    if (tysig[j] != tysig[i]) continue;
                    bpf(&c->code, "    if (cpy_ok && !ash%d && aL%d == aL%d) { ab%d = ab%d; an%d = an%d; ash%d = 1; }\n",
                        i, i, j, i, j, i, j, i);
                }
                bpf(&c->code, "    if (cpy_ok && !ash%d) {\n", i);
                bpf(&c->code, "      Py_ssize_t q;\n");
                bpf(&c->code, "      an%d = PyList_GET_SIZE(aL%d);\n", i, i);
                bpf(&c->code, "      ab%d = (%s*)malloc((size_t)(an%d ? an%d : 1) * 8);\n",
                    i, tysig[i] == TY_ARRI ? "int64_t" : "double", i, i);
                bpf(&c->code, "      if (!ab%d) cpy_ok = 0;\n", i);
                bpf(&c->code, "      for (q = 0; cpy_ok && q < an%d; q++) {\n", i);
                bpf(&c->code, "        PyObject *e = PyList_GET_ITEM(aL%d, q);\n", i);
                if (tysig[i] == TY_ARRI) {
                    bpf(&c->code, "        if (!PyLong_CheckExact(e)) { cpy_ok = 0; break; }\n");
                    bpf(&c->code, "        ab%d[q] = cpy_pl_i64(e);\n", i);
                    bpf(&c->code, "        if (ab%d[q] == -1 && PyErr_Occurred()) { PyErr_Clear(); cpy_ok = 0; }\n", i);
                } else {
                    bpf(&c->code, "        if (!PyFloat_CheckExact(e)) { cpy_ok = 0; break; }\n");
                    bpf(&c->code, "        ab%d[q] = PyFloat_AS_DOUBLE(e);\n", i);
                }
                bpf(&c->code, "      }\n    }\n");
            }
        }
        bpf(&c->code, "    if (cpy_ok) fs = cpyf_%d_fast(G ? G : PyEval_GetGlobals()", index);
        for (i = 0; i < f.argcount; i++) {
            if (tysig[i] == TY_ARRI || tysig[i] == TY_ARRF)
                bpf(&c->code, ", ab%d, an%d", i, i);
            else
                bpf(&c->code, ", %scpy_a[%d].%s", tysig[i] == TY_FLT ? "cv_d(" : "",
                    i, tysig[i] == TY_FLT ? "b)" : "b");
        }
        bpf(&c->code, ", &fr, &cpy_wr);\n");
        if (ty_hasarr) {
            bpf(&c->code, "    if (cpy_ok && fs == 0 && cpy_wr) {\n");
            for (i = 0; i < f.argcount; i++) {
                if (tysig[i] != TY_ARRI && tysig[i] != TY_ARRF) continue;
                bpf(&c->code, "      if (!ash%d) { Py_ssize_t q;\n", i);
                bpf(&c->code, "        for (q = 0; q < an%d; q++) {\n", i);
                if (tysig[i] == TY_ARRI)
                    bpf(&c->code, "          PyObject *nv = PyLong_FromLongLong(ab%d[q]);\n", i);
                else
                    bpf(&c->code, "          PyObject *nv = PyFloat_FromDouble(ab%d[q]);\n", i);
                bpf(&c->code, "          PyObject *old;\n");
                bpf(&c->code, "          if (!nv) { PyErr_NoMemory(); fs = 3; break; }\n");
                bpf(&c->code, "          old = PyList_GET_ITEM(aL%d, q);\n", i);
                bpf(&c->code, "          PyList_SET_ITEM(aL%d, q, nv);\n", i);
                bpf(&c->code, "          Py_DECREF(old);\n        }\n      }\n");
            }
            bpf(&c->code, "    }\n");
            for (i = 0; i < f.argcount; i++) {
                if (tysig[i] != TY_ARRI && tysig[i] != TY_ARRF) continue;
                bpf(&c->code, "    if (!ash%d) free(ab%d);\n", i, i);
            }
            bpf(&c->code, "    if (fs == 3) return -1;\n");
        }
        if (ty_hasarr) {
            bpf(&c->code, "    if (cpy_ok && fs == 0) cpy_strikes_%d = 0;\n", index);
            bpf(&c->code, "    else if (cpy_strikes_%d <= 2) cpy_strikes_%d++;\n", index, index);
        }
        if (ty_ret == TY_NONE)
            bpf(&c->code, "    if (cpy_ok && fs == 0) { (void)fr; Py_INCREF(Py_None); *cpy_out = cv_obj(Py_None); return 0; }\n");
        else
            bpf(&c->code, "    if (cpy_ok && fs == 0) { *cpy_out = %s; return 0; }\n",
                ty_ret == TY_FLT ? "cv_flt(fr)" : (ty_ret == TY_BOOL ? "cv_bool(fr)" : "cv_int(fr)"));
        bpf(&c->code, "    if (cpy_ok && fs == 2) return -1;\n  }\n");
    }
    for (i = 0; i < f.stacksize; i++) bpf(&c->code, "  s%d = CV_NIL;\n", i);
    for (i = f.argcount; i < f.nlocals; i++) bpf(&c->code, "  l%d = CV_NIL;\n", i);
    if (f.uses_iter)
        for (i = 0; i < f.stacksize; i++)
            bpf(&c->code, "  it%d.fast = 0; it%d.it = NULL;\n", i, i);
    for (i = 0; i < f.argcount; i++)
        bpf(&c->code, "  l%d = cpy_a[%d]; cv_hold(l%d);\n", i, i, i);
    bput(&c->code, body.p ? body.p : "", body.p ? body.n : 0);
    bpf(&c->code, "  PyErr_SetString(PyExc_SystemError, \"compyler: fell out of %s\");\n", qual);
    bpf(&c->code, "  cpy_rc = -1; goto DONE;\n");
    for (i = f.maxerr; i > 0; i--)
        bpf(&c->code, " E%d: cv_clear(&s%d);\n", i, i - 1);
    bpf(&c->code, " E0: cpy_rc = -1;\n");
    bpf(&c->code, " DONE:\n");
    for (i = 0; i < f.nlocals; i++)
        bpf(&c->code, "  cv_clear(&l%d);\n", i);
    if (f.uses_iter)
        for (i = 0; i < f.stacksize; i++) bpf(&c->code, "  cv_iter_clear(&it%d);\n", i);
    bpf(&c->code, "  return cpy_rc;\n}\n");
    if (f.nglob) bpf(&c->code, "#undef CPYG\n");
    if (f.nattr) bpf(&c->code, "#undef CPYA\n");
    bpf(&c->code, "\n");

    bpf(&c->code, "static PyObject *cpyf_%d(PyObject *self, PyObject *const *a, Py_ssize_t na)\n{\n", index);
    bpf(&c->code, "  cv tmp[14], out; int i, rc;\n  (void)self;\n");
    bpf(&c->code, "  if (!cpy_tc[%d]) { PyFrameObject *fr = PyEval_GetFrame();\n"
                  "    if (fr) cpy_tc[%d] = (PyObject *)PyFrame_GetCode(fr); }\n", index, index);
    bpf(&c->code, "  if (cv_argcount(na, %d, \"%s\")) return NULL;\n", f.argcount, qual);
    bpf(&c->code, "  for (i = 0; i < %d; i++) tmp[i] = cv_norm(a[i]);\n", f.argcount);
    bpf(&c->code, "  rc = cpyf_%d_core(PyEval_GetGlobals(), tmp, %d, &out);\n", index, f.argcount);
    bpf(&c->code, "  for (i = 0; i < %d; i++) cv_clear(&tmp[i]);\n", f.argcount);
    bpf(&c->code, "  if (rc) return NULL;\n");
    bpf(&c->code, "  { PyObject *o = cv_box(out); cv_clear(&out); return o; }\n}\n\n");
    bpf(&c->tab, "  { \"__cpy_%d\", (PyCFunction)cpyf_%d, METH_FASTCALL, NULL },\n", index, index);

    free(body.p);
    free(f.origin); free(f.bid);
        if (ty_ok && c->ntyreg < 256 && strlen(selfname) < 96 && f_argc_probe <= 14) {
            int rg;
            for (rg = 0; rg < c->ntyreg; rg++)
                if (!strcmp(c->tyreg[rg].name, selfname)) break;
            if (rg == c->ntyreg) {
                strcpy(c->tyreg[rg].name, selfname);
                c->tyreg[rg].idx = index;
                c->tyreg[rg].np = f_argc_probe;
                for (i = 0; i < f_argc_probe; i++) c->tyreg[rg].par[i] = tysig[i];
                c->tyreg[rg].ret = (signed char)ty_ret;
                c->ntyreg++;
            } else if (c->tyreg[rg].idx != index) {
                c->tyreg[rg].np = -1;
            }
        }
    if (trial) {
        c->code.n = save_code;
        c->tab.n = save_tab;
        if (c->code.p) c->code.p[save_code] = 0;
        if (c->tab.p) c->tab.p[save_tab] = 0;
    }
    return 1;
}

static int nonempty(nc_ctx *c, PyObj code, const char *attr)
{
    PyObj a = getattr_(c, code, attr);
    cpy_ssize n;
    if (!a) return 0;
    n = PY->PyTuple_Size(a);
    if (n < 0) { PY->PyErr_Clear(); n = 0; }
    PY->Py_DecRef(a);
    return n > 0;
}

#define CO_VARARGS   0x04
#define CO_VARKEYWORDS 0x08
#define CO_GENERATOR 0x20
#define CO_COROUTINE 0x80
#define CO_ASYNC_GEN 0x200

static int eligible(nc_ctx *c, PyObj code, const char *name)
{
    PyObj a;
    long flags, kwonly, argcount, nlocals;

    if (!name || name[0] == '<') { c->nskip++; return 0; }
    a = getattr_(c, code, "co_flags");            flags = as_long(c, a, -1);
    a = getattr_(c, code, "co_kwonlyargcount");   kwonly = as_long(c, a, 0);
    a = getattr_(c, code, "co_argcount");         argcount = as_long(c, a, -1);
    a = getattr_(c, code, "co_nlocals");          nlocals = as_long(c, a, -1);
    if (flags < 0 || argcount < 0 || nlocals < 0) return 0;
    if (flags & (CO_GENERATOR | CO_COROUTINE | CO_ASYNC_GEN)) { note(c, "generator"); return 0; }
    if (flags & (CO_VARARGS | CO_VARKEYWORDS)) { note(c, "*args/**kwargs"); return 0; }
    if (kwonly) { note(c, "keyword-only args"); return 0; }
    if (argcount > 12) { note(c, "too many args"); return 0; }
    if (nonempty(c, code, "co_freevars") || nonempty(c, code, "co_cellvars")) {
        note(c, "closure");
        return 0;
    }
    {
        PyObj et = getattr_(c, code, "co_exceptiontable");
        cpy_ssize n = et ? PY->PyBytes_Size(et) : 0;
        if (n < 0) { PY->PyErr_Clear(); n = 1; }
        if (et) PY->Py_DecRef(et);
        if (n > 0) { note(c, "try/except/with"); return 0; }
    }
    return 1;
}

static PyObj make_trampoline(nc_ctx *c, PyObj code, int index, const char *name)
{
    char src[4096];
    size_t at = 0;
    PyObj vn, mod, consts, tramp = NULL;
    long argcount, posonly;
    cpy_ssize i, n;

    { PyObj a = getattr_(c, code, "co_argcount"); argcount = as_long(c, a, 0); }
    { PyObj a = getattr_(c, code, "co_posonlyargcount"); posonly = as_long(c, a, 0); }
    vn = getattr_(c, code, "co_varnames");
    if (!vn) return NULL;

    at += (size_t)_snprintf(src + at, sizeof(src) - at - 1, "def __cpy_t(");
    for (i = 0; i < argcount; i++) {
        const char *p = PY->PyUnicode_AsUTF8(PY->PyTuple_GetItem(vn, i));
        if (!p) { PY->Py_DecRef(vn); return NULL; }
        at += (size_t)_snprintf(src + at, sizeof(src) - at - 1, "%s%s", i ? ", " : "", p);
        if (i + 1 == posonly)
            at += (size_t)_snprintf(src + at, sizeof(src) - at - 1, ", /");
    }
    at += (size_t)_snprintf(src + at, sizeof(src) - at - 1, "):\n    return __cpy_%d(", index);
    for (i = 0; i < argcount; i++) {
        const char *p = PY->PyUnicode_AsUTF8(PY->PyTuple_GetItem(vn, i));
        at += (size_t)_snprintf(src + at, sizeof(src) - at - 1, "%s%s", i ? ", " : "", p);
    }
    at += (size_t)_snprintf(src + at, sizeof(src) - at - 1, ")\n");
    src[at] = 0;
    PY->Py_DecRef(vn);

    mod = PY->Py_CompileStringExFlags(src, "<compyler>", CPY_FILE_INPUT, NULL, 0);
    if (!mod) { PY->PyErr_Clear(); return NULL; }
    consts = getattr_(c, mod, "co_consts");
    n = consts ? PY->PyTuple_Size(consts) : 0;
    for (i = 0; i < n; i++) {
        PyObj o = PY->PyTuple_GetItem(consts, i);
        if (PY->PyObject_IsInstance(o, c->codetype) == 1) { tramp = o; PY->Py_IncRef(o); break; }
    }
    PY->PyErr_Clear();
    if (consts) PY->Py_DecRef(consts);
    PY->Py_DecRef(mod);
    if (!tramp) return NULL;

    {
        PyObj rep = PY->PyObject_GetAttrString(tramp, "replace");
        PyObj kw = PY->PyDict_New();
        PyObj empty = PY->PyTuple_New(0);
        PyObj out;
        const char *keys[] = { "co_name", "co_qualname", "co_filename", "co_firstlineno", NULL };
        int q;
        for (q = 0; keys[q]; q++) {
            PyObj v = getattr_(c, code, keys[q]);
            if (v) { PY->PyDict_SetItemString(kw, keys[q], v); PY->Py_DecRef(v); }
        }
        out = PY->PyObject_Call(rep, empty, kw);
        PY->Py_DecRef(rep);
        PY->Py_DecRef(kw);
        PY->Py_DecRef(empty);
        PY->Py_DecRef(tramp);
        if (!out) { PY->PyErr_Clear(); return NULL; }
        return out;
    }
}

static PyObj rebuild_tree(nc_ctx *c, PyObj code, int *changed);

static PyObj rebuild(nc_ctx *c, PyObj code, PyObj newconsts)
{
    PyObj rep = PY->PyObject_GetAttrString(code, "replace");
    PyObj kw = PY->PyDict_New();
    PyObj empty = PY->PyTuple_New(0);
    PyObj out;
    if (!rep) { PY->PyErr_Clear(); return NULL; }
    PY->PyDict_SetItemString(kw, "co_consts", newconsts);
    out = PY->PyObject_Call(rep, empty, kw);
    PY->Py_DecRef(rep);
    PY->Py_DecRef(kw);
    PY->Py_DecRef(empty);
    if (!out) { PY->PyErr_Clear(); return NULL; }
    return out;
}

static void collect_fns(nc_ctx *c, PyObj code, const char *prefix, int depth)
{
    PyObj consts = getattr_(c, code, "co_consts");
    cpy_ssize n, i;

    if (!consts) return;
    n = PY->PyTuple_Size(consts);
    if (n < 0) { PY->PyErr_Clear(); PY->Py_DecRef(consts); return; }

    for (i = 0; i < n; i++) {
        PyObj o = PY->PyTuple_GetItem(consts, i);
        PyObj nm;
        const char *name;
        char qual[256];
        if (PY->PyObject_IsInstance(o, c->codetype) != 1) continue;
        nm = getattr_(c, o, "co_name");
        name = nm ? PY->PyUnicode_AsUTF8(nm) : NULL;
        _snprintf(qual, sizeof(qual) - 1, "%s%s", prefix, name ? name : "?");
        qual[sizeof(qual) - 1] = 0;

        if (name && c->npend < 2048 && eligible(c, o, name) && compile_fn(c, o, c->nfun, qual, 1)) {
            int idx = c->nfun++;
            PY->Py_IncRef(o);
            c->pend[c->npend].code = o;
            c->pend[c->npend].idx = idx;
            strncpy(c->pend[c->npend].qual, qual, sizeof(c->pend[0].qual) - 1);
            c->pend[c->npend].qual[sizeof(c->pend[0].qual) - 1] = 0;
            c->npend++;
            if (depth == 0 && c->nfnmap < 512 && strlen(name) < 96) {
                PyObj a = getattr_(c, o, "co_argcount");
                strcpy(c->fnmap[c->nfnmap].name, name);
                c->fnmap[c->nfnmap].idx = idx;
                c->fnmap[c->nfnmap].argc = (int)as_long(c, a, -1);
                c->nfnmap++;
            }
        } else {
            char sub[256];
            _snprintf(sub, sizeof(sub) - 1, "%s.", qual);
            sub[sizeof(sub) - 1] = 0;
            collect_fns(c, o, sub, depth + 1);
        }
        if (nm) PY->Py_DecRef(nm);
    }
    PY->Py_DecRef(consts);
}

static int pend_index(nc_ctx *c, PyObj o)
{
    int i;
    for (i = 0; i < c->npend; i++)
        if (c->pend[i].code == o) return i;
    return -1;
}

static PyObj rebuild_tree(nc_ctx *c, PyObj code, int *changed)
{
    PyObj consts = getattr_(c, code, "co_consts");
    PyObj newlist;
    cpy_ssize n, i;
    int local_changed = 0;

    if (!consts) return NULL;
    n = PY->PyTuple_Size(consts);
    if (n < 0) { PY->PyErr_Clear(); PY->Py_DecRef(consts); return NULL; }
    newlist = PY->PyList_New(0);

    for (i = 0; i < n; i++) {
        PyObj o = PY->PyTuple_GetItem(consts, i);
        PyObj repl = NULL;
        if (PY->PyObject_IsInstance(o, c->codetype) == 1) {
            int p = pend_index(c, o);
            if (p >= 0 && c->pend[p].idx >= 0) {
                PyObj nm = getattr_(c, o, "co_name");
                const char *name = nm ? PY->PyUnicode_AsUTF8(nm) : NULL;
                if (name) repl = make_trampoline(c, o, c->pend[p].idx, name);
                if (repl) {
                    local_changed = 1;
                    if (c->verbose) fprintf(stderr, "  native %s\n", c->pend[p].qual);
                }
                if (nm) PY->Py_DecRef(nm);
            }
            if (!repl) {
                int sub_changed = 0;
                repl = rebuild_tree(c, o, &sub_changed);
                if (repl && !sub_changed) { PY->Py_DecRef(repl); repl = NULL; }
                if (repl) local_changed = 1;
            }
        }
        if (repl) {
            PY->PyList_Append(newlist, repl);
            PY->Py_DecRef(repl);
        } else {
            PY->PyList_Append(newlist, o);
        }
    }
    PY->Py_DecRef(consts);

    if (!local_changed) { PY->Py_DecRef(newlist); return NULL; }
    {
        PyObj tup = PY->PyTuple_New(PY->PyList_Size(newlist));
        cpy_ssize k, m = PY->PyList_Size(newlist);
        PyObj out;
        for (k = 0; k < m; k++) {
            PyObj v = PY->PyList_GetItem(newlist, k);
            PY->Py_IncRef(v);
            PY->PyTuple_SetItem(tup, k, v);
        }
        PY->Py_DecRef(newlist);
        out = rebuild(c, code, tup);
        PY->Py_DecRef(tup);
        if (out) *changed = 1;
        return out;
    }
}

PyObj nc_transform(nc_ctx *c, PyObj code, const char *modname)
{
    int changed = 0, i;
    PyObj out;
    char prefix[128];

    if (!c->ready) return NULL;
    if (!c->codetype) {
        c->codetype = PY->PyObject_GetAttrString((PyObj)code, "__class__");
        if (!c->codetype) { PY->PyErr_Clear(); return NULL; }
    }
    _snprintf(prefix, sizeof(prefix) - 1, "%s.", modname ? modname : "m");
    prefix[sizeof(prefix) - 1] = 0;

    c->npend = 0;
    c->nfnmap = 0;
    c->ntyreg = 0;
    collect_fns(c, code, prefix, 0);
    for (i = 0; i < c->npend; i++) {
        if (!compile_fn(c, c->pend[i].code, c->pend[i].idx, c->pend[i].qual, 0))
            c->pend[i].idx = -1;
    }
    out = rebuild_tree(c, code, &changed);
    return changed ? out : NULL;
}

static const char *SELFTEST_SRC =
    "def __cpy_selftest__(n, d):\n"
    "    out = 0.0\n"
    "    acc = 0\n"
    "    y = 0\n"
    "    while y < n:\n"
    "        v = y * 2.0 / d - 1.0\n"
    "        if v > 0.0:\n"
    "            acc = acc + 1\n"
    "        out = out + v\n"
    "        acc = acc + y * y\n"
    "        y = y + 1\n"
    "    return out * 1000.0 + acc\n";

int nc_add_selftest(nc_ctx *c, PyObj *orig, PyObj *xform)
{
    PyObj code, t;
    if (!c->ready) return 0;
    code = PY->Py_CompileStringExFlags(SELFTEST_SRC, "<compyler-selftest>", CPY_FILE_INPUT, NULL, 0);
    if (!code) { PY->PyErr_Clear(); return 0; }
    t = nc_transform(c, code, "__cpy_st__");
    if (!t) { PY->Py_DecRef(code); return 0; }
    *orig = code;
    *xform = t;
    return 1;
}

int nc_write(nc_ctx *c, const wchar_t *path)
{
    buf out;
    PyObj pair, blob;
    char *bytes;
    cpy_ssize blen, i;
    int rc;

    if (!c->nfun) return 0;
    memset(&out, 0, sizeof(out));

    pair = PY->PyTuple_New(2);
    {
        cpy_ssize n = PY->PyList_Size(c->consts), k;
        PyObj t = PY->PyTuple_New(n);
        for (k = 0; k < n; k++) { PyObj v = PY->PyList_GetItem(c->consts, k); PY->Py_IncRef(v); PY->PyTuple_SetItem(t, k, v); }
        PY->PyTuple_SetItem(pair, 0, t);
        n = PY->PyList_Size(c->names);
        t = PY->PyTuple_New(n);
        for (k = 0; k < n; k++) { PyObj v = PY->PyList_GetItem(c->names, k); PY->Py_IncRef(v); PY->PyTuple_SetItem(t, k, v); }
        PY->PyTuple_SetItem(pair, 1, t);
    }
    blob = PY->PyMarshal_WriteObjectToString(pair, CPY_MARSHAL_VERSION);
    PY->Py_DecRef(pair);
    if (!blob) { PY->PyErr_Clear(); return 0; }
    bytes = PY->PyBytes_AsString(blob);
    blen = PY->PyBytes_Size(blob);

    bpf(&out, "#include \"cpyrt.h\"\n\n");
    if (getenv("COMPYLER_UNSAFE")) {
        bpf(&out, "#define cpy_addo(a,b,r) (*(r)=(a)+(b), 0)\n");
        bpf(&out, "#define cpy_addo_k(a,b,r) (*(r)=(a)+(b), 0)\n");
        bpf(&out, "#define cpy_subo(a,b,r) (*(r)=(a)-(b), 0)\n");
        bpf(&out, "#define cpy_subo_k(a,b,r) (*(r)=(a)-(b), 0)\n");
        bpf(&out, "#define cpy_ksub_o(a,b,r) (*(r)=(a)-(b), 0)\n");
        bpf(&out, "#define cpy_mulo(a,b,r) (*(r)=(a)*(b), 0)\n");
        bpf(&out, "#define cpy_mulo_k(a,b,r) (*(r)=(a)*(b), 0)\n\n");
    }
    bpf(&out, "#ifdef _MSC_VER\n#pragma warning(disable: 4102)\n#pragma warning(disable: 4101)\n#endif\n\n");
    bpf(&out, "static PyObject *cpy_consts;\nstatic PyObject *cpy_names;\n");
    bpf(&out, "#define K(i) PyTuple_GET_ITEM(cpy_consts, i)\n");
    bpf(&out, "#define N(i) PyTuple_GET_ITEM(cpy_names, i)\n\n");
    bpf(&out, "static PyObject *cpy_tc[%d];\n\n", c->nfun);
    for (i = 0; i < c->nfun; i++)
        bpf(&out, "static int cpyf_%d_core(PyObject *, cv *, int, cv *);\n", (int)i);
    bpf(&out, "\n");
    bpf(&out, "static int cv_call_bound(cv *r, cv fn, cv self, cv *args, int n)\n{\n"
              "  cv tmp[14]; int i;\n"
              "  if (n > 12) { PyErr_SetString(PyExc_SystemError, \"too many args\"); return -1; }\n"
              "  tmp[0] = self;\n"
              "  for (i = 0; i < n; i++) tmp[i + 1] = args[i];\n"
              "  return cv_call(r, fn, tmp, n + 1);\n}\n\n");
    bpf(&out, "static const unsigned char cpy_blob[] = {");
    for (i = 0; i < blen; i++)
        bpf(&out, "%s%u", (i % 24) ? "," : (i ? ",\n" : "\n"), (unsigned char)bytes[i]);
    bpf(&out, "};\n\n");
    PY->Py_DecRef(blob);

    bput(&out, c->code.p ? c->code.p : "", c->code.p ? c->code.n : 0);

    bpf(&out, "static PyObject *cpy_install(PyObject *self, PyObject *unused)\n{\n");
    bpf(&out, "  PyObject *b = PyEval_GetBuiltins();\n"
              "  PyObject *m = PyImport_ImportModule(\"_compyler_native\");\n"
              "  PyObject *d;\n  Py_ssize_t i = 0;\n  PyObject *k, *v;\n"
              "  (void)self; (void)unused;\n"
              "  if (!m || !b) return NULL;\n"
              "  d = PyModule_GetDict(m);\n"
              "  while (PyDict_Next(d, &i, &k, &v)) {\n"
              "    const char *s = PyUnicode_AsUTF8(k);\n"
              "    if (s && s[0] == '_' && s[1] == '_' && s[2] == 'c' && s[3] == 'p' && s[4] == 'y' && s[5] == '_')\n"
              "      PyDict_SetItem(b, k, v);\n"
              "  }\n"
              "  Py_DECREF(m);\n  Py_RETURN_NONE;\n}\n\n");

    bpf(&out, "static PyMethodDef cpy_methods[] = {\n");
    bput(&out, c->tab.p ? c->tab.p : "", c->tab.p ? c->tab.n : 0);
    bpf(&out, "  { \"install\", (PyCFunction)cpy_install, METH_NOARGS, NULL },\n");
    bpf(&out, "  { NULL, NULL, 0, NULL }\n};\n\n");
    bpf(&out, "static struct PyModuleDef cpy_module = {\n"
              "  PyModuleDef_HEAD_INIT, \"_compyler_native\", NULL, -1, cpy_methods,\n"
              "  NULL, NULL, NULL, NULL\n};\n\n");
    bpf(&out, "PyMODINIT_FUNC PyInit__compyler_native(void)\n{\n"
              "  PyObject *m, *pair;\n"
              "  pair = PyMarshal_ReadObjectFromString((const char *)cpy_blob, (Py_ssize_t)sizeof(cpy_blob));\n"
              "  if (!pair) return NULL;\n"
              "  cpy_consts = PyTuple_GET_ITEM(pair, 0);\n"
              "  cpy_names = PyTuple_GET_ITEM(pair, 1);\n"
              "  Py_INCREF(cpy_consts); Py_INCREF(cpy_names);\n"
              "  Py_DECREF(pair);\n"
              "  m = PyModule_Create(&cpy_module);\n"
              "  return m;\n}\n");

    rc = cpy_write_file(path, out.p, out.n);
    free(out.p);
    return rc;
}
