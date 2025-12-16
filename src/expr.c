/*
 * xBase3 - dBASE III+ Compatible Database System
 * expr.c - Expression evaluator implementation
 */

#include "expr.h"
#include "functions.h"
#include "variables.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

/* Create values */
Value value_nil(void) {
    Value v = {0};
    v.type = VAL_NIL;
    return v;
}

Value value_number(double n) {
    Value v = {0};
    v.type = VAL_NUMBER;
    v.data.number = n;
    return v;
}

Value value_string(const char *s) {
    Value v = {0};
    v.type = VAL_STRING;
    v.data.string = xstrdup(s ? s : "");
    return v;
}

Value value_date(const char *d) {
    Value v = {0};
    v.type = VAL_DATE;
    if (d && strlen(d) == 8) {
        memcpy(v.data.date, d, 8);
        v.data.date[8] = '\0';
    } else {
        memset(v.data.date, ' ', 8);
        v.data.date[8] = '\0';
    }
    return v;
}

Value value_logical(bool b) {
    Value v = {0};
    v.type = VAL_LOGICAL;
    v.data.logical = b;
    return v;
}

void value_free(Value *v) {
    if (!v) return;

    switch (v->type) {
        case VAL_STRING:
            xfree(v->data.string);
            v->data.string = NULL;
            break;
        case VAL_ARRAY:
            for (int i = 0; i < v->data.array.count; i++) {
                value_free(&v->data.array.elements[i]);
            }
            xfree(v->data.array.elements);
            break;
        default:
            break;
    }
    v->type = VAL_NIL;
}

Value value_copy(const Value *v) {
    if (!v) return value_nil();

    switch (v->type) {
        case VAL_NUMBER:
            return value_number(v->data.number);
        case VAL_STRING:
            return value_string(v->data.string);
        case VAL_DATE:
            return value_date(v->data.date);
        case VAL_LOGICAL:
            return value_logical(v->data.logical);
        case VAL_ARRAY: {
            Value copy = {0};
            copy.type = VAL_ARRAY;
            copy.data.array.count = v->data.array.count;
            copy.data.array.elements = xcalloc((size_t)v->data.array.count, sizeof(Value));
            for (int i = 0; i < v->data.array.count; i++) {
                copy.data.array.elements[i] = value_copy(&v->data.array.elements[i]);
            }
            return copy;
        }
        default:
            return value_nil();
    }
}

void value_to_string(const Value *v, char *buf, size_t bufsize) {
    if (!v || !buf || bufsize == 0) return;

    switch (v->type) {
        case VAL_NIL:
            snprintf(buf, bufsize, "NIL");
            break;
        case VAL_NUMBER:
            snprintf(buf, bufsize, "%g", v->data.number);
            break;
        case VAL_STRING:
            snprintf(buf, bufsize, "%s", v->data.string ? v->data.string : "");
            break;
        case VAL_DATE:
            snprintf(buf, bufsize, "%s", v->data.date);
            break;
        case VAL_LOGICAL:
            snprintf(buf, bufsize, "%s", v->data.logical ? ".T." : ".F.");
            break;
        case VAL_ARRAY:
            snprintf(buf, bufsize, "ARRAY[%d]", v->data.array.count);
            break;
    }
}

double value_to_number(const Value *v) {
    if (!v) return 0.0;

    switch (v->type) {
        case VAL_NUMBER:
            return v->data.number;
        case VAL_STRING:
            if (v->data.string) {
                double result;
                if (str_to_num(v->data.string, &result)) {
                    return result;
                }
            }
            return 0.0;
        case VAL_LOGICAL:
            return v->data.logical ? 1.0 : 0.0;
        case VAL_DATE:
            return (double)date_to_julian(v->data.date);
        default:
            return 0.0;
    }
}

bool value_to_logical(const Value *v) {
    if (!v) return false;

    switch (v->type) {
        case VAL_LOGICAL:
            return v->data.logical;
        case VAL_NUMBER:
            return v->data.number != 0.0;
        case VAL_STRING:
            return v->data.string && v->data.string[0] != '\0';
        case VAL_DATE:
            return v->data.date[0] != ' ';
        default:
            return false;
    }
}

bool value_is_truthy(const Value *v) {
    return value_to_logical(v);
}

void eval_context_init(EvalContext *ctx) {
    memset(ctx, 0, sizeof(EvalContext));
}

