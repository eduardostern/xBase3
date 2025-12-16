/*
 * xBase3 - dBASE III+ Compatible Database System
 * expr.h - Expression evaluator header
 */

#ifndef XBASE3_EXPR_H
#define XBASE3_EXPR_H

#include "ast.h"
#include "dbf.h"

/* Value types */
typedef enum {
    VAL_NIL,
    VAL_NUMBER,
    VAL_STRING,
    VAL_DATE,
    VAL_LOGICAL,
    VAL_ARRAY
} ValueType;

/* Value structure */
typedef struct Value {
    ValueType type;
    union {
        double number;
        char *string;
        char date[9];    /* YYYYMMDD + null */
        bool logical;
        struct {
            struct Value *elements;
            int count;
        } array;
    } data;
} Value;

/* Evaluation context */
typedef struct {
    DBF *current_dbf;       /* Current database */
    /* Add more context as needed: work areas, etc. */
} EvalContext;

/* Create values */
Value value_nil(void);
Value value_number(double n);
Value value_string(const char *s);
Value value_date(const char *d);
Value value_logical(bool b);

/* Free value (if string or array) */
void value_free(Value *v);

/* Copy value */
Value value_copy(const Value *v);

/* Convert value to string representation */
void value_to_string(const Value *v, char *buf, size_t bufsize);

/* Convert value to number */
double value_to_number(const Value *v);

/* Convert value to logical */
bool value_to_logical(const Value *v);

/* Check if value is truthy */
bool value_is_truthy(const Value *v);

/* Evaluate expression */
Value expr_eval(ASTExpr *expr, EvalContext *ctx);

/* Initialize evaluation context */
void eval_context_init(EvalContext *ctx);

#endif /* XBASE3_EXPR_H */
