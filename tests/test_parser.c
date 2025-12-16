/*
 * xBase3 - Parser Tests
 */

#include "parser.h"
#include "ast.h"
#include <stdio.h>
#include <string.h>

#define TEST(name) printf("Testing %s... ", name)
#define PASS() printf("PASSED\n")
#define FAIL(msg) do { printf("FAILED: %s\n", msg); return 1; } while(0)

int main(void) {
    /* Test USE command */
    TEST("USE command");
    {
        Parser p;
        parser_init(&p, "USE test ALIAS t");

        ASTNode *node = parser_parse_command(&p);
        if (!node) FAIL("Parse returned NULL");
        if (node->type != CMD_USE) FAIL("Expected CMD_USE");
        if (!node->data.use.filename) FAIL("Filename is NULL");
        if (strcmp(node->data.use.filename, "test") != 0) FAIL("Filename mismatch");
        if (!node->data.use.alias) FAIL("Alias is NULL");
        if (strcmp(node->data.use.alias, "t") != 0) FAIL("Alias mismatch");

        ast_node_free(node);
        PASS();
    }

    /* Test LIST command */
    TEST("LIST command");
    {
        Parser p;
        parser_init(&p, "LIST name, age FOR age > 18");

        ASTNode *node = parser_parse_command(&p);
        if (!node) FAIL("Parse returned NULL");
        if (node->type != CMD_LIST) FAIL("Expected CMD_LIST");
        if (node->data.list.field_count != 2) FAIL("Expected 2 fields");
        if (!node->condition) FAIL("FOR condition is NULL");

        ast_node_free(node);
        PASS();
    }

    /* Test GO command */
    TEST("GO command");
    {
        Parser p;

        /* GO TOP */
        parser_init(&p, "GO TOP");
        ASTNode *node = parser_parse_command(&p);
        if (!node) FAIL("Parse returned NULL");
        if (node->type != CMD_GO) FAIL("Expected CMD_GO");
        if (!node->data.go.top) FAIL("Expected top=true");
        ast_node_free(node);

        /* GO BOTTOM */
        parser_init(&p, "GO BOTTOM");
        node = parser_parse_command(&p);
        if (!node) FAIL("Parse returned NULL");
        if (!node->data.go.bottom) FAIL("Expected bottom=true");
        ast_node_free(node);

        /* GO n */
        parser_init(&p, "GO 5");
        node = parser_parse_command(&p);
        if (!node) FAIL("Parse returned NULL");
        if (!node->data.go.recno) FAIL("Expected recno expression");
        ast_node_free(node);

        PASS();
    }

    /* Test REPLACE command */
    TEST("REPLACE command");
    {
        Parser p;
        parser_init(&p, "REPLACE name WITH \"John\", age WITH 25");

        ASTNode *node = parser_parse_command(&p);
        if (!node) FAIL("Parse returned NULL");
        if (node->type != CMD_REPLACE) FAIL("Expected CMD_REPLACE");
        if (node->data.replace.count != 2) FAIL("Expected 2 replacements");
        if (strcmp(node->data.replace.fields[0], "name") != 0) FAIL("Field 0 mismatch");
        if (strcmp(node->data.replace.fields[1], "age") != 0) FAIL("Field 1 mismatch");

        ast_node_free(node);
        PASS();
    }

    /* Test STORE command */
    TEST("STORE command");
    {
        Parser p;
        parser_init(&p, "STORE 42 TO x");

        ASTNode *node = parser_parse_command(&p);
        if (!node) FAIL("Parse returned NULL");
        if (node->type != CMD_STORE) FAIL("Expected CMD_STORE");
        if (!node->data.store.value) FAIL("Value is NULL");
        if (!node->data.store.var) FAIL("Var is NULL");
        if (strcmp(node->data.store.var, "x") != 0) FAIL("Var name mismatch");

        ast_node_free(node);
        PASS();
    }

    /* Test ? command */
    TEST("? command");
    {
        Parser p;
        parser_init(&p, "? 1 + 2, \"hello\"");

        ASTNode *node = parser_parse_command(&p);
        if (!node) FAIL("Parse returned NULL");
        if (node->type != CMD_QUESTION) FAIL("Expected CMD_QUESTION");
        if (node->data.print.expr_count != 2) FAIL("Expected 2 expressions");

        ast_node_free(node);
        PASS();
    }

    /* Test expression parsing */
    TEST("expression parsing");
    {
        Parser p;
        parser_init(&p, "1 + 2 * 3");

        ASTExpr *expr = parser_parse_expr(&p);
        if (!expr) FAIL("Parse returned NULL");
        if (expr->type != EXPR_BINARY) FAIL("Expected BINARY");
        if (expr->data.binary.op != TOK_PLUS) FAIL("Expected PLUS");

        /* Check right operand is also binary (2 * 3) */
        ASTExpr *right = expr->data.binary.right;
        if (right->type != EXPR_BINARY) FAIL("Right should be BINARY");
        if (right->data.binary.op != TOK_STAR) FAIL("Right should be STAR");

        ast_expr_free(expr);
        PASS();
    }

    /* Test function call parsing */
    TEST("function call parsing");
    {
        Parser p;
        parser_init(&p, "UPPER(name)");

        ASTExpr *expr = parser_parse_expr(&p);
        if (!expr) FAIL("Parse returned NULL");
        if (expr->type != EXPR_FUNC) FAIL("Expected FUNC");
        if (strcmp(expr->data.func.name, "UPPER") != 0) FAIL("Function name mismatch");
        if (expr->data.func.arg_count != 1) FAIL("Expected 1 argument");

        ast_expr_free(expr);
        PASS();
    }

    printf("\nAll parser tests passed!\n");
    return 0;
}