/* Evaluate binary operations */
static Value eval_binary(TokenType op, Value *left, Value *right) {
    /* String concatenation */
    if (op == TOK_PLUS && left->type == VAL_STRING && right->type == VAL_STRING) {
        size_t len1 = strlen(left->data.string);
        size_t len2 = strlen(right->data.string);
        char *result = xmalloc(len1 + len2 + 1);
        strcpy(result, left->data.string);
        strcat(result, right->data.string);
        Value v = value_string(result);
        xfree(result);
        return v;
    }

    /* String minus (trim right and concatenate) */
    if (op == TOK_MINUS && left->type == VAL_STRING && right->type == VAL_STRING) {
        char *trimmed = xstrdup(left->data.string);
        str_trim_right(trimmed);
        size_t len1 = strlen(trimmed);
        size_t len2 = strlen(right->data.string);
        char *result = xmalloc(len1 + len2 + 1);
        strcpy(result, trimmed);
        strcat(result, right->data.string);
        xfree(trimmed);
        Value v = value_string(result);
        xfree(result);
        return v;
    }

    /* String comparison */
    if (left->type == VAL_STRING && right->type == VAL_STRING) {
        int cmp = strcmp(left->data.string, right->data.string);
        switch (op) {
            case TOK_EQ: return value_logical(cmp == 0);
            case TOK_NE: return value_logical(cmp != 0);
            case TOK_LT: return value_logical(cmp < 0);
            case TOK_LE: return value_logical(cmp <= 0);
            case TOK_GT: return value_logical(cmp > 0);
            case TOK_GE: return value_logical(cmp >= 0);
            case TOK_DOLLAR: {
                /* $ operator - substring containment */
                bool contains = strstr(right->data.string, left->data.string) != NULL;
                return value_logical(contains);
            }
            default:
                break;
        }
    }

    /* Date arithmetic */
    if (left->type == VAL_DATE && right->type == VAL_NUMBER) {
        long julian = date_to_julian(left->data.date);
        if (op == TOK_PLUS) {
            julian += (long)right->data.number;
        } else if (op == TOK_MINUS) {
            julian -= (long)right->data.number;
        }
        char date[9];
        date_from_julian(date, julian);
        return value_date(date);
    }

    /* Date subtraction (returns number of days) */
    if (left->type == VAL_DATE && right->type == VAL_DATE && op == TOK_MINUS) {
        long j1 = date_to_julian(left->data.date);
        long j2 = date_to_julian(right->data.date);
        return value_number((double)(j1 - j2));
    }

    /* Date comparison */
    if (left->type == VAL_DATE && right->type == VAL_DATE) {
        long j1 = date_to_julian(left->data.date);
        long j2 = date_to_julian(right->data.date);
        switch (op) {
            case TOK_EQ: return value_logical(j1 == j2);
            case TOK_NE: return value_logical(j1 != j2);
            case TOK_LT: return value_logical(j1 < j2);
            case TOK_LE: return value_logical(j1 <= j2);
            case TOK_GT: return value_logical(j1 > j2);
            case TOK_GE: return value_logical(j1 >= j2);
            default: break;
        }
    }

    /* Logical operations */
    if (op == TOK_AND) {
        return value_logical(value_to_logical(left) && value_to_logical(right));
    }
    if (op == TOK_OR) {
        return value_logical(value_to_logical(left) || value_to_logical(right));
    }

    /* Numeric operations */
    double l = value_to_number(left);
    double r = value_to_number(right);

    switch (op) {
        case TOK_PLUS:
            return value_number(l + r);
        case TOK_MINUS:
            return value_number(l - r);
        case TOK_STAR:
            return value_number(l * r);
        case TOK_SLASH:
            if (r == 0.0) {
                error_set(ERR_DIVISION_BY_ZERO, NULL);
                return value_number(0.0);
            }
            return value_number(l / r);
        case TOK_PERCENT:
            if (r == 0.0) {
                error_set(ERR_DIVISION_BY_ZERO, NULL);
                return value_number(0.0);
            }
            return value_number(fmod(l, r));
        case TOK_CARET:
            return value_number(pow(l, r));
        case TOK_EQ:
            return value_logical(l == r);
        case TOK_NE:
            return value_logical(l != r);
        case TOK_LT:
            return value_logical(l < r);
        case TOK_LE:
            return value_logical(l <= r);
        case TOK_GT:
            return value_logical(l > r);
        case TOK_GE:
            return value_logical(l >= r);
        default:
            return value_nil();
    }
}

/* Evaluate unary operations */
static Value eval_unary(TokenType op, Value *operand) {
    switch (op) {
        case TOK_MINUS:
            return value_number(-value_to_number(operand));
        case TOK_PLUS:
            return value_number(value_to_number(operand));
        case TOK_NOT:
            return value_logical(!value_to_logical(operand));
        default:
            return value_nil();
    }
}

