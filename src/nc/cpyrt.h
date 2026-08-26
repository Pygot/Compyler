#ifndef CPY_RT_H
#define CPY_RT_H

#include <Python.h>
#include <marshal.h>
#include <stdint.h>
#include <string.h>

#if defined(_MSC_VER)
#include <intrin.h>
#define CPY_INLINE static __forceinline
#else
#define CPY_INLINE static inline
#endif

#if !defined(Py_GIL_DISABLED) && PY_VERSION_HEX < 0x030E0000
#define CPY_DICT_VERSION(d) (((PyDictObject *)(d))->ma_version_tag)
#else
#define CPY_DICT_VERSION(d) ((uint64_t)0)
#endif

#define CPY_T_NIL 0
#define CPY_T_INT 1
#define CPY_T_FLT 2
#define CPY_T_OBJ 3

typedef struct {
    int32_t t;
    int64_t b;
} cv;

typedef struct {
    int       fast;
    int64_t   cur, stop, step;
    PyObject *it;
} cv_iter;

static const cv CV_NIL = { CPY_T_NIL, 0 };

CPY_INLINE cv cv_int(int64_t v) { cv r; r.t = CPY_T_INT; r.b = v; return r; }

CPY_INLINE cv cv_flt(double v)
{
    cv r;
    int64_t x;
    memcpy(&x, &v, sizeof(x));
    r.t = CPY_T_FLT;
    r.b = x;
    return r;
}

CPY_INLINE cv cv_obj(PyObject *o)
{
    cv r;
    int64_t x = 0;
    memcpy(&x, &o, sizeof(o));
    r.t = CPY_T_OBJ;
    r.b = x;
    return r;
}

CPY_INLINE double cv_d(cv v)
{
    double d;
    memcpy(&d, &v.b, sizeof(d));
    return d;
}

CPY_INLINE PyObject *cv_o(cv v)
{
    PyObject *p;
    memcpy(&p, &v.b, sizeof(p));
    return p;
}

CPY_INLINE cv cv_bool(int b)
{
    PyObject *o = b ? Py_True : Py_False;
    Py_INCREF(o);
    return cv_obj(o);
}

CPY_INLINE void cv_clear(cv *v)
{
    if (v->t == CPY_T_OBJ) {
        PyObject *o = cv_o(*v);
        v->t = CPY_T_NIL;
        v->b = 0;
        Py_XDECREF(o);
    } else {
        v->t = CPY_T_NIL;
    }
}

CPY_INLINE void cv_hold(cv v)
{
    if (v.t == CPY_T_OBJ) Py_INCREF(cv_o(v));
}

CPY_INLINE int64_t cpy_pl_i64(PyObject *e)
{
#if PY_VERSION_HEX >= 0x030C0000
    if (PyUnstable_Long_IsCompact((PyLongObject *)e))
        return (int64_t)PyUnstable_Long_CompactValue((PyLongObject *)e);
#endif
    return PyLong_AsLongLong(e);
}

CPY_INLINE cv cv_norm(PyObject *o)
{
    if (PyLong_CheckExact(o)) {
        int ovf = 0;
        long long v;
#if PY_VERSION_HEX >= 0x030C0000
        if (PyUnstable_Long_IsCompact((PyLongObject *)o))
            return cv_int((int64_t)PyUnstable_Long_CompactValue((PyLongObject *)o));
#endif
        v = PyLong_AsLongLongAndOverflow(o, &ovf);
        if (!ovf && !(v == -1 && PyErr_Occurred())) return cv_int((int64_t)v);
        PyErr_Clear();
    } else if (PyFloat_CheckExact(o)) {
        return cv_flt(PyFloat_AS_DOUBLE(o));
    }
    Py_INCREF(o);
    return cv_obj(o);
}

CPY_INLINE cv cv_norm_steal(PyObject *o)
{
    if (PyLong_CheckExact(o) || PyFloat_CheckExact(o)) {
        cv r = cv_norm(o);
        Py_DECREF(o);
        return r;
    }
    return cv_obj(o);
}

CPY_INLINE PyObject *cv_box(cv v)
{
    switch (v.t) {
    case CPY_T_INT: return PyLong_FromLongLong((long long)v.b);
    case CPY_T_FLT: return PyFloat_FromDouble(cv_d(v));
    case CPY_T_OBJ: { PyObject *o = cv_o(v); Py_INCREF(o); return o; }
    default:        Py_RETURN_NONE;
    }
}

CPY_INLINE int cv_is_none(cv v)
{
    return v.t == CPY_T_OBJ && cv_o(v) == Py_None;
}

CPY_INLINE int cpy_addo(int64_t a, int64_t b, int64_t *r)
{
#if defined(__clang__) || (defined(__GNUC__) && !defined(_MSC_VER))
    return (int)__builtin_add_overflow(a, b, r);
#else
    uint64_t u = (uint64_t)a + (uint64_t)b;
    *r = (int64_t)u;
    return (int)((((uint64_t)(a ^ (int64_t)u)) & ((uint64_t)(b ^ (int64_t)u))) >> 63);
#endif
}

CPY_INLINE int cpy_subo(int64_t a, int64_t b, int64_t *r)
{
#if defined(__clang__) || (defined(__GNUC__) && !defined(_MSC_VER))
    return (int)__builtin_sub_overflow(a, b, r);
#else
    uint64_t u = (uint64_t)a - (uint64_t)b;
    *r = (int64_t)u;
    return (int)((((uint64_t)(a ^ b)) & ((uint64_t)(a ^ (int64_t)u))) >> 63);
#endif
}

CPY_INLINE int cpy_mulo(int64_t a, int64_t b, int64_t *r)
{
#if defined(__clang__) || (defined(__GNUC__) && !defined(_MSC_VER))
    return (int)__builtin_mul_overflow(a, b, r);
#elif defined(_MSC_VER) && defined(_M_X64)
    __int64 hi;
    __int64 lo = _mul128((__int64)a, (__int64)b, &hi);
    *r = (int64_t)lo;
    return hi != (lo >> 63);
#else
    __int128 p = (__int128)a * (__int128)b;
    *r = (int64_t)p;
    return p != (__int128)(int64_t)p;
#endif
}

CPY_INLINE int cpy_addo_k(int64_t a, int64_t k, int64_t *r)
{
#if defined(__clang__) || (defined(__GNUC__) && !defined(_MSC_VER))
    return (int)__builtin_add_overflow(a, k, r);
#else
    if (k >= 0) { if (a > INT64_MAX - k) return 1; }
    else { if (a < INT64_MIN - k) return 1; }
    *r = a + k;
    return 0;
#endif
}

