/*
 * xBase3 - dBASE III+ Compatible Database System
 * functions.c - Built-in functions implementation
 */

#include "functions.h"
#include "variables.h"
#include "dbf.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <time.h>

/* Function signature */
typedef Value (*BuiltinFunc)(Value *args, int arg_count, EvalContext *ctx);

/* Function table entry */
typedef struct {
    const char *name;
    BuiltinFunc func;
    int min_args;
    int max_args;
} FuncEntry;

/* === String Functions === */

static Value fn_len(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1) return value_number(0);
    if (args[0].type != VAL_STRING) return value_number(0);
    return value_number((double)strlen(args[0].data.string));
}

static Value fn_trim(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAL_STRING) return value_string("");
    char *s = xstrdup(args[0].data.string);
    str_trim(s);
    Value v = value_string(s);
    xfree(s);
    return v;
}

static Value fn_ltrim(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAL_STRING) return value_string("");
    char *s = xstrdup(args[0].data.string);
    str_trim_left(s);
    Value v = value_string(s);
    xfree(s);
    return v;
}

static Value fn_rtrim(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAL_STRING) return value_string("");
    char *s = xstrdup(args[0].data.string);
    str_trim_right(s);
    Value v = value_string(s);
    xfree(s);
    return v;
}

static Value fn_upper(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAL_STRING) return value_string("");
    char *s = xstrdup(args[0].data.string);
    str_upper(s);
    Value v = value_string(s);
    xfree(s);
    return v;
}

static Value fn_lower(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAL_STRING) return value_string("");
    char *s = xstrdup(args[0].data.string);
    str_lower(s);
    Value v = value_string(s);
    xfree(s);
    return v;
}

static Value fn_substr(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 2 || args[0].type != VAL_STRING) return value_string("");

    const char *s = args[0].data.string;
    int start = (int)value_to_number(&args[1]) - 1;  /* 1-based */
    int len = (arg_count >= 3) ? (int)value_to_number(&args[2]) : (int)strlen(s);
    int slen = (int)strlen(s);

    if (start < 0) start = 0;
    if (start >= slen) return value_string("");
    if (len < 0) len = 0;
    if (start + len > slen) len = slen - start;

    char *result = xmalloc((size_t)len + 1);
    memcpy(result, s + start, (size_t)len);
    result[len] = '\0';

    Value v = value_string(result);
    xfree(result);
    return v;
}

static Value fn_left(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 2 || args[0].type != VAL_STRING) return value_string("");

    const char *s = args[0].data.string;
    int len = (int)value_to_number(&args[1]);
    int slen = (int)strlen(s);

    if (len <= 0) return value_string("");
    if (len > slen) len = slen;

    char *result = xmalloc((size_t)len + 1);
    memcpy(result, s, (size_t)len);
    result[len] = '\0';

    Value v = value_string(result);
    xfree(result);
    return v;
}

static Value fn_right(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 2 || args[0].type != VAL_STRING) return value_string("");

    const char *s = args[0].data.string;
    int len = (int)value_to_number(&args[1]);
    int slen = (int)strlen(s);

    if (len <= 0) return value_string("");
    if (len > slen) len = slen;

    Value v = value_string(s + slen - len);
    return v;
}

static Value fn_at(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING)
        return value_number(0);

    const char *needle = args[0].data.string;
    const char *haystack = args[1].data.string;

    char *pos = strstr(haystack, needle);
    if (!pos) return value_number(0);

    return value_number((double)(pos - haystack + 1));  /* 1-based */
}

static Value fn_space(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1) return value_string("");

    int len = (int)value_to_number(&args[0]);
    if (len <= 0) return value_string("");
    if (len > MAX_STRING_LEN) len = MAX_STRING_LEN;

    char *result = xmalloc((size_t)len + 1);
    memset(result, ' ', (size_t)len);
    result[len] = '\0';

    Value v = value_string(result);
    xfree(result);
    return v;
}

static Value fn_replicate(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 2 || args[0].type != VAL_STRING) return value_string("");

    const char *s = args[0].data.string;
    int count = (int)value_to_number(&args[1]);
    int slen = (int)strlen(s);

    if (count <= 0 || slen == 0) return value_string("");

    size_t result_len = (size_t)slen * (size_t)count;
    if (result_len > MAX_STRING_LEN) result_len = MAX_STRING_LEN;

    char *result = xmalloc(result_len + 1);
    result[0] = '\0';

    for (int i = 0; i < count && strlen(result) + (size_t)slen <= result_len; i++) {
        strcat(result, s);
    }

    Value v = value_string(result);
    xfree(result);
    return v;
}

