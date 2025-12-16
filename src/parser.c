/*
 * xBase3 - dBASE III+ Compatible Database System
 * parser.c - Recursive descent parser implementation
 */

#include "parser.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* Forward declarations */
static ASTExpr *parse_expression(Parser *p);
static ASTExpr *parse_or_expr(Parser *p);
static ASTExpr *parse_and_expr(Parser *p);
static ASTExpr *parse_not_expr(Parser *p);
static ASTExpr *parse_comparison(Parser *p);
static ASTExpr *parse_additive(Parser *p);
static ASTExpr *parse_multiplicative(Parser *p);
static ASTExpr *parse_power(Parser *p);
static ASTExpr *parse_unary(Parser *p);
static ASTExpr *parse_primary(Parser *p);

/* Helper to advance and return current token */
static Token *advance(Parser *p) {
    return lexer_next(&p->lexer);
}

/* Helper to peek current token */
static Token *peek(Parser *p) {
    return lexer_peek(&p->lexer);
}

/* Check if current token matches type */
static bool check(Parser *p, TokenType type) {
    return peek(p)->type == type;
}

/* Consume token if it matches, return true if matched */
static bool match(Parser *p, TokenType type) {
    if (check(p, type)) {
        advance(p);
        return true;
    }
    return false;
}

/* Expect specific token, error if not found */
static bool expect(Parser *p, TokenType type, const char *msg) {
    if (check(p, type)) {
        advance(p);
        return true;
    }
    error_set(ERR_SYNTAX, "%s (got %s)", msg, token_type_name(peek(p)->type));
    p->had_error = true;
    return false;
}

/* Skip to end of line (error recovery) */
static void synchronize(Parser *p) {
    while (!check(p, TOK_EOF) && !check(p, TOK_NEWLINE)) {
        advance(p);
    }
    if (check(p, TOK_NEWLINE)) {
        advance(p);
    }
    p->panic_mode = false;
}

/* Skip optional newlines */
static void skip_newlines(Parser *p) {
    while (check(p, TOK_NEWLINE) || check(p, TOK_COMMENT)) {
        advance(p);
    }
}

void parser_init(Parser *p, const char *input) {
    lexer_init(&p->lexer, input);
    p->had_error = false;
    p->panic_mode = false;
}

bool parser_had_error(Parser *p) {
    return p->had_error;
}

void parser_clear_error(Parser *p) {
    p->had_error = false;
    p->panic_mode = false;
}

/* Parse expression list */
static void parse_expr_list(Parser *p, ASTExpr ***list, int *count) {
    int capacity = 8;
    *list = ast_expr_list_new(capacity);
    *count = 0;

    do {
        ASTExpr *expr = parse_expression(p);
        if (!expr) break;
        ast_expr_list_add(list, count, &capacity, expr);
    } while (match(p, TOK_COMMA));
}

/* Parse identifier list */
static void parse_ident_list(Parser *p, char ***list, int *count) {
    int capacity = 8;
    *list = ast_string_list_new(capacity);
    *count = 0;

    do {
        if (!check(p, TOK_IDENT)) break;
        Token *tok = advance(p);
        ast_string_list_add(list, count, &capacity, tok->text);
    } while (match(p, TOK_COMMA));
}

/* Parse scope clause (ALL, NEXT n, RECORD n, REST) */
static void parse_scope(Parser *p, Scope *scope) {
    scope->type = SCOPE_ALL;
    scope->count = NULL;

    if (match(p, TOK_ALL)) {
        scope->type = SCOPE_ALL;
    } else if (match(p, TOK_NEXT)) {
        scope->type = SCOPE_NEXT;
        scope->count = parse_expression(p);
    } else if (match(p, TOK_RECORD)) {
        scope->type = SCOPE_RECORD;
        scope->count = parse_expression(p);
    } else if (match(p, TOK_REST)) {
        scope->type = SCOPE_REST;
    }
}

/* Parse FOR/WHILE clauses */
static void parse_conditions(Parser *p, ASTNode *node) {
    while (true) {
        if (match(p, TOK_FOR)) {
            node->condition = parse_expression(p);
        } else if (match(p, TOK_WHILE)) {
            node->while_cond = parse_expression(p);
        } else {
            break;
        }
    }
}