CPY_INLINE int cpy_subo_k(int64_t a, int64_t k, int64_t *r)
{
#if defined(__clang__) || (defined(__GNUC__) && !defined(_MSC_VER))
    return (int)__builtin_sub_overflow(a, k, r);
#else
    if (k >= 0) { if (a < INT64_MIN + k) return 1; }
    else { if (k == INT64_MIN) return 1; if (a > INT64_MAX + k) return 1; }
    *r = a - k;
    return 0;
#endif
}

CPY_INLINE int cpy_ksub_o(int64_t k, int64_t a, int64_t *r)
{
#if defined(__clang__) || (defined(__GNUC__) && !defined(_MSC_VER))
    return (int)__builtin_sub_overflow(k, a, r);
#else
    if (a == INT64_MIN) return 1;
    return cpy_addo_k(-a, k, r);
#endif
}

CPY_INLINE int cpy_mulo_k(int64_t a, int64_t k, int64_t *r)
{
#if defined(__clang__) || (defined(__GNUC__) && !defined(_MSC_VER))
    return (int)__builtin_mul_overflow(a, k, r);
#else
    if (k == 0) { *r = 0; return 0; }
    if (k == -1) { if (a == INT64_MIN) return 1; *r = -a; return 0; }
    if (k > 0) { if (a > INT64_MAX / k || a < INT64_MIN / k) return 1; }
    else { if (a < INT64_MAX / k || a > INT64_MIN / k) return 1; }
    *r = a * k;
    return 0;
#endif
}

CPY_INLINE int cpy_ifdiv(int64_t a, int64_t b, int64_t *r)
{
    int64_t q;
    if (b == 0) return 1;
    if (b == -1 && a == INT64_MIN) return 1;
    q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
    *r = q;
    return 0;
}

CPY_INLINE int cpy_imod(int64_t a, int64_t b, int64_t *r)
{
    int64_t m;
    if (b == 0) return 1;
    if (b == -1) { *r = 0; return 0; }
    m = a % b;
    if (m != 0 && ((m < 0) != (b < 0))) m += b;
    *r = m;
    return 0;
}

CPY_INLINE int cpy_ishl(int64_t a, int64_t b, int64_t *r)
{
    int64_t v;
    if (b < 0) return 1;
    if (a == 0) { *r = 0; return 0; }
    if (b >= 63) return 1;
    v = (int64_t)((uint64_t)a << b);
    if ((v >> b) != a) return 1;
    *r = v;
    return 0;
}

CPY_INLINE int cpy_ishr(int64_t a, int64_t b, int64_t *r)
{
    if (b < 0) return 1;
    *r = (b >= 63) ? (a < 0 ? -1 : 0) : (a >> b);
    return 0;
}

CPY_INLINE double cv_asdbl(cv v) { return v.t == CPY_T_INT ? (double)v.b : cv_d(v); }
#define CPY_NUM(v) ((v).t == CPY_T_INT || (v).t == CPY_T_FLT)

static int cv_binary_slow(cv *r, cv a, cv b, int op);
static int cv_binary_slow_ip(cv *r, cv a, cv b, int op);

#define CPY_OP_ADD  0
#define CPY_OP_SUB  1
#define CPY_OP_MUL  2
#define CPY_OP_TDIV 3
#define CPY_OP_FDIV 4
#define CPY_OP_MOD  5
#define CPY_OP_POW  6
#define CPY_OP_AND  7
#define CPY_OP_OR   8
#define CPY_OP_XOR  9
#define CPY_OP_SHL  10
#define CPY_OP_SHR  11
#define CPY_OP_MATM 12

CPY_INLINE int cv_add(cv *r, cv a, cv b, int ip)
{
    if (a.t == CPY_T_INT && b.t == CPY_T_INT) {
        int64_t x;
        if (!cpy_addo(a.b, b.b, &x)) { *r = cv_int(x); return 0; }
    } else if (CPY_NUM(a) && CPY_NUM(b)) {
        *r = cv_flt(cv_asdbl(a) + cv_asdbl(b));
        return 0;
    }
    return ip ? cv_binary_slow_ip(r, a, b, CPY_OP_ADD) : cv_binary_slow(r, a, b, CPY_OP_ADD);
}

CPY_INLINE int cv_sub(cv *r, cv a, cv b, int ip)
{
    if (a.t == CPY_T_INT && b.t == CPY_T_INT) {
        int64_t x;
        if (!cpy_subo(a.b, b.b, &x)) { *r = cv_int(x); return 0; }
    } else if (CPY_NUM(a) && CPY_NUM(b)) {
        *r = cv_flt(cv_asdbl(a) - cv_asdbl(b));
        return 0;
    }
    return ip ? cv_binary_slow_ip(r, a, b, CPY_OP_SUB) : cv_binary_slow(r, a, b, CPY_OP_SUB);
}

CPY_INLINE int cv_mul(cv *r, cv a, cv b, int ip)
{
    if (a.t == CPY_T_INT && b.t == CPY_T_INT) {
        int64_t x;
        if (!cpy_mulo(a.b, b.b, &x)) { *r = cv_int(x); return 0; }
    } else if (CPY_NUM(a) && CPY_NUM(b)) {
        *r = cv_flt(cv_asdbl(a) * cv_asdbl(b));
        return 0;
    }
    return ip ? cv_binary_slow_ip(r, a, b, CPY_OP_MUL) : cv_binary_slow(r, a, b, CPY_OP_MUL);
}

CPY_INLINE int cv_tdiv(cv *r, cv a, cv b, int ip)
{
    if (CPY_NUM(a) && CPY_NUM(b)) {
        double y = cv_asdbl(b);
        if (y != 0.0) { *r = cv_flt(cv_asdbl(a) / y); return 0; }
        PyErr_SetString(PyExc_ZeroDivisionError, "division by zero");
        return -1;
    }
    return ip ? cv_binary_slow_ip(r, a, b, CPY_OP_TDIV) : cv_binary_slow(r, a, b, CPY_OP_TDIV);
}

CPY_INLINE int cv_fdiv(cv *r, cv a, cv b, int ip)
{
    if (a.t == CPY_T_INT && b.t == CPY_T_INT) {
        int64_t x = a.b, y = b.b, q;
        if (y == 0) { PyErr_SetString(PyExc_ZeroDivisionError, "integer division or modulo by zero"); return -1; }
        if (!(y == -1 && x == INT64_MIN)) {
            q = x / y;
            if ((x % y != 0) && ((x < 0) != (y < 0))) q--;
            *r = cv_int(q);
            return 0;
        }
    }
    return ip ? cv_binary_slow_ip(r, a, b, CPY_OP_FDIV) : cv_binary_slow(r, a, b, CPY_OP_FDIV);
}

