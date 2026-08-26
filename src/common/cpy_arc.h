#ifndef CPY_ARC_H
#define CPY_ARC_H

#include <stdint.h>

#define CPY_FOOTER_MAGIC "COMPYLR1"
#define CPY_ARC_MAGIC    "CPYARC02"
#define CPY_ARC_VERSION  2u

#pragma pack(push, 1)

typedef struct {
    char     magic[8];
    uint64_t arc_off;
    uint64_t arc_size;
    uint64_t payload_id;
} cpy_footer;

typedef struct {
    char     magic[8];
    uint32_t version;
    uint32_t flags;
    uint32_t entry_count;
    uint32_t group_count;
    uint64_t table_off;
    uint64_t group_off;
    uint64_t name_pool_off;
    uint64_t cfg_off;
    uint32_t cfg_len;
    uint32_t algo;
    char     dll_name[64];
    char     entry_path[208];
    char     app_name[64];
} cpy_arc_header;

typedef struct {
    uint32_t name_off;
    uint32_t name_len;
    uint32_t group;
    uint64_t goff;
    uint32_t usize;
    uint32_t flags;
    uint32_t crc;
} cpy_arc_entry;

typedef struct {
    uint64_t data_off;
    uint64_t csize;
    uint64_t usize;
    uint32_t first;
    uint32_t count;
    uint32_t flags;
} cpy_arc_group;

#pragma pack(pop)

#define CPY_EF_COMPRESSED 0x1u

#define CPY_HF_ONEDIR     0x1u
#define CPY_HF_HAS_HOOK   0x2u

#define CPY_ALGO_STORE  0
#define CPY_ALGO_FAST   4
#define CPY_ALGO_MAX    5

#define CPY_GROUP_TARGET (6u * 1024u * 1024u)
#define CPY_GROUP_MAX    64

#define CPY_CFG_PATH  'P'
#define CPY_CFG_DLL   'D'
#define CPY_CFG_ENV   'E'

#define CPY_HOOK_MODULE   "__compyler_rt__"

#endif
