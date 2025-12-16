/*
 * xBase3 - dBASE III+ Compatible Database System
 * lexer.c - Lexical analyzer implementation
 */

#include "lexer.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* Keyword table */
static struct {
    const char *name;
    TokenType type;
} keywords[] = {
    /* Logical operators */
    {"AND", TOK_AND},
    {"OR", TOK_OR},
    {"NOT", TOK_NOT},

    /* Commands */
    {"ACCEPT", TOK_ACCEPT},
    {"APPEND", TOK_APPEND},
    {"AVERAGE", TOK_AVERAGE},
    {"BLANK", TOK_BLANK},
    {"BROWSE", TOK_BROWSE},
    {"CANCEL", TOK_CANCEL},
    {"CASE", TOK_CASE},
    {"CLEAR", TOK_CLEAR},
    {"CLOSE", TOK_CLOSE},
    {"CONTINUE", TOK_CONTINUE},
    {"COPY", TOK_COPY},
    {"COUNT", TOK_COUNT},
    {"CREATE", TOK_CREATE},
    {"DECLARE", TOK_DECLARE},
    {"DELETE", TOK_DELETE},
    {"DISPLAY", TOK_DISPLAY},
    {"DO", TOK_DO},
    {"EDIT", TOK_EDIT},
    {"ELSE", TOK_ELSE},
    {"ENDCASE", TOK_ENDCASE},
    {"ENDDO", TOK_ENDDO},
    {"ENDIF", TOK_ENDIF},
    {"ENDFOR", TOK_ENDFOR},
    {"ERASE", TOK_ERASE},
    {"EXIT", TOK_EXIT},
    {"FIND", TOK_FIND},
    {"FOR", TOK_FOR},
    {"FUNCTION", TOK_FUNCTION},
    {"GO", TOK_GO},
    {"GOTO", TOK_GOTO},
    {"HELP", TOK_HELP},
    {"IF", TOK_IF},
    {"INDEX", TOK_INDEX},
    {"INPUT", TOK_INPUT},
    {"INSERT", TOK_INSERT},
    {"LIST", TOK_LIST},
    {"LOCATE", TOK_LOCATE},
    {"LOCAL", TOK_LOCAL},
    {"LOOP", TOK_LOOP},
    {"MEMORY", TOK_MEMORY},
    {"MODIFY", TOK_MODIFY},
    {"NEXT", TOK_NEXT},
    {"NOTE", TOK_NOTE},
    {"ON", TOK_ON},
    {"ORDER", TOK_ORDER},
    {"OTHERWISE", TOK_OTHERWISE},
    {"PACK", TOK_PACK},
    {"PARAMETERS", TOK_PARAMETERS},
    {"PRIVATE", TOK_PRIVATE},
    {"PROCEDURE", TOK_PROCEDURE},
    {"PUBLIC", TOK_PUBLIC},
    {"QUIT", TOK_QUIT},
    {"READ", TOK_READ},
    {"RECALL", TOK_RECALL},
    {"REINDEX", TOK_REINDEX},
    {"RELEASE", TOK_RELEASE},
    {"REPLACE", TOK_REPLACE},
    {"REPORT", TOK_REPORT},
    {"RETURN", TOK_RETURN},
    {"RUN", TOK_RUN},
    {"SAY", TOK_SAY},
    {"SCOPE", TOK_SCOPE},
    {"SEEK", TOK_SEEK},
    {"SELECT", TOK_SELECT},
    {"SET", TOK_SET},
    {"SKIP", TOK_SKIP},
    {"SORT", TOK_SORT},
    {"STORE", TOK_STORE},
    {"STRUCTURE", TOK_STRUCTURE},
    {"SUM", TOK_SUM},
    {"TEXT", TOK_TEXT},
    {"TO", TOK_TO},
    {"TOP", TOK_TOP},
    {"TOTAL", TOK_TOTAL},
    {"TYPE", TOK_TYPE},
    {"UNLOCK", TOK_UNLOCK},
    {"USE", TOK_USE},
    {"WAIT", TOK_WAIT},
    {"WHILE", TOK_WHILE},
    {"WITH", TOK_WITH},
    {"ZAP", TOK_ZAP},

    /* Clauses */
    {"ALL", TOK_ALL},
    {"ALIAS", TOK_ALIAS},
    {"BOTTOM", TOK_BOTTOM},
    {"DATABASES", TOK_DATABASES},
    {"DELETED", TOK_DELETED},
    {"DESCENDING", TOK_DESCENDING},
    {"EXACT", TOK_EXACT},
    {"EXCLUSIVE", TOK_EXCLUSIVE},
    {"FIELDS", TOK_FIELDS},
    {"FILE", TOK_FILE},
    {"FILTER", TOK_FILTER},
    {"FROM", TOK_FROM},
    {"GET", TOK_GET},
    {"INDEXES", TOK_INDEXES},
    {"LIKE", TOK_LIKE},
    {"OFF", TOK_OFF},
    {"RECORD", TOK_RECORD},
    {"REST", TOK_REST},
    {"SHARED", TOK_SHARED},
    {"STATUS", TOK_STATUS},
    {"STEP", TOK_STEP},
    {"TAG", TOK_TAG},
    {"UNIQUE", TOK_UNIQUE},

    {NULL, TOK_EOF}
};