/* === Expression Parsing === */

static ASTExpr *parse_expression(Parser *p) {
    return parse_or_expr(p);
}

static ASTExpr *parse_or_expr(Parser *p) {
    ASTExpr *left = parse_and_expr(p);
    if (!left) return NULL;

    while (match(p, TOK_OR)) {
        ASTExpr *right = parse_and_expr(p);
        if (!right) {
            ast_expr_free(left);
            return NULL;
        }
        left = ast_expr_binary(TOK_OR, left, right);
    }

    return left;
}

static ASTExpr *parse_and_expr(Parser *p) {
    ASTExpr *left = parse_not_expr(p);
    if (!left) return NULL;

    while (match(p, TOK_AND)) {
        ASTExpr *right = parse_not_expr(p);
        if (!right) {
            ast_expr_free(left);
            return NULL;
        }
        left = ast_expr_binary(TOK_AND, left, right);
    }

    return left;
}

static ASTExpr *parse_not_expr(Parser *p) {
    if (match(p, TOK_NOT)) {
        ASTExpr *operand = parse_not_expr(p);
        if (!operand) return NULL;
        return ast_expr_unary(TOK_NOT, operand);
    }
    return parse_comparison(p);
}

static ASTExpr *parse_comparison(Parser *p) {
    ASTExpr *left = parse_additive(p);
    if (!left) return NULL;

    while (check(p, TOK_EQ) || check(p, TOK_NE) ||
           check(p, TOK_LT) || check(p, TOK_LE) ||
           check(p, TOK_GT) || check(p, TOK_GE) ||
           check(p, TOK_DOLLAR)) {
        TokenType op = advance(p)->type;
        ASTExpr *right = parse_additive(p);
        if (!right) {
            ast_expr_free(left);
            return NULL;
        }
        left = ast_expr_binary(op, left, right);
    }

    return left;
}

static ASTExpr *parse_additive(Parser *p) {
    ASTExpr *left = parse_multiplicative(p);
    if (!left) return NULL;

    while (check(p, TOK_PLUS) || check(p, TOK_MINUS)) {
        TokenType op = advance(p)->type;
        ASTExpr *right = parse_multiplicative(p);
        if (!right) {
            ast_expr_free(left);
            return NULL;
        }
        left = ast_expr_binary(op, left, right);
    }

    return left;
}

static ASTExpr *parse_multiplicative(Parser *p) {
    ASTExpr *left = parse_power(p);
    if (!left) return NULL;

    while (check(p, TOK_STAR) || check(p, TOK_SLASH) || check(p, TOK_PERCENT)) {
        TokenType op = advance(p)->type;
        ASTExpr *right = parse_power(p);
        if (!right) {
            ast_expr_free(left);
            return NULL;
        }
        left = ast_expr_binary(op, left, right);
    }

    return left;
}

static ASTExpr *parse_power(Parser *p) {
    ASTExpr *left = parse_unary(p);
    if (!left) return NULL;

    if (check(p, TOK_CARET)) {
        TokenType op = advance(p)->type;
        ASTExpr *right = parse_power(p);  /* Right associative */
        if (!right) {
            ast_expr_free(left);
            return NULL;
        }
        left = ast_expr_binary(op, left, right);
    }

    return left;
}

static ASTExpr *parse_unary(Parser *p) {
    if (check(p, TOK_MINUS) || check(p, TOK_PLUS)) {
        TokenType op = advance(p)->type;
        ASTExpr *operand = parse_unary(p);
        if (!operand) return NULL;
        return ast_expr_unary(op, operand);
    }
    return parse_primary(p);
}

