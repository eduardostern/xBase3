/*
 * xBase3 - dBASE III+ Compatible Database System
 * xdx.h - XDX B-tree index engine header
 */

#ifndef XBASE3_XDX_H
#define XBASE3_XDX_H

#include "util.h"
#include "dbf.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* XDX file format constants */
#define XDX_MAGIC           "XDX"
#define XDX_VERSION         1
#define XDX_HEADER_SIZE     512
#define XDX_MAX_KEY_LEN     256
#define XDX_MAX_EXPR_LEN    256
#define XDX_DEFAULT_ORDER   50      /* Max keys per node */

/* Key types */
#define XDX_KEY_CHAR        'C'
#define XDX_KEY_NUMERIC     'N'
#define XDX_KEY_DATE        'D'

/* Flags */
#define XDX_FLAG_UNIQUE     0x01
#define XDX_FLAG_DESCENDING 0x02

/*
 * XDX Header Structure (512 bytes)
 *
 * Bytes 0-3:     Magic "XDX\0"
 * Byte 4:        Version (1)
 * Byte 5:        Key type (C/N/D)
 * Bytes 6-7:     Key length
 * Bytes 8-11:    Root node offset
 * Bytes 12-15:   Node count
 * Bytes 16-17:   Order (max keys per node)
 * Bytes 18-19:   Flags (unique, descending)
 * Bytes 20-275:  Key expression (256 bytes)
 * Bytes 276-511: Reserved
 */
typedef struct {
    char magic[4];              /* "XDX\0" */
    uint8_t version;            /* Format version */
    char key_type;              /* Key type: C, N, D */
    uint16_t key_length;        /* Key length in bytes */
    uint32_t root_offset;       /* Root node file offset */
    uint32_t node_count;        /* Total number of nodes */
    uint16_t order;             /* B-tree order (max keys per node) */
    uint16_t flags;             /* Flags: unique, descending */
    char key_expr[XDX_MAX_EXPR_LEN]; /* Key expression string */
    uint8_t reserved[236];      /* Padding to 512 bytes */
} XDXHeader;

/*
 * B-tree Node Structure (variable size)
 *
 * Bytes 0-1:     Key count in this node
 * Byte 2:        Leaf flag (1=leaf, 0=internal)
 * Byte 3:        Reserved
 * Bytes 4-7:     Parent node offset (0 for root)
 *
 * For each key (up to 'order' keys):
 *   Key value (key_length bytes)
 *   Record number (4 bytes)
 *   Child node offset (4 bytes, internal nodes only)
 *
 * Right-most child offset (4 bytes, internal nodes only)
 */
typedef struct {
    uint16_t key_count;         /* Number of keys in this node */
    uint8_t is_leaf;            /* 1 if leaf node, 0 if internal */
    uint8_t reserved;
    uint32_t parent_offset;     /* Parent node offset (0 = root) */
    /* Followed by key data - allocated dynamically */
} XDXNodeHeader;

/* Key entry in a node */
typedef struct {
    uint8_t *key;               /* Key value (key_length bytes) */
    uint32_t recno;             /* Record number in DBF */
    uint32_t child_offset;      /* Child node offset (internal nodes) */
} XDXKeyEntry;

/* In-memory node representation */
typedef struct {
    XDXNodeHeader header;
    uint32_t file_offset;       /* This node's offset in file */
    XDXKeyEntry *entries;       /* Array of key entries */
    uint32_t right_child;       /* Right-most child (internal nodes) */
    bool dirty;                 /* Node modified flag */
} XDXNode;

/* XDX index handle */
typedef struct {
    FILE *fp;                   /* File pointer */
    char filename[MAX_PATH_LEN]; /* Index file path */
    XDXHeader header;           /* Index header */
    XDXNode *root;              /* Cached root node */
    uint32_t current_recno;     /* Last found record number */
    bool found;                 /* Last seek found exact match */
    bool modified;              /* Index modified flag */

    /* Key buffer for comparisons */
    uint8_t *key_buffer;        /* Temporary key storage */
} XDX;


/*
 * Index file operations
 */

/* Create a new index file */
XDX *xdx_create(const char *filename, const char *key_expr,
                char key_type, uint16_t key_length,
                bool unique, bool descending);

/* Open an existing index file */
XDX *xdx_open(const char *filename);

/* Close index file */
void xdx_close(XDX *xdx);

/* Flush changes to disk */
bool xdx_flush(XDX *xdx);

/*
 * Key operations
 */

/* Insert a key into the index */
bool xdx_insert(XDX *xdx, const void *key, uint32_t recno);

/* Delete a key from the index */
bool xdx_delete(XDX *xdx, const void *key, uint32_t recno);

/* Search for a key (exact match or nearest) */
bool xdx_seek(XDX *xdx, const void *key);

/* Get current record number after seek */
uint32_t xdx_recno(XDX *xdx);

/* Check if last seek found exact match */
bool xdx_found(XDX *xdx);

/* Move to first key */
bool xdx_go_top(XDX *xdx);

/* Move to last key */
bool xdx_go_bottom(XDX *xdx);

/* Move to next key */
bool xdx_skip(XDX *xdx, int count);

/* Check if at end of index */
bool xdx_eof(XDX *xdx);

/* Check if at beginning of index */
bool xdx_bof(XDX *xdx);

/*
 * Index maintenance
 */

/* Rebuild index from DBF */
bool xdx_reindex(XDX *xdx, DBF *dbf,
                 bool (*eval_key)(DBF *dbf, void *key, void *ctx),
                 void *ctx);

/* Get key expression */
const char *xdx_key_expr(XDX *xdx);

/* Get key type */
char xdx_key_type(XDX *xdx);

/* Get key length */
uint16_t xdx_key_length(XDX *xdx);

/* Check if index is unique */
bool xdx_is_unique(XDX *xdx);

/* Check if index is descending */
bool xdx_is_descending(XDX *xdx);

/*
 * Key comparison
 */

/* Compare two keys, returns <0, 0, >0 */
int xdx_key_compare(XDX *xdx, const void *key1, const void *key2);

#endif /* XBASE3_XDX_H */
