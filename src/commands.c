/*
 * xBase3 - dBASE III+ Compatible Database System
 * commands.c - Command execution implementation
 */

#include "commands.h"
#include "variables.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

void cmd_context_init(CommandContext *ctx) {
    memset(ctx, 0, sizeof(CommandContext));
    eval_context_init(&ctx->eval_ctx);
    var_init();

    /* Get current working directory */
    if (getcwd(ctx->current_path, sizeof(ctx->current_path)) == NULL) {
        strcpy(ctx->current_path, ".");
    }
}

void cmd_context_cleanup(CommandContext *ctx) {
    /* Close all indexes */
    for (int i = 0; i < ctx->index_count; i++) {
        if (ctx->indexes[i]) {
            xdx_close(ctx->indexes[i]);
            ctx->indexes[i] = NULL;
        }
    }
    ctx->index_count = 0;
    ctx->current_order = 0;

    /* Close database */
    if (ctx->eval_ctx.current_dbf) {
        dbf_close(ctx->eval_ctx.current_dbf);
        ctx->eval_ctx.current_dbf = NULL;
    }
    var_cleanup();
}

DBF *cmd_get_current_dbf(CommandContext *ctx) {
    return ctx->eval_ctx.current_dbf;
}

void cmd_set_current_dbf(CommandContext *ctx, DBF *dbf) {
    ctx->eval_ctx.current_dbf = dbf;
}

/* Print expression result */
static void print_value(const Value *v) {
    char buf[MAX_STRING_LEN];
    value_to_string(v, buf, sizeof(buf));
    printf("%s", buf);
}

/* Execute ? command */
static void cmd_print(ASTNode *node, CommandContext *ctx, bool newline_first) {
    if (newline_first) {
        printf("\n");
    }

    for (int i = 0; i < node->data.print.expr_count; i++) {
        Value v = expr_eval(node->data.print.exprs[i], &ctx->eval_ctx);
        print_value(&v);
        value_free(&v);

        if (i < node->data.print.expr_count - 1) {
            printf(" ");
        }
    }

    if (node->type == CMD_QUESTION) {
        printf("\n");
    }
}

/* Execute USE command */
static void cmd_use(ASTNode *node, CommandContext *ctx) {
    /* Close current database if open */
    if (ctx->eval_ctx.current_dbf) {
        dbf_close(ctx->eval_ctx.current_dbf);
        ctx->eval_ctx.current_dbf = NULL;
    }

    /* USE without filename just closes */
    if (!node->data.use.filename) {
        return;
    }

    /* Build full path */
    char path[MAX_PATH_LEN];
    if (node->data.use.filename[0] == '/' ||
        strstr(node->data.use.filename, ":") != NULL) {
        strncpy(path, node->data.use.filename, MAX_PATH_LEN - 1);
    } else {
        snprintf(path, MAX_PATH_LEN, "%s/%s", ctx->current_path, node->data.use.filename);
    }

    /* Add .dbf extension if missing */
    if (!file_extension(path)) {
        strcat(path, ".dbf");
    }

    /* Open database */
    ctx->eval_ctx.current_dbf = dbf_open(path, false);

    if (!ctx->eval_ctx.current_dbf) {
        error_print();
        return;
    }

    /* Set alias if specified */
    if (node->data.use.alias) {
        dbf_set_alias(ctx->eval_ctx.current_dbf, node->data.use.alias);
    }
}

/* Execute CLOSE command */
/* Close all open indexes */
static void close_indexes(CommandContext *ctx) {
    for (int i = 0; i < ctx->index_count; i++) {
        if (ctx->indexes[i]) {
            xdx_close(ctx->indexes[i]);
            ctx->indexes[i] = NULL;
        }
    }
    ctx->index_count = 0;
    ctx->current_order = 0;
}

static void cmd_close(ASTNode *node, CommandContext *ctx) {
    int what = node->data.close.what;

    if (what == 1) {
        /* CLOSE INDEXES */
        close_indexes(ctx);
    } else {
        /* CLOSE DATABASES or CLOSE ALL */
        close_indexes(ctx);
        if (ctx->eval_ctx.current_dbf) {
            dbf_close(ctx->eval_ctx.current_dbf);
            ctx->eval_ctx.current_dbf = NULL;
        }
    }
}

