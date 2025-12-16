/*
 * xBase3 - dBASE III+ Compatible Database System
 * handlers.c - REST API handlers implementation
 */

#include "handlers.h"
#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdarg.h>

/* Helper: get JSON body or return error */
static JsonValue *get_json_body(HttpRequest *req, HttpResponse *resp) {
    if (!req->body || req->body_len == 0) {
        http_response_error(resp, 400, "ERR_NO_BODY", "Request body required");
        return NULL;
    }

    /* Null-terminate body for parsing */
    char *body = malloc(req->body_len + 1);
    memcpy(body, req->body, req->body_len);
    body[req->body_len] = '\0';

    JsonValue *json = json_parse(body);
    free(body);

    if (!json) {
        http_response_error(resp, 400, "ERR_INVALID_JSON", json_parse_error());
        return NULL;
    }

    return json;
}

/* Helper: check database is open */
static bool check_database(HttpResponse *resp, CommandContext *ctx) {
    if (!cmd_get_current_dbf(ctx)) {
        http_response_error(resp, 400, "ERR_NO_DATABASE", "No database open");
        return false;
    }
    return true;
}

/* Helper: record to JSON object */
static JsonValue *record_to_json(DBF *dbf) {
    JsonValue *record = json_object();

    json_object_set(record, "recno", json_number((double)dbf_recno(dbf)));
    json_object_set(record, "deleted", json_bool(dbf_deleted(dbf)));

    JsonValue *fields = json_object();
    int fc = dbf_field_count(dbf);
    for (int i = 0; i < fc; i++) {
        const DBFField *field = dbf_field_info(dbf, i);
        char value[MAX_FIELD_LEN + 1];
        dbf_get_string(dbf, i, value, sizeof(value));

        /* Trim trailing spaces */
        size_t len = strlen(value);
        while (len > 0 && value[len - 1] == ' ') {
            value[--len] = '\0';
        }

        switch (field->type) {
            case FIELD_TYPE_NUMERIC: {
                double num;
                if (str_to_num(value, &num)) {
                    json_object_set(fields, field->name, json_number(num));
                } else {
                    json_object_set(fields, field->name, json_null());
                }
                break;
            }
            case FIELD_TYPE_LOGICAL:
                json_object_set(fields, field->name,
                    json_bool(value[0] == 'T' || value[0] == 'Y' ||
                              value[0] == 't' || value[0] == 'y'));
                break;
            default:
                json_object_set(fields, field->name, json_string(value));
                break;
        }
    }
    json_object_set(record, "fields", fields);

    return record;
}

/* Helper: field info to JSON */
static JsonValue *field_to_json(const DBFField *field) {
    JsonValue *f = json_object();
    json_object_set(f, "name", json_string(field->name));
    char type[2] = {field->type, '\0'};
    json_object_set(f, "type", json_string(type));
    json_object_set(f, "length", json_number(field->length));
    json_object_set(f, "decimals", json_number(field->decimals));
    return f;
}

/*
 * Database endpoints
 */
void handle_database_open(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    JsonValue *body = get_json_body(req, resp);
    if (!body) return;

    JsonValue *filename_val = json_object_get(body, "filename");
    const char *filename = json_get_string(filename_val);
    if (!filename) {
        http_response_error(resp, 400, "ERR_MISSING_PARAM", "filename is required");
        json_free(body);
        return;
    }

    /* Close current database if open */
    if (cmd_get_current_dbf(ctx)) {
        dbf_close(cmd_get_current_dbf(ctx));
        cmd_set_current_dbf(ctx, NULL);
    }

    /* Build path */
    char path[MAX_PATH_LEN];
    if (filename[0] == '/') {
        strncpy(path, filename, MAX_PATH_LEN - 1);
    } else {
        snprintf(path, MAX_PATH_LEN, "%s/%s", ctx->current_path, filename);
    }
    if (!file_extension(path)) {
        strcat(path, ".dbf");
    }

    /* Open database */
    DBF *dbf = dbf_open(path, false);
    if (!dbf) {
        http_response_error(resp, 400, "ERR_OPEN_FAILED", error_string(g_last_error));
        json_free(body);
        return;
    }

    cmd_set_current_dbf(ctx, dbf);

    /* Build response */
    JsonValue *data = json_object();
    json_object_set(data, "filename", json_string(path));
    json_object_set(data, "records", json_number((double)dbf_reccount(dbf)));
    json_object_set(data, "fields", json_number((double)dbf_field_count(dbf)));

    JsonValue *response = json_response_ok(data);
    http_response_json(resp, response);
    json_free(response);
    json_free(body);
}