CPY_INLINE int cv_mod(cv *r, cv a, cv b, int ip)
{
    if (a.t == CPY_T_INT && b.t == CPY_T_INT) {
        int64_t x = a.b, y = b.b, m;
        if (y == 0) { PyErr_SetString(PyExc_ZeroDivisionError, "integer division or modulo by zero"); return -1; }
        if (y == -1) { *r = cv_int(0); return 0; }
        m = x % y;
        if (m != 0 && ((m < 0) != (y < 0))) m += y;
        *r = cv_int(m);
        return 0;
    }
    return ip ? cv_binary_slow_ip(r, a, b, CPY_OP_MOD) : cv_binary_slow(r, a, b, CPY_OP_MOD);
}

CPY_INLINE int cv_bitop(cv *r, cv a, cv b, int op, int ip)
{
    if (a.t == CPY_T_INT && b.t == CPY_T_INT) {
        int64_t x = a.b, y = b.b;
        switch (op) {
        case CPY_OP_AND: *r = cv_int(x & y); return 0;
        case CPY_OP_OR:  *r = cv_int(x | y); return 0;
        case CPY_OP_XOR: *r = cv_int(x ^ y); return 0;
        case CPY_OP_SHL:
            if (y >= 0 && y < 40 && x > -((int64_t)1 << 22) && x < ((int64_t)1 << 22)) {
                *r = cv_int(x << y);
                return 0;
            }
            break;
        case CPY_OP_SHR:
            if (y >= 0) { *r = cv_int(y >= 63 ? (x < 0 ? -1 : 0) : (x >> y)); return 0; }
            break;
        default: break;
        }
    }
    return ip ? cv_binary_slow_ip(r, a, b, op) : cv_binary_slow(r, a, b, op);
}

static int cv_binary_slow(cv *r, cv a, cv b, int op)
{
    PyObject *x = cv_box(a), *y, *z = NULL;
    if (!x) return -1;
    y = cv_box(b);
    if (!y) { Py_DECREF(x); return -1; }
    switch (op) {
    case CPY_OP_ADD:  z = PyNumber_Add(x, y); break;
    case CPY_OP_SUB:  z = PyNumber_Subtract(x, y); break;
    case CPY_OP_MUL:  z = PyNumber_Multiply(x, y); break;
    case CPY_OP_TDIV: z = PyNumber_TrueDivide(x, y); break;
    case CPY_OP_FDIV: z = PyNumber_FloorDivide(x, y); break;
    case CPY_OP_MOD:  z = PyNumber_Remainder(x, y); break;
    case CPY_OP_POW:  z = PyNumber_Power(x, y, Py_None); break;
    case CPY_OP_AND:  z = PyNumber_And(x, y); break;
    case CPY_OP_OR:   z = PyNumber_Or(x, y); break;
    case CPY_OP_XOR:  z = PyNumber_Xor(x, y); break;
    case CPY_OP_SHL:  z = PyNumber_Lshift(x, y); break;
    case CPY_OP_SHR:  z = PyNumber_Rshift(x, y); break;
    case CPY_OP_MATM: z = PyNumber_MatrixMultiply(x, y); break;
    }
    Py_DECREF(x);
    Py_DECREF(y);
    if (!z) return -1;
    *r = cv_norm_steal(z);
    return 0;
}

static int cv_binary_slow_ip(cv *r, cv a, cv b, int op)
{
    PyObject *x = cv_box(a), *y, *z = NULL;
    if (!x) return -1;
    y = cv_box(b);
    if (!y) { Py_DECREF(x); return -1; }
    switch (op) {
    case CPY_OP_ADD:  z = PyNumber_InPlaceAdd(x, y); break;
    case CPY_OP_SUB:  z = PyNumber_InPlaceSubtract(x, y); break;
    case CPY_OP_MUL:  z = PyNumber_InPlaceMultiply(x, y); break;
    case CPY_OP_TDIV: z = PyNumber_InPlaceTrueDivide(x, y); break;
    case CPY_OP_FDIV: z = PyNumber_InPlaceFloorDivide(x, y); break;
    case CPY_OP_MOD:  z = PyNumber_InPlaceRemainder(x, y); break;
    case CPY_OP_POW:  z = PyNumber_InPlacePower(x, y, Py_None); break;
    case CPY_OP_AND:  z = PyNumber_InPlaceAnd(x, y); break;
    case CPY_OP_OR:   z = PyNumber_InPlaceOr(x, y); break;
    case CPY_OP_XOR:  z = PyNumber_InPlaceXor(x, y); break;
    case CPY_OP_SHL:  z = PyNumber_InPlaceLshift(x, y); break;
    case CPY_OP_SHR:  z = PyNumber_InPlaceRshift(x, y); break;
    case CPY_OP_MATM: z = PyNumber_InPlaceMatrixMultiply(x, y); break;
    }
    Py_DECREF(x);
    Py_DECREF(y);
    if (!z) return -1;
    *r = cv_norm_steal(z);
    return 0;
}

CPY_INLINE int cv_pow(cv *r, cv a, cv b, int ip)
{
    if (a.t == CPY_T_INT && b.t == CPY_T_INT && b.b >= 0 && b.b <= 64) {
        int64_t base = a.b, acc = 1, e = b.b;
        while (e > 0) {
            if (e & 1) { if (cpy_mulo(acc, base, &acc)) goto slow; }
            e >>= 1;
            if (e && cpy_mulo(base, base, &base)) goto slow;
        }
        *r = cv_int(acc);
        return 0;
    }
slow:
    return ip ? cv_binary_slow_ip(r, a, b, CPY_OP_POW) : cv_binary_slow(r, a, b, CPY_OP_POW);
}

CPY_INLINE int cv_cmp(cv *r, cv a, cv b, int op)
{
    if (a.t == CPY_T_INT && b.t == CPY_T_INT) {
        int64_t x = a.b, y = b.b;
        int v;
        switch (op) {
        case Py_LT: v = x <  y; break;
        case Py_LE: v = x <= y; break;
        case Py_EQ: v = x == y; break;
        case Py_NE: v = x != y; break;
        case Py_GT: v = x >  y; break;
        default:    v = x >= y; break;
        }
        *r = cv_bool(v);
        return 0;
    }
    if (CPY_NUM(a) && CPY_NUM(b)) {
        double x = cv_asdbl(a), y = cv_asdbl(b);
        int v;
        switch (op) {
        case Py_LT: v = x <  y; break;
        case Py_LE: v = x <= y; break;
        case Py_EQ: v = x == y; break;
        case Py_NE: v = x != y; break;
        case Py_GT: v = x >  y; break;
        default:    v = x >= y; break;
        }
        *r = cv_bool(v);
        return 0;
    }
    {
        PyObject *x = cv_box(a), *y, *z;
        if (!x) return -1;
        y = cv_box(b);
        if (!y) { Py_DECREF(x); return -1; }
        z = PyObject_RichCompare(x, y, op);
        Py_DECREF(x);
        Py_DECREF(y);
        if (!z) return -1;
        *r = cv_obj(z);
        return 0;
    }
}