static ASTExpr *parse_primary(Parser *p) {
    Token *tok = peek(p);

    switch (tok->type) {
        case TOK_NUMBER:
            advance(p);
            return ast_expr_number(tok->num_value);

        case TOK_STRING:
            advance(p);
            return ast_expr_string(tok->text);

        case TOK_DATE:
            advance(p);
            return ast_expr_date(tok->text);

        case TOK_TRUE:
            advance(p);
            return ast_expr_logical(true);

        case TOK_FALSE:
            advance(p);
            return ast_expr_logical(false);

        case TOK_IDENT: {
            char name[MAX_EXPR_LEN];
            strcpy(name, tok->text);
            advance(p);

            /* Check for function call */
            if (check(p, TOK_LPAREN)) {
                advance(p);
                ASTExpr **args = NULL;
                int arg_count = 0;

                if (!check(p, TOK_RPAREN)) {
                    int capacity = 8;
                    args = ast_expr_list_new(capacity);
                    do {
                        ASTExpr *arg = parse_expression(p);
                        if (!arg) {
                            for (int i = 0; i < arg_count; i++) {
                                ast_expr_free(args[i]);
                            }
                            xfree(args);
                            return NULL;
                        }
                        ast_expr_list_add(&args, &arg_count, &capacity, arg);
                    } while (match(p, TOK_COMMA));
                }

                if (!expect(p, TOK_RPAREN, "Expected ')' after function arguments")) {
                    for (int i = 0; i < arg_count; i++) {
                        ast_expr_free(args[i]);
                    }
                    xfree(args);
                    return NULL;
                }

                return ast_expr_func(name, args, arg_count);
            }

            /* Check for array subscript */
            if (check(p, TOK_LBRACKET)) {
                advance(p);
                ASTExpr *index = parse_expression(p);
                if (!index) return NULL;
                if (!expect(p, TOK_RBRACKET, "Expected ']' after array index")) {
                    ast_expr_free(index);
                    return NULL;
                }
                return ast_expr_array(name, index);
            }

            /* Check for alias->field */
            if (check(p, TOK_ARROW)) {
                advance(p);
                if (!check(p, TOK_IDENT)) {
                    error_set(ERR_SYNTAX, "Expected field name after '->'");
                    p->had_error = true;
                    return NULL;
                }
                Token *field = advance(p);
                return ast_expr_field(name, field->text);
            }

            return ast_expr_ident(name);
        }

        case TOK_AMPERSAND: {
            advance(p);
            if (!check(p, TOK_IDENT)) {
                error_set(ERR_SYNTAX, "Expected variable name after '&'");
                p->had_error = true;
                return NULL;
            }
            Token *var = advance(p);
            return ast_expr_macro(var->text);
        }

        case TOK_LPAREN: {
            advance(p);
            ASTExpr *expr = parse_expression(p);
            if (!expr) return NULL;
            if (!expect(p, TOK_RPAREN, "Expected ')' after expression")) {
                ast_expr_free(expr);
                return NULL;
            }
            return expr;
        }

        default:
            error_set(ERR_SYNTAX, "Unexpected token: %s", token_type_name(tok->type));
            p->had_error = true;
            return NULL;
    }
}

ASTExpr *parser_parse_expr(Parser *p) {
    return parse_expression(p);
}

/* === Command Parsing === */

/* Parse USE command */
static ASTNode *parse_use(Parser *p) {
    ASTNode *node = ast_node_new(CMD_USE);

    /* USE without filename closes current database */
    if (check(p, TOK_EOF) || check(p, TOK_NEWLINE)) {
        return node;
    }

    /* Filename */
    if (check(p, TOK_IDENT) || check(p, TOK_STRING)) {
        Token *tok = advance(p);
        node->data.use.filename = xstrdup(tok->text);
    }

    /* Options */
    while (!check(p, TOK_EOF) && !check(p, TOK_NEWLINE)) {
        if (match(p, TOK_ALIAS)) {
            if (check(p, TOK_IDENT)) {
                Token *tok = advance(p);
                node->data.use.alias = xstrdup(tok->text);
            }
        } else if (match(p, TOK_EXCLUSIVE)) {
            node->data.use.exclusive = true;
        } else if (match(p, TOK_SHARED)) {
            node->data.use.shared = true;
        } else {
            break;
        }
    }

    return node;
}