void handle_database_close(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    (void)req;

    if (cmd_get_current_dbf(ctx)) {
        dbf_close(cmd_get_current_dbf(ctx));
        cmd_set_current_dbf(ctx, NULL);
    }

    JsonValue *response = json_response_ok(json_bool(true));
    http_response_json(resp, response);
    json_free(response);
}

void handle_database_info(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    (void)req;

    if (!check_database(resp, ctx)) return;
    DBF *dbf = cmd_get_current_dbf(ctx);

    JsonValue *data = json_object();
    json_object_set(data, "filename", json_string(dbf->filename));
    json_object_set(data, "records", json_number((double)dbf_reccount(dbf)));
    json_object_set(data, "field_count", json_number((double)dbf_field_count(dbf)));
    json_object_set(data, "record_size", json_number((double)dbf->header.record_size));

    /* Add field info */
    JsonValue *fields = json_array();
    int fc = dbf_field_count(dbf);
    for (int i = 0; i < fc; i++) {
        json_array_push(fields, field_to_json(dbf_field_info(dbf, i)));
    }
    json_object_set(data, "fields", fields);

    JsonValue *response = json_response_ok(data);
    http_response_json(resp, response);
    json_free(response);
}

void handle_database_create(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    JsonValue *body = get_json_body(req, resp);
    if (!body) return;

    JsonValue *filename_val = json_object_get(body, "filename");
    const char *filename = json_get_string(filename_val);
    if (!filename) {
        http_response_error(resp, 400, "ERR_MISSING_PARAM", "filename is required");
        json_free(body);
        return;
    }

    JsonValue *fields_arr = json_object_get(body, "fields");
    if (!json_is_array(fields_arr) || json_array_length(fields_arr) == 0) {
        http_response_error(resp, 400, "ERR_MISSING_PARAM", "fields array is required");
        json_free(body);
        return;
    }

    /* Parse field definitions */
    DBFField fields[MAX_FIELDS];
    int field_count = 0;

    size_t arr_len = json_array_length(fields_arr);
    for (size_t i = 0; i < arr_len && field_count < MAX_FIELDS; i++) {
        JsonValue *f = json_array_get(fields_arr, i);
        if (!json_is_object(f)) continue;

        const char *name = json_get_string(json_object_get(f, "name"));
        const char *type = json_get_string(json_object_get(f, "type"));
        double length = 0;
        json_get_number(json_object_get(f, "length"), &length);
        double decimals = 0;
        json_get_number(json_object_get(f, "decimals"), &decimals);

        if (!name || !type || length <= 0) continue;

        memset(&fields[field_count], 0, sizeof(DBFField));
        strncpy(fields[field_count].name, name, MAX_FIELD_NAME - 1);
        str_upper(fields[field_count].name);
        fields[field_count].type = (char)toupper(type[0]);
        fields[field_count].length = (uint16_t)length;
        fields[field_count].decimals = (uint8_t)decimals;
        field_count++;
    }

    if (field_count == 0) {
        http_response_error(resp, 400, "ERR_NO_FIELDS", "No valid field definitions");
        json_free(body);
        return;
    }

    /* Build path */
    char path[MAX_PATH_LEN];
    if (filename[0] == '/') {
        strncpy(path, filename, MAX_PATH_LEN - 1);
    } else {
        snprintf(path, MAX_PATH_LEN, "%s/%s", ctx->current_path, filename);
    }
    if (!file_extension(path)) {
        strcat(path, ".dbf");
    }

    /* Close current database */
    if (cmd_get_current_dbf(ctx)) {
        dbf_close(cmd_get_current_dbf(ctx));
        cmd_set_current_dbf(ctx, NULL);
    }

    /* Create database */
    DBF *dbf = dbf_create(path, fields, field_count);
    if (!dbf) {
        http_response_error(resp, 500, "ERR_CREATE_FAILED", error_string(g_last_error));
        json_free(body);
        return;
    }

    cmd_set_current_dbf(ctx, dbf);

    JsonValue *data = json_object();
    json_object_set(data, "filename", json_string(path));
    json_object_set(data, "fields", json_number((double)field_count));

    JsonValue *response = json_response_ok(data);
    http_response_json(resp, response);
    json_free(response);
    json_free(body);
}