static Value fn_stuff(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 4 || args[0].type != VAL_STRING || args[3].type != VAL_STRING)
        return value_string("");

    const char *s = args[0].data.string;
    int start = (int)value_to_number(&args[1]) - 1;  /* 1-based */
    int del_count = (int)value_to_number(&args[2]);
    const char *insert = args[3].data.string;

    int slen = (int)strlen(s);
    int ilen = (int)strlen(insert);

    if (start < 0) start = 0;
    if (start > slen) start = slen;
    if (del_count < 0) del_count = 0;
    if (start + del_count > slen) del_count = slen - start;

    size_t result_len = (size_t)(slen - del_count + ilen);
    char *result = xmalloc(result_len + 1);

    memcpy(result, s, (size_t)start);
    memcpy(result + start, insert, (size_t)ilen);
    strcpy(result + start + ilen, s + start + del_count);

    Value v = value_string(result);
    xfree(result);
    return v;
}

static Value fn_chr(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1) return value_string("");

    int code = (int)value_to_number(&args[0]);
    if (code < 0 || code > 255) code = 0;

    char result[2] = {(char)code, '\0'};
    return value_string(result);
}

static Value fn_asc(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAL_STRING) return value_number(0);
    if (args[0].data.string[0] == '\0') return value_number(0);
    return value_number((double)(unsigned char)args[0].data.string[0]);
}

/* === Numeric Functions === */

static Value fn_abs(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1) return value_number(0);
    return value_number(fabs(value_to_number(&args[0])));
}

static Value fn_int(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1) return value_number(0);
    return value_number(floor(value_to_number(&args[0])));
}

static Value fn_round(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1) return value_number(0);

    double val = value_to_number(&args[0]);
    int decimals = (arg_count >= 2) ? (int)value_to_number(&args[1]) : 0;

    double factor = pow(10.0, decimals);
    return value_number(round(val * factor) / factor);
}

static Value fn_sqrt(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1) return value_number(0);
    double val = value_to_number(&args[0]);
    if (val < 0) return value_number(0);
    return value_number(sqrt(val));
}

static Value fn_mod(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 2) return value_number(0);
    double a = value_to_number(&args[0]);
    double b = value_to_number(&args[1]);
    if (b == 0) return value_number(0);
    return value_number(fmod(a, b));
}

static Value fn_max(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 2) return value_number(0);
    double a = value_to_number(&args[0]);
    double b = value_to_number(&args[1]);
    return value_number(a > b ? a : b);
}

static Value fn_min(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 2) return value_number(0);
    double a = value_to_number(&args[0]);
    double b = value_to_number(&args[1]);
    return value_number(a < b ? a : b);
}

static Value fn_log(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1) return value_number(0);
    double val = value_to_number(&args[0]);
    if (val <= 0) return value_number(0);
    return value_number(log(val));
}

static Value fn_exp(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1) return value_number(0);
    return value_number(exp(value_to_number(&args[0])));
}

/* === Conversion Functions === */

static Value fn_str(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1) return value_string("");

    double val = value_to_number(&args[0]);
    int width = (arg_count >= 2) ? (int)value_to_number(&args[1]) : 10;
    int decimals = (arg_count >= 3) ? (int)value_to_number(&args[2]) : 0;

    if (width < 1) width = 1;
    if (width > 100) width = 100;
    if (decimals < 0) decimals = 0;

    char buf[128];
    num_to_str(val, buf, width, decimals);

    return value_string(buf);
}

static Value fn_val(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAL_STRING) return value_number(0);

    double result;
    str_to_num(args[0].data.string, &result);
    return value_number(result);
}

/* === Date Functions === */

static Value fn_date(Value *args, int arg_count, EvalContext *ctx) {
    (void)args; (void)arg_count; (void)ctx;
    char buf[9];
    date_today(buf);
    return value_date(buf);
}

static Value fn_year(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAL_DATE) return value_number(0);
    return value_number((double)date_year(args[0].data.date));
}

static Value fn_month(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAL_DATE) return value_number(0);
    return value_number((double)date_month(args[0].data.date));
}

static Value fn_day(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAL_DATE) return value_number(0);
    return value_number((double)date_day(args[0].data.date));
}

static Value fn_dow(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAL_DATE) return value_number(0);
    return value_number((double)date_dow(args[0].data.date));
}

static Value fn_cdow(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAL_DATE) return value_string("");
    return value_string(date_cdow(args[0].data.date));
}

static Value fn_cmonth(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAL_DATE) return value_string("");
    return value_string(date_cmonth(args[0].data.date));
}