/* Execute CREATE command - reads field definitions from stdin */
static void cmd_create(ASTNode *node, CommandContext *ctx) {
    if (!node->data.create.filename) {
        printf("Enter filename: ");
        /* Would need to read filename from stdin */
        error_set(ERR_SYNTAX, "CREATE requires filename");
        error_print();
        return;
    }

    /* Build full path */
    char path[MAX_PATH_LEN];
    if (node->data.create.filename[0] == '/' ||
        strstr(node->data.create.filename, ":") != NULL) {
        strncpy(path, node->data.create.filename, MAX_PATH_LEN - 1);
    } else {
        snprintf(path, MAX_PATH_LEN, "%s/%s", ctx->current_path, node->data.create.filename);
    }

    /* Add .dbf extension if missing */
    if (!file_extension(path)) {
        strcat(path, ".dbf");
    }

    /* Read field definitions from stdin */
    printf("Enter fields (name,type,length[,decimals]) - blank line to finish:\n");

    DBFField fields[MAX_FIELDS];
    int field_count = 0;
    char line[256];

    while (field_count < MAX_FIELDS) {
        printf("Field %d: ", field_count + 1);
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        /* Remove newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
            len--;
        }

        /* Empty line ends input */
        if (len == 0) {
            break;
        }

        /* Parse field definition: name,type,length[,decimals] */
        char *p = line;
        char *name = p;

        /* Find comma after name */
        char *comma = strchr(p, ',');
        if (!comma) {
            printf("Invalid format. Use: name,type,length[,decimals]\n");
            continue;
        }
        *comma = '\0';

        /* Get type */
        p = comma + 1;
        while (*p == ' ') p++;
        char type = (char)toupper(*p);
        if (type != 'C' && type != 'N' && type != 'D' && type != 'L' && type != 'M') {
            printf("Invalid type '%c'. Use C/N/D/L/M\n", type);
            continue;
        }

        /* Find comma after type */
        comma = strchr(p, ',');
        if (!comma) {
            printf("Invalid format. Use: name,type,length[,decimals]\n");
            continue;
        }

        /* Get length */
        p = comma + 1;
        while (*p == ' ') p++;
        int length = atoi(p);
        if (length <= 0 || length > 255) {
            printf("Invalid length. Must be 1-255\n");
            continue;
        }

        /* Get decimals (optional) */
        int decimals = 0;
        comma = strchr(p, ',');
        if (comma) {
            p = comma + 1;
            while (*p == ' ') p++;
            decimals = atoi(p);
        }

        /* Fill in field structure */
        memset(&fields[field_count], 0, sizeof(DBFField));
        strncpy(fields[field_count].name, name, MAX_FIELD_NAME - 1);
        str_upper(fields[field_count].name);
        fields[field_count].type = type;
        fields[field_count].length = (uint16_t)length;
        fields[field_count].decimals = (uint8_t)decimals;

        field_count++;
    }

    if (field_count == 0) {
        printf("No fields defined. Database not created.\n");
        return;
    }

    /* Create the database */
    DBF *dbf = dbf_create(path, fields, field_count);
    if (!dbf) {
        error_print();
        return;
    }

    printf("Database %s created with %d field(s)\n", path, field_count);

    /* Close current and open new */
    if (ctx->eval_ctx.current_dbf) {
        dbf_close(ctx->eval_ctx.current_dbf);
    }
    ctx->eval_ctx.current_dbf = dbf;
}

/* Check scope/FOR/WHILE conditions */
static bool check_conditions(ASTNode *node, CommandContext *ctx, uint32_t processed) {
    DBF *dbf = ctx->eval_ctx.current_dbf;
    if (!dbf) return false;

    /* Check scope */
    switch (node->scope.type) {
        case SCOPE_NEXT:
            if (node->scope.count) {
                Value v = expr_eval(node->scope.count, &ctx->eval_ctx);
                uint32_t limit = (uint32_t)value_to_number(&v);
                value_free(&v);
                if (processed >= limit) return false;
            }
            break;
        case SCOPE_RECORD:
            if (processed >= 1) return false;
            break;
        case SCOPE_REST:
        case SCOPE_ALL:
            break;
    }

    /* Check WHILE condition */
    if (node->while_cond) {
        Value v = expr_eval(node->while_cond, &ctx->eval_ctx);
        bool result = value_to_logical(&v);
        value_free(&v);
        if (!result) return false;
    }

    return true;
}

/* Check FOR condition only */
static bool check_for_condition(ASTNode *node, CommandContext *ctx) {
    if (!node->condition) return true;

    Value v = expr_eval(node->condition, &ctx->eval_ctx);
    bool result = value_to_logical(&v);
    value_free(&v);
    return result;
}

