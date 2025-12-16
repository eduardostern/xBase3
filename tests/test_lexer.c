/*
 * xBase3 - Lexer Tests
 */

#include "lexer.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define TEST(name) printf("Testing %s... ", name)
#define PASS() printf("PASSED\n")
#define FAIL(msg) do { printf("FAILED: %s\n", msg); return 1; } while(0)

int main(void) {
    /* Test basic tokens */
    TEST("basic tokens");
    {
        Lexer lex;
        lexer_init(&lex, "USE test");

        Token *tok = lexer_next(&lex);
        if (tok->type != TOK_USE) FAIL("Expected USE");

        tok = lexer_next(&lex);
        if (tok->type != TOK_IDENT) FAIL("Expected IDENT");
        if (strcmp(tok->text, "test") != 0) FAIL("Expected 'test'");

        tok = lexer_next(&lex);
        if (tok->type != TOK_EOF) FAIL("Expected EOF");

        PASS();
    }

    /* Test numbers */
    TEST("numbers");
    {
        Lexer lex;
        lexer_init(&lex, "123 45.67");

        Token *tok = lexer_next(&lex);
        if (tok->type != TOK_NUMBER) FAIL("Expected NUMBER");
        if (tok->num_value != 123) FAIL("Expected 123");

        tok = lexer_next(&lex);
        if (tok->type != TOK_NUMBER) FAIL("Expected NUMBER");
        if (tok->num_value != 45.67) FAIL("Expected 45.67");

        PASS();
    }

    /* Test strings */
    TEST("strings");
    {
        Lexer lex;
        lexer_init(&lex, "\"hello\" 'world'");

        Token *tok = lexer_next(&lex);
        if (tok->type != TOK_STRING) FAIL("Expected STRING");
        if (strcmp(tok->text, "hello") != 0) FAIL("Expected 'hello'");

        tok = lexer_next(&lex);
        if (tok->type != TOK_STRING) FAIL("Expected STRING");
        if (strcmp(tok->text, "world") != 0) FAIL("Expected 'world'");

        PASS();
    }

    /* Test operators */
    TEST("operators");
    {
        Lexer lex;
        lexer_init(&lex, "+ - * / = <> < <= > >= :=");

        Token *tok;
        tok = lexer_next(&lex); if (tok->type != TOK_PLUS) FAIL("Expected PLUS");
        tok = lexer_next(&lex); if (tok->type != TOK_MINUS) FAIL("Expected MINUS");
        tok = lexer_next(&lex); if (tok->type != TOK_STAR) FAIL("Expected STAR");
        tok = lexer_next(&lex); if (tok->type != TOK_SLASH) FAIL("Expected SLASH");
        tok = lexer_next(&lex); if (tok->type != TOK_EQ) FAIL("Expected EQ");
        tok = lexer_next(&lex); if (tok->type != TOK_NE) FAIL("Expected NE");
        tok = lexer_next(&lex); if (tok->type != TOK_LT) FAIL("Expected LT");
        tok = lexer_next(&lex); if (tok->type != TOK_LE) FAIL("Expected LE");
        tok = lexer_next(&lex); if (tok->type != TOK_GT) FAIL("Expected GT");
        tok = lexer_next(&lex); if (tok->type != TOK_GE) FAIL("Expected GE");
        tok = lexer_next(&lex); if (tok->type != TOK_ASSIGN) FAIL("Expected ASSIGN");

        PASS();
    }

    /* Test logical operators */
    TEST("logical operators");
    {
        Lexer lex;
        lexer_init(&lex, ".AND. .OR. .NOT. .T. .F.");

        Token *tok;
        tok = lexer_next(&lex); if (tok->type != TOK_AND) FAIL("Expected AND");
        tok = lexer_next(&lex); if (tok->type != TOK_OR) FAIL("Expected OR");
        tok = lexer_next(&lex); if (tok->type != TOK_NOT) FAIL("Expected NOT");
        tok = lexer_next(&lex); if (tok->type != TOK_TRUE) FAIL("Expected TRUE");
        tok = lexer_next(&lex); if (tok->type != TOK_FALSE) FAIL("Expected FALSE");

        PASS();
    }

    /* Test keywords */
    TEST("keywords");
    {
        Lexer lex;
        lexer_init(&lex, "USE LIST DISPLAY GO SKIP APPEND DELETE QUIT");

        Token *tok;
        tok = lexer_next(&lex); if (tok->type != TOK_USE) FAIL("Expected USE");
        tok = lexer_next(&lex); if (tok->type != TOK_LIST) FAIL("Expected LIST");
        tok = lexer_next(&lex); if (tok->type != TOK_DISPLAY) FAIL("Expected DISPLAY");
        tok = lexer_next(&lex); if (tok->type != TOK_GO) FAIL("Expected GO");
        tok = lexer_next(&lex); if (tok->type != TOK_SKIP) FAIL("Expected SKIP");
        tok = lexer_next(&lex); if (tok->type != TOK_APPEND) FAIL("Expected APPEND");
        tok = lexer_next(&lex); if (tok->type != TOK_DELETE) FAIL("Expected DELETE");
        tok = lexer_next(&lex); if (tok->type != TOK_QUIT) FAIL("Expected QUIT");

        PASS();
    }

    /* Test date literal */
    TEST("date literal");
    {
        Lexer lex;
        lexer_init(&lex, "{12/31/2024}");

        Token *tok = lexer_next(&lex);
        if (tok->type != TOK_DATE) FAIL("Expected DATE");
        if (strcmp(tok->text, "12/31/2024") != 0) FAIL("Date text mismatch");

        PASS();
    }

    /* Test peek */
    TEST("peek");
    {
        Lexer lex;
        lexer_init(&lex, "USE test");

        Token *peeked = lexer_peek(&lex);
        if (peeked->type != TOK_USE) FAIL("Peek should return USE");

        Token *tok = lexer_next(&lex);
        if (tok->type != TOK_USE) FAIL("Next should return USE");

        peeked = lexer_peek(&lex);
        if (peeked->type != TOK_IDENT) FAIL("Peek should return IDENT");

        PASS();
    }

    printf("\nAll lexer tests passed!\n");
    return 0;
}
