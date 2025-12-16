/*
 * xBase3 - dBASE III+ Compatible Database System
 * lexer.h - Lexical analyzer header
 */

#ifndef XBASE3_LEXER_H
#define XBASE3_LEXER_H

#include "util.h"

/* Token types */
typedef enum {
    /* End/Error */
    TOK_EOF = 0,
    TOK_ERROR,

    /* Literals */
    TOK_NUMBER,         /* 123, 45.67 */
    TOK_STRING,         /* "hello", 'world', [text] */
    TOK_DATE,           /* {12/31/2024} */
    TOK_IDENT,          /* variable/field name */

    /* Operators */
    TOK_PLUS,           /* + */
    TOK_MINUS,          /* - */
    TOK_STAR,           /* * */
    TOK_SLASH,          /* / */
    TOK_PERCENT,        /* % */
    TOK_CARET,          /* ^ (power) */
    TOK_DOLLAR,         /* $ (substring) */
    TOK_EQ,             /* = */
    TOK_NE,             /* <>, #, != */
    TOK_LT,             /* < */
    TOK_LE,             /* <= */
    TOK_GT,             /* > */
    TOK_GE,             /* >= */
    TOK_ASSIGN,         /* := */
    TOK_COMMA,          /* , */
    TOK_DOT,            /* . */
    TOK_COLON,          /* : */
    TOK_SEMI,           /* ; */
    TOK_LPAREN,         /* ( */
    TOK_RPAREN,         /* ) */
    TOK_LBRACKET,       /* [ */
    TOK_RBRACKET,       /* ] */
    TOK_AMPERSAND,      /* & (macro) */
    TOK_AT,             /* @ */
    TOK_QUESTION,       /* ? */
    TOK_DQUESTION,      /* ?? */
    TOK_ARROW,          /* -> */

    /* Logical operators */
    TOK_AND,            /* .AND. */
    TOK_OR,             /* .OR. */
    TOK_NOT,            /* .NOT., ! */

    /* Logical constants */
    TOK_TRUE,           /* .T., .Y. */
    TOK_FALSE,          /* .F., .N. */

    /* Keywords - Commands */
    TOK_ACCEPT,
    TOK_APPEND,
    TOK_AVERAGE,
    TOK_BLANK,
    TOK_BROWSE,
    TOK_CANCEL,
    TOK_CASE,
    TOK_CLEAR,
    TOK_CLOSE,
    TOK_CONTINUE,
    TOK_COPY,
    TOK_COUNT,
    TOK_CREATE,
    TOK_DECLARE,
    TOK_DELETE,
    TOK_DISPLAY,
    TOK_DO,
    TOK_EDIT,
    TOK_ELSE,
    TOK_ENDCASE,
    TOK_ENDDO,
    TOK_ENDIF,
    TOK_ENDFOR,
    TOK_ERASE,
    TOK_EXIT,
    TOK_FIND,
    TOK_FOR,
    TOK_FUNCTION,
    TOK_GO,
    TOK_HELP,
    TOK_GOTO,
    TOK_IF,
    TOK_INDEX,
    TOK_INPUT,
    TOK_INSERT,
    TOK_LIST,
    TOK_LOCATE,
    TOK_LOCAL,
    TOK_LOOP,
    TOK_MEMORY,
    TOK_MODIFY,
    TOK_NEXT,
    TOK_NOTE,
    TOK_ON,
    TOK_ORDER,
    TOK_OTHERWISE,
    TOK_PACK,
    TOK_PARAMETERS,
    TOK_PRIVATE,
    TOK_PROCEDURE,
    TOK_PUBLIC,
    TOK_QUIT,
    TOK_READ,
    TOK_RECALL,
    TOK_REINDEX,
    TOK_RELEASE,
    TOK_REPLACE,
    TOK_REPORT,
    TOK_RETURN,
    TOK_RUN,
    TOK_SAY,
    TOK_SCOPE,
    TOK_SEEK,
    TOK_SELECT,
    TOK_SET,
    TOK_SKIP,
    TOK_SORT,
    TOK_STORE,
    TOK_STRUCTURE,
    TOK_SUM,
    TOK_TEXT,
    TOK_TO,
    TOK_TOP,
    TOK_TOTAL,
    TOK_TYPE,
    TOK_UNLOCK,
    TOK_USE,
    TOK_WAIT,
    TOK_WHILE,
    TOK_WITH,
    TOK_ZAP,

    /* Keywords - Clauses */
    TOK_ALL,
    TOK_ALIAS,
    TOK_BOTTOM,
    TOK_DATABASES,
    TOK_DELETED,
    TOK_DESCENDING,
    TOK_EXACT,
    TOK_EXCLUSIVE,
    TOK_FIELDS,
    TOK_FILE,
    TOK_FILTER,
    TOK_FROM,
    TOK_GET,
    TOK_INDEXES,
    TOK_LIKE,
    TOK_OFF,
    TOK_RECORD,
    TOK_REST,
    TOK_SHARED,
    TOK_STATUS,
    TOK_STEP,
    TOK_STRUCTURE_KW,
    TOK_TAG,
    TOK_UNIQUE,

    /* Special */
    TOK_NEWLINE,
    TOK_COMMENT,

    TOK_COUNT_TOKENS    /* Number of token types */
} TokenType;

/* Token structure */
typedef struct {
    TokenType type;
    char text[MAX_EXPR_LEN];    /* Token text */
    double num_value;           /* Numeric value (for TOK_NUMBER) */
    int line;                   /* Source line number */
    int column;                 /* Source column */
} Token;

/* Lexer state */
typedef struct {
    const char *input;          /* Input string */
    const char *current;        /* Current position */
    int line;                   /* Current line */
    int column;                 /* Current column */
    Token token;                /* Current token */
    bool has_peeked;            /* Peeked token available */
    Token peeked;               /* Peeked token */
} Lexer;

/* Initialize lexer with input string */
void lexer_init(Lexer *lex, const char *input);

/* Get next token */
Token *lexer_next(Lexer *lex);

/* Peek at next token without consuming */
Token *lexer_peek(Lexer *lex);

/* Check if current token matches type */
bool lexer_match(Lexer *lex, TokenType type);

/* Expect and consume specific token type */
bool lexer_expect(Lexer *lex, TokenType type);

/* Get token type name (for error messages) */
const char *token_type_name(TokenType type);

/* Check if token is a keyword */
bool token_is_keyword(TokenType type);

/* Check if token is a command */
bool token_is_command(TokenType type);

/* Get keyword type from string (or TOK_IDENT if not keyword) */
TokenType keyword_lookup(const char *text);

#endif /* XBASE3_LEXER_H */
