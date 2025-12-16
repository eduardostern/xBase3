/*
 * xBase3 - dBASE III+ Compatible Database System
 * json.c - Lightweight JSON parser and builder
 */

#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* Parser state */
static const char *g_parse_ptr = NULL;
static char g_parse_error[256] = {0};

/* String buffer for building output */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

static void strbuf_init(StrBuf *sb) {
    sb->data = malloc(256);
    sb->data[0] = '\0';
    sb->len = 0;
    sb->cap = 256;
}

static void strbuf_grow(StrBuf *sb, size_t needed) {
    if (sb->len + needed >= sb->cap) {
        size_t new_cap = sb->cap * 2;
        while (new_cap < sb->len + needed + 1) {
            new_cap *= 2;
        }
        sb->data = realloc(sb->data, new_cap);
        sb->cap = new_cap;
    }
}

static void strbuf_append(StrBuf *sb, const char *str) {
    size_t slen = strlen(str);
    strbuf_grow(sb, slen);
    memcpy(sb->data + sb->len, str, slen + 1);
    sb->len += slen;
}

static void strbuf_append_char(StrBuf *sb, char c) {
    strbuf_grow(sb, 1);
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
}

static char *strbuf_finish(StrBuf *sb) {
    return sb->data;  /* Caller owns the buffer */
}

/*
 * JSON Value Creation
 */

JsonValue *json_null(void) {
    JsonValue *v = calloc(1, sizeof(JsonValue));
    v->type = JSON_NULL;
    return v;
}

JsonValue *json_bool(bool val) {
    JsonValue *v = calloc(1, sizeof(JsonValue));
    v->type = JSON_BOOL;
    v->data.bool_val = val;
    return v;
}

JsonValue *json_number(double val) {
    JsonValue *v = calloc(1, sizeof(JsonValue));
    v->type = JSON_NUMBER;
    v->data.number_val = val;
    return v;
}

JsonValue *json_string(const char *val) {
    JsonValue *v = calloc(1, sizeof(JsonValue));
    v->type = JSON_STRING;
    v->data.string_val = val ? strdup(val) : strdup("");
    return v;
}

JsonValue *json_array(void) {
    JsonValue *v = calloc(1, sizeof(JsonValue));
    v->type = JSON_ARRAY;
    v->data.array_val = NULL;
    return v;
}

JsonValue *json_object(void) {
    JsonValue *v = calloc(1, sizeof(JsonValue));
    v->type = JSON_OBJECT;
    v->data.object_val = NULL;
    return v;
}

/*
 * Array Operations
 */

void json_array_push(JsonValue *arr, JsonValue *val) {
    if (!arr || arr->type != JSON_ARRAY || !val) return;

    JsonElement *elem = calloc(1, sizeof(JsonElement));
    elem->value = val;
    elem->next = NULL;

    if (!arr->data.array_val) {
        arr->data.array_val = elem;
    } else {
        JsonElement *last = arr->data.array_val;
        while (last->next) last = last->next;
        last->next = elem;
    }
}

size_t json_array_length(JsonValue *arr) {
    if (!arr || arr->type != JSON_ARRAY) return 0;
    size_t count = 0;
    JsonElement *e = arr->data.array_val;
    while (e) {
        count++;
        e = e->next;
    }
    return count;
}

JsonValue *json_array_get(JsonValue *arr, size_t index) {
    if (!arr || arr->type != JSON_ARRAY) return NULL;
    JsonElement *e = arr->data.array_val;
    for (size_t i = 0; e && i < index; i++) {
        e = e->next;
    }
    return e ? e->value : NULL;
}

/*
 * Object Operations
 */