/* Token type names are handled in token_type_name() function */

void lexer_init(Lexer *lex, const char *input) {
    memset(lex, 0, sizeof(Lexer));
    lex->input = input;
    lex->current = input;
    lex->line = 1;
    lex->column = 1;
    lex->has_peeked = false;
}

static char peek_char(Lexer *lex) {
    return *lex->current;
}

static char peek_next_char(Lexer *lex) {
    if (*lex->current == '\0') return '\0';
    return *(lex->current + 1);
}

static char advance(Lexer *lex) {
    char c = *lex->current;
    if (c != '\0') {
        lex->current++;
        if (c == '\n') {
            lex->line++;
            lex->column = 1;
        } else {
            lex->column++;
        }
    }
    return c;
}

static void skip_whitespace(Lexer *lex) {
    while (1) {
        char c = peek_char(lex);
        if (c == ' ' || c == '\t' || c == '\r') {
            advance(lex);
        } else {
            break;
        }
    }
}

static void skip_comment(Lexer *lex) {
    /* Skip to end of line */
    while (peek_char(lex) != '\0' && peek_char(lex) != '\n') {
        advance(lex);
    }
}

static void make_token(Lexer *lex, Token *tok, TokenType type) {
    tok->type = type;
    tok->line = lex->line;
    tok->column = lex->column;
    tok->num_value = 0;
    tok->text[0] = '\0';
}

static void scan_number(Lexer *lex, Token *tok) {
    const char *start = lex->current;
    size_t len = 0;

    /* Integer part */
    while (isdigit((unsigned char)peek_char(lex))) {
        advance(lex);
        len++;
    }

    /* Decimal part */
    if (peek_char(lex) == '.' && isdigit((unsigned char)peek_next_char(lex))) {
        advance(lex);  /* . */
        len++;
        while (isdigit((unsigned char)peek_char(lex))) {
            advance(lex);
            len++;
        }
    }

    if (len >= MAX_EXPR_LEN) len = MAX_EXPR_LEN - 1;
    memcpy(tok->text, start, len);
    tok->text[len] = '\0';
    tok->type = TOK_NUMBER;
    tok->num_value = strtod(tok->text, NULL);
}

static void scan_string(Lexer *lex, Token *tok, char quote) {
    advance(lex);  /* Skip opening quote */

    const char *start = lex->current;
    size_t len = 0;

    while (peek_char(lex) != '\0' && peek_char(lex) != quote) {
        if (peek_char(lex) == '\n') {
            /* Unterminated string */
            error_set(ERR_SYNTAX, "Unterminated string");
            tok->type = TOK_ERROR;
            return;
        }
        advance(lex);
        len++;
    }

    if (peek_char(lex) == quote) {
        advance(lex);  /* Skip closing quote */
    }

    if (len >= MAX_EXPR_LEN) len = MAX_EXPR_LEN - 1;
    memcpy(tok->text, start, len);
    tok->text[len] = '\0';
    tok->type = TOK_STRING;
}

