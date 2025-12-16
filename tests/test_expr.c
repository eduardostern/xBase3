/*
 * xBase3 - Expression Evaluator Tests
 */

#include "parser.h"
#include "expr.h"
#include "variables.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define TEST(name) printf("Testing %s... ", name)
#define PASS() printf("PASSED\n")
#define FAIL(msg) do { printf("FAILED: %s\n", msg); return 1; } while(0)

/* Helper to evaluate expression string */
static Value eval_string(const char *input, EvalContext *ctx) {
    Parser p;
    parser_init(&p, input);
    ASTExpr *expr = parser_parse_expr(&p);
    if (!expr) return value_nil();
    Value result = expr_eval(expr, ctx);
    ast_expr_free(expr);
    return result;
}

int main(void) {
    EvalContext ctx;
    eval_context_init(&ctx);
    var_init();

    /* Test arithmetic */
    TEST("arithmetic");
    {
        Value v;

        v = eval_string("1 + 2", &ctx);
        if (v.type != VAL_NUMBER || v.data.number != 3) FAIL("1 + 2 != 3");
        value_free(&v);

        v = eval_string("10 - 3", &ctx);
        if (v.data.number != 7) FAIL("10 - 3 != 7");
        value_free(&v);

        v = eval_string("4 * 5", &ctx);
        if (v.data.number != 20) FAIL("4 * 5 != 20");
        value_free(&v);

        v = eval_string("20 / 4", &ctx);
        if (v.data.number != 5) FAIL("20 / 4 != 5");
        value_free(&v);

        v = eval_string("2 ^ 3", &ctx);
        if (v.data.number != 8) FAIL("2 ^ 3 != 8");
        value_free(&v);

        PASS();
    }

    /* Test operator precedence */
    TEST("operator precedence");
    {
        Value v;

        v = eval_string("1 + 2 * 3", &ctx);
        if (v.data.number != 7) FAIL("1 + 2 * 3 != 7");
        value_free(&v);

        v = eval_string("(1 + 2) * 3", &ctx);
        if (v.data.number != 9) FAIL("(1 + 2) * 3 != 9");
        value_free(&v);

        v = eval_string("2 ^ 3 ^ 2", &ctx);
        if (v.data.number != 512) FAIL("2 ^ 3 ^ 2 != 512 (right assoc)");
        value_free(&v);

        PASS();
    }

    /* Test string operations */
    TEST("string operations");
    {
        Value v;

        v = eval_string("\"hello\" + \" world\"", &ctx);
        if (v.type != VAL_STRING) FAIL("Expected STRING");
        if (strcmp(v.data.string, "hello world") != 0) FAIL("String concat failed");
        value_free(&v);

        v = eval_string("\"abc\" = \"abc\"", &ctx);
        if (v.type != VAL_LOGICAL || !v.data.logical) FAIL("String equality failed");
        value_free(&v);

        v = eval_string("\"abc\" < \"abd\"", &ctx);
        if (!v.data.logical) FAIL("String comparison failed");
        value_free(&v);

        PASS();
    }

    /* Test logical operations */
    TEST("logical operations");
    {
        Value v;

        v = eval_string(".T. .AND. .T.", &ctx);
        if (!v.data.logical) FAIL(".T. AND .T. should be .T.");
        value_free(&v);

        v = eval_string(".T. .AND. .F.", &ctx);
        if (v.data.logical) FAIL(".T. AND .F. should be .F.");
        value_free(&v);

        v = eval_string(".F. .OR. .T.", &ctx);
        if (!v.data.logical) FAIL(".F. OR .T. should be .T.");
        value_free(&v);

        v = eval_string(".NOT. .F.", &ctx);
        if (!v.data.logical) FAIL("NOT .F. should be .T.");
        value_free(&v);

        PASS();
    }

    /* Test comparisons */
    TEST("comparisons");
    {
        Value v;

        v = eval_string("5 > 3", &ctx);
        if (!v.data.logical) FAIL("5 > 3 should be true");
        value_free(&v);

        v = eval_string("3 >= 3", &ctx);
        if (!v.data.logical) FAIL("3 >= 3 should be true");
        value_free(&v);

        v = eval_string("2 <> 3", &ctx);
        if (!v.data.logical) FAIL("2 <> 3 should be true");
        value_free(&v);

        PASS();
    }

    /* Test variables */
    TEST("variables");
    {
        Value val = value_number(42);
        var_set("X", &val);
        value_free(&val);

        Value v = eval_string("X", &ctx);
        if (v.type != VAL_NUMBER || v.data.number != 42) FAIL("Variable X != 42");
        value_free(&v);

        v = eval_string("X + 8", &ctx);
        if (v.data.number != 50) FAIL("X + 8 != 50");
        value_free(&v);

        PASS();
    }

    /* Test functions */
    TEST("functions");
    {
        Value v;

        v = eval_string("ABS(-5)", &ctx);
        if (v.data.number != 5) FAIL("ABS(-5) != 5");
        value_free(&v);

        v = eval_string("INT(3.7)", &ctx);
        if (v.data.number != 3) FAIL("INT(3.7) != 3");
        value_free(&v);

        v = eval_string("ROUND(3.567, 2)", &ctx);
        if (fabs(v.data.number - 3.57) > 0.001) FAIL("ROUND(3.567, 2) != 3.57");
        value_free(&v);

        v = eval_string("LEN(\"hello\")", &ctx);
        if (v.data.number != 5) FAIL("LEN(\"hello\") != 5");
        value_free(&v);

        v = eval_string("UPPER(\"hello\")", &ctx);
        if (strcmp(v.data.string, "HELLO") != 0) FAIL("UPPER failed");
        value_free(&v);

        v = eval_string("SUBSTR(\"hello\", 2, 3)", &ctx);
        if (strcmp(v.data.string, "ell") != 0) FAIL("SUBSTR failed");
        value_free(&v);

        v = eval_string("TRIM(\"  hello  \")", &ctx);
        if (strcmp(v.data.string, "hello") != 0) FAIL("TRIM failed");
        value_free(&v);

        PASS();
    }

    /* Test IIF */
    TEST("IIF function");
    {
        Value v;

        v = eval_string("IIF(.T., \"yes\", \"no\")", &ctx);
        if (strcmp(v.data.string, "yes") != 0) FAIL("IIF true case failed");
        value_free(&v);

        v = eval_string("IIF(.F., \"yes\", \"no\")", &ctx);
        if (strcmp(v.data.string, "no") != 0) FAIL("IIF false case failed");
        value_free(&v);

        v = eval_string("IIF(5 > 3, 10, 20)", &ctx);
        if (v.data.number != 10) FAIL("IIF condition failed");
        value_free(&v);

        PASS();
    }

    /* Test nested functions */
    TEST("nested functions");
    {
        Value v;

        v = eval_string("LEN(TRIM(\"  hi  \"))", &ctx);
        if (v.data.number != 2) FAIL("Nested function failed");
        value_free(&v);

        v = eval_string("UPPER(SUBSTR(\"hello\", 1, 3))", &ctx);
        if (strcmp(v.data.string, "HEL") != 0) FAIL("Nested string function failed");
        value_free(&v);

        PASS();
    }

    var_cleanup();
    printf("\nAll expression tests passed!\n");
    return 0;
}