CPY_INLINE int cv_cmp_br(cv a, cv b, int op)
{
    if (a.t == CPY_T_INT && b.t == CPY_T_INT) {
        int64_t x = a.b, y = b.b;
        switch (op) {
        case Py_LT: return x <  y;
        case Py_LE: return x <= y;
        case Py_EQ: return x == y;
        case Py_NE: return x != y;
        case Py_GT: return x >  y;
        default:    return x >= y;
        }
    }
    if (CPY_NUM(a) && CPY_NUM(b)) {
        double x = cv_asdbl(a), y = cv_asdbl(b);
        switch (op) {
        case Py_LT: return x <  y;
        case Py_LE: return x <= y;
        case Py_EQ: return x == y;
        case Py_NE: return x != y;
        case Py_GT: return x >  y;
        default:    return x >= y;
        }
    }
    {
        cv r;
        int v;
        if (cv_cmp(&r, a, b, op)) return -1;
        v = PyObject_IsTrue(cv_o(r));
        cv_clear(&r);
        return v;
    }
}

CPY_INLINE int cv_truth(cv v)
{
    switch (v.t) {
    case CPY_T_INT: return v.b != 0;
    case CPY_T_FLT: return cv_d(v) != 0.0;
    case CPY_T_OBJ: {
        PyObject *o = cv_o(v);
        if (o == Py_True) return 1;
        if (o == Py_False || o == Py_None) return 0;
        return PyObject_IsTrue(o);
    }
    default: return 0;
    }
}

CPY_INLINE int cv_is(cv a, cv b)
{
    if (a.t == CPY_T_INT && b.t == CPY_T_INT) return a.b == b.b;
    if (a.t != b.t) return 0;
    if (a.t == CPY_T_FLT) return cv_d(a) == cv_d(b);
    return cv_o(a) == cv_o(b);
}

CPY_INLINE int cv_neg(cv *r, cv a)
{
    if (a.t == CPY_T_INT && a.b != INT64_MIN) { *r = cv_int(-a.b); return 0; }
    if (a.t == CPY_T_FLT) { *r = cv_flt(-cv_d(a)); return 0; }
    {
        PyObject *x = cv_box(a), *z;
        if (!x) return -1;
        z = PyNumber_Negative(x);
        Py_DECREF(x);
        if (!z) return -1;
        *r = cv_norm_steal(z);
        return 0;
    }
}

CPY_INLINE int cv_invert(cv *r, cv a)
{
    if (a.t == CPY_T_INT) { *r = cv_int(~a.b); return 0; }
    {
        PyObject *x = cv_box(a), *z;
        if (!x) return -1;
        z = PyNumber_Invert(x);
        Py_DECREF(x);
        if (!z) return -1;
        *r = cv_norm_steal(z);
        return 0;
    }
}

CPY_INLINE int cv_getitem(cv *r, cv c, cv k)
{
    if (c.t == CPY_T_OBJ && k.t == CPY_T_INT) {
        PyObject *o = cv_o(c);
        if (PyList_CheckExact(o)) {
            Py_ssize_t n = PyList_GET_SIZE(o), i = (Py_ssize_t)k.b;
            if (i < 0) i += n;
            if (i >= 0 && i < n) { *r = cv_norm(PyList_GET_ITEM(o, i)); return 0; }
            PyErr_SetString(PyExc_IndexError, "list index out of range");
            return -1;
        }
        if (PyTuple_CheckExact(o)) {
            Py_ssize_t n = PyTuple_GET_SIZE(o), i = (Py_ssize_t)k.b;
            if (i < 0) i += n;
            if (i >= 0 && i < n) { *r = cv_norm(PyTuple_GET_ITEM(o, i)); return 0; }
            PyErr_SetString(PyExc_IndexError, "tuple index out of range");
            return -1;
        }
        if (PyUnicode_CheckExact(o) && PyUnicode_IS_READY(o)) {
            Py_ssize_t n = PyUnicode_GET_LENGTH(o), i = (Py_ssize_t)k.b;
            if (i < 0) i += n;
            if (i >= 0 && i < n) {
                PyObject *z = PyUnicode_FromOrdinal((int)PyUnicode_READ_CHAR(o, i));
                if (!z) return -1;
                *r = cv_obj(z);
                return 0;
            }
            PyErr_SetString(PyExc_IndexError, "string index out of range");
            return -1;
        }
    }
    if (c.t == CPY_T_OBJ && PyDict_CheckExact(cv_o(c))) {
        PyObject *o = cv_o(c), *i = cv_box(k), *v;
        if (!i) return -1;
        v = PyDict_GetItemWithError(o, i);
        if (v) { Py_INCREF(v); Py_DECREF(i); *r = cv_norm_steal(v); return 0; }
        if (PyErr_Occurred()) { Py_DECREF(i); return -1; }
        {
            PyObject *t = PyTuple_Pack(1, i);
            if (t) { PyErr_SetObject(PyExc_KeyError, t); Py_DECREF(t); }
        }
        Py_DECREF(i);
        return -1;
    }
    {
        PyObject *o = cv_box(c), *i, *z;
        if (!o) return -1;
        i = cv_box(k);
        if (!i) { Py_DECREF(o); return -1; }
        z = PyObject_GetItem(o, i);
        Py_DECREF(o);
        Py_DECREF(i);
        if (!z) return -1;
        *r = cv_norm_steal(z);
        return 0;
    }
}

CPY_INLINE int cv_setitem(cv c, cv k, cv v)
{
    if (c.t == CPY_T_OBJ && k.t == CPY_T_INT && PyList_CheckExact(cv_o(c))) {
        PyObject *o = cv_o(c);
        Py_ssize_t n = PyList_GET_SIZE(o), i = (Py_ssize_t)k.b;
        if (i < 0) i += n;
        if (i >= 0 && i < n) {
            PyObject *nv = cv_box(v), *old;
            if (!nv) return -1;
            old = PyList_GET_ITEM(o, i);
            PyList_SET_ITEM(o, i, nv);
            Py_XDECREF(old);
            return 0;
        }
        PyErr_SetString(PyExc_IndexError, "list assignment index out of range");
        return -1;
    }
    if (c.t == CPY_T_OBJ && PyDict_CheckExact(cv_o(c))) {
        PyObject *o = cv_o(c), *i = cv_box(k), *x;
        int rc;
        if (!i) return -1;
        x = cv_box(v);
        if (!x) { Py_DECREF(i); return -1; }
        rc = PyDict_SetItem(o, i, x);
        Py_DECREF(i);
        Py_DECREF(x);
        return rc;
    }
    {
        PyObject *o = cv_box(c), *i, *x;
        int rc;
        if (!o) return -1;
        i = cv_box(k);
        if (!i) { Py_DECREF(o); return -1; }
        x = cv_box(v);
        if (!x) { Py_DECREF(o); Py_DECREF(i); return -1; }
        rc = PyObject_SetItem(o, i, x);
        Py_DECREF(o);
        Py_DECREF(i);
        Py_DECREF(x);
        return rc;
    }
}

