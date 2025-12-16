/*
 * xBase3 - dBASE III+ Compatible Database System
 * ast.h - Abstract Syntax Tree definitions
 */

#ifndef XBASE3_AST_H
#define XBASE3_AST_H

#include "util.h"
#include "lexer.h"

/* Forward declarations */
typedef struct ASTNode ASTNode;
typedef struct ASTExpr ASTExpr;

/* Expression types */
typedef enum {
    EXPR_NUMBER,        /* Numeric literal */
    EXPR_STRING,        /* String literal */
    EXPR_DATE,          /* Date literal */
    EXPR_LOGICAL,       /* Logical literal (.T./.F.) */
    EXPR_IDENT,         /* Variable or field name */
    EXPR_FIELD,         /* alias->field */
    EXPR_ARRAY,         /* array[index] */
    EXPR_FUNC,          /* function call */
    EXPR_UNARY,         /* unary operator */
    EXPR_BINARY,        /* binary operator */
    EXPR_MACRO          /* &variable */
} ExprType;

/* Expression node */
struct ASTExpr {
    ExprType type;
    union {
        double number;
        char *string;
        char *date;
        bool logical;
        char *ident;
        struct {
            char *alias;
            char *field;
        } field_ref;
        struct {
            char *name;
            ASTExpr *index;
        } array;
        struct {
            char *name;
            ASTExpr **args;
            int arg_count;
        } func;
        struct {
            TokenType op;
            ASTExpr *operand;
        } unary;
        struct {
            TokenType op;
            ASTExpr *left;
            ASTExpr *right;
        } binary;
        struct {
            char *var_name;
        } macro;
    } data;
};

/* Command types */
typedef enum {
    CMD_NONE,
    CMD_QUESTION,       /* ? expr */
    CMD_DQUESTION,      /* ?? expr */
    CMD_USE,            /* USE [file] [ALIAS name] [EXCLUSIVE/SHARED] */
    CMD_CLOSE,          /* CLOSE [DATABASES/INDEXES/ALL] */
    CMD_LIST,           /* LIST [expr-list] [scope] [FOR cond] [WHILE cond] */
    CMD_DISPLAY,        /* DISPLAY [expr-list] [scope] [FOR cond] [WHILE cond] */
    CMD_GO,             /* GO/GOTO n / GO TOP / GO BOTTOM */
    CMD_SKIP,           /* SKIP [n] */
    CMD_LOCATE,         /* LOCATE [scope] FOR cond */
    CMD_CONTINUE,       /* CONTINUE */
    CMD_APPEND,         /* APPEND BLANK / APPEND FROM file */
    CMD_DELETE,         /* DELETE [scope] [FOR cond] [WHILE cond] */
    CMD_RECALL,         /* RECALL [scope] [FOR cond] [WHILE cond] */
    CMD_PACK,           /* PACK */
    CMD_ZAP,            /* ZAP */
    CMD_REPLACE,        /* REPLACE field WITH expr [, ...] [scope] [FOR] [WHILE] */
    CMD_STORE,          /* STORE expr TO var / var = expr */
    CMD_CREATE,         /* CREATE file */
    CMD_INDEX,          /* INDEX ON expr TO file [UNIQUE] */
    CMD_REINDEX,        /* REINDEX */
    CMD_SEEK,           /* SEEK expr */
    CMD_FIND,           /* FIND literal */
    CMD_SET,            /* SET option [TO value] */
    CMD_SELECT,         /* SELECT n / SELECT alias */
    CMD_CLEAR,          /* CLEAR [ALL/MEMORY/GETS] */
    CMD_QUIT,           /* QUIT */
    CMD_CANCEL,         /* CANCEL */
    CMD_RETURN,         /* RETURN [expr] */
    CMD_PUBLIC,         /* PUBLIC var [, var ...] */
    CMD_PRIVATE,        /* PRIVATE var [, var ...] */
    CMD_LOCAL,          /* LOCAL var [, var ...] */
    CMD_RELEASE,        /* RELEASE var [, var ...] / RELEASE ALL */
    CMD_DECLARE,        /* DECLARE array[size] */
    CMD_IF,             /* IF cond ... ENDIF */
    CMD_DO_WHILE,       /* DO WHILE cond ... ENDDO */
    CMD_DO_CASE,        /* DO CASE ... ENDCASE */
    CMD_FOR,            /* FOR var = start TO end [STEP n] ... NEXT */
    CMD_EXIT,           /* EXIT */
    CMD_LOOP,           /* LOOP */
    CMD_DO,             /* DO procedure [WITH params] */
    CMD_PROCEDURE,      /* PROCEDURE name */
    CMD_FUNCTION,       /* FUNCTION name */
    CMD_PARAMETERS,     /* PARAMETERS p1, p2, ... */
    CMD_COPY,           /* COPY TO file [FIELDS ...] [scope] [FOR] [WHILE] */
    CMD_SORT,           /* SORT TO file ON field [/A/D] [, ...] */
    CMD_COUNT,          /* COUNT [scope] [FOR cond] TO var */
    CMD_SUM,            /* SUM expr-list TO var-list [scope] [FOR] [WHILE] */
    CMD_AVERAGE,        /* AVERAGE expr-list TO var-list [scope] [FOR] [WHILE] */
    CMD_WAIT,           /* WAIT [prompt] [TO var] */
    CMD_ACCEPT,         /* ACCEPT [prompt] TO var */
    CMD_INPUT,          /* INPUT [prompt] TO var */
    CMD_AT_SAY,         /* @ row, col SAY expr */
    CMD_AT_GET,         /* @ row, col GET var */
    CMD_READ,           /* READ */
    CMD_BROWSE,         /* BROWSE */
    CMD_EDIT,           /* EDIT */
    CMD_ERASE,          /* ERASE file */
    CMD_RUN,            /* RUN command / ! command */
    CMD_NOTE,           /* NOTE comment */
    CMD_HELP,           /* HELP [command] */
    CMD_UNKNOWN         /* Unknown command */
} CommandType;

