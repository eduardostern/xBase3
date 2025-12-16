/*
 * xBase3 - dBASE III+ Compatible Database System
 * json.h - Lightweight JSON parser and builder
 */

#ifndef XBASE3_JSON_H
#define XBASE3_JSON_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* JSON value types */
typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

/* Forward declarations */
typedef struct JsonValue JsonValue;
typedef struct JsonPair JsonPair;

/* JSON object key-value pair */
struct JsonPair {
    char *key;
    JsonValue *value;
    JsonPair *next;
};

/* JSON array element */
typedef struct JsonElement {
    JsonValue *value;
    struct JsonElement *next;
} JsonElement;

/* JSON value */
struct JsonValue {
    JsonType type;
    union {
        bool bool_val;
        double number_val;
        char *string_val;
        JsonElement *array_val;     /* Linked list of elements */
        JsonPair *object_val;       /* Linked list of key-value pairs */
    } data;
};

/*
 * JSON Builder API
 */

/* Create JSON values */
JsonValue *json_null(void);
JsonValue *json_bool(bool val);
JsonValue *json_number(double val);
JsonValue *json_string(const char *val);
JsonValue *json_array(void);
JsonValue *json_object(void);

/* Array operations */
void json_array_push(JsonValue *arr, JsonValue *val);

/* Object operations */
void json_object_set(JsonValue *obj, const char *key, JsonValue *val);
JsonValue *json_object_get(JsonValue *obj, const char *key);
bool json_object_has(JsonValue *obj, const char *key);

/* Free JSON value and all children */
void json_free(JsonValue *val);

/* Serialize to string (caller must free result) */
char *json_stringify(JsonValue *val);
char *json_stringify_pretty(JsonValue *val, int indent);

/*
 * JSON Parser API
 */

/* Parse JSON string, returns NULL on error */
JsonValue *json_parse(const char *str);

/* Get parse error message (valid after json_parse returns NULL) */
const char *json_parse_error(void);

/*
 * Value accessors (with type checking)
 */

bool json_is_null(JsonValue *val);
bool json_is_bool(JsonValue *val);
bool json_is_number(JsonValue *val);
bool json_is_string(JsonValue *val);
bool json_is_array(JsonValue *val);
bool json_is_object(JsonValue *val);

bool json_get_bool(JsonValue *val, bool *out);
bool json_get_number(JsonValue *val, double *out);
const char *json_get_string(JsonValue *val);

/* Array iteration */
size_t json_array_length(JsonValue *arr);
JsonValue *json_array_get(JsonValue *arr, size_t index);

/* Object iteration */
size_t json_object_size(JsonValue *obj);
JsonPair *json_object_pairs(JsonValue *obj);

/*
 * Response helpers
 */

/* Create standard API response: {"ok": true/false, "data": ..., "error": ...} */
JsonValue *json_response_ok(JsonValue *data);
JsonValue *json_response_error(const char *code, const char *message);

#endif /* XBASE3_JSON_H */