/*
 * Navigation endpoints
 */
void handle_navigate_goto(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    if (!check_database(resp, ctx)) return;
    DBF *dbf = cmd_get_current_dbf(ctx);

    JsonValue *body = get_json_body(req, resp);
    if (!body) return;

    double recno = 0;
    if (!json_get_number(json_object_get(body, "recno"), &recno) || recno < 1) {
        http_response_error(resp, 400, "ERR_INVALID_RECNO", "Valid recno required");
        json_free(body);
        return;
    }

    dbf_goto(dbf, (uint32_t)recno);

    JsonValue *data = json_object();
    json_object_set(data, "recno", json_number((double)dbf_recno(dbf)));
    json_object_set(data, "eof", json_bool(dbf_eof(dbf)));

    JsonValue *response = json_response_ok(data);
    http_response_json(resp, response);
    json_free(response);
    json_free(body);
}

void handle_navigate_skip(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    if (!check_database(resp, ctx)) return;
    DBF *dbf = cmd_get_current_dbf(ctx);

    int count = 1;
    JsonValue *body = get_json_body(req, resp);
    if (body) {
        double n = 0;
        if (json_get_number(json_object_get(body, "count"), &n)) {
            count = (int)n;
        }
        json_free(body);
    }

    dbf_skip(dbf, count);

    JsonValue *data = json_object();
    json_object_set(data, "recno", json_number((double)dbf_recno(dbf)));
    json_object_set(data, "eof", json_bool(dbf_eof(dbf)));
    json_object_set(data, "bof", json_bool(dbf_bof(dbf)));

    JsonValue *response = json_response_ok(data);
    http_response_json(resp, response);
    json_free(response);
}

void handle_navigate_top(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    (void)req;
    if (!check_database(resp, ctx)) return;
    DBF *dbf = cmd_get_current_dbf(ctx);

    dbf_go_top(dbf);

    JsonValue *data = json_object();
    json_object_set(data, "recno", json_number((double)dbf_recno(dbf)));
    json_object_set(data, "eof", json_bool(dbf_eof(dbf)));

    JsonValue *response = json_response_ok(data);
    http_response_json(resp, response);
    json_free(response);
}

void handle_navigate_bottom(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    (void)req;
    if (!check_database(resp, ctx)) return;
    DBF *dbf = cmd_get_current_dbf(ctx);

    dbf_go_bottom(dbf);

    JsonValue *data = json_object();
    json_object_set(data, "recno", json_number((double)dbf_recno(dbf)));
    json_object_set(data, "eof", json_bool(dbf_eof(dbf)));

    JsonValue *response = json_response_ok(data);
    http_response_json(resp, response);
    json_free(response);
}

void handle_navigate_position(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    (void)req;
    if (!check_database(resp, ctx)) return;
    DBF *dbf = cmd_get_current_dbf(ctx);

    JsonValue *data = json_object();
    json_object_set(data, "recno", json_number((double)dbf_recno(dbf)));
    json_object_set(data, "reccount", json_number((double)dbf_reccount(dbf)));
    json_object_set(data, "eof", json_bool(dbf_eof(dbf)));
    json_object_set(data, "bof", json_bool(dbf_bof(dbf)));

    JsonValue *response = json_response_ok(data);
    http_response_json(resp, response);
    json_free(response);
}

/*
 * Record endpoints
 */