/* Parse LIST/DISPLAY command */
static ASTNode *parse_list(Parser *p, bool is_display) {
    ASTNode *node = ast_node_new(is_display ? CMD_DISPLAY : CMD_LIST);

    /* Check for STRUCTURE */
    if (match(p, TOK_STRUCTURE)) {
        /* LIST STRUCTURE - handled separately */
        node->data.list.all = false;
        return node;
    }

    /* Check for OFF (suppress record numbers) */
    if (match(p, TOK_OFF)) {
        node->data.list.off = true;
    }

    /* Parse field list or ALL */
    if (match(p, TOK_ALL)) {
        node->data.list.all = true;
    } else if (!check(p, TOK_EOF) && !check(p, TOK_NEWLINE) &&
               !check(p, TOK_FOR) && !check(p, TOK_WHILE) &&
               !check(p, TOK_NEXT) && !check(p, TOK_REST) &&
               !check(p, TOK_RECORD)) {
        parse_expr_list(p, &node->data.list.fields, &node->data.list.field_count);
    }

    /* Parse scope */
    parse_scope(p, &node->scope);

    /* Parse FOR/WHILE conditions */
    parse_conditions(p, node);

    return node;
}

/* Parse GO/GOTO command */
static ASTNode *parse_go(Parser *p) {
    ASTNode *node = ast_node_new(CMD_GO);

    if (match(p, TOK_TOP)) {
        node->data.go.top = true;
    } else if (match(p, TOK_BOTTOM)) {
        node->data.go.bottom = true;
    } else {
        node->data.go.recno = parse_expression(p);
    }

    return node;
}

/* Parse SKIP command */
static ASTNode *parse_skip(Parser *p) {
    ASTNode *node = ast_node_new(CMD_SKIP);

    if (!check(p, TOK_EOF) && !check(p, TOK_NEWLINE)) {
        node->data.skip.count = parse_expression(p);
    }

    return node;
}

/* Parse LOCATE command */
static ASTNode *parse_locate(Parser *p) {
    ASTNode *node = ast_node_new(CMD_LOCATE);

    parse_scope(p, &node->scope);
    parse_conditions(p, node);

    return node;
}

/* Parse APPEND command */
static ASTNode *parse_append(Parser *p) {
    ASTNode *node = ast_node_new(CMD_APPEND);

    if (match(p, TOK_BLANK)) {
        /* APPEND BLANK */
    } else if (match(p, TOK_FROM)) {
        /* APPEND FROM file - not fully implemented */
        if (check(p, TOK_IDENT) || check(p, TOK_STRING)) {
            advance(p);
        }
    }

    return node;
}

/* Parse DELETE/RECALL command */
static ASTNode *parse_delete(Parser *p, bool is_recall) {
    ASTNode *node = ast_node_new(is_recall ? CMD_RECALL : CMD_DELETE);

    parse_scope(p, &node->scope);
    parse_conditions(p, node);

    return node;
}

/* Parse REPLACE command */
static ASTNode *parse_replace(Parser *p) {
    ASTNode *node = ast_node_new(CMD_REPLACE);
    int capacity = 8;
    node->data.replace.fields = ast_string_list_new(capacity);
    node->data.replace.values = ast_expr_list_new(capacity);
    node->data.replace.count = 0;

    do {
        /* Field name */
        if (!check(p, TOK_IDENT)) {
            error_set(ERR_SYNTAX, "Expected field name in REPLACE");
            p->had_error = true;
            return node;
        }
        Token *field_tok = advance(p);
        char *field_name = xstrdup(field_tok->text);  /* Copy before lexer overwrites */

        /* WITH */
        if (!expect(p, TOK_WITH, "Expected WITH in REPLACE")) {
            free(field_name);
            return node;
        }

        /* Value expression */
        ASTExpr *value = parse_expression(p);
        if (!value) {
            free(field_name);
            return node;
        }

        /* Add field name directly (already allocated) */
        if (node->data.replace.count >= capacity) {
            capacity = capacity * 2;
            node->data.replace.fields = xrealloc(node->data.replace.fields,
                                                  (size_t)capacity * sizeof(char *));
            node->data.replace.values = xrealloc(node->data.replace.values,
                                                  (size_t)capacity * sizeof(ASTExpr *));
        }
        node->data.replace.fields[node->data.replace.count] = field_name;
        node->data.replace.values[node->data.replace.count] = value;
        node->data.replace.count++;

    } while (match(p, TOK_COMMA));

    parse_scope(p, &node->scope);
    parse_conditions(p, node);

    return node;
}