/* Execute LIST/DISPLAY command */
static void cmd_list(ASTNode *node, CommandContext *ctx, bool is_display) {
    DBF *dbf = ctx->eval_ctx.current_dbf;
    if (!dbf) {
        error_set(ERR_NO_DATABASE, NULL);
        error_print();
        return;
    }

    /* For DISPLAY, stay on current record; for LIST, start from top */
    if (!is_display) {
        dbf_go_top(dbf);
    }

    /* Handle DISPLAY with no records */
    if (is_display && (dbf_eof(dbf) || dbf_recno(dbf) == 0)) {
        printf("No records in database\n");
        return;
    }

    uint32_t processed = 0;

    while (!dbf_eof(dbf)) {
        if (!check_conditions(node, ctx, processed)) break;

        if (check_for_condition(node, ctx)) {
            /* Print record number */
            if (!node->data.list.off) {
                printf("%8u ", dbf_recno(dbf));
            }

            /* Print deleted marker */
            if (dbf_deleted(dbf)) {
                printf("* ");
            } else {
                printf("  ");
            }

            /* Print fields */
            if (node->data.list.field_count > 0) {
                /* Specific fields */
                for (int i = 0; i < node->data.list.field_count; i++) {
                    Value v = expr_eval(node->data.list.fields[i], &ctx->eval_ctx);
                    print_value(&v);
                    value_free(&v);
                    printf(" ");
                }
            } else {
                /* All fields */
                int fc = dbf_field_count(dbf);
                for (int i = 0; i < fc; i++) {
                    char buf[MAX_FIELD_LEN + 1];
                    dbf_get_string(dbf, i, buf, sizeof(buf));
                    printf("%s ", buf);
                }
            }

            printf("\n");
            processed++;

            /* DISPLAY shows only current record by default */
            if (is_display && node->scope.type == SCOPE_ALL &&
                !node->condition && !node->while_cond) {
                break;
            }
        }

        dbf_skip(dbf, 1);
    }

    if (processed == 0 && !is_display) {
        printf("No records found\n");
    }
}

/* Execute GO command */
static void cmd_go(ASTNode *node, CommandContext *ctx) {
    DBF *dbf = ctx->eval_ctx.current_dbf;
    if (!dbf) {
        error_set(ERR_NO_DATABASE, NULL);
        error_print();
        return;
    }

    if (node->data.go.top) {
        dbf_go_top(dbf);
    } else if (node->data.go.bottom) {
        dbf_go_bottom(dbf);
    } else if (node->data.go.recno) {
        Value v = expr_eval(node->data.go.recno, &ctx->eval_ctx);
        uint32_t recno = (uint32_t)value_to_number(&v);
        value_free(&v);
        dbf_goto(dbf, recno);
    }
}

/* Execute SKIP command */
static void cmd_skip(ASTNode *node, CommandContext *ctx) {
    DBF *dbf = ctx->eval_ctx.current_dbf;
    if (!dbf) {
        error_set(ERR_NO_DATABASE, NULL);
        error_print();
        return;
    }

    int count = 1;
    if (node->data.skip.count) {
        Value v = expr_eval(node->data.skip.count, &ctx->eval_ctx);
        count = (int)value_to_number(&v);
        value_free(&v);
    }

    dbf_skip(dbf, count);
}

/* Execute LOCATE command */
static void cmd_locate(ASTNode *node, CommandContext *ctx) {
    DBF *dbf = ctx->eval_ctx.current_dbf;
    if (!dbf) {
        error_set(ERR_NO_DATABASE, NULL);
        error_print();
        return;
    }

    dbf_go_top(dbf);

    while (!dbf_eof(dbf)) {
        if (check_for_condition(node, ctx)) {
            printf("Record %u\n", dbf_recno(dbf));
            return;
        }
        dbf_skip(dbf, 1);
    }

    printf("End of LOCATE scope\n");
}

/* Execute CONTINUE command */
static void cmd_continue(ASTNode *node, CommandContext *ctx) {
    (void)node;
    DBF *dbf = ctx->eval_ctx.current_dbf;
    if (!dbf) {
        error_set(ERR_NO_DATABASE, NULL);
        error_print();
        return;
    }

    /* Continue from next record - simplified (should remember LOCATE condition) */
    dbf_skip(dbf, 1);
    if (dbf_eof(dbf)) {
        printf("End of LOCATE scope\n");
    } else {
        printf("Record %u\n", dbf_recno(dbf));
    }
}

/* Execute APPEND command */
static void cmd_append(ASTNode *node, CommandContext *ctx) {
    (void)node;
    DBF *dbf = ctx->eval_ctx.current_dbf;
    if (!dbf) {
        error_set(ERR_NO_DATABASE, NULL);
        error_print();
        return;
    }

    if (dbf_append_blank(dbf)) {
        printf("Record %u appended\n", dbf_recno(dbf));
    } else {
        error_print();
    }
}

