/*
 * xBase3 - dBASE III+ Compatible Database System
 * commands.h - Command execution header
 */

#ifndef XBASE3_COMMANDS_H
#define XBASE3_COMMANDS_H

#include "ast.h"
#include "expr.h"
#include "dbf.h"
#include "xdx.h"
#include <pthread.h>

/* Maximum open indexes per work area */
#define MAX_INDEXES 10

/* Output callback for redirecting output (e.g., to HTTP response) */
typedef void (*OutputFunc)(void *ctx, const char *fmt, ...);

/* Command execution context */
typedef struct {
    EvalContext eval_ctx;
    bool quit_requested;
    bool cancel_requested;
    char current_path[MAX_PATH_LEN];

    /* Index support */
    XDX *indexes[MAX_INDEXES];      /* Open indexes */
    int index_count;                /* Number of open indexes */
    int current_order;              /* Current controlling index (0 = none) */

    /* Thread safety */
    pthread_mutex_t mutex;          /* Protects shared state */
    bool mutex_initialized;

    /* Output redirection */
    OutputFunc output_func;         /* Output callback (NULL = printf) */
    void *output_ctx;               /* Context for output callback */
} CommandContext;

/* Output macro - use instead of printf in commands */
#define CMD_OUTPUT(ctx, ...) do { \
    if ((ctx)->output_func) { \
        (ctx)->output_func((ctx)->output_ctx, __VA_ARGS__); \
    } else { \
        printf(__VA_ARGS__); \
    } \
} while(0)

/* Initialize command context */
void cmd_context_init(CommandContext *ctx);

/* Cleanup command context */
void cmd_context_cleanup(CommandContext *ctx);

/* Execute command */
void cmd_execute(ASTNode *node, CommandContext *ctx);

/* Get current DBF (for REPL display) */
DBF *cmd_get_current_dbf(CommandContext *ctx);

/* Set current DBF */
void cmd_set_current_dbf(CommandContext *ctx, DBF *dbf);

/* Thread safety - lock/unlock context */
void cmd_lock(CommandContext *ctx);
void cmd_unlock(CommandContext *ctx);

/* Set output function for redirecting output */
void cmd_set_output(CommandContext *ctx, OutputFunc func, void *output_ctx);

#endif /* XBASE3_COMMANDS_H */