CPY_INLINE int cpy_fin(double x) { return x - x == 0.0; }
CPY_INLINE double cpy_deg(double x) { return x * (180.0 / 3.141592653589793); }
CPY_INLINE double cpy_rad(double x) { return x * (3.141592653589793 / 180.0); }

static PyObject *cpy_mathmod;

CPY_INLINE int cpy_intr_ok(PyObject *mod, PyObject *name)
{
    PyObject *cur, *d;
    const char *mn, *wn;
    if (!mod || !PyModule_CheckExact(mod)) return 0;
    if (!cpy_mathmod) {
        cpy_mathmod = PyImport_ImportModule("math");
        if (!cpy_mathmod) { PyErr_Clear(); return 0; }
    }
    if (mod != cpy_mathmod) return 0;
    d = PyModule_GetDict(mod);
    if (!d) return 0;
    cur = PyDict_GetItemWithError(d, name);
    if (!cur) { PyErr_Clear(); return 0; }
    if (!PyCFunction_Check(cur)) return 0;
    if (PyCFunction_GET_SELF(cur) != mod) return 0;
    mn = ((PyCFunctionObject *)cur)->m_ml->ml_name;
    wn = PyUnicode_AsUTF8(name);
    if (!mn || !wn || strcmp(mn, wn)) return 0;
    return 1;
}

CPY_INLINE int cpy_builtin_ok(PyObject *f, PyObject *name)
{
    const char *mn, *wn;
    if (!f || !PyCFunction_Check(f)) return 0;
    mn = ((PyCFunctionObject *)f)->m_ml->ml_name;
    wn = PyUnicode_AsUTF8(name);
    if (!mn || !wn || strcmp(mn, wn)) return 0;
    return 1;
}

CPY_INLINE int cv_getattr(cv *r, cv a, PyObject *name)
{
    PyObject *o = cv_box(a), *z;
    if (!o) return -1;
    z = PyObject_GetAttr(o, name);
    Py_DECREF(o);
    if (!z) return -1;
    *r = cv_norm_steal(z);
    return 0;
}

typedef struct {
    PyObject *recv;
    uint64_t  ver;
    PyObject *val;
} cpy_ac;

CPY_INLINE int cv_getattr_c(cv *r, cv a, PyObject *name, cpy_ac *ac)
{
    if (a.t == CPY_T_OBJ) {
        PyObject *o = cv_o(a);
        if (PyModule_CheckExact(o)) {
            PyObject *d = PyModule_GetDict(o), *v;
            uint64_t ver;
            if (!d) return cv_getattr(r, a, name);
            ver = CPY_DICT_VERSION(d);
            if (ver && ac->val && ac->recv == o && ac->ver == ver) {
                *r = cv_norm(ac->val);
                return 0;
            }
            v = PyDict_GetItemWithError(d, name);
            if (v) {
                if (ver) { ac->recv = o; ac->ver = ver; ac->val = v; }
                *r = cv_norm(v);
                return 0;
            }
            if (PyErr_Occurred()) return -1;
        }
    }
    return cv_getattr(r, a, name);
}

CPY_INLINE int cv_setattr(cv a, PyObject *name, cv v)
{
    PyObject *o = cv_box(a), *x;
    int rc;
    if (!o) return -1;
    x = cv_box(v);
    if (!x) { Py_DECREF(o); return -1; }
    rc = PyObject_SetAttr(o, name, x);
    Py_DECREF(o);
    Py_DECREF(x);
    return rc;
}

static int cv_iter_init(cv_iter *it, cv src)
{
    it->fast = 0;
    it->it = NULL;
    if (src.t == CPY_T_OBJ && PyRange_Check(cv_o(src))) {
        PyObject *o = cv_o(src);
        PyObject *a = PyObject_GetAttrString(o, "start");
        PyObject *b = PyObject_GetAttrString(o, "stop");
        PyObject *c = PyObject_GetAttrString(o, "step");
        if (a && b && c && PyLong_CheckExact(a) && PyLong_CheckExact(b) && PyLong_CheckExact(c)) {
            int e1 = 0, e2 = 0, e3 = 0;
            long long s = PyLong_AsLongLongAndOverflow(a, &e1);
            long long t = PyLong_AsLongLongAndOverflow(b, &e2);
            long long p = PyLong_AsLongLongAndOverflow(c, &e3);
            if (!e1 && !e2 && !e3 && p != 0) {
                it->fast = 1;
                it->cur = (int64_t)s;
                it->stop = (int64_t)t;
                it->step = (int64_t)p;
            }
        }
        Py_XDECREF(a);
        Py_XDECREF(b);
        Py_XDECREF(c);
        PyErr_Clear();
        if (it->fast) return 0;
    }
    {
        PyObject *o = cv_box(src);
        if (!o) return -1;
        it->it = PyObject_GetIter(o);
        Py_DECREF(o);
        return it->it ? 0 : -1;
    }
}

CPY_INLINE int cv_iter_next(cv_iter *it, cv *out)
{
    if (it->fast) {
        if (it->step > 0 ? it->cur >= it->stop : it->cur <= it->stop) return 0;
        *out = cv_int(it->cur);
        it->cur += it->step;
        return 1;
    }
    {
        PyObject *v = Py_TYPE(it->it)->tp_iternext(it->it);
        if (v) { *out = cv_norm_steal(v); return 1; }
        if (PyErr_Occurred()) {
            if (!PyErr_ExceptionMatches(PyExc_StopIteration)) return -1;
            PyErr_Clear();
        }
        return 0;
    }
}

CPY_INLINE void cv_iter_clear(cv_iter *it)
{
    it->fast = 0;
    if (it->it) { PyObject *o = it->it; it->it = NULL; Py_DECREF(o); }
}

#define CPY_B_LEN   0
#define CPY_B_ORD   1
#define CPY_B_ABS   2
#define CPY_B_CHR   3
#define CPY_B_FLOAT 4
#define CPY_B_INT   5
#define CPY_B_N     6