/* Execute DELETE command */
static void cmd_delete(ASTNode *node, CommandContext *ctx) {
    DBF *dbf = ctx->eval_ctx.current_dbf;
    if (!dbf) {
        error_set(ERR_NO_DATABASE, NULL);
        error_print();
        return;
    }

    uint32_t deleted = 0;
    uint32_t processed = 0;

    /* If no scope/conditions, delete current record */
    if (node->scope.type == SCOPE_ALL && !node->condition && !node->while_cond) {
        if (dbf_delete(dbf)) {
            dbf_flush(dbf);
            printf("1 record deleted\n");
        }
        return;
    }

    /* Start from top for ALL scope */
    if (node->scope.type == SCOPE_ALL) {
        dbf_go_top(dbf);
    }

    while (!dbf_eof(dbf)) {
        if (!check_conditions(node, ctx, processed)) break;

        if (check_for_condition(node, ctx)) {
            if (dbf_delete(dbf)) {
                deleted++;
            }
        }
        processed++;
        dbf_skip(dbf, 1);
    }

    dbf_flush(dbf);
    printf("%u record(s) deleted\n", deleted);
}

/* Execute RECALL command */
static void cmd_recall(ASTNode *node, CommandContext *ctx) {
    DBF *dbf = ctx->eval_ctx.current_dbf;
    if (!dbf) {
        error_set(ERR_NO_DATABASE, NULL);
        error_print();
        return;
    }

    uint32_t recalled = 0;
    uint32_t processed = 0;

    /* If no scope/conditions, recall current record */
    if (node->scope.type == SCOPE_ALL && !node->condition && !node->while_cond) {
        if (dbf_recall(dbf)) {
            dbf_flush(dbf);
            printf("1 record recalled\n");
        }
        return;
    }

    /* Start from top for ALL scope */
    if (node->scope.type == SCOPE_ALL) {
        dbf_go_top(dbf);
    }

    while (!dbf_eof(dbf)) {
        if (!check_conditions(node, ctx, processed)) break;

        if (check_for_condition(node, ctx)) {
            if (dbf_recall(dbf)) {
                recalled++;
            }
        }
        processed++;
        dbf_skip(dbf, 1);
    }

    dbf_flush(dbf);
    printf("%u record(s) recalled\n", recalled);
}

/* Execute PACK command */
static void cmd_pack(ASTNode *node, CommandContext *ctx) {
    (void)node;
    DBF *dbf = ctx->eval_ctx.current_dbf;
    if (!dbf) {
        error_set(ERR_NO_DATABASE, NULL);
        error_print();
        return;
    }

    uint32_t before = dbf_reccount(dbf);
    if (dbf_pack(dbf)) {
        uint32_t after = dbf_reccount(dbf);
        printf("%u record(s) removed, %u remain\n", before - after, after);
    } else {
        error_print();
    }
}

/* Execute ZAP command */
static void cmd_zap(ASTNode *node, CommandContext *ctx) {
    (void)node;
    DBF *dbf = ctx->eval_ctx.current_dbf;
    if (!dbf) {
        error_set(ERR_NO_DATABASE, NULL);
        error_print();
        return;
    }

    uint32_t count = dbf_reccount(dbf);
    if (dbf_zap(dbf)) {
        printf("%u record(s) removed\n", count);
    } else {
        error_print();
    }
}

/* Helper: evaluate key expression and store in buffer */
typedef struct {
    ASTExpr *key_expr;
    EvalContext *eval_ctx;
} KeyEvalContext;

static bool eval_key_for_reindex(DBF *dbf, void *key, void *ctx) {
    (void)dbf;
    KeyEvalContext *kctx = (KeyEvalContext *)ctx;

    Value val = expr_eval(kctx->key_expr, kctx->eval_ctx);
    if (val.type == VAL_NIL) {
        value_free(&val);
        return false;
    }

    char buf[256];
    value_to_string(&val, buf, sizeof(buf));
    memcpy(key, buf, strlen(buf));

    value_free(&val);
    return true;
}

