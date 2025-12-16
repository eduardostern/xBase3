/*
 * xBase3 - dBASE III+ Compatible Database System
 * main.c - Entry point and REPL
 */

#include "util.h"
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "expr.h"
#include "commands.h"
#include "dbf.h"
#include "server.h"
#include "handlers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

/* Version info */
#define XBASE3_VERSION "0.1.0"

/* Global context */
static CommandContext g_ctx;
static volatile bool g_interrupted = false;

/* Signal handler for Ctrl+C */
static void signal_handler(int sig) {
    (void)sig;
    g_interrupted = true;
    printf("\n");
}

/* Print banner */
static void print_banner(void) {
    printf("\n");
    printf("xBase3 version %s\n", XBASE3_VERSION);
    printf("dBASE III+ Compatible Database System\n");
    printf("Type QUIT to exit, ? expr to evaluate\n");
    printf("\n");
}

/* Print prompt */
static void print_prompt(void) {
    DBF *dbf = cmd_get_current_dbf(&g_ctx);
    if (dbf) {
        printf("%s> ", dbf_get_alias(dbf));
    } else {
        printf(". ");
    }
    fflush(stdout);
}

/* Read a line of input */
static char *read_line(void) {
#ifdef HAVE_READLINE
    DBF *dbf = cmd_get_current_dbf(&g_ctx);
    char prompt[64];
    if (dbf) {
        snprintf(prompt, sizeof(prompt), "%s> ", dbf_get_alias(dbf));
    } else {
        strcpy(prompt, ". ");
    }
    char *line = readline(prompt);
    if (line && *line) {
        add_history(line);
    }
    return line;
#else
    static char buffer[MAX_LINE_LEN];
    print_prompt();

    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return NULL;
    }

    /* Remove trailing newline */
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }

    return buffer;
#endif
}

/* Free input line if needed */
static void free_line(char *line) {
#ifdef HAVE_READLINE
    free(line);
#else
    (void)line;  /* Static buffer, don't free */
#endif
}

/* Execute a single line */
static bool execute_line(const char *line) {
    if (!line || !*line) return true;

    /* Skip empty lines and comments */
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0' || *p == '*') return true;

    /* Set up error recovery */
    if (setjmp(g_error_jmp) != 0) {
        /* Error occurred */
        error_print();
        error_clear();
        return true;  /* Continue REPL */
    }

    /* Parse and execute */
    Parser parser;
    parser_init(&parser, line);

    ASTNode *node = parser_parse_command(&parser);

    if (parser_had_error(&parser)) {
        error_print();
        error_clear();
        if (node) ast_node_free(node);
        return true;
    }

    if (node) {
        cmd_execute(node, &g_ctx);
        ast_node_free(node);

        if (g_ctx.quit_requested) {
            return false;
        }
    }

    return true;
}

/* Interactive REPL */
static void repl(void) {
    print_banner();

    while (!g_ctx.quit_requested) {
        g_interrupted = false;

        char *line = read_line();
        if (line == NULL) {
            /* EOF (Ctrl+D) */
            printf("\n");
            break;
        }

        if (g_interrupted) {
            free_line(line);
            continue;
        }

        bool cont = execute_line(line);
        free_line(line);

        if (!cont) break;
    }
}

/* Execute a script file */
static int execute_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        return 1;
    }

    char line[MAX_LINE_LEN];

    while (fgets(line, sizeof(line), fp) != NULL) {

        /* Remove trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        if (!execute_line(line)) {
            break;
        }

        if (g_ctx.quit_requested) {
            break;
        }
    }

    fclose(fp);
    return 0;
}

/* Print usage */
static void print_usage(const char *program) {
    printf("Usage: %s [options] [script.prg]\n", program);
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help       Show this help message\n");
    printf("  -v, --version    Show version information\n");
    printf("  -c <command>     Execute command and exit\n");
    printf("  --server         Start in HTTP server mode\n");
    printf("  --port <port>    Server port (default: 8080)\n");
    printf("\n");
    printf("If no script is specified, enters interactive mode.\n");
    printf("\n");
    printf("Server mode example:\n");
    printf("  %s --server --port 8080\n", program);
}

/* Main entry point */
int main(int argc, char *argv[]) {
    /* Set up signal handler */
    signal(SIGINT, signal_handler);

    /* Initialize context */
    cmd_context_init(&g_ctx);

    int result = 0;
    const char *script_file = NULL;
    const char *command = NULL;
    bool server_mode = false;
    int server_port = SERVER_DEFAULT_PORT;

    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            goto cleanup;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("xBase3 version %s\n", XBASE3_VERSION);
            goto cleanup;
        } else if (strcmp(argv[i], "-c") == 0) {
            if (i + 1 < argc) {
                command = argv[++i];
            } else {
                fprintf(stderr, "Error: -c requires a command argument\n");
                result = 1;
                goto cleanup;
            }
        } else if (strcmp(argv[i], "--server") == 0) {
            server_mode = true;
        } else if (strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                server_port = atoi(argv[++i]);
                if (server_port <= 0 || server_port > 65535) {
                    fprintf(stderr, "Error: Invalid port number\n");
                    result = 1;
                    goto cleanup;
                }
            } else {
                fprintf(stderr, "Error: --port requires a port number\n");
                result = 1;
                goto cleanup;
            }
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            result = 1;
            goto cleanup;
        } else {
            script_file = argv[i];
        }
    }

    /* Execute based on mode */
    if (server_mode) {
        /* HTTP server mode */
        printf("\n");
        printf("xBase3 HTTP Server v%s\n", XBASE3_VERSION);
        printf("dBASE III+ Compatible REST API\n");
        printf("\n");

        ServerConfig cfg;
        server_init(&cfg, (uint16_t)server_port);
        handlers_register(&cfg);

        result = server_start(&cfg, &g_ctx);

        server_cleanup(&cfg);
    } else if (command) {
        /* Execute single command */
        execute_line(command);
    } else if (script_file) {
        /* Execute script file */
        result = execute_file(script_file);
    } else {
        /* Interactive mode */
        repl();
    }

cleanup:
    cmd_context_cleanup(&g_ctx);
    return result;
}
