#ifndef CPY_TYPED_H
#define CPY_TYPED_H

#define TY_UNSET 0
#define TY_INT   1
#define TY_FLT   2
#define TY_BOOL  3
#define TY_BAD   4
#define TY_NONE  5
#define TY_ARRI  6
#define TY_ARRF  7
#define TY_STR   8
#define TY_CHR   9
#define TY_MAPI 10
#define TY_LOL  11

#define TY_MAXRNG 16
#define TY_MAXMATH 24
#define TY_MAXARR 16
#define TY_MAXSIB 8

typedef struct {
    int lg;
    int nargs;
    int argi[3];
    int foriter;
} ty_rng;

#define TY_K_MATH  0
#define TY_K_FLOAT 1
#define TY_K_INT   2
#define TY_K_ABS   3
#define TY_K_LEN   4
#define TY_K_ORD   5
#define TY_K_MIN   6
#define TY_K_MAX   7

typedef struct {
    int lg;
    int attr;
    int call;
    int nargs;
    int fn;
    int kind;
} ty_math;

typedef struct {
    int store_i;
    int elem;
    int len_i;
    long long iv;
    double dv;
} ty_acr;

#endif