static void scan_bracket_string(Lexer *lex, Token *tok) {
    advance(lex);  /* Skip [ */

    const char *start = lex->current;
    size_t len = 0;

    while (peek_char(lex) != '\0' && peek_char(lex) != ']') {
        advance(lex);
        len++;
    }

    if (peek_char(lex) == ']') {
        advance(lex);  /* Skip ] */
    }

    if (len >= MAX_EXPR_LEN) len = MAX_EXPR_LEN - 1;
    memcpy(tok->text, start, len);
    tok->text[len] = '\0';
    tok->type = TOK_STRING;
}

static void scan_date(Lexer *lex, Token *tok) {
    advance(lex);  /* Skip { */

    const char *start = lex->current;
    size_t len = 0;

    while (peek_char(lex) != '\0' && peek_char(lex) != '}') {
        advance(lex);
        len++;
    }

    if (peek_char(lex) == '}') {
        advance(lex);  /* Skip } */
    }

    if (len >= MAX_EXPR_LEN) len = MAX_EXPR_LEN - 1;
    memcpy(tok->text, start, len);
    tok->text[len] = '\0';
    tok->type = TOK_DATE;
}

static void scan_identifier(Lexer *lex, Token *tok) {
    const char *start = lex->current;
    size_t len = 0;

    /* First char: letter or underscore */
    if (isalpha((unsigned char)peek_char(lex)) || peek_char(lex) == '_') {
        advance(lex);
        len++;
    }

    /* Rest: letter, digit, or underscore */
    while (isalnum((unsigned char)peek_char(lex)) || peek_char(lex) == '_') {
        advance(lex);
        len++;
    }

    if (len >= MAX_EXPR_LEN) len = MAX_EXPR_LEN - 1;
    memcpy(tok->text, start, len);
    tok->text[len] = '\0';

    /* Check for keyword */
    tok->type = keyword_lookup(tok->text);
}

static void scan_dot_keyword(Lexer *lex, Token *tok) {
    advance(lex);  /* Skip first . */

    const char *start = lex->current;
    size_t len = 0;

    while (isalpha((unsigned char)peek_char(lex))) {
        advance(lex);
        len++;
    }

    if (peek_char(lex) == '.') {
        advance(lex);  /* Skip closing . */
    }

    if (len >= MAX_EXPR_LEN) len = MAX_EXPR_LEN - 1;
    char keyword[MAX_EXPR_LEN];
    memcpy(keyword, start, len);
    keyword[len] = '\0';
    str_upper(keyword);

    /* Check for logical operators */
    if (strcmp(keyword, "AND") == 0) {
        tok->type = TOK_AND;
        strcpy(tok->text, ".AND.");
    } else if (strcmp(keyword, "OR") == 0) {
        tok->type = TOK_OR;
        strcpy(tok->text, ".OR.");
    } else if (strcmp(keyword, "NOT") == 0) {
        tok->type = TOK_NOT;
        strcpy(tok->text, ".NOT.");
    } else if (strcmp(keyword, "T") == 0 || strcmp(keyword, "Y") == 0) {
        tok->type = TOK_TRUE;
        strcpy(tok->text, ".T.");
    } else if (strcmp(keyword, "F") == 0 || strcmp(keyword, "N") == 0) {
        tok->type = TOK_FALSE;
        strcpy(tok->text, ".F.");
    } else {
        /* Not a recognized dot keyword - could be field separator */
        /* Back up and return DOT */
        lex->current = start - 1;
        lex->column -= (int)len + 1;
        tok->type = TOK_DOT;
        strcpy(tok->text, ".");
    }
}