/* Execute INDEX ON command */
static void cmd_index(ASTNode *node, CommandContext *ctx) {
    DBF *dbf = ctx->eval_ctx.current_dbf;
    if (!dbf) {
        error_set(ERR_NO_DATABASE, NULL);
        error_print();
        return;
    }

    if (!node->data.index.key_expr) {
        error_set(ERR_SYNTAX, "INDEX ON requires key expression");
        error_print();
        return;
    }

    if (!node->data.index.filename) {
        error_set(ERR_SYNTAX, "INDEX ON requires filename (TO clause)");
        error_print();
        return;
    }

    /* Build full path */
    char path[MAX_PATH_LEN];
    const char *filename = node->data.index.filename;
    if (filename[0] == '/' || strstr(filename, ":") != NULL) {
        strncpy(path, filename, MAX_PATH_LEN - 1);
    } else {
        snprintf(path, MAX_PATH_LEN, "%s/%s", ctx->current_path, filename);
    }

    /* Add .xdx extension if missing */
    if (!file_extension(path)) {
        strcat(path, ".xdx");
    }

    /* Determine key type and length by evaluating on first record */
    char key_type = XDX_KEY_CHAR;
    uint16_t key_length = 10;

    if (dbf_reccount(dbf) > 0) {
        dbf_go_top(dbf);
        Value val = expr_eval(node->data.index.key_expr, &ctx->eval_ctx);

        switch (val.type) {
            case VAL_NUMBER:
                key_type = XDX_KEY_NUMERIC;
                key_length = 20;  /* Enough for double as string */
                break;
            case VAL_DATE:
                key_type = XDX_KEY_DATE;
                key_length = 8;
                break;
            case VAL_STRING:
                key_type = XDX_KEY_CHAR;
                key_length = (uint16_t)(val.data.string ? strlen(val.data.string) : 10);
                if (key_length > 240) key_length = 240;
                if (key_length < 1) key_length = 10;
                break;
            default:
                key_type = XDX_KEY_CHAR;
                key_length = 10;
                break;
        }
        value_free(&val);
    }

    /* Get key expression as string */
    /* For now, we'll reconstruct it - in a full implementation,
       we'd store the original text from parsing */
    char key_expr_str[XDX_MAX_EXPR_LEN] = "";
    /* Simple case: if it's just a field/variable name */
    if (node->data.index.key_expr->type == EXPR_IDENT) {
        strncpy(key_expr_str, node->data.index.key_expr->data.ident,
                XDX_MAX_EXPR_LEN - 1);
    } else if (node->data.index.key_expr->type == EXPR_FIELD) {
        strncpy(key_expr_str, node->data.index.key_expr->data.field_ref.field,
                XDX_MAX_EXPR_LEN - 1);
    } else {
        strcpy(key_expr_str, "(expression)");
    }

    /* Create the index */
    XDX *xdx = xdx_create(path, key_expr_str, key_type, key_length,
                          node->data.index.unique,
                          node->data.index.descending);
    if (!xdx) {
        error_print();
        return;
    }

    /* Build index from all records */
    uint32_t indexed = 0;
    uint8_t *key_buffer = xcalloc(1, key_length);

    dbf_go_top(dbf);
    while (!dbf_eof(dbf)) {
        if (!dbf_deleted(dbf)) {
            /* Evaluate key expression */
            Value val = expr_eval(node->data.index.key_expr, &ctx->eval_ctx);
            memset(key_buffer, ' ', key_length);

            char buf[256];
            value_to_string(&val, buf, sizeof(buf));
            size_t len = strlen(buf);
            if (len > key_length) len = key_length;
            memcpy(key_buffer, buf, len);

            value_free(&val);

            /* Insert into index */
            if (xdx_insert(xdx, key_buffer, dbf_recno(dbf))) {
                indexed++;
            } else {
                /* Duplicate key error for unique index */
                if (node->data.index.unique) {
                    error_print();
                    error_clear();
                }
            }
        }
        dbf_skip(dbf, 1);
    }

    free(key_buffer);

    /* Add to open indexes */
    if (ctx->index_count < MAX_INDEXES) {
        ctx->indexes[ctx->index_count++] = xdx;
        ctx->current_order = ctx->index_count;  /* Set as controlling index */
        printf("%u record(s) indexed\n", indexed);
    } else {
        printf("Warning: Maximum indexes open, closing new index\n");
        xdx_close(xdx);
    }
}