void json_object_set(JsonValue *obj, const char *key, JsonValue *val) {
    if (!obj || obj->type != JSON_OBJECT || !key || !val) return;

    /* Check if key exists */
    JsonPair *p = obj->data.object_val;
    while (p) {
        if (strcmp(p->key, key) == 0) {
            json_free(p->value);
            p->value = val;
            return;
        }
        p = p->next;
    }

    /* Add new pair */
    JsonPair *pair = calloc(1, sizeof(JsonPair));
    pair->key = strdup(key);
    pair->value = val;
    pair->next = obj->data.object_val;
    obj->data.object_val = pair;
}

JsonValue *json_object_get(JsonValue *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT || !key) return NULL;
    JsonPair *p = obj->data.object_val;
    while (p) {
        if (strcmp(p->key, key) == 0) return p->value;
        p = p->next;
    }
    return NULL;
}

bool json_object_has(JsonValue *obj, const char *key) {
    return json_object_get(obj, key) != NULL;
}

size_t json_object_size(JsonValue *obj) {
    if (!obj || obj->type != JSON_OBJECT) return 0;
    size_t count = 0;
    JsonPair *p = obj->data.object_val;
    while (p) {
        count++;
        p = p->next;
    }
    return count;
}

JsonPair *json_object_pairs(JsonValue *obj) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return obj->data.object_val;
}

/*
 * Memory Management
 */

void json_free(JsonValue *val) {
    if (!val) return;

    switch (val->type) {
        case JSON_STRING:
            free(val->data.string_val);
            break;
        case JSON_ARRAY: {
            JsonElement *e = val->data.array_val;
            while (e) {
                JsonElement *next = e->next;
                json_free(e->value);
                free(e);
                e = next;
            }
            break;
        }
        case JSON_OBJECT: {
            JsonPair *p = val->data.object_val;
            while (p) {
                JsonPair *next = p->next;
                free(p->key);
                json_free(p->value);
                free(p);
                p = next;
            }
            break;
        }
        default:
            break;
    }
    free(val);
}

/*
 * Type Checking
 */

bool json_is_null(JsonValue *val) { return val && val->type == JSON_NULL; }
bool json_is_bool(JsonValue *val) { return val && val->type == JSON_BOOL; }
bool json_is_number(JsonValue *val) { return val && val->type == JSON_NUMBER; }
bool json_is_string(JsonValue *val) { return val && val->type == JSON_STRING; }
bool json_is_array(JsonValue *val) { return val && val->type == JSON_ARRAY; }
bool json_is_object(JsonValue *val) { return val && val->type == JSON_OBJECT; }

bool json_get_bool(JsonValue *val, bool *out) {
    if (!val || val->type != JSON_BOOL) return false;
    if (out) *out = val->data.bool_val;
    return true;
}

bool json_get_number(JsonValue *val, double *out) {
    if (!val || val->type != JSON_NUMBER) return false;
    if (out) *out = val->data.number_val;
    return true;
}

const char *json_get_string(JsonValue *val) {
    if (!val || val->type != JSON_STRING) return NULL;
    return val->data.string_val;
}

/*
 * Serialization
 */

static void json_stringify_value(StrBuf *sb, JsonValue *val, int indent, int depth);

static void json_stringify_string(StrBuf *sb, const char *str) {
    strbuf_append_char(sb, '"');
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '"':  strbuf_append(sb, "\\\""); break;
            case '\\': strbuf_append(sb, "\\\\"); break;
            case '\b': strbuf_append(sb, "\\b"); break;
            case '\f': strbuf_append(sb, "\\f"); break;
            case '\n': strbuf_append(sb, "\\n"); break;
            case '\r': strbuf_append(sb, "\\r"); break;
            case '\t': strbuf_append(sb, "\\t"); break;
            default:
                if ((unsigned char)*p < 32) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)*p);
                    strbuf_append(sb, buf);
                } else {
                    strbuf_append_char(sb, *p);
                }
        }
    }
    strbuf_append_char(sb, '"');
}

static void json_add_indent(StrBuf *sb, int indent, int depth) {
    if (indent <= 0) return;
    strbuf_append_char(sb, '\n');
    for (int i = 0; i < indent * depth; i++) {
        strbuf_append_char(sb, ' ');
    }
}