Token *lexer_next(Lexer *lex) {
    /* Return peeked token if available */
    if (lex->has_peeked) {
        lex->has_peeked = false;
        lex->token = lex->peeked;
        return &lex->token;
    }

    skip_whitespace(lex);

    Token *tok = &lex->token;
    make_token(lex, tok, TOK_EOF);

    char c = peek_char(lex);

    /* End of input */
    if (c == '\0') {
        tok->type = TOK_EOF;
        return tok;
    }

    /* Newline */
    if (c == '\n') {
        advance(lex);
        tok->type = TOK_NEWLINE;
        return tok;
    }

    /* Comments */
    if (c == '*' && lex->column == 1) {
        skip_comment(lex);
        tok->type = TOK_COMMENT;
        return tok;
    }

    if (c == '&' && peek_next_char(lex) == '&') {
        advance(lex);
        advance(lex);
        skip_comment(lex);
        tok->type = TOK_COMMENT;
        return tok;
    }

    /* NOTE comment */
    if ((c == 'N' || c == 'n') &&
        str_ncasecmp(lex->current, "NOTE", 4) == 0 &&
        !isalnum((unsigned char)lex->current[4])) {
        skip_comment(lex);
        tok->type = TOK_COMMENT;
        return tok;
    }

    /* Numbers */
    if (isdigit((unsigned char)c)) {
        scan_number(lex, tok);
        return tok;
    }

    /* Strings */
    if (c == '"' || c == '\'') {
        scan_string(lex, tok, c);
        return tok;
    }

    /* Bracket strings [text] */
    if (c == '[') {
        /* Could be array subscript or string - peek ahead */
        /* For now, treat as string if followed by non-digit */
        if (!isdigit((unsigned char)peek_next_char(lex))) {
            scan_bracket_string(lex, tok);
            return tok;
        }
        advance(lex);
        tok->type = TOK_LBRACKET;
        strcpy(tok->text, "[");
        return tok;
    }

    /* Date literals {mm/dd/yyyy} */
    if (c == '{') {
        scan_date(lex, tok);
        return tok;
    }

    /* Identifiers and keywords */
    if (isalpha((unsigned char)c) || c == '_') {
        scan_identifier(lex, tok);
        return tok;
    }

    /* Dot keywords (.AND., .OR., etc.) or just dot */
    if (c == '.') {
        if (isalpha((unsigned char)peek_next_char(lex))) {
            scan_dot_keyword(lex, tok);
            return tok;
        }
        advance(lex);
        tok->type = TOK_DOT;
        strcpy(tok->text, ".");
        return tok;
    }

    /* Operators */
    advance(lex);

    switch (c) {
        case '+':
            tok->type = TOK_PLUS;
            strcpy(tok->text, "+");
            break;

        case '-':
            if (peek_char(lex) == '>') {
                advance(lex);
                tok->type = TOK_ARROW;
                strcpy(tok->text, "->");
            } else {
                tok->type = TOK_MINUS;
                strcpy(tok->text, "-");
            }
            break;

        case '*':
            tok->type = TOK_STAR;
            strcpy(tok->text, "*");
            break;

        case '/':
            tok->type = TOK_SLASH;
            strcpy(tok->text, "/");
            break;

        case '%':
            tok->type = TOK_PERCENT;
            strcpy(tok->text, "%");
            break;

        case '^':
            tok->type = TOK_CARET;
            strcpy(tok->text, "^");
            break;

        case '$':
            tok->type = TOK_DOLLAR;
            strcpy(tok->text, "$");
            break;

        case '=':
            if (peek_char(lex) == '=') {
                advance(lex);
                tok->type = TOK_EQ;
                strcpy(tok->text, "==");
            } else {
                tok->type = TOK_EQ;
                strcpy(tok->text, "=");
            }
            break;

        case '<':
            if (peek_char(lex) == '=') {
                advance(lex);
                tok->type = TOK_LE;
                strcpy(tok->text, "<=");
            } else if (peek_char(lex) == '>') {
                advance(lex);
                tok->type = TOK_NE;
                strcpy(tok->text, "<>");
            } else {
                tok->type = TOK_LT;
                strcpy(tok->text, "<");
            }
            break;

        case '>':
            if (peek_char(lex) == '=') {
                advance(lex);
                tok->type = TOK_GE;
                strcpy(tok->text, ">=");
            } else {
                tok->type = TOK_GT;
                strcpy(tok->text, ">");
            }
            break;

        case '#':
            tok->type = TOK_NE;
            strcpy(tok->text, "#");
            break;

        case '!':
            if (peek_char(lex) == '=') {
                advance(lex);
                tok->type = TOK_NE;
                strcpy(tok->text, "!=");
            } else {
                tok->type = TOK_NOT;
                strcpy(tok->text, "!");
            }
            break;

        case ':':
            if (peek_char(lex) == '=') {
                advance(lex);
                tok->type = TOK_ASSIGN;
                strcpy(tok->text, ":=");
            } else {
                tok->type = TOK_COLON;
                strcpy(tok->text, ":");
            }
            break;

        case ',':
            tok->type = TOK_COMMA;
            strcpy(tok->text, ",");
            break;

        case ';':
            tok->type = TOK_SEMI;
            strcpy(tok->text, ";");
            break;

        case '(':
            tok->type = TOK_LPAREN;
            strcpy(tok->text, "(");
            break;

        case ')':
            tok->type = TOK_RPAREN;
            strcpy(tok->text, ")");
            break;

        case ']':
            tok->type = TOK_RBRACKET;
            strcpy(tok->text, "]");
            break;

        case '&':
            tok->type = TOK_AMPERSAND;
            strcpy(tok->text, "&");
            break;

        case '@':
            tok->type = TOK_AT;
            strcpy(tok->text, "@");
            break;

        case '?':
            if (peek_char(lex) == '?') {
                advance(lex);
                tok->type = TOK_DQUESTION;
                strcpy(tok->text, "??");
            } else {
                tok->type = TOK_QUESTION;
                strcpy(tok->text, "?");
            }
            break;

        default:
            error_set(ERR_SYNTAX, "Unexpected character: '%c'", c);
            tok->type = TOK_ERROR;
            tok->text[0] = c;
            tok->text[1] = '\0';
            break;
    }

    return tok;
}