void handle_records_list(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    if (!check_database(resp, ctx)) return;
    DBF *dbf = cmd_get_current_dbf(ctx);

    /* Parse query params */
    int limit = 100;
    int offset = 0;

    const char *limit_str = http_get_param(req, "limit");
    if (limit_str) limit = atoi(limit_str);
    if (limit <= 0) limit = 100;
    if (limit > 1000) limit = 1000;

    const char *offset_str = http_get_param(req, "offset");
    if (offset_str) offset = atoi(offset_str);
    if (offset < 0) offset = 0;

    JsonValue *records = json_array();

    dbf_goto(dbf, (uint32_t)(offset + 1));
    int count = 0;

    while (!dbf_eof(dbf) && count < limit) {
        json_array_push(records, record_to_json(dbf));
        count++;
        dbf_skip(dbf, 1);
    }

    JsonValue *data = json_object();
    json_object_set(data, "records", records);
    json_object_set(data, "count", json_number((double)count));
    json_object_set(data, "total", json_number((double)dbf_reccount(dbf)));
    json_object_set(data, "offset", json_number((double)offset));
    json_object_set(data, "limit", json_number((double)limit));

    JsonValue *response = json_response_ok(data);
    http_response_json(resp, response);
    json_free(response);
}

void handle_records_get(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    if (!check_database(resp, ctx)) return;
    DBF *dbf = cmd_get_current_dbf(ctx);

    /* Extract record number from path */
    char recno_str[32];
    if (!http_get_path_param(req, "/api/v1/records/:recno", "recno", recno_str, sizeof(recno_str))) {
        http_response_error(resp, 400, "ERR_INVALID_PATH", "Invalid record number in path");
        return;
    }

    uint32_t recno = (uint32_t)atoi(recno_str);
    if (recno < 1 || recno > dbf_reccount(dbf)) {
        http_response_error(resp, 404, "ERR_RECORD_NOT_FOUND", "Record not found");
        return;
    }

    dbf_goto(dbf, recno);
    JsonValue *data = record_to_json(dbf);

    JsonValue *response = json_response_ok(data);
    http_response_json(resp, response);
    json_free(response);
}

void handle_records_append(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    if (!check_database(resp, ctx)) return;
    DBF *dbf = cmd_get_current_dbf(ctx);

    if (!dbf_append_blank(dbf)) {
        http_response_error(resp, 500, "ERR_APPEND_FAILED", "Failed to append record");
        return;
    }

    /* If body provided, set field values */
    JsonValue *body = NULL;
    if (req->body && req->body_len > 0) {
        body = get_json_body(req, resp);
        if (body) {
            JsonPair *p = json_object_pairs(body);
            while (p) {
                int idx = dbf_field_index(dbf, p->key);
                if (idx >= 0) {
                    const DBFField *field = dbf_field_info(dbf, idx);
                    if (json_is_string(p->value)) {
                        dbf_put_string(dbf, idx, json_get_string(p->value));
                    } else if (json_is_number(p->value)) {
                        double n;
                        json_get_number(p->value, &n);
                        if (field->type == FIELD_TYPE_NUMERIC) {
                            dbf_put_double(dbf, idx, n);
                        }
                    } else if (json_is_bool(p->value)) {
                        bool b;
                        json_get_bool(p->value, &b);
                        if (field->type == FIELD_TYPE_LOGICAL) {
                            dbf_put_logical(dbf, idx, b);
                        }
                    }
                }
                p = p->next;
            }
            json_free(body);
        }
    }

    dbf_flush(dbf);
    JsonValue *data = record_to_json(dbf);

    JsonValue *response = json_response_ok(data);
    http_response_json(resp, response);
    json_free(response);
}