static void json_stringify_value(StrBuf *sb, JsonValue *val, int indent, int depth) {
    if (!val) {
        strbuf_append(sb, "null");
        return;
    }

    switch (val->type) {
        case JSON_NULL:
            strbuf_append(sb, "null");
            break;

        case JSON_BOOL:
            strbuf_append(sb, val->data.bool_val ? "true" : "false");
            break;

        case JSON_NUMBER: {
            char buf[64];
            double num = val->data.number_val;
            if (floor(num) == num && fabs(num) < 1e15) {
                snprintf(buf, sizeof(buf), "%.0f", num);
            } else {
                snprintf(buf, sizeof(buf), "%.15g", num);
            }
            strbuf_append(sb, buf);
            break;
        }

        case JSON_STRING:
            json_stringify_string(sb, val->data.string_val);
            break;

        case JSON_ARRAY: {
            strbuf_append_char(sb, '[');
            JsonElement *e = val->data.array_val;
            bool first = true;
            while (e) {
                if (!first) strbuf_append_char(sb, ',');
                first = false;
                if (indent > 0) json_add_indent(sb, indent, depth + 1);
                json_stringify_value(sb, e->value, indent, depth + 1);
                e = e->next;
            }
            if (val->data.array_val && indent > 0) {
                json_add_indent(sb, indent, depth);
            }
            strbuf_append_char(sb, ']');
            break;
        }

        case JSON_OBJECT: {
            strbuf_append_char(sb, '{');
            JsonPair *p = val->data.object_val;
            bool first = true;
            while (p) {
                if (!first) strbuf_append_char(sb, ',');
                first = false;
                if (indent > 0) json_add_indent(sb, indent, depth + 1);
                json_stringify_string(sb, p->key);
                strbuf_append_char(sb, ':');
                if (indent > 0) strbuf_append_char(sb, ' ');
                json_stringify_value(sb, p->value, indent, depth + 1);
                p = p->next;
            }
            if (val->data.object_val && indent > 0) {
                json_add_indent(sb, indent, depth);
            }
            strbuf_append_char(sb, '}');
            break;
        }
    }
}

char *json_stringify(JsonValue *val) {
    StrBuf sb;
    strbuf_init(&sb);
    json_stringify_value(&sb, val, 0, 0);
    return strbuf_finish(&sb);
}

char *json_stringify_pretty(JsonValue *val, int indent) {
    StrBuf sb;
    strbuf_init(&sb);
    json_stringify_value(&sb, val, indent > 0 ? indent : 2, 0);
    return strbuf_finish(&sb);
}

/*
 * Parser
 */

static void skip_whitespace(void) {
    while (*g_parse_ptr && isspace((unsigned char)*g_parse_ptr)) {
        g_parse_ptr++;
    }
}

static bool parse_literal(const char *lit) {
    size_t len = strlen(lit);
    if (strncmp(g_parse_ptr, lit, len) == 0) {
        g_parse_ptr += len;
        return true;
    }
    return false;
}

static JsonValue *parse_value(void);

