/*
 * xBase3 - dBASE III+ Compatible Database System
 * ast.c - Abstract Syntax Tree implementation
 */

#include "ast.h"
#include <stdlib.h>
#include <string.h>

/* Expression creation */
ASTExpr *ast_expr_number(double value) {
    ASTExpr *expr = xcalloc(1, sizeof(ASTExpr));
    expr->type = EXPR_NUMBER;
    expr->data.number = value;
    return expr;
}

ASTExpr *ast_expr_string(const char *value) {
    ASTExpr *expr = xcalloc(1, sizeof(ASTExpr));
    expr->type = EXPR_STRING;
    expr->data.string = xstrdup(value);
    return expr;
}

ASTExpr *ast_expr_date(const char *value) {
    ASTExpr *expr = xcalloc(1, sizeof(ASTExpr));
    expr->type = EXPR_DATE;
    expr->data.date = xstrdup(value);
    return expr;
}

ASTExpr *ast_expr_logical(bool value) {
    ASTExpr *expr = xcalloc(1, sizeof(ASTExpr));
    expr->type = EXPR_LOGICAL;
    expr->data.logical = value;
    return expr;
}

ASTExpr *ast_expr_ident(const char *name) {
    ASTExpr *expr = xcalloc(1, sizeof(ASTExpr));
    expr->type = EXPR_IDENT;
    expr->data.ident = xstrdup(name);
    return expr;
}

ASTExpr *ast_expr_field(const char *alias, const char *field) {
    ASTExpr *expr = xcalloc(1, sizeof(ASTExpr));
    expr->type = EXPR_FIELD;
    expr->data.field_ref.alias = xstrdup(alias);
    expr->data.field_ref.field = xstrdup(field);
    return expr;
}

ASTExpr *ast_expr_array(const char *name, ASTExpr *index) {
    ASTExpr *expr = xcalloc(1, sizeof(ASTExpr));
    expr->type = EXPR_ARRAY;
    expr->data.array.name = xstrdup(name);
    expr->data.array.index = index;
    return expr;
}

ASTExpr *ast_expr_func(const char *name, ASTExpr **args, int arg_count) {
    ASTExpr *expr = xcalloc(1, sizeof(ASTExpr));
    expr->type = EXPR_FUNC;
    expr->data.func.name = xstrdup(name);
    expr->data.func.args = args;
    expr->data.func.arg_count = arg_count;
    return expr;
}

ASTExpr *ast_expr_unary(TokenType op, ASTExpr *operand) {
    ASTExpr *expr = xcalloc(1, sizeof(ASTExpr));
    expr->type = EXPR_UNARY;
    expr->data.unary.op = op;
    expr->data.unary.operand = operand;
    return expr;
}

ASTExpr *ast_expr_binary(TokenType op, ASTExpr *left, ASTExpr *right) {
    ASTExpr *expr = xcalloc(1, sizeof(ASTExpr));
    expr->type = EXPR_BINARY;
    expr->data.binary.op = op;
    expr->data.binary.left = left;
    expr->data.binary.right = right;
    return expr;
}

ASTExpr *ast_expr_macro(const char *var_name) {
    ASTExpr *expr = xcalloc(1, sizeof(ASTExpr));
    expr->type = EXPR_MACRO;
    expr->data.macro.var_name = xstrdup(var_name);
    return expr;
}

/* Free expression tree */
void ast_expr_free(ASTExpr *expr) {
    if (!expr) return;

    switch (expr->type) {
        case EXPR_STRING:
            xfree(expr->data.string);
            break;
        case EXPR_DATE:
            xfree(expr->data.date);
            break;
        case EXPR_IDENT:
            xfree(expr->data.ident);
            break;
        case EXPR_FIELD:
            xfree(expr->data.field_ref.alias);
            xfree(expr->data.field_ref.field);
            break;
        case EXPR_ARRAY:
            xfree(expr->data.array.name);
            ast_expr_free(expr->data.array.index);
            break;
        case EXPR_FUNC:
            xfree(expr->data.func.name);
            for (int i = 0; i < expr->data.func.arg_count; i++) {
                ast_expr_free(expr->data.func.args[i]);
            }
            xfree(expr->data.func.args);
            break;
        case EXPR_UNARY:
            ast_expr_free(expr->data.unary.operand);
            break;
        case EXPR_BINARY:
            ast_expr_free(expr->data.binary.left);
            ast_expr_free(expr->data.binary.right);
            break;
        case EXPR_MACRO:
            xfree(expr->data.macro.var_name);
            break;
        default:
            break;
    }

    xfree(expr);
}

/* Create new command node */
ASTNode *ast_node_new(CommandType type) {
    ASTNode *node = xcalloc(1, sizeof(ASTNode));
    node->type = type;
    node->scope.type = SCOPE_ALL;
    return node;
}

/* Free node list */
static void free_node_list(ASTNode **nodes, int count) {
    if (!nodes) return;
    for (int i = 0; i < count; i++) {
        ast_node_free(nodes[i]);
    }
    xfree(nodes);
}

/* Free string list */
static void free_string_list(char **strings, int count) {
    if (!strings) return;
    for (int i = 0; i < count; i++) {
        xfree(strings[i]);
    }
    xfree(strings);
}

/* Free expression list */
static void free_expr_list(ASTExpr **exprs, int count) {
    if (!exprs) return;
    for (int i = 0; i < count; i++) {
        ast_expr_free(exprs[i]);
    }
    xfree(exprs);
}

