/*
 * xBase3 - dBASE III+ Compatible Database System
 * util.c - Utility functions implementation
 */

#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <math.h>

/* Global error state */
jmp_buf g_error_jmp;
ErrorCode g_last_error = ERR_NONE;
char g_error_msg[256] = {0};

/* Error strings */
static const char *error_strings[] = {
    "No error",
    "File not found",
    "Cannot create file",
    "Error reading file",
    "Error writing file",
    "Invalid DBF file",
    "Invalid field",
    "Invalid record number",
    "Out of memory",
    "Syntax error",
    "Type mismatch",
    "Undefined variable",
    "Undefined function",
    "Division by zero",
    "Numeric overflow",
    "No database in use",
    "End of file",
    "Beginning of file",
    "Not implemented",
    "Internal error"
};

void error_set(ErrorCode code, const char *fmt, ...) {
    g_last_error = code;
    if (fmt) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(g_error_msg, sizeof(g_error_msg), fmt, args);
        va_end(args);
    } else {
        g_error_msg[0] = '\0';
    }
}

void error_clear(void) {
    g_last_error = ERR_NONE;
    g_error_msg[0] = '\0';
}

const char *error_string(ErrorCode code) {
    if (code >= 0 && code < (int)(sizeof(error_strings) / sizeof(error_strings[0]))) {
        return error_strings[code];
    }
    return "Unknown error";
}

void error_print(void) {
    if (g_last_error != ERR_NONE) {
        fprintf(stderr, "Error: %s", error_string(g_last_error));
        if (g_error_msg[0]) {
            fprintf(stderr, " - %s", g_error_msg);
        }
        fprintf(stderr, "\n");
    }
}

/* Memory allocation */
void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr && size > 0) {
        error_set(ERR_OUT_OF_MEMORY, "Failed to allocate %zu bytes", size);
        longjmp(g_error_jmp, 1);
    }
    return ptr;
}

void *xcalloc(size_t count, size_t size) {
    void *ptr = calloc(count, size);
    if (!ptr && count > 0 && size > 0) {
        error_set(ERR_OUT_OF_MEMORY, "Failed to allocate %zu bytes", count * size);
        longjmp(g_error_jmp, 1);
    }
    return ptr;
}

void *xrealloc(void *ptr, size_t size) {
    void *newptr = realloc(ptr, size);
    if (!newptr && size > 0) {
        error_set(ERR_OUT_OF_MEMORY, "Failed to reallocate %zu bytes", size);
        longjmp(g_error_jmp, 1);
    }
    return newptr;
}

char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *dup = xmalloc(len);
    memcpy(dup, s, len);
    return dup;
}

void xfree(void *ptr) {
    free(ptr);
}

/* String utilities */
void str_upper(char *s) {
    if (!s) return;
    while (*s) {
        *s = (char)toupper((unsigned char)*s);
        s++;
    }
}

void str_lower(char *s) {
    if (!s) return;
    while (*s) {
        *s = (char)tolower((unsigned char)*s);
        s++;
    }
}

void str_trim_right(char *s) {
    if (!s || !*s) return;
    char *end = s + strlen(s) - 1;
    while (end >= s && isspace((unsigned char)*end)) {
        *end-- = '\0';
    }
}

void str_trim_left(char *s) {
    if (!s || !*s) return;
    char *start = s;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
}

void str_trim(char *s) {
    str_trim_right(s);
    str_trim_left(s);
}

void str_pad_right(char *dest, const char *src, size_t len) {
    size_t srclen = src ? strlen(src) : 0;
    if (srclen > len) srclen = len;
    if (src) memcpy(dest, src, srclen);
    memset(dest + srclen, ' ', len - srclen);
    dest[len] = '\0';
}

void str_pad_left(char *dest, const char *src, size_t len) {
    size_t srclen = src ? strlen(src) : 0;
    if (srclen > len) srclen = len;
    size_t padding = len - srclen;
    memset(dest, ' ', padding);
    if (src) memcpy(dest + padding, src, srclen);
    dest[len] = '\0';
}