static PyObject *cpy_bfn[CPY_B_N];

CPY_INLINE PyObject *cpy_bresolve(int id)
{
    static const char *BN[CPY_B_N] = { "len", "ord", "abs", "chr", "float", "int" };
    PyObject *d;
    if (cpy_bfn[id]) return cpy_bfn[id];
    d = PyEval_GetBuiltins();
    if (!d) return NULL;
    cpy_bfn[id] = PyDict_GetItemString(d, BN[id]);
    if (!cpy_bfn[id]) PyErr_Clear();
    return cpy_bfn[id];
}

static int cv_bcall(cv *r, cv fn, cv *a, int n, int id)
{
    PyObject *f, *o;

    if (n != 1 || fn.t != CPY_T_OBJ) return 1;
    f = cv_o(fn);
    if (id == CPY_B_FLOAT) { if (f != (PyObject *)&PyFloat_Type) return 1; }
    else if (id == CPY_B_INT) { if (f != (PyObject *)&PyLong_Type) return 1; }
    else if (f != cpy_bresolve(id)) return 1;

    switch (id) {
    case CPY_B_LEN: {
        Py_ssize_t z;
        if (a[0].t != CPY_T_OBJ) return 1;
        o = cv_o(a[0]);
        if (PyList_CheckExact(o)) z = PyList_GET_SIZE(o);
        else if (PyUnicode_CheckExact(o)) z = PyUnicode_GET_LENGTH(o);
        else if (PyTuple_CheckExact(o)) z = PyTuple_GET_SIZE(o);
        else if (PyDict_CheckExact(o)) z = PyDict_GET_SIZE(o);
        else if (PyBytes_CheckExact(o)) z = PyBytes_GET_SIZE(o);
        else return 1;
        *r = cv_int((int64_t)z);
        return 0;
    }
    case CPY_B_ORD:
        if (a[0].t != CPY_T_OBJ) return 1;
        o = cv_o(a[0]);
        if (PyUnicode_CheckExact(o) && PyUnicode_GET_LENGTH(o) == 1) {
            *r = cv_int((int64_t)PyUnicode_READ_CHAR(o, 0));
            return 0;
        }
        if (PyBytes_CheckExact(o) && PyBytes_GET_SIZE(o) == 1) {
            *r = cv_int((int64_t)(unsigned char)PyBytes_AS_STRING(o)[0]);
            return 0;
        }
        return 1;
    case CPY_B_ABS:
        if (a[0].t == CPY_T_INT) {
            if (a[0].b == INT64_MIN) return 1;
            *r = cv_int(a[0].b < 0 ? -a[0].b : a[0].b);
            return 0;
        }
        if (a[0].t == CPY_T_FLT) { *r = cv_flt(fabs(cv_d(a[0]))); return 0; }
        return 1;
    case CPY_B_CHR:
        if (a[0].t == CPY_T_INT && a[0].b >= 0 && a[0].b <= 0x10FFFF) {
            PyObject *z = PyUnicode_FromOrdinal((int)a[0].b);
            if (!z) return -1;
            *r = cv_obj(z);
            return 0;
        }
        return 1;
    case CPY_B_FLOAT:
        if (a[0].t == CPY_T_INT) { *r = cv_flt((double)a[0].b); return 0; }
        if (a[0].t == CPY_T_FLT) { *r = a[0]; return 0; }
        return 1;
    case CPY_B_INT:
        if (a[0].t == CPY_T_INT) { *r = a[0]; return 0; }
        if (a[0].t == CPY_T_FLT) {
            double v = cv_d(a[0]);
            if (cpy_fin(v) && v > -9223372036854775809.0 && v < 9223372036854775808.0) {
                *r = cv_int((int64_t)v);
                return 0;
            }
            return 1;
        }
        return 1;
    default:
        return 1;
    }
}

static int cv_call(cv *r, cv fn, cv *args, int n)
{
    PyObject *stack[16];
    PyObject *f, *z;
    int i, k;

    if (n < 0 || n > 15) {
        PyErr_SetString(PyExc_SystemError, "compyler: call arity out of range");
        return -1;
    }
    f = cv_box(fn);
    if (!f) return -1;
    for (i = 0; i < n; i++) {
        stack[i] = cv_box(args[i]);
        if (!stack[i]) {
            for (k = 0; k < i; k++) Py_DECREF(stack[k]);
            Py_DECREF(f);
            return -1;
        }
    }
    z = PyObject_Vectorcall(f, stack, (size_t)n, NULL);
    for (i = 0; i < n; i++) Py_DECREF(stack[i]);
    Py_DECREF(f);
    if (!z) return -1;
    *r = cv_norm_steal(z);
    return 0;
}

typedef struct {
    uint64_t  gver, bver;
    PyObject *val;
} cpy_gc;


CPY_INLINE int cv_global(cv *r, PyObject *globals, PyObject *builtins,
                         PyObject *name, cpy_gc *gc)
{
    uint64_t gv = CPY_DICT_VERSION(globals);
    uint64_t bv = CPY_DICT_VERSION(builtins);
    PyObject *v;

    if (gv && gc->val && gc->gver == gv && gc->bver == bv) {
        *r = cv_norm(gc->val);
        return 0;
    }
    v = PyDict_GetItemWithError(globals, name);
    if (!v) {
        if (PyErr_Occurred()) return -1;
        v = PyDict_GetItemWithError(builtins, name);
        if (!v) {
            if (!PyErr_Occurred())
                PyErr_Format(PyExc_NameError, "name '%U' is not defined", name);
            return -1;
        }
    }
    if (gv) { gc->val = v; gc->gver = gv; gc->bver = bv; }
    *r = cv_norm(v);
    return 0;
}

CPY_INLINE int cv_build_list(cv *r, cv *items, int n)
{
    PyObject *l = PyList_New(n);
    int i;
    if (!l) return -1;
    for (i = 0; i < n; i++) {
        PyObject *o = cv_box(items[i]);
        if (!o) { Py_DECREF(l); return -1; }
        PyList_SET_ITEM(l, i, o);
    }
    *r = cv_obj(l);
    return 0;
}

CPY_INLINE int cv_build_tuple(cv *r, cv *items, int n)
{
    PyObject *l = PyTuple_New(n);
    int i;
    if (!l) return -1;
    for (i = 0; i < n; i++) {
        PyObject *o = cv_box(items[i]);
        if (!o) { Py_DECREF(l); return -1; }
        PyTuple_SET_ITEM(l, i, o);
    }
    *r = cv_obj(l);
    return 0;
}