/* Parse STORE command or assignment */
static ASTNode *parse_store(Parser *p) {
    ASTNode *node = ast_node_new(CMD_STORE);

    /* STORE expr TO var */
    node->data.store.value = parse_expression(p);

    if (!expect(p, TOK_TO, "Expected TO in STORE")) {
        return node;
    }

    if (!check(p, TOK_IDENT)) {
        error_set(ERR_SYNTAX, "Expected variable name");
        p->had_error = true;
        return node;
    }

    Token *var = advance(p);
    node->data.store.var = xstrdup(var->text);

    return node;
}

/* Parse assignment (var = expr) */
static ASTNode *parse_assignment(Parser *p, const char *var_name) {
    ASTNode *node = ast_node_new(CMD_STORE);
    node->data.store.var = xstrdup(var_name);

    /* Skip = or := */
    advance(p);

    node->data.store.value = parse_expression(p);

    return node;
}

/* Parse SET command */
static ASTNode *parse_set(Parser *p) {
    ASTNode *node = ast_node_new(CMD_SET);

    if (!check(p, TOK_IDENT) && !token_is_keyword(peek(p)->type)) {
        error_set(ERR_SYNTAX, "Expected SET option");
        p->had_error = true;
        return node;
    }

    Token *option = advance(p);
    node->data.set.option = xstrdup(option->text);

    /* TO value or ON/OFF */
    if (match(p, TOK_TO)) {
        node->data.set.value = parse_expression(p);
    } else if (str_casecmp(peek(p)->text, "ON") == 0) {
        advance(p);
        node->data.set.on = true;
    } else if (str_casecmp(peek(p)->text, "OFF") == 0) {
        advance(p);
        node->data.set.on = false;
    }

    return node;
}

/* Parse SELECT command */
static ASTNode *parse_select(Parser *p) {
    ASTNode *node = ast_node_new(CMD_SELECT);
    node->data.select.area = parse_expression(p);
    return node;
}

/* Parse variable declaration (PUBLIC/PRIVATE/LOCAL) */
static ASTNode *parse_var_decl(Parser *p, CommandType type) {
    ASTNode *node = ast_node_new(type);
    parse_ident_list(p, &node->data.vars.names, &node->data.vars.count);
    return node;
}

/* Parse RELEASE command */
static ASTNode *parse_release(Parser *p) {
    ASTNode *node = ast_node_new(CMD_RELEASE);

    if (match(p, TOK_ALL)) {
        node->data.vars.all = true;
    } else {
        parse_ident_list(p, &node->data.vars.names, &node->data.vars.count);
    }

    return node;
}

/* Parse DECLARE command */
static ASTNode *parse_declare(Parser *p) {
    ASTNode *node = ast_node_new(CMD_DECLARE);

    if (!check(p, TOK_IDENT)) {
        error_set(ERR_SYNTAX, "Expected array name");
        p->had_error = true;
        return node;
    }

    Token *name = advance(p);
    node->data.declare.name = xstrdup(name->text);

    if (!expect(p, TOK_LBRACKET, "Expected '[' after array name")) {
        return node;
    }

    node->data.declare.size = parse_expression(p);

    expect(p, TOK_RBRACKET, "Expected ']' after array size");

    return node;
}

/* Parse ? or ?? command */
static ASTNode *parse_print(Parser *p, bool double_question) {
    ASTNode *node = ast_node_new(double_question ? CMD_DQUESTION : CMD_QUESTION);

    if (!check(p, TOK_EOF) && !check(p, TOK_NEWLINE)) {
        parse_expr_list(p, &node->data.print.exprs, &node->data.print.expr_count);
    }

    return node;
}

