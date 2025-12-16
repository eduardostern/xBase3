/*
 * xBase3 - dBASE III+ Compatible Database System
 * parser.h - Parser header
 */

#ifndef XBASE3_PARSER_H
#define XBASE3_PARSER_H

#include "lexer.h"
#include "ast.h"

/* Parser state */
typedef struct {
    Lexer lexer;
    bool had_error;
    bool panic_mode;
} Parser;

/* Initialize parser with input string */
void parser_init(Parser *p, const char *input);

/* Parse a single command/statement */
ASTNode *parser_parse_command(Parser *p);

/* Parse an expression */
ASTExpr *parser_parse_expr(Parser *p);

/* Check for errors */
bool parser_had_error(Parser *p);

/* Reset error state */
void parser_clear_error(Parser *p);

#endif /* XBASE3_PARSER_H */