void handle_records_update(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    if (!check_database(resp, ctx)) return;
    DBF *dbf = cmd_get_current_dbf(ctx);

    /* Extract record number from path */
    char recno_str[32];
    if (!http_get_path_param(req, "/api/v1/records/:recno", "recno", recno_str, sizeof(recno_str))) {
        http_response_error(resp, 400, "ERR_INVALID_PATH", "Invalid record number in path");
        return;
    }

    uint32_t recno = (uint32_t)atoi(recno_str);
    if (recno < 1 || recno > dbf_reccount(dbf)) {
        http_response_error(resp, 404, "ERR_RECORD_NOT_FOUND", "Record not found");
        return;
    }

    dbf_goto(dbf, recno);

    JsonValue *body = get_json_body(req, resp);
    if (!body) return;

    /* Update fields */
    JsonPair *p = json_object_pairs(body);
    while (p) {
        int idx = dbf_field_index(dbf, p->key);
        if (idx >= 0) {
            const DBFField *field = dbf_field_info(dbf, idx);
            if (json_is_string(p->value)) {
                dbf_put_string(dbf, idx, json_get_string(p->value));
            } else if (json_is_number(p->value)) {
                double n;
                json_get_number(p->value, &n);
                if (field->type == FIELD_TYPE_NUMERIC) {
                    dbf_put_double(dbf, idx, n);
                }
            } else if (json_is_bool(p->value)) {
                bool b;
                json_get_bool(p->value, &b);
                if (field->type == FIELD_TYPE_LOGICAL) {
                    dbf_put_logical(dbf, idx, b);
                }
            }
        }
        p = p->next;
    }
    json_free(body);

    dbf_flush(dbf);
    JsonValue *data = record_to_json(dbf);

    JsonValue *response = json_response_ok(data);
    http_response_json(resp, response);
    json_free(response);
}

void handle_records_delete(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    if (!check_database(resp, ctx)) return;
    DBF *dbf = cmd_get_current_dbf(ctx);

    /* Extract record number from path */
    char recno_str[32];
    if (!http_get_path_param(req, "/api/v1/records/:recno", "recno", recno_str, sizeof(recno_str))) {
        http_response_error(resp, 400, "ERR_INVALID_PATH", "Invalid record number in path");
        return;
    }

    uint32_t recno = (uint32_t)atoi(recno_str);
    if (recno < 1 || recno > dbf_reccount(dbf)) {
        http_response_error(resp, 404, "ERR_RECORD_NOT_FOUND", "Record not found");
        return;
    }

    dbf_goto(dbf, recno);
    dbf_delete(dbf);
    dbf_flush(dbf);

    JsonValue *data = json_object();
    json_object_set(data, "recno", json_number((double)recno));
    json_object_set(data, "deleted", json_bool(true));

    JsonValue *response = json_response_ok(data);
    http_response_json(resp, response);
    json_free(response);
}

void handle_records_recall(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    if (!check_database(resp, ctx)) return;
    DBF *dbf = cmd_get_current_dbf(ctx);

    /* Extract record number from path */
    char recno_str[32];
    if (!http_get_path_param(req, "/api/v1/records/:recno/recall", "recno", recno_str, sizeof(recno_str))) {
        http_response_error(resp, 400, "ERR_INVALID_PATH", "Invalid record number in path");
        return;
    }

    uint32_t recno = (uint32_t)atoi(recno_str);
    if (recno < 1 || recno > dbf_reccount(dbf)) {
        http_response_error(resp, 404, "ERR_RECORD_NOT_FOUND", "Record not found");
        return;
    }

    dbf_goto(dbf, recno);
    dbf_recall(dbf);
    dbf_flush(dbf);

    JsonValue *data = json_object();
    json_object_set(data, "recno", json_number((double)recno));
    json_object_set(data, "deleted", json_bool(false));

    JsonValue *response = json_response_ok(data);
    http_response_json(resp, response);
    json_free(response);
}

/*
 * Query endpoints
 */
void handle_query_locate(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    if (!check_database(resp, ctx)) return;
    DBF *dbf = cmd_get_current_dbf(ctx);

    JsonValue *body = get_json_body(req, resp);
    if (!body) return;

    const char *field_name = json_get_string(json_object_get(body, "field"));
    JsonValue *value = json_object_get(body, "value");

    if (!field_name || !value) {
        http_response_error(resp, 400, "ERR_MISSING_PARAM", "field and value required");
        json_free(body);
        return;
    }

    int field_idx = dbf_field_index(dbf, field_name);
    if (field_idx < 0) {
        http_response_error(resp, 400, "ERR_INVALID_FIELD", "Field not found");
        json_free(body);
        return;
    }

    /* Get search value as string */
    char search_val[MAX_FIELD_LEN + 1] = "";
    if (json_is_string(value)) {
        strncpy(search_val, json_get_string(value), MAX_FIELD_LEN);
    } else if (json_is_number(value)) {
        double n;
        json_get_number(value, &n);
        snprintf(search_val, sizeof(search_val), "%g", n);
    }

    /* Search */
    dbf_go_top(dbf);
    while (!dbf_eof(dbf)) {
        char field_val[MAX_FIELD_LEN + 1];
        dbf_get_string(dbf, field_idx, field_val, sizeof(field_val));
        str_trim(field_val);

        if (str_casecmp(field_val, search_val) == 0) {
            JsonValue *data = record_to_json(dbf);
            json_object_set(data, "found", json_bool(true));

            JsonValue *response = json_response_ok(data);
            http_response_json(resp, response);
            json_free(response);
            json_free(body);
            return;
        }

        dbf_skip(dbf, 1);
    }

    JsonValue *data = json_object();
    json_object_set(data, "found", json_bool(false));

    JsonValue *response = json_response_ok(data);
    http_response_json(resp, response);
    json_free(response);
    json_free(body);
}