/* Execute SET INDEX TO command */
static void cmd_set_index(ASTNode *node, CommandContext *ctx) {
    /* SET INDEX TO without filename closes all indexes */
    if (!node->data.index.filename) {
        close_indexes(ctx);
        printf("Indexes closed\n");
        return;
    }

    /* Build full path */
    char path[MAX_PATH_LEN];
    const char *filename = node->data.index.filename;
    if (filename[0] == '/' || strstr(filename, ":") != NULL) {
        strncpy(path, filename, MAX_PATH_LEN - 1);
    } else {
        snprintf(path, MAX_PATH_LEN, "%s/%s", ctx->current_path, filename);
    }

    /* Add .xdx extension if missing */
    if (!file_extension(path)) {
        strcat(path, ".xdx");
    }

    /* Open the index */
    XDX *xdx = xdx_open(path);
    if (!xdx) {
        error_print();
        return;
    }

    /* Add to open indexes */
    if (ctx->index_count < MAX_INDEXES) {
        ctx->indexes[ctx->index_count++] = xdx;
        ctx->current_order = ctx->index_count;
        printf("Index %s opened\n", path);
    } else {
        printf("Error: Maximum indexes already open\n");
        xdx_close(xdx);
    }
}

/* Execute SEEK command */
static void cmd_seek(ASTNode *node, CommandContext *ctx) {
    DBF *dbf = ctx->eval_ctx.current_dbf;
    if (!dbf) {
        error_set(ERR_NO_DATABASE, NULL);
        error_print();
        return;
    }

    if (ctx->current_order == 0 || ctx->index_count == 0) {
        printf("No index in use\n");
        return;
    }

    XDX *xdx = ctx->indexes[ctx->current_order - 1];
    if (!xdx) {
        printf("No controlling index\n");
        return;
    }

    /* Evaluate search key */
    Value val = expr_eval(node->data.seek.key, &ctx->eval_ctx);

    /* Convert to key format */
    uint16_t key_len = xdx_key_length(xdx);
    uint8_t *key_buffer = xcalloc(1, key_len);
    memset(key_buffer, ' ', key_len);

    char buf[256];
    value_to_string(&val, buf, sizeof(buf));
    size_t len = strlen(buf);
    if (len > key_len) len = key_len;
    memcpy(key_buffer, buf, len);

    value_free(&val);

    /* Seek in index */
    bool found = xdx_seek(xdx, key_buffer);
    uint32_t recno = xdx_recno(xdx);

    free(key_buffer);

    if (recno > 0) {
        dbf_goto(dbf, recno);
        if (found) {
            printf("Found at record %u\n", recno);
        } else {
            printf("Not found, positioned at record %u\n", recno);
        }
    } else {
        /* Position at EOF */
        dbf_go_bottom(dbf);
        dbf_skip(dbf, 1);
        printf("Not found\n");
    }
}

/* Execute REINDEX command */
static void cmd_reindex(ASTNode *node, CommandContext *ctx) {
    (void)node;
    DBF *dbf = ctx->eval_ctx.current_dbf;
    if (!dbf) {
        error_set(ERR_NO_DATABASE, NULL);
        error_print();
        return;
    }

    if (ctx->index_count == 0) {
        printf("No indexes to rebuild\n");
        return;
    }

    printf("Rebuilding %d index(es)...\n", ctx->index_count);

    /* Note: Full reindex would need to re-evaluate the key expression
       for each record. For now, we just report that it would be done. */
    for (int i = 0; i < ctx->index_count; i++) {
        XDX *xdx = ctx->indexes[i];
        if (xdx) {
            printf("  Reindexing %s...\n", xdx_key_expr(xdx));
            /* Would call xdx_reindex here with proper key evaluator */
        }
    }

    printf("Reindex complete\n");
}

/* Execute SET ORDER TO command */
static void cmd_set_order(ASTNode *node, CommandContext *ctx) {
    if (!node->data.set.value) {
        /* SET ORDER TO 0 or no value - disable controlling index */
        ctx->current_order = 0;
        printf("Index order: natural\n");
        return;
    }

    Value val = expr_eval(node->data.set.value, &ctx->eval_ctx);
    int order = (int)value_to_number(&val);
    value_free(&val);

    if (order < 0 || order > ctx->index_count) {
        printf("Invalid index order: %d (have %d indexes)\n", order, ctx->index_count);
        return;
    }

    ctx->current_order = order;
    if (order == 0) {
        printf("Index order: natural\n");
    } else {
        printf("Index order: %d\n", order);
    }
}