static Value fn_dtoc(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAL_DATE) return value_string("");

    /* Convert YYYYMMDD to MM/DD/YY */
    char buf[12];
    snprintf(buf, sizeof(buf), "%c%c/%c%c/%c%c",
             args[0].data.date[4], args[0].data.date[5],
             args[0].data.date[6], args[0].data.date[7],
             args[0].data.date[2], args[0].data.date[3]);

    return value_string(buf);
}

static Value fn_ctod(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAL_STRING) return value_date("");

    /* Convert MM/DD/YY to YYYYMMDD */
    int m, d, y;
    if (sscanf(args[0].data.string, "%d/%d/%d", &m, &d, &y) == 3) {
        if (y < 100) y += (y < 50) ? 2000 : 1900;
        char buf[9];
        snprintf(buf, sizeof(buf), "%04d%02d%02d", y, m, d);
        return value_date(buf);
    }

    return value_date("");
}

/* === Type Functions === */

static Value fn_type(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1) return value_string("U");

    switch (args[0].type) {
        case VAL_NUMBER:  return value_string("N");
        case VAL_STRING:  return value_string("C");
        case VAL_DATE:    return value_string("D");
        case VAL_LOGICAL: return value_string("L");
        case VAL_ARRAY:   return value_string("A");
        default:          return value_string("U");
    }
}

static Value fn_empty(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1) return value_logical(true);

    switch (args[0].type) {
        case VAL_NIL:
            return value_logical(true);
        case VAL_NUMBER:
            return value_logical(args[0].data.number == 0);
        case VAL_STRING:
            return value_logical(str_empty(args[0].data.string));
        case VAL_DATE:
            return value_logical(args[0].data.date[0] == ' ');
        case VAL_LOGICAL:
            return value_logical(!args[0].data.logical);
        case VAL_ARRAY:
            return value_logical(args[0].data.array.count == 0);
    }
    return value_logical(true);
}

static Value fn_isalpha(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAL_STRING) return value_logical(false);
    const char *s = args[0].data.string;
    return value_logical(s[0] != '\0' && isalpha((unsigned char)s[0]));
}

static Value fn_isdigit(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAL_STRING) return value_logical(false);
    const char *s = args[0].data.string;
    return value_logical(s[0] != '\0' && isdigit((unsigned char)s[0]));
}

static Value fn_isupper(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAL_STRING) return value_logical(false);
    const char *s = args[0].data.string;
    return value_logical(s[0] != '\0' && isupper((unsigned char)s[0]));
}

static Value fn_islower(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 1 || args[0].type != VAL_STRING) return value_logical(false);
    const char *s = args[0].data.string;
    return value_logical(s[0] != '\0' && islower((unsigned char)s[0]));
}

/* === Database Functions === */

static Value fn_recno(Value *args, int arg_count, EvalContext *ctx) {
    (void)args; (void)arg_count;
    if (!ctx->current_dbf) return value_number(0);
    return value_number((double)dbf_recno(ctx->current_dbf));
}

static Value fn_reccount(Value *args, int arg_count, EvalContext *ctx) {
    (void)args; (void)arg_count;
    if (!ctx->current_dbf) return value_number(0);
    return value_number((double)dbf_reccount(ctx->current_dbf));
}

static Value fn_eof(Value *args, int arg_count, EvalContext *ctx) {
    (void)args; (void)arg_count;
    if (!ctx->current_dbf) return value_logical(true);
    return value_logical(dbf_eof(ctx->current_dbf));
}

static Value fn_bof(Value *args, int arg_count, EvalContext *ctx) {
    (void)args; (void)arg_count;
    if (!ctx->current_dbf) return value_logical(true);
    return value_logical(dbf_bof(ctx->current_dbf));
}

static Value fn_deleted(Value *args, int arg_count, EvalContext *ctx) {
    (void)args; (void)arg_count;
    if (!ctx->current_dbf) return value_logical(false);
    return value_logical(dbf_deleted(ctx->current_dbf));
}

static Value fn_fcount(Value *args, int arg_count, EvalContext *ctx) {
    (void)args; (void)arg_count;
    if (!ctx->current_dbf) return value_number(0);
    return value_number((double)dbf_field_count(ctx->current_dbf));
}

static Value fn_field(Value *args, int arg_count, EvalContext *ctx) {
    if (arg_count < 1 || !ctx->current_dbf) return value_string("");

    int idx = (int)value_to_number(&args[0]) - 1;  /* 1-based */
    const DBFField *field = dbf_field_info(ctx->current_dbf, idx);
    if (!field) return value_string("");

    return value_string(field->name);
}