CPY_INLINE int cv_conv(cv *r, cv v, int conv)
{
    PyObject *o = cv_box(v), *t;
    if (!o) return -1;
    switch (conv) {
    case 1: t = PyObject_Str(o); break;
    case 2: t = PyObject_Repr(o); break;
    case 3: t = PyObject_ASCII(o); break;
    default: *r = cv_obj(o); return 0;
    }
    Py_DECREF(o);
    if (!t) return -1;
    *r = cv_obj(t);
    return 0;
}

CPY_INLINE int cv_format(cv *r, cv v, cv *spec)
{
    PyObject *o = cv_box(v), *sp = NULL, *t;
    if (!o) return -1;
    if (spec) {
        sp = cv_box(*spec);
        if (!sp) { Py_DECREF(o); return -1; }
    }
    if (!sp && PyUnicode_CheckExact(o)) { *r = cv_obj(o); return 0; }
    t = PyObject_Format(o, sp);
    Py_DECREF(o);
    Py_XDECREF(sp);
    if (!t) return -1;
    *r = cv_obj(t);
    return 0;
}

CPY_INLINE int cv_build_string(cv *r, cv *items, int n)
{
    PyObject *sep, *l, *t;
    int i;
    l = PyList_New(n);
    if (!l) return -1;
    for (i = 0; i < n; i++) {
        PyObject *o = cv_box(items[i]);
        if (!o) { Py_DECREF(l); return -1; }
        PyList_SET_ITEM(l, i, o);
    }
    sep = PyUnicode_FromStringAndSize("", 0);
    if (!sep) { Py_DECREF(l); return -1; }
    t = PyUnicode_Join(sep, l);
    Py_DECREF(sep);
    Py_DECREF(l);
    if (!t) return -1;
    *r = cv_obj(t);
    return 0;
}

CPY_INLINE int cv_build_map(cv *r, cv *items, int n)
{
    PyObject *d = PyDict_New();
    int i;
    if (!d) return -1;
    for (i = 0; i < n; i++) {
        PyObject *k = cv_box(items[i * 2]), *v;
        if (!k) { Py_DECREF(d); return -1; }
        v = cv_box(items[i * 2 + 1]);
        if (!v) { Py_DECREF(k); Py_DECREF(d); return -1; }
        if (PyDict_SetItem(d, k, v)) { Py_DECREF(k); Py_DECREF(v); Py_DECREF(d); return -1; }
        Py_DECREF(k);
        Py_DECREF(v);
    }
    *r = cv_obj(d);
    return 0;
}

CPY_INLINE int cv_const_key_map(cv *r, cv *vals, cv keys, int n)
{
    PyObject *d, *kt = cv_box(keys);
    int i;
    if (!kt) return -1;
    if (!PyTuple_CheckExact(kt) || PyTuple_GET_SIZE(kt) != n) {
        Py_DECREF(kt);
        PyErr_SetString(PyExc_SystemError, "compyler: bad const key map");
        return -1;
    }
    d = PyDict_New();
    if (!d) { Py_DECREF(kt); return -1; }
    for (i = 0; i < n; i++) {
        PyObject *v = cv_box(vals[i]);
        if (!v) { Py_DECREF(kt); Py_DECREF(d); return -1; }
        if (PyDict_SetItem(d, PyTuple_GET_ITEM(kt, i), v)) {
            Py_DECREF(v); Py_DECREF(kt); Py_DECREF(d); return -1;
        }
        Py_DECREF(v);
    }
    Py_DECREF(kt);
    *r = cv_obj(d);
    return 0;
}

CPY_INLINE int cv_map_add(cv d, cv k, cv v)
{
    PyObject *o = cv_box(d), *i, *x;
    int rc;
    if (!o) return -1;
    i = cv_box(k);
    if (!i) { Py_DECREF(o); return -1; }
    x = cv_box(v);
    if (!x) { Py_DECREF(i); Py_DECREF(o); return -1; }
    rc = PyDict_SetItem(o, i, x);
    Py_DECREF(o);
    Py_DECREF(i);
    Py_DECREF(x);
    return rc;
}

CPY_INLINE int cv_slice(cv *r, cv c, cv a, cv b)
{
    PyObject *o = cv_box(c), *x, *y, *sl, *z;
    if (!o) return -1;
    x = cv_box(a);
    if (!x) { Py_DECREF(o); return -1; }
    y = cv_box(b);
    if (!y) { Py_DECREF(x); Py_DECREF(o); return -1; }
    sl = PySlice_New(x, y, NULL);
    Py_DECREF(x);
    Py_DECREF(y);
    if (!sl) { Py_DECREF(o); return -1; }
    z = PyObject_GetItem(o, sl);
    Py_DECREF(sl);
    Py_DECREF(o);
    if (!z) return -1;
    *r = cv_norm_steal(z);
    return 0;
}

CPY_INLINE int cv_store_slice(cv c, cv a, cv b, cv v)
{
    PyObject *o = cv_box(c), *x, *y, *sl, *nv;
    int rc;
    if (!o) return -1;
    x = cv_box(a);
    if (!x) { Py_DECREF(o); return -1; }
    y = cv_box(b);
    if (!y) { Py_DECREF(x); Py_DECREF(o); return -1; }
    sl = PySlice_New(x, y, NULL);
    Py_DECREF(x);
    Py_DECREF(y);
    if (!sl) { Py_DECREF(o); return -1; }
    nv = cv_box(v);
    if (!nv) { Py_DECREF(sl); Py_DECREF(o); return -1; }
    rc = PyObject_SetItem(o, sl, nv);
    Py_DECREF(sl);
    Py_DECREF(nv);
    Py_DECREF(o);
    return rc;
}

CPY_INLINE int cv_list_append(cv lst, cv v)
{
    PyObject *o = cv_box(v);
    int rc;
    if (!o) return -1;
    if (lst.t != CPY_T_OBJ) {
        Py_DECREF(o);
        PyErr_SetString(PyExc_SystemError, "append target is not a list");
        return -1;
    }
    rc = PyList_Append(cv_o(lst), o);
    Py_DECREF(o);
    return rc;
}

CPY_INLINE int cv_contains(cv *r, cv item, cv container, int invert)
{
    PyObject *a = cv_box(item), *b;
    int rc;
    if (!a) return -1;
    b = cv_box(container);
    if (!b) { Py_DECREF(a); return -1; }
    rc = PySequence_Contains(b, a);
    Py_DECREF(a);
    Py_DECREF(b);
    if (rc < 0) return -1;
    if (invert) rc = !rc;
    *r = cv_bool(rc);
    return 0;
}