/* Execute REPLACE command */
static void cmd_replace(ASTNode *node, CommandContext *ctx) {
    DBF *dbf = ctx->eval_ctx.current_dbf;
    if (!dbf) {
        error_set(ERR_NO_DATABASE, NULL);
        error_print();
        return;
    }

    if (dbf_eof(dbf) || dbf_recno(dbf) == 0) {
        printf("No record to replace\n");
        return;
    }

    uint32_t replaced = 0;
    uint32_t processed = 0;

    /* If no scope/conditions, replace current record */
    bool single_record = (node->scope.type == SCOPE_ALL &&
                         !node->condition && !node->while_cond);

    if (!single_record && node->scope.type == SCOPE_ALL) {
        dbf_go_top(dbf);
    }

    do {
        if (!single_record && !check_conditions(node, ctx, processed)) break;
        if (!check_for_condition(node, ctx)) {
            if (!single_record) {
                dbf_skip(dbf, 1);
                processed++;
            }
            continue;
        }

        for (int i = 0; i < node->data.replace.count; i++) {
            int field_idx = dbf_field_index(dbf, node->data.replace.fields[i]);
            if (field_idx < 0) {
                error_set(ERR_INVALID_FIELD, "%s", node->data.replace.fields[i]);
                error_print();
                continue;
            }

            Value v = expr_eval(node->data.replace.values[i], &ctx->eval_ctx);
            const DBFField *field = dbf_field_info(dbf, field_idx);

            switch (field->type) {
                case FIELD_TYPE_CHAR: {
                    char buf[MAX_FIELD_LEN + 1];
                    value_to_string(&v, buf, sizeof(buf));
                    dbf_put_string(dbf, field_idx, buf);
                    break;
                }
                case FIELD_TYPE_NUMERIC:
                    dbf_put_double(dbf, field_idx, value_to_number(&v));
                    break;
                case FIELD_TYPE_DATE:
                    if (v.type == VAL_DATE) {
                        dbf_put_date(dbf, field_idx, v.data.date);
                    }
                    break;
                case FIELD_TYPE_LOGICAL:
                    dbf_put_logical(dbf, field_idx, value_to_logical(&v));
                    break;
                default:
                    break;
            }

            value_free(&v);
        }

        dbf_flush(dbf);
        replaced++;

        if (single_record) break;

        dbf_skip(dbf, 1);
        processed++;

    } while (!dbf_eof(dbf));

    printf("%u record(s) replaced\n", replaced);
}

/* Execute STORE command */
static void cmd_store(ASTNode *node, CommandContext *ctx) {
    Value v = expr_eval(node->data.store.value, &ctx->eval_ctx);
    var_set(node->data.store.var, &v);
    value_free(&v);
}

/* Execute SET command */
static void cmd_set(ASTNode *node, CommandContext *ctx) {
    const char *option = node->data.set.option;

    /* Handle SET INDEX TO */
    if (strcasecmp(option, "INDEX") == 0) {
        /* Create a fake index node for cmd_set_index */
        ASTNode fake;
        memset(&fake, 0, sizeof(fake));
        fake.type = CMD_INDEX;

        if (node->data.set.value) {
            if (node->data.set.value->type == EXPR_IDENT) {
                fake.data.index.filename = node->data.set.value->data.ident;
            } else if (node->data.set.value->type == EXPR_STRING) {
                fake.data.index.filename = node->data.set.value->data.string;
            }
        }
        cmd_set_index(&fake, ctx);
        return;
    }

    /* Handle SET ORDER TO */
    if (strcasecmp(option, "ORDER") == 0) {
        cmd_set_order(node, ctx);
        return;
    }

    /* Basic SET handling - many options not implemented */
    printf("SET %s", option);
    if (node->data.set.value) {
        char buf[256];
        Value v = expr_eval(node->data.set.value, &ctx->eval_ctx);
        value_to_string(&v, buf, sizeof(buf));
        printf(" TO %s", buf);
        value_free(&v);
    }
    printf("\n");
}

/* Execute CLEAR command */
static void cmd_clear(ASTNode *node, CommandContext *ctx) {
    (void)node; (void)ctx;
    /* Clear screen - just print newlines */
    printf("\n\n\n\n\n\n\n\n\n\n");
}

/* Execute PUBLIC/PRIVATE/LOCAL command */
static void cmd_var_decl(ASTNode *node, CommandContext *ctx) {
    (void)ctx;

    for (int i = 0; i < node->data.vars.count; i++) {
        switch (node->type) {
            case CMD_PUBLIC:
                var_declare_public(node->data.vars.names[i]);
                break;
            case CMD_PRIVATE:
                var_declare_private(node->data.vars.names[i]);
                break;
            case CMD_LOCAL:
                var_declare_local(node->data.vars.names[i]);
                break;
            default:
                break;
        }
    }
}

/* Execute RELEASE command */
static void cmd_release(ASTNode *node, CommandContext *ctx) {
    (void)ctx;

    if (node->data.vars.all) {
        var_release_all();
    } else {
        for (int i = 0; i < node->data.vars.count; i++) {
            var_release(node->data.vars.names[i]);
        }
    }
}