/* Main expression evaluator */
Value expr_eval(ASTExpr *expr, EvalContext *ctx) {
    if (!expr) return value_nil();

    switch (expr->type) {
        case EXPR_NUMBER:
            return value_number(expr->data.number);

        case EXPR_STRING:
            return value_string(expr->data.string);

        case EXPR_DATE: {
            /* Parse date literal like "12/31/2024" or "20241231" */
            const char *d = expr->data.date;
            char normalized[9];

            if (strlen(d) == 8 && isdigit((unsigned char)d[0])) {
                /* Already YYYYMMDD */
                memcpy(normalized, d, 8);
            } else {
                /* Try MM/DD/YYYY format */
                int m, day, y;
                if (sscanf(d, "%d/%d/%d", &m, &day, &y) == 3) {
                    snprintf(normalized, 9, "%04d%02d%02d", y, m, day);
                } else {
                    memset(normalized, ' ', 8);
                }
            }
            normalized[8] = '\0';
            return value_date(normalized);
        }

        case EXPR_LOGICAL:
            return value_logical(expr->data.logical);

        case EXPR_IDENT: {
            /* First check if it's a field name */
            if (ctx->current_dbf) {
                int idx = dbf_field_index(ctx->current_dbf, expr->data.ident);
                if (idx >= 0) {
                    const DBFField *field = dbf_field_info(ctx->current_dbf, idx);
                    char buf[MAX_FIELD_LEN + 1];

                    switch (field->type) {
                        case FIELD_TYPE_CHAR:
                            dbf_get_string(ctx->current_dbf, idx, buf, sizeof(buf));
                            return value_string(buf);
                        case FIELD_TYPE_NUMERIC: {
                            double val;
                            dbf_get_double(ctx->current_dbf, idx, &val);
                            return value_number(val);
                        }
                        case FIELD_TYPE_DATE:
                            dbf_get_date(ctx->current_dbf, idx, buf);
                            return value_date(buf);
                        case FIELD_TYPE_LOGICAL: {
                            bool val;
                            dbf_get_logical(ctx->current_dbf, idx, &val);
                            return value_logical(val);
                        }
                        default:
                            return value_nil();
                    }
                }
            }

            /* Check memory variables */
            Value *var = var_get(expr->data.ident);
            if (var) {
                return value_copy(var);
            }

            /* Undefined - return empty string */
            return value_string("");
        }

        case EXPR_FIELD: {
            /* alias->field - not fully implemented */
            /* For now, just look up field in current DBF */
            if (ctx->current_dbf) {
                int idx = dbf_field_index(ctx->current_dbf, expr->data.field_ref.field);
                if (idx >= 0) {
                    const DBFField *field = dbf_field_info(ctx->current_dbf, idx);
                    char buf[MAX_FIELD_LEN + 1];

                    switch (field->type) {
                        case FIELD_TYPE_CHAR:
                            dbf_get_string(ctx->current_dbf, idx, buf, sizeof(buf));
                            return value_string(buf);
                        case FIELD_TYPE_NUMERIC: {
                            double val;
                            dbf_get_double(ctx->current_dbf, idx, &val);
                            return value_number(val);
                        }
                        default:
                            return value_nil();
                    }
                }
            }
            return value_nil();
        }

        case EXPR_ARRAY: {
            /* Array element access */
            Value *arr = var_get(expr->data.array.name);
            if (arr && arr->type == VAL_ARRAY) {
                Value idx_val = expr_eval(expr->data.array.index, ctx);
                int idx = (int)value_to_number(&idx_val) - 1;  /* 1-based */
                value_free(&idx_val);

                if (idx >= 0 && idx < arr->data.array.count) {
                    return value_copy(&arr->data.array.elements[idx]);
                }
            }
            return value_nil();
        }

        case EXPR_FUNC: {
            /* Evaluate arguments */
            Value *args = NULL;
            if (expr->data.func.arg_count > 0) {
                args = xcalloc((size_t)expr->data.func.arg_count, sizeof(Value));
                for (int i = 0; i < expr->data.func.arg_count; i++) {
                    args[i] = expr_eval(expr->data.func.args[i], ctx);
                }
            }

            /* Call function */
            Value result = func_call(expr->data.func.name, args,
                                    expr->data.func.arg_count, ctx);

            /* Free arguments */
            for (int i = 0; i < expr->data.func.arg_count; i++) {
                value_free(&args[i]);
            }
            xfree(args);

            return result;
        }

        case EXPR_UNARY: {
            Value operand = expr_eval(expr->data.unary.operand, ctx);
            Value result = eval_unary(expr->data.unary.op, &operand);
            value_free(&operand);
            return result;
        }

        case EXPR_BINARY: {
            Value left = expr_eval(expr->data.binary.left, ctx);
            Value right = expr_eval(expr->data.binary.right, ctx);
            Value result = eval_binary(expr->data.binary.op, &left, &right);
            value_free(&left);
            value_free(&right);
            return result;
        }

        case EXPR_MACRO: {
            /* Macro substitution - get variable value and evaluate as expression */
            Value *var = var_get(expr->data.macro.var_name);
            if (var && var->type == VAL_STRING) {
                /* Would need to parse and evaluate the string */
                /* For now, just return the string value */
                return value_copy(var);
            }
            return value_nil();
        }

        default:
            return value_nil();
    }
}