int str_casecmp(const char *s1, const char *s2) {
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

int str_ncasecmp(const char *s1, const char *s2, size_t n) {
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    while (n > 0 && *s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

bool str_empty(const char *s) {
    if (!s) return true;
    while (*s) {
        if (!isspace((unsigned char)*s)) return false;
        s++;
    }
    return true;
}

/* Number formatting */
void num_to_str(double val, char *buf, int width, int decimals) {
    if (decimals < 0) decimals = 0;
    if (width < 1) width = 1;

    char fmt[32];
    snprintf(fmt, sizeof(fmt), "%%%d.%df", width, decimals);
    snprintf(buf, (size_t)(width + 1), fmt, val);

    /* Ensure null termination */
    buf[width] = '\0';
}

bool str_to_num(const char *s, double *val) {
    if (!s || !val) return false;

    /* Skip leading spaces */
    while (isspace((unsigned char)*s)) s++;

    if (!*s) {
        *val = 0.0;
        return true;
    }

    char *end;
    *val = strtod(s, &end);

    /* Skip trailing spaces */
    while (isspace((unsigned char)*end)) end++;

    return *end == '\0';
}

/* Date utilities */
void date_today(char *buf) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    snprintf(buf, 9, "%04d%02d%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
}

bool date_valid(const char *date) {
    if (!date || strlen(date) != 8) return false;

    for (int i = 0; i < 8; i++) {
        if (!isdigit((unsigned char)date[i])) return false;
    }

    int year = date_year(date);
    int month = date_month(date);
    int day = date_day(date);

    if (year < 1 || month < 1 || month > 12 || day < 1) return false;

    static const int days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int max_day = days_in_month[month];

    /* Leap year check */
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
        max_day = 29;
    }

    return day <= max_day;
}

int date_year(const char *date) {
    if (!date || strlen(date) < 4) return 0;
    return (date[0] - '0') * 1000 + (date[1] - '0') * 100 +
           (date[2] - '0') * 10 + (date[3] - '0');
}

int date_month(const char *date) {
    if (!date || strlen(date) < 6) return 0;
    return (date[4] - '0') * 10 + (date[5] - '0');
}

int date_day(const char *date) {
    if (!date || strlen(date) < 8) return 0;
    return (date[6] - '0') * 10 + (date[7] - '0');
}

/* Calculate day of week (1=Sunday, 7=Saturday) */
int date_dow(const char *date) {
    if (!date_valid(date)) return 0;

    int y = date_year(date);
    int m = date_month(date);
    int d = date_day(date);

    /* Zeller's congruence */
    if (m < 3) {
        m += 12;
        y--;
    }
    int k = y % 100;
    int j = y / 100;
    int dow = (d + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
    dow = ((dow + 6) % 7) + 1;  /* Convert to 1=Sunday */
    return dow;
}

static const char *day_names[] = {
    "", "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

static const char *month_names[] = {
    "", "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

const char *date_cdow(const char *date) {
    int dow = date_dow(date);
    if (dow < 1 || dow > 7) return "";
    return day_names[dow];
}

const char *date_cmonth(const char *date) {
    int m = date_month(date);
    if (m < 1 || m > 12) return "";
    return month_names[m];
}

void date_from_parts(char *buf, int year, int month, int day) {
    snprintf(buf, 9, "%04d%02d%02d", year, month, day);
}

long date_to_julian(const char *date) {
    if (!date_valid(date)) return 0;

    int y = date_year(date);
    int m = date_month(date);
    int d = date_day(date);

    if (m <= 2) {
        y--;
        m += 12;
    }

    long a = y / 100;
    long b = 2 - a + a / 4;

    return (long)(365.25 * (y + 4716)) + (long)(30.6001 * (m + 1)) + d + b - 1524;
}

void date_from_julian(char *buf, long julian) {
    long z = julian;
    long a = (long)((z - 1867216.25) / 36524.25);
    a = z + 1 + a - a / 4;
    long b = a + 1524;
    long c = (long)((b - 122.1) / 365.25);
    long d = (long)(365.25 * c);
    long e = (long)((b - d) / 30.6001);

    int day = (int)(b - d - (long)(30.6001 * e));
    int month = (int)(e < 14 ? e - 1 : e - 13);
    int year = (int)(month > 2 ? c - 4716 : c - 4715);

    date_from_parts(buf, year, month, day);
}

/* File utilities */
bool file_exists(const char *path) {
    FILE *f = fopen(path, "r");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

char *file_extension(const char *path) {
    if (!path) return NULL;
    char *dot = strrchr(path, '.');
    if (!dot || dot == path) return NULL;
    /* Make sure dot is after any path separator */
    char *sep = strrchr(path, '/');
    if (sep && dot < sep) return NULL;
    return dot;
}

void file_change_ext(char *path, const char *newext) {
    char *dot = file_extension(path);
    if (dot) {
        *dot = '\0';
    }
    strcat(path, newext);
}

void file_basename(char *dest, const char *path) {
    if (!path || !dest) return;

    const char *base = strrchr(path, '/');
    if (!base) {
        base = path;
    } else {
        base++;
    }

    strcpy(dest, base);

    /* Remove extension */
    char *dot = strrchr(dest, '.');
    if (dot) {
        *dot = '\0';
    }
}

/* Byte order utilities */
uint16_t read_u16_le(const uint8_t *buf) {
    return (uint16_t)(buf[0] | (buf[1] << 8));
}

uint32_t read_u32_le(const uint8_t *buf) {
    return (uint32_t)(buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24));
}

void write_u16_le(uint8_t *buf, uint16_t val) {
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
}

void write_u32_le(uint8_t *buf, uint32_t val) {
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
    buf[2] = (uint8_t)((val >> 16) & 0xFF);
    buf[3] = (uint8_t)((val >> 24) & 0xFF);
}