/* Parse CLEAR command */
static ASTNode *parse_clear(Parser *p) {
    ASTNode *node = ast_node_new(CMD_CLEAR);

    /* CLEAR ALL, CLEAR MEMORY, CLEAR GETS, etc. */
    if (check(p, TOK_ALL) || check(p, TOK_MEMORY) || check(p, TOK_IDENT)) {
        advance(p);
    }

    return node;
}

/* Parse INDEX command */
static ASTNode *parse_index(Parser *p) {
    ASTNode *node = ast_node_new(CMD_INDEX);

    if (!expect(p, TOK_ON, "Expected ON in INDEX command")) {
        return node;
    }

    node->data.index.key_expr = parse_expression(p);

    if (!expect(p, TOK_TO, "Expected TO in INDEX command")) {
        return node;
    }

    if (check(p, TOK_IDENT) || check(p, TOK_STRING)) {
        Token *tok = advance(p);
        node->data.index.filename = xstrdup(tok->text);
    }

    /* Options */
    while (!check(p, TOK_EOF) && !check(p, TOK_NEWLINE)) {
        if (match(p, TOK_UNIQUE)) {
            node->data.index.unique = true;
        } else if (match(p, TOK_DESCENDING)) {
            node->data.index.descending = true;
        } else {
            break;
        }
    }

    return node;
}

/* Parse SEEK command */
static ASTNode *parse_seek(Parser *p) {
    ASTNode *node = ast_node_new(CMD_SEEK);
    node->data.seek.key = parse_expression(p);
    return node;
}

/* Parse CLOSE command */
static ASTNode *parse_close(Parser *p) {
    ASTNode *node = ast_node_new(CMD_CLOSE);

    if (match(p, TOK_DATABASES)) {
        node->data.close.what = 0;
    } else if (match(p, TOK_INDEXES)) {
        node->data.close.what = 1;
    } else if (match(p, TOK_ALL)) {
        node->data.close.what = 2;
    }

    return node;
}

/* Parse CREATE command */
static ASTNode *parse_create(Parser *p) {
    ASTNode *node = ast_node_new(CMD_CREATE);

    if (check(p, TOK_IDENT) || check(p, TOK_STRING)) {
        Token *tok = advance(p);
        node->data.create.filename = xstrdup(tok->text);
    }

    return node;
}

/* Parse WAIT command */
static ASTNode *parse_wait(Parser *p) {
    ASTNode *node = ast_node_new(CMD_WAIT);

    if (check(p, TOK_STRING)) {
        node->data.input.prompt = parse_expression(p);
    }

    if (match(p, TOK_TO)) {
        if (check(p, TOK_IDENT)) {
            Token *tok = advance(p);
            node->data.input.var = xstrdup(tok->text);
        }
    }

    return node;
}

/* Parse ACCEPT/INPUT command */
static ASTNode *parse_input(Parser *p, bool is_accept) {
    ASTNode *node = ast_node_new(is_accept ? CMD_ACCEPT : CMD_INPUT);

    if (check(p, TOK_STRING)) {
        node->data.input.prompt = parse_expression(p);
    }

    if (!expect(p, TOK_TO, "Expected TO in ACCEPT/INPUT")) {
        return node;
    }

    if (check(p, TOK_IDENT)) {
        Token *tok = advance(p);
        node->data.input.var = xstrdup(tok->text);
    }

    return node;
}

/* Parse @ command (@ row, col SAY/GET) */
static ASTNode *parse_at(Parser *p) {
    ASTNode *node = ast_node_new(CMD_AT_SAY);

    node->data.at.row = parse_expression(p);

    if (!expect(p, TOK_COMMA, "Expected ',' after row in @")) {
        return node;
    }

    node->data.at.col = parse_expression(p);

    if (match(p, TOK_SAY)) {
        node->type = CMD_AT_SAY;
        node->data.at.expr = parse_expression(p);
    } else if (match(p, TOK_GET)) {
        node->type = CMD_AT_GET;
        node->data.at.is_get = true;
        if (check(p, TOK_IDENT)) {
            Token *tok = advance(p);
            node->data.at.var = xstrdup(tok->text);
        }
    }

    return node;
}