static JsonValue *parse_string(void) {
    if (*g_parse_ptr != '"') return NULL;
    g_parse_ptr++;

    StrBuf sb;
    strbuf_init(&sb);

    while (*g_parse_ptr && *g_parse_ptr != '"') {
        if (*g_parse_ptr == '\\') {
            g_parse_ptr++;
            switch (*g_parse_ptr) {
                case '"':  strbuf_append_char(&sb, '"'); break;
                case '\\': strbuf_append_char(&sb, '\\'); break;
                case '/':  strbuf_append_char(&sb, '/'); break;
                case 'b':  strbuf_append_char(&sb, '\b'); break;
                case 'f':  strbuf_append_char(&sb, '\f'); break;
                case 'n':  strbuf_append_char(&sb, '\n'); break;
                case 'r':  strbuf_append_char(&sb, '\r'); break;
                case 't':  strbuf_append_char(&sb, '\t'); break;
                case 'u': {
                    /* Parse 4 hex digits */
                    g_parse_ptr++;
                    char hex[5] = {0};
                    for (int i = 0; i < 4 && isxdigit((unsigned char)*g_parse_ptr); i++) {
                        hex[i] = *g_parse_ptr++;
                    }
                    g_parse_ptr--;  /* Will be incremented below */
                    int code = (int)strtol(hex, NULL, 16);
                    if (code < 128) {
                        strbuf_append_char(&sb, (char)code);
                    } else if (code < 2048) {
                        strbuf_append_char(&sb, (char)(0xC0 | (code >> 6)));
                        strbuf_append_char(&sb, (char)(0x80 | (code & 0x3F)));
                    } else {
                        strbuf_append_char(&sb, (char)(0xE0 | (code >> 12)));
                        strbuf_append_char(&sb, (char)(0x80 | ((code >> 6) & 0x3F)));
                        strbuf_append_char(&sb, (char)(0x80 | (code & 0x3F)));
                    }
                    break;
                }
                default:
                    strbuf_append_char(&sb, *g_parse_ptr);
            }
            g_parse_ptr++;
        } else {
            strbuf_append_char(&sb, *g_parse_ptr++);
        }
    }

    if (*g_parse_ptr != '"') {
        free(sb.data);
        snprintf(g_parse_error, sizeof(g_parse_error), "Unterminated string");
        return NULL;
    }
    g_parse_ptr++;

    JsonValue *v = calloc(1, sizeof(JsonValue));
    v->type = JSON_STRING;
    v->data.string_val = strbuf_finish(&sb);
    return v;
}

static JsonValue *parse_number(void) {
    const char *start = g_parse_ptr;

    if (*g_parse_ptr == '-') g_parse_ptr++;

    if (*g_parse_ptr == '0') {
        g_parse_ptr++;
    } else if (isdigit((unsigned char)*g_parse_ptr)) {
        while (isdigit((unsigned char)*g_parse_ptr)) g_parse_ptr++;
    } else {
        snprintf(g_parse_error, sizeof(g_parse_error), "Invalid number");
        return NULL;
    }

    if (*g_parse_ptr == '.') {
        g_parse_ptr++;
        if (!isdigit((unsigned char)*g_parse_ptr)) {
            snprintf(g_parse_error, sizeof(g_parse_error), "Invalid number after decimal");
            return NULL;
        }
        while (isdigit((unsigned char)*g_parse_ptr)) g_parse_ptr++;
    }

    if (*g_parse_ptr == 'e' || *g_parse_ptr == 'E') {
        g_parse_ptr++;
        if (*g_parse_ptr == '+' || *g_parse_ptr == '-') g_parse_ptr++;
        if (!isdigit((unsigned char)*g_parse_ptr)) {
            snprintf(g_parse_error, sizeof(g_parse_error), "Invalid exponent");
            return NULL;
        }
        while (isdigit((unsigned char)*g_parse_ptr)) g_parse_ptr++;
    }

    char *end;
    double val = strtod(start, &end);

    JsonValue *v = calloc(1, sizeof(JsonValue));
    v->type = JSON_NUMBER;
    v->data.number_val = val;
    return v;
}

static JsonValue *parse_array(void) {
    if (*g_parse_ptr != '[') return NULL;
    g_parse_ptr++;

    JsonValue *arr = json_array();

    skip_whitespace();
    if (*g_parse_ptr == ']') {
        g_parse_ptr++;
        return arr;
    }

    while (1) {
        skip_whitespace();
        JsonValue *elem = parse_value();
        if (!elem) {
            json_free(arr);
            return NULL;
        }
        json_array_push(arr, elem);

        skip_whitespace();
        if (*g_parse_ptr == ']') {
            g_parse_ptr++;
            return arr;
        }
        if (*g_parse_ptr != ',') {
            snprintf(g_parse_error, sizeof(g_parse_error), "Expected ',' or ']' in array");
            json_free(arr);
            return NULL;
        }
        g_parse_ptr++;
    }
}