/* Free command node */
void ast_node_free(ASTNode *node) {
    if (!node) return;

    ast_expr_free(node->condition);
    ast_expr_free(node->while_cond);
    ast_expr_free(node->scope.count);

    switch (node->type) {
        case CMD_QUESTION:
        case CMD_DQUESTION:
            free_expr_list(node->data.print.exprs, node->data.print.expr_count);
            break;

        case CMD_USE:
            xfree(node->data.use.filename);
            xfree(node->data.use.alias);
            break;

        case CMD_LIST:
        case CMD_DISPLAY:
            free_expr_list(node->data.list.fields, node->data.list.field_count);
            break;

        case CMD_GO:
            ast_expr_free(node->data.go.recno);
            break;

        case CMD_SKIP:
            ast_expr_free(node->data.skip.count);
            break;

        case CMD_REPLACE:
            free_string_list(node->data.replace.fields, node->data.replace.count);
            free_expr_list(node->data.replace.values, node->data.replace.count);
            break;

        case CMD_STORE:
            ast_expr_free(node->data.store.value);
            xfree(node->data.store.var);
            break;

        case CMD_CREATE:
            xfree(node->data.create.filename);
            break;

        case CMD_INDEX:
            ast_expr_free(node->data.index.key_expr);
            xfree(node->data.index.filename);
            break;

        case CMD_SEEK:
        case CMD_FIND:
            ast_expr_free(node->data.seek.key);
            break;

        case CMD_SET:
            xfree(node->data.set.option);
            ast_expr_free(node->data.set.value);
            break;

        case CMD_SELECT:
            ast_expr_free(node->data.select.area);
            break;

        case CMD_PUBLIC:
        case CMD_PRIVATE:
        case CMD_LOCAL:
        case CMD_RELEASE:
            free_string_list(node->data.vars.names, node->data.vars.count);
            break;

        case CMD_DECLARE:
            xfree(node->data.declare.name);
            ast_expr_free(node->data.declare.size);
            break;

        case CMD_IF:
            ast_expr_free(node->data.if_stmt.cond);
            free_node_list(node->data.if_stmt.then_block, node->data.if_stmt.then_count);
            free_node_list(node->data.if_stmt.else_block, node->data.if_stmt.else_count);
            break;

        case CMD_DO_WHILE:
            ast_expr_free(node->data.do_while.cond);
            free_node_list(node->data.do_while.body, node->data.do_while.body_count);
            break;

        case CMD_DO_CASE:
            for (int i = 0; i < node->data.do_case.case_count; i++) {
                ast_expr_free(node->data.do_case.cases[i]);
                free_node_list(node->data.do_case.case_blocks[i],
                              node->data.do_case.case_counts[i]);
            }
            xfree(node->data.do_case.cases);
            xfree(node->data.do_case.case_blocks);
            xfree(node->data.do_case.case_counts);
            free_node_list(node->data.do_case.otherwise, node->data.do_case.otherwise_count);
            break;

        case CMD_FOR:
            xfree(node->data.for_loop.var);
            ast_expr_free(node->data.for_loop.start);
            ast_expr_free(node->data.for_loop.end);
            ast_expr_free(node->data.for_loop.step);
            free_node_list(node->data.for_loop.body, node->data.for_loop.body_count);
            break;

        case CMD_DO:
            xfree(node->data.do_proc.name);
            free_expr_list(node->data.do_proc.args, node->data.do_proc.arg_count);
            break;

        case CMD_PROCEDURE:
        case CMD_FUNCTION:
            xfree(node->data.proc.name);
            free_node_list(node->data.proc.body, node->data.proc.body_count);
            break;

        case CMD_PARAMETERS:
            free_string_list(node->data.params.names, node->data.params.count);
            break;

        case CMD_COPY:
        case CMD_SORT:
            xfree(node->data.copy.filename);
            free_string_list(node->data.copy.fields, node->data.copy.field_count);
            break;

        case CMD_COUNT:
        case CMD_SUM:
        case CMD_AVERAGE:
            free_expr_list(node->data.aggregate.exprs, node->data.aggregate.count);
            free_string_list(node->data.aggregate.vars, node->data.aggregate.count);
            break;

        case CMD_WAIT:
        case CMD_ACCEPT:
        case CMD_INPUT:
            ast_expr_free(node->data.input.prompt);
            xfree(node->data.input.var);
            break;

        case CMD_AT_SAY:
        case CMD_AT_GET:
            ast_expr_free(node->data.at.row);
            ast_expr_free(node->data.at.col);
            ast_expr_free(node->data.at.expr);
            xfree(node->data.at.var);
            break;

        case CMD_RETURN:
            ast_expr_free(node->data.ret.value);
            break;

        case CMD_RUN:
            xfree(node->data.run.command);
            break;

        default:
            break;
    }

    xfree(node);
}

/* List helpers */
ASTExpr **ast_expr_list_new(int capacity) {
    return xcalloc((size_t)capacity, sizeof(ASTExpr *));
}

void ast_expr_list_add(ASTExpr ***list, int *count, int *capacity, ASTExpr *expr) {
    if (*count >= *capacity) {
        *capacity = (*capacity == 0) ? 8 : *capacity * 2;
        *list = xrealloc(*list, (size_t)*capacity * sizeof(ASTExpr *));
    }
    (*list)[(*count)++] = expr;
}

char **ast_string_list_new(int capacity) {
    return xcalloc((size_t)capacity, sizeof(char *));
}

void ast_string_list_add(char ***list, int *count, int *capacity, const char *str) {
    if (*count >= *capacity) {
        *capacity = (*capacity == 0) ? 8 : *capacity * 2;
        *list = xrealloc(*list, (size_t)*capacity * sizeof(char *));
    }
    (*list)[(*count)++] = xstrdup(str);
}