void handle_query_count(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    (void)req;
    if (!check_database(resp, ctx)) return;
    DBF *dbf = cmd_get_current_dbf(ctx);

    /* Count non-deleted records */
    uint32_t total = dbf_reccount(dbf);
    uint32_t active = 0;

    dbf_go_top(dbf);
    while (!dbf_eof(dbf)) {
        if (!dbf_deleted(dbf)) active++;
        dbf_skip(dbf, 1);
    }

    JsonValue *data = json_object();
    json_object_set(data, "total", json_number((double)total));
    json_object_set(data, "active", json_number((double)active));
    json_object_set(data, "deleted", json_number((double)(total - active)));

    JsonValue *response = json_response_ok(data);
    http_response_json(resp, response);
    json_free(response);
}

void handle_query_seek(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    if (!check_database(resp, ctx)) return;
    DBF *dbf = cmd_get_current_dbf(ctx);

    if (ctx->current_order == 0 || ctx->index_count == 0) {
        http_response_error(resp, 400, "ERR_NO_INDEX", "No index in use");
        return;
    }

    JsonValue *body = get_json_body(req, resp);
    if (!body) return;

    const char *key = json_get_string(json_object_get(body, "key"));
    if (!key) {
        double n;
        if (json_get_number(json_object_get(body, "key"), &n)) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%g", n);
            key = buf;
        }
    }

    if (!key) {
        http_response_error(resp, 400, "ERR_MISSING_PARAM", "key is required");
        json_free(body);
        return;
    }

    XDX *xdx = ctx->indexes[ctx->current_order - 1];
    uint16_t key_len = xdx_key_length(xdx);
    uint8_t *key_buffer = xcalloc(1, key_len);
    memset(key_buffer, ' ', key_len);
    size_t len = strlen(key);
    if (len > key_len) len = key_len;
    memcpy(key_buffer, key, len);

    bool found = xdx_seek(xdx, key_buffer);
    uint32_t recno = xdx_recno(xdx);
    free(key_buffer);

    JsonValue *data = json_object();
    json_object_set(data, "found", json_bool(found));

    if (recno > 0) {
        dbf_goto(dbf, recno);
        json_object_set(data, "record", record_to_json(dbf));
    }

    JsonValue *response = json_response_ok(data);
    http_response_json(resp, response);
    json_free(response);
    json_free(body);
}

/*
 * Index endpoints
 */
void handle_index_create(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    http_response_error(resp, 501, "ERR_NOT_IMPLEMENTED",
                        "Index creation via API not yet implemented");
    (void)req;
    (void)ctx;
}

