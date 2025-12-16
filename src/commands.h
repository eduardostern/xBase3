/*
 * xBase3 - dBASE III+ Compatible Database System
 * commands.h - Command execution header
 */

#ifndef XBASE3_COMMANDS_H
#define XBASE3_COMMANDS_H

#include "ast.h"
#include "expr.h"
#include "dbf.h"

/* Command execution context */
typedef struct {
    EvalContext eval_ctx;
    bool quit_requested;
    bool cancel_requested;
    char current_path[MAX_PATH_LEN];
} CommandContext;

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

#endif /* XBASE3_COMMANDS_H */