Token *lexer_peek(Lexer *lex) {
    if (!lex->has_peeked) {
        lex->peeked = *lexer_next(lex);
        lex->has_peeked = true;
        lex->token = lex->peeked;
    }
    return &lex->peeked;
}

bool lexer_match(Lexer *lex, TokenType type) {
    Token *tok = lexer_peek(lex);
    return tok->type == type;
}

bool lexer_expect(Lexer *lex, TokenType type) {
    Token *tok = lexer_next(lex);
    if (tok->type != type) {
        error_set(ERR_SYNTAX, "Expected %s, got %s",
                  token_type_name(type), token_type_name(tok->type));
        return false;
    }
    return true;
}

const char *token_type_name(TokenType type) {
    switch (type) {
        case TOK_EOF: return "end of input";
        case TOK_ERROR: return "error";
        case TOK_NUMBER: return "number";
        case TOK_STRING: return "string";
        case TOK_DATE: return "date";
        case TOK_IDENT: return "identifier";
        case TOK_PLUS: return "+";
        case TOK_MINUS: return "-";
        case TOK_STAR: return "*";
        case TOK_SLASH: return "/";
        case TOK_PERCENT: return "%";
        case TOK_CARET: return "^";
        case TOK_DOLLAR: return "$";
        case TOK_EQ: return "=";
        case TOK_NE: return "<>";
        case TOK_LT: return "<";
        case TOK_LE: return "<=";
        case TOK_GT: return ">";
        case TOK_GE: return ">=";
        case TOK_ASSIGN: return ":=";
        case TOK_COMMA: return ",";
        case TOK_DOT: return ".";
        case TOK_COLON: return ":";
        case TOK_SEMI: return ";";
        case TOK_LPAREN: return "(";
        case TOK_RPAREN: return ")";
        case TOK_LBRACKET: return "[";
        case TOK_RBRACKET: return "]";
        case TOK_AMPERSAND: return "&";
        case TOK_AT: return "@";
        case TOK_QUESTION: return "?";
        case TOK_DQUESTION: return "??";
        case TOK_ARROW: return "->";
        case TOK_AND: return ".AND.";
        case TOK_OR: return ".OR.";
        case TOK_NOT: return ".NOT.";
        case TOK_TRUE: return ".T.";
        case TOK_FALSE: return ".F.";
        case TOK_NEWLINE: return "newline";
        case TOK_COMMENT: return "comment";
        default:
            /* For keywords, return the keyword name */
            for (int i = 0; keywords[i].name; i++) {
                if (keywords[i].type == type) {
                    return keywords[i].name;
                }
            }
            return "unknown";
    }
}

bool token_is_keyword(TokenType type) {
    return type >= TOK_ACCEPT && type < TOK_NEWLINE;
}

bool token_is_command(TokenType type) {
    return type >= TOK_ACCEPT && type <= TOK_ZAP;
}

TokenType keyword_lookup(const char *text) {
    for (int i = 0; keywords[i].name; i++) {
        if (str_casecmp(text, keywords[i].name) == 0) {
            return keywords[i].type;
        }
    }
    return TOK_IDENT;
}