/* Scope types for LIST, DISPLAY, etc. */
typedef enum {
    SCOPE_ALL,
    SCOPE_NEXT,
    SCOPE_RECORD,
    SCOPE_REST
} ScopeType;

/* Scope specification */
typedef struct {
    ScopeType type;
    ASTExpr *count;     /* For NEXT n or RECORD n */
} Scope;

/* Command node */
struct ASTNode {
    CommandType type;
    int line;           /* Source line */

    /* Common elements */
    ASTExpr *condition; /* FOR condition */
    ASTExpr *while_cond; /* WHILE condition */
    Scope scope;

    /* Command-specific data */
    union {
        /* ? and ?? */
        struct {
            ASTExpr **exprs;
            int expr_count;
        } print;

        /* USE */
        struct {
            char *filename;
            char *alias;
            bool exclusive;
            bool shared;
        } use;

        /* CLOSE */
        struct {
            int what;  /* 0=databases, 1=indexes, 2=all */
        } close;

        /* LIST/DISPLAY */
        struct {
            ASTExpr **fields;
            int field_count;
            bool all;
            bool off;  /* Suppress record numbers */
        } list;

        /* GO/GOTO */
        struct {
            ASTExpr *recno;
            bool top;
            bool bottom;
        } go;

        /* SKIP */
        struct {
            ASTExpr *count;
        } skip;

        /* REPLACE */
        struct {
            char **fields;
            ASTExpr **values;
            int count;
        } replace;

        /* STORE / assignment */
        struct {
            ASTExpr *value;
            char *var;
        } store;

        /* CREATE */
        struct {
            char *filename;
        } create;

        /* INDEX */
        struct {
            ASTExpr *key_expr;
            char *filename;
            bool unique;
            bool descending;
        } index;

        /* SEEK/FIND */
        struct {
            ASTExpr *key;
        } seek;

        /* SET */
        struct {
            char *option;
            ASTExpr *value;
            bool on;  /* For SET option ON/OFF */
        } set;

        /* SELECT */
        struct {
            ASTExpr *area;
        } select;

        /* PUBLIC/PRIVATE/LOCAL/RELEASE */
        struct {
            char **names;
            int count;
            bool all;
        } vars;

        /* DECLARE */
        struct {
            char *name;
            ASTExpr *size;
        } declare;

        /* IF */
        struct {
            ASTExpr *cond;
            ASTNode **then_block;
            int then_count;
            ASTNode **else_block;
            int else_count;
        } if_stmt;

        /* DO WHILE */
        struct {
            ASTExpr *cond;
            ASTNode **body;
            int body_count;
        } do_while;

        /* DO CASE */
        struct {
            ASTExpr **cases;
            ASTNode ***case_blocks;
            int *case_counts;
            int case_count;
            ASTNode **otherwise;
            int otherwise_count;
        } do_case;

        /* FOR */
        struct {
            char *var;
            ASTExpr *start;
            ASTExpr *end;
            ASTExpr *step;
            ASTNode **body;
            int body_count;
        } for_loop;

        /* DO procedure */
        struct {
            char *name;
            ASTExpr **args;
            int arg_count;
        } do_proc;

        /* PROCEDURE/FUNCTION */
        struct {
            char *name;
            ASTNode **body;
            int body_count;
        } proc;

        /* PARAMETERS */
        struct {
            char **names;
            int count;
        } params;

        /* COPY/SORT */
        struct {
            char *filename;
            char **fields;
            int field_count;
        } copy;

        /* COUNT/SUM/AVERAGE */
        struct {
            ASTExpr **exprs;
            char **vars;
            int count;
        } aggregate;

        /* WAIT/ACCEPT/INPUT */
        struct {
            ASTExpr *prompt;
            char *var;
        } input;

        /* @ SAY/GET */
        struct {
            ASTExpr *row;
            ASTExpr *col;
            ASTExpr *expr;
            char *var;
            bool is_get;
        } at;

        /* RETURN */
        struct {
            ASTExpr *value;
        } ret;

        /* RUN */
        struct {
            char *command;
        } run;
    } data;
};

/* Expression creation */
ASTExpr *ast_expr_number(double value);
ASTExpr *ast_expr_string(const char *value);
ASTExpr *ast_expr_date(const char *value);
ASTExpr *ast_expr_logical(bool value);
ASTExpr *ast_expr_ident(const char *name);
ASTExpr *ast_expr_field(const char *alias, const char *field);
ASTExpr *ast_expr_array(const char *name, ASTExpr *index);
ASTExpr *ast_expr_func(const char *name, ASTExpr **args, int arg_count);
ASTExpr *ast_expr_unary(TokenType op, ASTExpr *operand);
ASTExpr *ast_expr_binary(TokenType op, ASTExpr *left, ASTExpr *right);
ASTExpr *ast_expr_macro(const char *var_name);

/* Command creation */
ASTNode *ast_node_new(CommandType type);
void ast_node_free(ASTNode *node);
void ast_expr_free(ASTExpr *expr);

/* List allocation helpers */
ASTExpr **ast_expr_list_new(int capacity);
void ast_expr_list_add(ASTExpr ***list, int *count, int *capacity, ASTExpr *expr);
char **ast_string_list_new(int capacity);
void ast_string_list_add(char ***list, int *count, int *capacity, const char *str);

#endif /* XBASE3_AST_H */
