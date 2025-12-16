/*
 * xBase3 - dBASE III+ Compatible Database System
 * util.h - Utility functions header
 */

#ifndef XBASE3_UTIL_H
#define XBASE3_UTIL_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <setjmp.h>

/* ANSI Color codes */
#define CLR_RESET       "\033[0m"
#define CLR_BOLD        "\033[1m"
#define CLR_DIM         "\033[2m"
#define CLR_RED         "\033[31m"
#define CLR_GREEN       "\033[32m"
#define CLR_YELLOW      "\033[33m"
#define CLR_BLUE        "\033[34m"
#define CLR_MAGENTA     "\033[35m"
#define CLR_CYAN        "\033[36m"
#define CLR_WHITE       "\033[37m"
#define CLR_BRED        "\033[91m"
#define CLR_BGREEN      "\033[92m"
#define CLR_BYELLOW     "\033[93m"
#define CLR_BBLUE       "\033[94m"
#define CLR_BMAGENTA    "\033[95m"
#define CLR_BCYAN       "\033[96m"

/* Maximum lengths */
#define MAX_PATH_LEN        260
#define MAX_FIELD_NAME      11
#define MAX_FIELD_LEN       254
#define MAX_RECORD_LEN      4000
#define MAX_EXPR_LEN        1024
#define MAX_LINE_LEN        4096
#define MAX_FIELDS          128
#define MAX_STRING_LEN      65535

/* Error codes */
typedef enum {
    ERR_NONE = 0,
    ERR_FILE_NOT_FOUND,
    ERR_FILE_CREATE,
    ERR_FILE_READ,
    ERR_FILE_WRITE,
    ERR_INVALID_DBF,
    ERR_INVALID_INDEX,
    ERR_INVALID_FIELD,
    ERR_INVALID_RECORD,
    ERR_OUT_OF_MEMORY,
    ERR_SYNTAX,
    ERR_TYPE_MISMATCH,
    ERR_UNDEFINED_VAR,
    ERR_UNDEFINED_FUNC,
    ERR_DIVISION_BY_ZERO,
    ERR_OVERFLOW,
    ERR_NO_DATABASE,
    ERR_DUPLICATE_KEY,
    ERR_EOF,
    ERR_BOF,
    ERR_NOT_IMPLEMENTED,
    ERR_INTERNAL
} ErrorCode;

/* Thread-local error handling */
extern __thread jmp_buf g_error_jmp;
extern __thread ErrorCode g_last_error;
extern __thread char g_error_msg[256];
extern __thread bool g_error_jmp_enabled;

/* Error handling functions */
void error_set(ErrorCode code, const char *fmt, ...);
void error_clear(void);
const char *error_string(ErrorCode code);
void error_print(void);
void error_enable_longjmp(bool enable);
bool error_longjmp_enabled(void);

/* Memory allocation with error handling */
void *xmalloc(size_t size);
void *xcalloc(size_t count, size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);
void xfree(void *ptr);

/* String utilities (dBASE style - fixed length, space padded) */
void str_upper(char *s);
void str_lower(char *s);
void str_trim_right(char *s);
void str_trim_left(char *s);
void str_trim(char *s);
void str_pad_right(char *dest, const char *src, size_t len);
void str_pad_left(char *dest, const char *src, size_t len);
int str_casecmp(const char *s1, const char *s2);
int str_ncasecmp(const char *s1, const char *s2, size_t n);
bool str_empty(const char *s);

/* Number formatting */
void num_to_str(double val, char *buf, int width, int decimals);
bool str_to_num(const char *s, double *val);

/* Date utilities (YYYYMMDD format) */
void date_today(char *buf);  /* Returns YYYYMMDD */
bool date_valid(const char *date);
int date_year(const char *date);
int date_month(const char *date);
int date_day(const char *date);
int date_dow(const char *date);  /* 1=Sunday, 7=Saturday */
const char *date_cdow(const char *date);  /* Day name */
const char *date_cmonth(const char *date);  /* Month name */
void date_from_parts(char *buf, int year, int month, int day);
long date_to_julian(const char *date);
void date_from_julian(char *buf, long julian);

/* File utilities */
bool file_exists(const char *path);
char *file_extension(const char *path);
void file_change_ext(char *path, const char *newext);
void file_basename(char *dest, const char *path);

/* Byte order utilities (for DBF format - little endian) */
uint16_t read_u16_le(const uint8_t *buf);
uint32_t read_u32_le(const uint8_t *buf);
void write_u16_le(uint8_t *buf, uint16_t val);
void write_u32_le(uint8_t *buf, uint32_t val);

#endif /* XBASE3_UTIL_H */