/* Parse RETURN command */
static ASTNode *parse_return(Parser *p) {
    ASTNode *node = ast_node_new(CMD_RETURN);

    if (!check(p, TOK_EOF) && !check(p, TOK_NEWLINE)) {
        node->data.ret.value = parse_expression(p);
    }

    return node;
}

/* Parse COUNT command */
static ASTNode *parse_count(Parser *p) {
    ASTNode *node = ast_node_new(CMD_COUNT);

    parse_scope(p, &node->scope);
    parse_conditions(p, node);

    if (match(p, TOK_TO)) {
        if (check(p, TOK_IDENT)) {
            Token *tok = advance(p);
            int capacity = 1;
            node->data.aggregate.vars = ast_string_list_new(capacity);
            ast_string_list_add(&node->data.aggregate.vars,
                               &node->data.aggregate.count, &capacity, tok->text);
        }
    }

    return node;
}

/* Parse SUM/AVERAGE command */
static ASTNode *parse_sum_avg(Parser *p, bool is_sum) {
    ASTNode *node = ast_node_new(is_sum ? CMD_SUM : CMD_AVERAGE);

    parse_expr_list(p, &node->data.aggregate.exprs, &node->data.aggregate.count);

    if (match(p, TOK_TO)) {
        int var_count = 0;
        parse_ident_list(p, &node->data.aggregate.vars, &var_count);
    }

    parse_scope(p, &node->scope);
    parse_conditions(p, node);

    return node;
}