static JsonValue *parse_object(void) {
    if (*g_parse_ptr != '{') return NULL;
    g_parse_ptr++;

    JsonValue *obj = json_object();

    skip_whitespace();
    if (*g_parse_ptr == '}') {
        g_parse_ptr++;
        return obj;
    }

    while (1) {
        skip_whitespace();
        if (*g_parse_ptr != '"') {
            snprintf(g_parse_error, sizeof(g_parse_error), "Expected string key in object");
            json_free(obj);
            return NULL;
        }

        JsonValue *key_val = parse_string();
        if (!key_val) {
            json_free(obj);
            return NULL;
        }
        char *key = strdup(key_val->data.string_val);
        json_free(key_val);

        skip_whitespace();
        if (*g_parse_ptr != ':') {
            snprintf(g_parse_error, sizeof(g_parse_error), "Expected ':' after key");
            free(key);
            json_free(obj);
            return NULL;
        }
        g_parse_ptr++;

        skip_whitespace();
        JsonValue *val = parse_value();
        if (!val) {
            free(key);
            json_free(obj);
            return NULL;
        }

        json_object_set(obj, key, val);
        free(key);

        skip_whitespace();
        if (*g_parse_ptr == '}') {
            g_parse_ptr++;
            return obj;
        }
        if (*g_parse_ptr != ',') {
            snprintf(g_parse_error, sizeof(g_parse_error), "Expected ',' or '}' in object");
            json_free(obj);
            return NULL;
        }
        g_parse_ptr++;
    }
}

static JsonValue *parse_value(void) {
    skip_whitespace();

    if (parse_literal("null")) return json_null();
    if (parse_literal("true")) return json_bool(true);
    if (parse_literal("false")) return json_bool(false);
    if (*g_parse_ptr == '"') return parse_string();
    if (*g_parse_ptr == '[') return parse_array();
    if (*g_parse_ptr == '{') return parse_object();
    if (*g_parse_ptr == '-' || isdigit((unsigned char)*g_parse_ptr)) return parse_number();

    snprintf(g_parse_error, sizeof(g_parse_error), "Unexpected character '%c'", *g_parse_ptr);
    return NULL;
}

JsonValue *json_parse(const char *str) {
    if (!str) {
        snprintf(g_parse_error, sizeof(g_parse_error), "NULL input");
        return NULL;
    }

    g_parse_ptr = str;
    g_parse_error[0] = '\0';

    JsonValue *val = parse_value();
    if (!val) return NULL;

    skip_whitespace();
    if (*g_parse_ptr != '\0') {
        snprintf(g_parse_error, sizeof(g_parse_error), "Unexpected data after JSON value");
        json_free(val);
        return NULL;
    }

    return val;
}

const char *json_parse_error(void) {
    return g_parse_error;
}

/*
 * Response Helpers
 */

JsonValue *json_response_ok(JsonValue *data) {
    JsonValue *resp = json_object();
    json_object_set(resp, "ok", json_bool(true));
    json_object_set(resp, "data", data ? data : json_null());
    json_object_set(resp, "error", json_null());
    return resp;
}

JsonValue *json_response_error(const char *code, const char *message) {
    JsonValue *err = json_object();
    json_object_set(err, "code", json_string(code ? code : "ERR_UNKNOWN"));
    json_object_set(err, "message", json_string(message ? message : "Unknown error"));

    JsonValue *resp = json_object();
    json_object_set(resp, "ok", json_bool(false));
    json_object_set(resp, "data", json_null());
    json_object_set(resp, "error", err);
    return resp;
}
