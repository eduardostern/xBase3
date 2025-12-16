/*
 * xBase3 - dBASE III+ Compatible Database System
 * dbf.h - DBF file engine header
 */

#ifndef XBASE3_DBF_H
#define XBASE3_DBF_H

#include "util.h"
#include <stdio.h>

/* DBF version byte */
#define DBF_VERSION_DBASE3      0x03
#define DBF_VERSION_DBASE3_MEMO 0x83

/* Field types */
#define FIELD_TYPE_CHAR     'C'
#define FIELD_TYPE_NUMERIC  'N'
#define FIELD_TYPE_DATE     'D'
#define FIELD_TYPE_LOGICAL  'L'
#define FIELD_TYPE_MEMO     'M'

/* Special bytes */
#define DBF_HEADER_TERM     0x0D
#define DBF_EOF_MARKER      0x1A
#define DBF_RECORD_ACTIVE   ' '
#define DBF_RECORD_DELETED  '*'

/* DBF header structure (32 bytes) */
typedef struct {
    uint8_t version;           /* Version byte */
    uint8_t year;              /* Last update year (since 1900) */
    uint8_t month;             /* Last update month */
    uint8_t day;               /* Last update day */
    uint32_t record_count;     /* Number of records */
    uint16_t header_size;      /* Header size in bytes */
    uint16_t record_size;      /* Record size in bytes */
    uint8_t reserved[20];      /* Reserved */
} DBFHeader;

/* Field descriptor structure (32 bytes) */
typedef struct {
    char name[MAX_FIELD_NAME]; /* Field name (null-terminated) */
    char type;                 /* Field type (C/N/D/L/M) */
    uint16_t length;           /* Field length */
    uint8_t decimals;          /* Decimal places (for N type) */
    uint16_t offset;           /* Offset within record (calculated) */
} DBFField;

/* DBF file handle */
typedef struct {
    FILE *fp;                  /* File pointer */
    char filename[MAX_PATH_LEN]; /* File path */
    char alias[MAX_FIELD_NAME];  /* Alias name */
    DBFHeader header;          /* File header */
    DBFField *fields;          /* Field array */
    int field_count;           /* Number of fields */
    uint32_t current_record;   /* Current record number (1-based, 0=BOF) */
    uint8_t *record_buffer;    /* Current record data */
    bool modified;             /* Record modified flag */
    bool eof;                  /* End of file flag */
    bool bof;                  /* Beginning of file flag */
    bool deleted;              /* Current record deleted flag */
    bool exclusive;            /* Exclusive access */
    bool readonly;             /* Read-only mode */
} DBF;

/* Open/close operations */
DBF *dbf_open(const char *filename, bool readonly);
DBF *dbf_create(const char *filename, const DBFField *fields, int field_count);
void dbf_close(DBF *dbf);

/* Navigation */
bool dbf_goto(DBF *dbf, uint32_t recno);
bool dbf_skip(DBF *dbf, int count);
bool dbf_go_top(DBF *dbf);
bool dbf_go_bottom(DBF *dbf);

/* Status functions */
bool dbf_eof(DBF *dbf);
bool dbf_bof(DBF *dbf);
uint32_t dbf_recno(DBF *dbf);
uint32_t dbf_reccount(DBF *dbf);
bool dbf_deleted(DBF *dbf);

/* Record operations */
bool dbf_append_blank(DBF *dbf);
bool dbf_delete(DBF *dbf);
bool dbf_recall(DBF *dbf);
bool dbf_flush(DBF *dbf);

/* Field access */
int dbf_field_index(DBF *dbf, const char *name);
const DBFField *dbf_field_info(DBF *dbf, int index);
int dbf_field_count(DBF *dbf);

/* Field value get/set */
bool dbf_get_string(DBF *dbf, int field_index, char *buffer, size_t bufsize);
bool dbf_get_double(DBF *dbf, int field_index, double *value);
bool dbf_get_logical(DBF *dbf, int field_index, bool *value);
bool dbf_get_date(DBF *dbf, int field_index, char *buffer); /* YYYYMMDD */

bool dbf_put_string(DBF *dbf, int field_index, const char *value);
bool dbf_put_double(DBF *dbf, int field_index, double value);
bool dbf_put_logical(DBF *dbf, int field_index, bool value);
bool dbf_put_date(DBF *dbf, int field_index, const char *value); /* YYYYMMDD */

/* Bulk operations */
bool dbf_pack(DBF *dbf);
bool dbf_zap(DBF *dbf);

/* Utility */
void dbf_set_alias(DBF *dbf, const char *alias);
const char *dbf_get_alias(DBF *dbf);

#endif /* XBASE3_DBF_H */