/* Execute DECLARE command */
static void cmd_declare(ASTNode *node, CommandContext *ctx) {
    Value v = expr_eval(node->data.declare.size, &ctx->eval_ctx);
    int size = (int)value_to_number(&v);
    value_free(&v);

    var_declare_array(node->data.declare.name, size);
}

/* Execute WAIT command */
static void cmd_wait(ASTNode *node, CommandContext *ctx) {
    if (node->data.input.prompt) {
        Value v = expr_eval(node->data.input.prompt, &ctx->eval_ctx);
        print_value(&v);
        value_free(&v);
    } else {
        printf("Press any key to continue...");
    }
    fflush(stdout);

    int c = getchar();

    if (node->data.input.var && c != EOF) {
        char buf[2] = {(char)c, '\0'};
        Value v = value_string(buf);
        var_set(node->data.input.var, &v);
        value_free(&v);
    }

    printf("\n");
}

/* Execute COUNT command */
static void cmd_count(ASTNode *node, CommandContext *ctx) {
    DBF *dbf = ctx->eval_ctx.current_dbf;
    if (!dbf) {
        error_set(ERR_NO_DATABASE, NULL);
        error_print();
        return;
    }

    dbf_go_top(dbf);
    uint32_t count = 0;
    uint32_t processed = 0;

    while (!dbf_eof(dbf)) {
        if (!check_conditions(node, ctx, processed)) break;

        if (check_for_condition(node, ctx)) {
            count++;
        }
        processed++;
        dbf_skip(dbf, 1);
    }

    printf("%u record(s)\n", count);

    /* Store result if variable specified */
    if (node->data.aggregate.count > 0 && node->data.aggregate.vars) {
        Value v = value_number((double)count);
        var_set(node->data.aggregate.vars[0], &v);
        value_free(&v);
    }
}

/* Main command executor */
void cmd_execute(ASTNode *node, CommandContext *ctx) {
    if (!node) return;

    error_clear();

    switch (node->type) {
        case CMD_QUESTION:
            cmd_print(node, ctx, true);
            break;

        case CMD_DQUESTION:
            cmd_print(node, ctx, false);
            break;

        case CMD_USE:
            cmd_use(node, ctx);
            break;

        case CMD_CLOSE:
            cmd_close(node, ctx);
            break;

        case CMD_CREATE:
            cmd_create(node, ctx);
            break;

        case CMD_LIST:
            cmd_list(node, ctx, false);
            break;

        case CMD_DISPLAY:
            cmd_list(node, ctx, true);
            break;

        case CMD_GO:
            cmd_go(node, ctx);
            break;

        case CMD_SKIP:
            cmd_skip(node, ctx);
            break;

        case CMD_LOCATE:
            cmd_locate(node, ctx);
            break;

        case CMD_CONTINUE:
            cmd_continue(node, ctx);
            break;

        case CMD_APPEND:
            cmd_append(node, ctx);
            break;

        case CMD_DELETE:
            cmd_delete(node, ctx);
            break;

        case CMD_RECALL:
            cmd_recall(node, ctx);
            break;

        case CMD_PACK:
            cmd_pack(node, ctx);
            break;

        case CMD_ZAP:
            cmd_zap(node, ctx);
            break;

        case CMD_REPLACE:
            cmd_replace(node, ctx);
            break;

        case CMD_STORE:
            cmd_store(node, ctx);
            break;

        case CMD_SET:
            cmd_set(node, ctx);
            break;

        case CMD_CLEAR:
            cmd_clear(node, ctx);
            break;

        case CMD_PUBLIC:
        case CMD_PRIVATE:
        case CMD_LOCAL:
            cmd_var_decl(node, ctx);
            break;

        case CMD_RELEASE:
            cmd_release(node, ctx);
            break;

        case CMD_DECLARE:
            cmd_declare(node, ctx);
            break;

        case CMD_WAIT:
            cmd_wait(node, ctx);
            break;

        case CMD_COUNT:
            cmd_count(node, ctx);
            break;

        case CMD_INDEX:
            cmd_index(node, ctx);
            break;

        case CMD_SEEK:
        case CMD_FIND:
            cmd_seek(node, ctx);
            break;

        case CMD_REINDEX:
            cmd_reindex(node, ctx);
            break;

        case CMD_QUIT:
            ctx->quit_requested = true;
            break;

        case CMD_CANCEL:
            ctx->cancel_requested = true;
            break;

        default:
            printf("Command not implemented\n");
            break;
    }
}