/* === Miscellaneous Functions === */

static Value fn_iif(Value *args, int arg_count, EvalContext *ctx) {
    (void)ctx;
    if (arg_count < 3) return value_nil();

    if (value_to_logical(&args[0])) {
        return value_copy(&args[1]);
    } else {
        return value_copy(&args[2]);
    }
}

static Value fn_time(Value *args, int arg_count, EvalContext *ctx) {
    (void)args; (void)arg_count; (void)ctx;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char buf[9];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
    return value_string(buf);
}

/* Function table */
static FuncEntry functions[] = {
    /* String functions */
    {"LEN",       fn_len,       1, 1},
    {"TRIM",      fn_trim,      1, 1},
    {"ALLTRIM",   fn_trim,      1, 1},
    {"LTRIM",     fn_ltrim,     1, 1},
    {"RTRIM",     fn_rtrim,     1, 1},
    {"UPPER",     fn_upper,     1, 1},
    {"LOWER",     fn_lower,     1, 1},
    {"SUBSTR",    fn_substr,    2, 3},
    {"LEFT",      fn_left,      2, 2},
    {"RIGHT",     fn_right,     2, 2},
    {"AT",        fn_at,        2, 2},
    {"SPACE",     fn_space,     1, 1},
    {"REPLICATE", fn_replicate, 2, 2},
    {"STUFF",     fn_stuff,     4, 4},
    {"CHR",       fn_chr,       1, 1},
    {"ASC",       fn_asc,       1, 1},

    /* Numeric functions */
    {"ABS",       fn_abs,       1, 1},
    {"INT",       fn_int,       1, 1},
    {"ROUND",     fn_round,     1, 2},
    {"SQRT",      fn_sqrt,      1, 1},
    {"MOD",       fn_mod,       2, 2},
    {"MAX",       fn_max,       2, 2},
    {"MIN",       fn_min,       2, 2},
    {"LOG",       fn_log,       1, 1},
    {"EXP",       fn_exp,       1, 1},

    /* Conversion functions */
    {"STR",       fn_str,       1, 3},
    {"VAL",       fn_val,       1, 1},

    /* Date functions */
    {"DATE",      fn_date,      0, 0},
    {"YEAR",      fn_year,      1, 1},
    {"MONTH",     fn_month,     1, 1},
    {"DAY",       fn_day,       1, 1},
    {"DOW",       fn_dow,       1, 1},
    {"CDOW",      fn_cdow,      1, 1},
    {"CMONTH",    fn_cmonth,    1, 1},
    {"DTOC",      fn_dtoc,      1, 1},
    {"CTOD",      fn_ctod,      1, 1},

    /* Type functions */
    {"TYPE",      fn_type,      1, 1},
    {"EMPTY",     fn_empty,     1, 1},
    {"ISALPHA",   fn_isalpha,   1, 1},
    {"ISDIGIT",   fn_isdigit,   1, 1},
    {"ISUPPER",   fn_isupper,   1, 1},
    {"ISLOWER",   fn_islower,   1, 1},

    /* Database functions */
    {"RECNO",     fn_recno,     0, 0},
    {"RECCOUNT",  fn_reccount,  0, 0},
    {"LASTREC",   fn_reccount,  0, 0},
    {"EOF",       fn_eof,       0, 0},
    {"BOF",       fn_bof,       0, 0},
    {"DELETED",   fn_deleted,   0, 0},
    {"FCOUNT",    fn_fcount,    0, 0},
    {"FIELD",     fn_field,     1, 1},

    /* Miscellaneous */
    {"IIF",       fn_iif,       3, 3},
    {"TIME",      fn_time,      0, 0},

    {NULL, NULL, 0, 0}
};

Value func_call(const char *name, Value *args, int arg_count, EvalContext *ctx) {
    for (int i = 0; functions[i].name; i++) {
        if (str_casecmp(functions[i].name, name) == 0) {
            if (arg_count < functions[i].min_args) {
                error_set(ERR_SYNTAX, "Too few arguments for %s()", name);
                return value_nil();
            }
            if (arg_count > functions[i].max_args) {
                error_set(ERR_SYNTAX, "Too many arguments for %s()", name);
                return value_nil();
            }
            return functions[i].func(args, arg_count, ctx);
        }
    }

    error_set(ERR_UNDEFINED_FUNC, "%s()", name);
    return value_nil();
}

bool func_exists(const char *name) {
    for (int i = 0; functions[i].name; i++) {
        if (str_casecmp(functions[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}