void handle_index_open(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    if (!check_database(resp, ctx)) return;

    JsonValue *body = get_json_body(req, resp);
    if (!body) return;

    const char *filename = json_get_string(json_object_get(body, "filename"));
    if (!filename) {
        http_response_error(resp, 400, "ERR_MISSING_PARAM", "filename is required");
        json_free(body);
        return;
    }

    /* Build path */
    char path[MAX_PATH_LEN];
    if (filename[0] == '/') {
        strncpy(path, filename, MAX_PATH_LEN - 1);
    } else {
        snprintf(path, MAX_PATH_LEN, "%s/%s", ctx->current_path, filename);
    }
    if (!file_extension(path)) {
        strcat(path, ".xdx");
    }

    XDX *xdx = xdx_open(path);
    if (!xdx) {
        http_response_error(resp, 400, "ERR_OPEN_FAILED", error_string(g_last_error));
        json_free(body);
        return;
    }

    if (ctx->index_count < MAX_INDEXES) {
        ctx->indexes[ctx->index_count++] = xdx;
        ctx->current_order = ctx->index_count;
    } else {
        xdx_close(xdx);
        http_response_error(resp, 400, "ERR_TOO_MANY_INDEXES", "Maximum indexes open");
        json_free(body);
        return;
    }

    JsonValue *data = json_object();
    json_object_set(data, "filename", json_string(path));
    json_object_set(data, "key_expr", json_string(xdx_key_expr(xdx)));
    json_object_set(data, "order", json_number((double)ctx->current_order));

    JsonValue *response = json_response_ok(data);
    http_response_json(resp, response);
    json_free(response);
    json_free(body);
}

void handle_index_close(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    (void)req;

    for (int i = 0; i < ctx->index_count; i++) {
        if (ctx->indexes[i]) {
            xdx_close(ctx->indexes[i]);
            ctx->indexes[i] = NULL;
        }
    }
    ctx->index_count = 0;
    ctx->current_order = 0;

    JsonValue *response = json_response_ok(json_bool(true));
    http_response_json(resp, response);
    json_free(response);
}

void handle_index_reindex(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    http_response_error(resp, 501, "ERR_NOT_IMPLEMENTED",
                        "Reindex via API not yet implemented");
    (void)req;
    (void)ctx;
}

/*
 * Execute endpoints
 */

/* Output buffer for capturing command output */
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} OutputBuffer;

/* Output capture function (must be static, not nested) */
static void capture_output(void *ctx_ptr, const char *fmt, ...) {
    OutputBuffer *buf = (OutputBuffer *)ctx_ptr;
    va_list args;
    va_start(args, fmt);

    char temp[4096];
    int len = vsnprintf(temp, sizeof(temp), fmt, args);
    va_end(args);

    if (len > 0) {
        if (buf->len + (size_t)len >= buf->cap) {
            buf->cap = buf->cap * 2 + (size_t)len;
            buf->data = realloc(buf->data, buf->cap);
        }
        memcpy(buf->data + buf->len, temp, (size_t)len + 1);
        buf->len += (size_t)len;
    }
}

void handle_execute(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    JsonValue *body = get_json_body(req, resp);
    if (!body) return;

    const char *command = json_get_string(json_object_get(body, "command"));
    if (!command) {
        http_response_error(resp, 400, "ERR_MISSING_PARAM", "command is required");
        json_free(body);
        return;
    }

    /* Capture output */
    OutputBuffer output = {NULL, 0, 0};
    output.data = malloc(4096);
    output.cap = 4096;
    output.data[0] = '\0';

    /* Set output function */
    OutputFunc old_func = ctx->output_func;
    void *old_ctx = ctx->output_ctx;
    cmd_set_output(ctx, capture_output, &output);

    /* Parse and execute */
    Parser parser;
    parser_init(&parser, command);

    ASTNode *node = parser_parse_command(&parser);

    if (node) {
        cmd_execute(node, ctx);
        ast_node_free(node);
    }

    /* Restore output function */
    cmd_set_output(ctx, old_func, old_ctx);

    JsonValue *data = json_object();
    json_object_set(data, "output", json_string(output.data));
    json_object_set(data, "success", json_bool(g_last_error == ERR_NONE));

    if (g_last_error != ERR_NONE) {
        json_object_set(data, "error_code", json_number((double)g_last_error));
        json_object_set(data, "error_message", json_string(error_string(g_last_error)));
    }

    free(output.data);

    JsonValue *response = json_response_ok(data);
    http_response_json(resp, response);
    json_free(response);
    json_free(body);
}