/* Main command parser */
ASTNode *parser_parse_command(Parser *p) {
    skip_newlines(p);

    if (check(p, TOK_EOF)) {
        return NULL;
    }

    Token *tok = peek(p);
    ASTNode *node = NULL;

    /* Check for assignment (ident = expr) first */
    if (tok->type == TOK_IDENT) {
        /* Save identifier and check next token */
        char ident[MAX_EXPR_LEN];
        strcpy(ident, tok->text);

        /* Check if next token is = or := */
        lexer_next(&p->lexer);  /* Consume ident */
        if (check(p, TOK_EQ) || check(p, TOK_ASSIGN)) {
            node = parse_assignment(p, ident);
            return node;
        }
        /* Not an assignment, need to handle as expression or unknown */
        /* For now, treat standalone identifier as print */
        p->lexer.has_peeked = false;
        lexer_init(&p->lexer, p->lexer.input);  /* Reset - hacky, but works */
        /* Actually, let's just re-parse. Need to backtrack. */
        /* This is a limitation - we'll handle it differently */
    }

    /* Get command token */
    tok = peek(p);

    switch (tok->type) {
        case TOK_QUESTION:
            advance(p);
            node = parse_print(p, false);
            break;

        case TOK_DQUESTION:
            advance(p);
            node = parse_print(p, true);
            break;

        case TOK_USE:
            advance(p);
            node = parse_use(p);
            break;

        case TOK_CLOSE:
            advance(p);
            node = parse_close(p);
            break;

        case TOK_LIST:
            advance(p);
            node = parse_list(p, false);
            break;

        case TOK_DISPLAY:
            advance(p);
            node = parse_list(p, true);
            break;

        case TOK_GO:
        case TOK_GOTO:
            advance(p);
            node = parse_go(p);
            break;

        case TOK_SKIP:
            advance(p);
            node = parse_skip(p);
            break;

        case TOK_LOCATE:
            advance(p);
            node = parse_locate(p);
            break;

        case TOK_CONTINUE:
            advance(p);
            node = ast_node_new(CMD_CONTINUE);
            break;

        case TOK_APPEND:
            advance(p);
            node = parse_append(p);
            break;

        case TOK_DELETE:
            advance(p);
            node = parse_delete(p, false);
            break;

        case TOK_RECALL:
            advance(p);
            node = parse_delete(p, true);
            break;

        case TOK_PACK:
            advance(p);
            node = ast_node_new(CMD_PACK);
            break;

        case TOK_ZAP:
            advance(p);
            node = ast_node_new(CMD_ZAP);
            break;

        case TOK_REPLACE:
            advance(p);
            node = parse_replace(p);
            break;

        case TOK_STORE:
            advance(p);
            node = parse_store(p);
            break;

        case TOK_SET:
            advance(p);
            node = parse_set(p);
            break;

        case TOK_SELECT:
            advance(p);
            node = parse_select(p);
            break;

        case TOK_PUBLIC:
            advance(p);
            node = parse_var_decl(p, CMD_PUBLIC);
            break;

        case TOK_PRIVATE:
            advance(p);
            node = parse_var_decl(p, CMD_PRIVATE);
            break;

        case TOK_LOCAL:
            advance(p);
            node = parse_var_decl(p, CMD_LOCAL);
            break;

        case TOK_RELEASE:
            advance(p);
            node = parse_release(p);
            break;

        case TOK_DECLARE:
            advance(p);
            node = parse_declare(p);
            break;

        case TOK_CLEAR:
            advance(p);
            node = parse_clear(p);
            break;

        case TOK_QUIT:
            advance(p);
            node = ast_node_new(CMD_QUIT);
            break;

        case TOK_HELP:
            advance(p);
            node = ast_node_new(CMD_HELP);
            break;

        case TOK_CANCEL:
            advance(p);
            node = ast_node_new(CMD_CANCEL);
            break;

        case TOK_RETURN:
            advance(p);
            node = parse_return(p);
            break;

        case TOK_INDEX:
            advance(p);
            node = parse_index(p);
            break;

        case TOK_REINDEX:
            advance(p);
            node = ast_node_new(CMD_REINDEX);
            break;

        case TOK_SEEK:
            advance(p);
            node = parse_seek(p);
            break;

        case TOK_FIND:
            advance(p);
            node = parse_seek(p);  /* Similar to SEEK */
            node->type = CMD_FIND;
            break;

        case TOK_CREATE:
            advance(p);
            node = parse_create(p);
            break;

        case TOK_WAIT:
            advance(p);
            node = parse_wait(p);
            break;

        case TOK_ACCEPT:
            advance(p);
            node = parse_input(p, true);
            break;

        case TOK_INPUT:
            advance(p);
            node = parse_input(p, false);
            break;

        case TOK_AT:
            advance(p);
            node = parse_at(p);
            break;

        case TOK_READ:
            advance(p);
            node = ast_node_new(CMD_READ);
            break;

        case TOK_BROWSE:
            advance(p);
            node = ast_node_new(CMD_BROWSE);
            break;

        case TOK_EDIT:
            advance(p);
            node = ast_node_new(CMD_EDIT);
            break;

        case TOK_COUNT:
            advance(p);
            node = parse_count(p);
            break;

        case TOK_SUM:
            advance(p);
            node = parse_sum_avg(p, true);
            break;

        case TOK_AVERAGE:
            advance(p);
            node = parse_sum_avg(p, false);
            break;

        case TOK_IDENT:
            /* Could be assignment or function call as statement */
            {
                char name[MAX_EXPR_LEN];
                strcpy(name, tok->text);
                advance(p);

                if (check(p, TOK_EQ) || check(p, TOK_ASSIGN)) {
                    node = parse_assignment(p, name);
                } else {
                    /* Treat as expression to evaluate */
                    /* Back up and parse as print */
                    node = ast_node_new(CMD_QUESTION);
                    /* Need to re-parse - for now just create identifier */
                    node->data.print.exprs = ast_expr_list_new(1);
                    node->data.print.exprs[0] = ast_expr_ident(name);
                    node->data.print.expr_count = 1;
                }
            }
            break;

        case TOK_COMMENT:
        case TOK_NOTE:
            advance(p);
            skip_newlines(p);
            return parser_parse_command(p);  /* Get next real command */

        default:
            error_set(ERR_SYNTAX, "Unknown command: %s", tok->text);
            p->had_error = true;
            synchronize(p);
            return NULL;
    }

    /* Skip to end of line */
    while (!check(p, TOK_EOF) && !check(p, TOK_NEWLINE)) {
        if (check(p, TOK_COMMENT)) {
            advance(p);
            continue;
        }
        advance(p);
    }

    if (check(p, TOK_NEWLINE)) {
        advance(p);
    }

    return node;
}