CPY_INLINE int cv_unpack(cv src, cv *out, int n)
{
    PyObject *o = cv_box(src), *seq;
    Py_ssize_t i;
    if (!o) return -1;
    seq = PySequence_Fast(o, "cannot unpack non-sequence");
    Py_DECREF(o);
    if (!seq) return -1;
    if (PySequence_Fast_GET_SIZE(seq) != n) {
        PyErr_Format(PyExc_ValueError, "expected %d values to unpack", n);
        Py_DECREF(seq);
        return -1;
    }
    for (i = 0; i < n; i++)
        out[n - 1 - i] = cv_norm(PySequence_Fast_GET_ITEM(seq, i));
    Py_DECREF(seq);
    return 0;
}

typedef struct {
    void          **rows;
    Py_ssize_t     *lens;
    unsigned char  *own;
    Py_ssize_t      n, cap;
} cpy_lol;

CPY_INLINE int cpy_lol_push(cpy_lol *l, void *row, Py_ssize_t len, int own)
{
    if (l->n == l->cap) {
        Py_ssize_t nc = l->cap ? l->cap * 2 : 16;
        void **nr = (void **)realloc(l->rows, (size_t)nc * sizeof(void *));
        Py_ssize_t *nl;
        unsigned char *no;
        if (!nr) return 1;
        l->rows = nr;
        nl = (Py_ssize_t *)realloc(l->lens, (size_t)nc * sizeof(Py_ssize_t));
        if (!nl) return 1;
        l->lens = nl;
        no = (unsigned char *)realloc(l->own, (size_t)nc);
        if (!no) return 1;
        l->own = no;
        l->cap = nc;
    }
    l->rows[l->n] = row;
    l->lens[l->n] = len;
    l->own[l->n] = (unsigned char)own;
    l->n++;
    return 0;
}

CPY_INLINE void cpy_lol_free(cpy_lol *l)
{
    Py_ssize_t i;
    for (i = 0; i < l->n; i++)
        if (l->own[i]) free(l->rows[i]);
    free(l->rows);
    free(l->lens);
    free(l->own);
    l->rows = 0;
    l->lens = 0;
    l->own = 0;
    l->n = 0;
    l->cap = 0;
}

typedef struct {
    int64_t   *kv;
    unsigned char *used;
    Py_ssize_t cap;
    Py_ssize_t n;
} cpy_mii;

CPY_INLINE uint64_t cpy_mii_h(int64_t k)
{
    uint64_t x = (uint64_t)k;
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    return x;
}

static int cpy_mii_grow(cpy_mii *m)
{
    Py_ssize_t ncap = m->cap ? m->cap * 2 : 64;
    int64_t *nkv = (int64_t *)malloc((size_t)ncap * 16);
    unsigned char *nu = (unsigned char *)calloc((size_t)ncap, 1);
    Py_ssize_t i;
    if (!nkv || !nu) { free(nkv); free(nu); return 1; }
    for (i = 0; i < m->cap; i++) {
        if (m->used[i]) {
            uint64_t j = cpy_mii_h(m->kv[i * 2]) & (uint64_t)(ncap - 1);
            while (nu[j]) j = (j + 1) & (uint64_t)(ncap - 1);
            nu[j] = 1;
            nkv[j * 2] = m->kv[i * 2];
            nkv[j * 2 + 1] = m->kv[i * 2 + 1];
        }
    }
    free(m->kv);
    free(m->used);
    m->kv = nkv;
    m->used = nu;
    m->cap = ncap;
    return 0;
}

CPY_INLINE int cpy_mii_set(cpy_mii *m, int64_t k, int64_t v)
{
    uint64_t j;
    if (m->n * 4 >= m->cap * 3 && cpy_mii_grow(m)) return 1;
    j = cpy_mii_h(k) & (uint64_t)(m->cap - 1);
    while (m->used[j]) {
        if (m->kv[j * 2] == k) { m->kv[j * 2 + 1] = v; return 0; }
        j = (j + 1) & (uint64_t)(m->cap - 1);
    }
    m->used[j] = 1;
    m->kv[j * 2] = k;
    m->kv[j * 2 + 1] = v;
    m->n++;
    return 0;
}

CPY_INLINE int cpy_mii_get(cpy_mii *m, int64_t k, int64_t *out)
{
    uint64_t j;
    if (!m->cap) return 1;
    j = cpy_mii_h(k) & (uint64_t)(m->cap - 1);
    while (m->used[j]) {
        if (m->kv[j * 2] == k) { *out = m->kv[j * 2 + 1]; return 0; }
        j = (j + 1) & (uint64_t)(m->cap - 1);
    }
    return 1;
}

CPY_INLINE void cpy_mii_free(cpy_mii *m)
{
    free(m->kv);
    free(m->used);
    m->kv = 0;
    m->used = 0;
    m->cap = 0;
    m->n = 0;
}

CPY_INLINE int cpy_i64s(char *out, int64_t v)
{
    char tmp[24];
    int n = 0, m = 0;
    uint64_t u;
    if (v < 0) { out[m++] = '-'; u = (uint64_t)(-(v + 1)) + 1; }
    else u = (uint64_t)v;
    do { tmp[n++] = (char)('0' + (u % 10)); u /= 10; } while (u);
    while (n) out[m++] = tmp[--n];
    return m;
}

CPY_INLINE int cpy_sib_ok(cv g, PyObject **tc, PyObject *G, const char *marker)
{
    PyObject *o, *code, *names;
    Py_ssize_t i, n;
    if (g.t != CPY_T_OBJ) return 0;
    o = cv_o(g);
    if (!PyFunction_Check(o)) return 0;
    if (PyFunction_GET_GLOBALS(o) != G) return 0;
    code = PyFunction_GET_CODE(o);
    if (*tc) return code == *tc;
    names = ((PyCodeObject *)code)->co_names;
    if (!names || !PyTuple_CheckExact(names)) return 0;
    n = PyTuple_GET_SIZE(names);
    for (i = 0; i < n; i++) {
        const char *s = PyUnicode_AsUTF8(PyTuple_GET_ITEM(names, i));
        if (s && !strcmp(s, marker)) { *tc = code; return 1; }
    }
    return 0;
}

CPY_INLINE int cpy_same_fn(cv f, PyObject *code, PyObject *G)
{
    PyObject *o;
    if (code == NULL || f.t != CPY_T_OBJ) return 0;
    o = cv_o(f);
    return PyFunction_Check(o) &&
           PyFunction_GET_CODE(o) == code &&
           PyFunction_GET_GLOBALS(o) == G;
}

CPY_INLINE int cv_argcount(Py_ssize_t got, int want, const char *name)
{
    if (got != want) {
        PyErr_Format(PyExc_TypeError, "%s() takes %d positional arguments but %zd were given",
                     name, want, got);
        return -1;
    }
    return 0;
}

#endif