void handle_eval(HttpRequest *req, HttpResponse *resp, CommandContext *ctx) {
    JsonValue *body = get_json_body(req, resp);
    if (!body) return;

    const char *expr_str = json_get_string(json_object_get(body, "expression"));
    if (!expr_str) {
        http_response_error(resp, 400, "ERR_MISSING_PARAM", "expression is required");
        json_free(body);
        return;
    }

    /* Parse expression */
    Parser parser;
    parser_init(&parser, expr_str);

    ASTExpr *expr = parser_parse_expr(&parser);
    if (!expr) {
        http_response_error(resp, 400, "ERR_PARSE_FAILED", "Failed to parse expression");
        json_free(body);
        return;
    }

    /* Evaluate */
    Value result = expr_eval(expr, &ctx->eval_ctx);
    ast_expr_free(expr);

    JsonValue *data = json_object();

    switch (result.type) {
        case VAL_NUMBER:
            json_object_set(data, "type", json_string("number"));
            json_object_set(data, "value", json_number(result.data.number));
            break;
        case VAL_STRING:
            json_object_set(data, "type", json_string("string"));
            json_object_set(data, "value", json_string(result.data.string ? result.data.string : ""));
            break;
        case VAL_LOGICAL:
            json_object_set(data, "type", json_string("logical"));
            json_object_set(data, "value", json_bool(result.data.logical));
            break;
        case VAL_DATE:
            json_object_set(data, "type", json_string("date"));
            json_object_set(data, "value", json_string(result.data.date));
            break;
        default:
            json_object_set(data, "type", json_string("nil"));
            json_object_set(data, "value", json_null());
            break;
    }

    value_free(&result);

    JsonValue *response = json_response_ok(data);
    http_response_json(resp, response);
    json_free(response);
    json_free(body);
}

/*
 * Route registration
 */
void handlers_register(ServerConfig *cfg) {
    /* Database */
    server_add_route(cfg, HTTP_POST, "/api/v1/database/open", handle_database_open);
    server_add_route(cfg, HTTP_POST, "/api/v1/database/close", handle_database_close);
    server_add_route(cfg, HTTP_GET, "/api/v1/database/info", handle_database_info);
    server_add_route(cfg, HTTP_POST, "/api/v1/database/create", handle_database_create);

    /* Navigation */
    server_add_route(cfg, HTTP_POST, "/api/v1/navigate/goto", handle_navigate_goto);
    server_add_route(cfg, HTTP_POST, "/api/v1/navigate/skip", handle_navigate_skip);
    server_add_route(cfg, HTTP_POST, "/api/v1/navigate/top", handle_navigate_top);
    server_add_route(cfg, HTTP_POST, "/api/v1/navigate/bottom", handle_navigate_bottom);
    server_add_route(cfg, HTTP_GET, "/api/v1/navigate/position", handle_navigate_position);

    /* Records */
    server_add_route(cfg, HTTP_GET, "/api/v1/records", handle_records_list);
    server_add_route(cfg, HTTP_GET, "/api/v1/records/:recno", handle_records_get);
    server_add_route(cfg, HTTP_POST, "/api/v1/records", handle_records_append);
    server_add_route(cfg, HTTP_PUT, "/api/v1/records/:recno", handle_records_update);
    server_add_route(cfg, HTTP_DELETE, "/api/v1/records/:recno", handle_records_delete);
    server_add_route(cfg, HTTP_POST, "/api/v1/records/:recno/recall", handle_records_recall);

    /* Query */
    server_add_route(cfg, HTTP_POST, "/api/v1/query/locate", handle_query_locate);
    server_add_route(cfg, HTTP_GET, "/api/v1/query/count", handle_query_count);
    server_add_route(cfg, HTTP_POST, "/api/v1/query/seek", handle_query_seek);

    /* Index */
    server_add_route(cfg, HTTP_POST, "/api/v1/index/create", handle_index_create);
    server_add_route(cfg, HTTP_POST, "/api/v1/index/open", handle_index_open);
    server_add_route(cfg, HTTP_POST, "/api/v1/index/close", handle_index_close);
    server_add_route(cfg, HTTP_POST, "/api/v1/index/reindex", handle_index_reindex);

    /* Execute */
    server_add_route(cfg, HTTP_POST, "/api/v1/execute", handle_execute);
    server_add_route(cfg, HTTP_POST, "/api/v1/eval", handle_eval);
}
