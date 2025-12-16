/*
 * xBase3 - dBASE III+ Compatible Database System
 * server.c - Minimal HTTP server implementation
 */

#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <ctype.h>

/* Thread pool worker context */
typedef struct {
    int client_fd;
    ServerConfig *cfg;
} WorkerContext;

/* Global for signal handler */
static volatile sig_atomic_t g_shutdown = 0;

static void signal_handler(int sig) {
    (void)sig;
    g_shutdown = 1;
}

/*
 * URL decode
 */
void url_decode(char *str) {
    char *src = str;
    char *dst = str;

    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], 0};
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

/*
 * HTTP method parsing
 */
HttpMethod http_parse_method(const char *str) {
    if (strcasecmp(str, "GET") == 0) return HTTP_GET;
    if (strcasecmp(str, "POST") == 0) return HTTP_POST;
    if (strcasecmp(str, "PUT") == 0) return HTTP_PUT;
    if (strcasecmp(str, "DELETE") == 0) return HTTP_DELETE;
    if (strcasecmp(str, "OPTIONS") == 0) return HTTP_OPTIONS;
    return HTTP_UNKNOWN;
}

/*
 * Request parsing
 */
bool http_parse_request(const char *data, size_t len, HttpRequest *req) {
    memset(req, 0, sizeof(HttpRequest));

    /* Parse request line */
    const char *line_end = strstr(data, "\r\n");
    if (!line_end) return false;

    char request_line[1024];
    size_t line_len = (size_t)(line_end - data);
    if (line_len >= sizeof(request_line)) return false;
    memcpy(request_line, data, line_len);
    request_line[line_len] = '\0';

    /* Parse: METHOD PATH HTTP/x.x */
    char method[16], path[512], version[16];
    if (sscanf(request_line, "%15s %511s %15s", method, path, version) != 3) {
        return false;
    }

    req->method = http_parse_method(method);

    /* Split path and query string */
    char *query = strchr(path, '?');
    if (query) {
        *query = '\0';
        strncpy(req->query, query + 1, sizeof(req->query) - 1);
        url_decode(req->query);
    }
    strncpy(req->path, path, sizeof(req->path) - 1);
    url_decode(req->path);

    /* Parse headers */
    const char *header_start = line_end + 2;
    while (header_start < data + len) {
        line_end = strstr(header_start, "\r\n");
        if (!line_end) break;

        /* Empty line = end of headers */
        if (line_end == header_start) {
            header_start = line_end + 2;
            break;
        }

        line_len = (size_t)(line_end - header_start);
        char header_line[512];
        if (line_len >= sizeof(header_line)) {
            header_start = line_end + 2;
            continue;
        }
        memcpy(header_line, header_start, line_len);
        header_line[line_len] = '\0';

        /* Parse: Header-Name: Value */
        char *colon = strchr(header_line, ':');
        if (colon && req->header_count < SERVER_MAX_HEADERS) {
            *colon = '\0';
            char *value = colon + 1;
            while (*value == ' ') value++;

            strncpy(req->headers[req->header_count][0], header_line, 255);
            strncpy(req->headers[req->header_count][1], value, 255);

            /* Store content-type specially */
            if (strcasecmp(header_line, "Content-Type") == 0) {
                strncpy(req->content_type, value, sizeof(req->content_type) - 1);
            }

            req->header_count++;
        }

        header_start = line_end + 2;
    }

    /* Body starts after \r\n\r\n */
    const char *body_marker = strstr(data, "\r\n\r\n");
    if (body_marker) {
        req->body = (char *)(body_marker + 4);
        req->body_len = len - (size_t)(req->body - data);
    }

    return true;
}

const char *http_get_header(HttpRequest *req, const char *name) {
    for (int i = 0; i < req->header_count; i++) {
        if (strcasecmp(req->headers[i][0], name) == 0) {
            return req->headers[i][1];
        }
    }
    return NULL;
}

const char *http_get_param(HttpRequest *req, const char *name) {
    static char value[256];
    char *query = req->query;

    size_t name_len = strlen(name);
    while (*query) {
        if (strncmp(query, name, name_len) == 0 && query[name_len] == '=') {
            char *start = query + name_len + 1;
            char *end = strchr(start, '&');
            size_t len = end ? (size_t)(end - start) : strlen(start);
            if (len >= sizeof(value)) len = sizeof(value) - 1;
            memcpy(value, start, len);
            value[len] = '\0';
            return value;
        }

        char *next = strchr(query, '&');
        if (!next) break;
        query = next + 1;
    }

    return NULL;
}

bool http_get_path_param(HttpRequest *req, const char *pattern,
                         const char *name, char *value, size_t len) {
    /* Simple path matching: /api/v1/records/:recno */
    const char *path = req->path;
    const char *pat = pattern;

    while (*pat && *path) {
        if (*pat == ':') {
            /* Parameter - extract name from pattern */
            pat++;
            const char *pat_end = pat;
            while (*pat_end && *pat_end != '/') pat_end++;
            char param_name[64];
            size_t plen = (size_t)(pat_end - pat);
            if (plen >= sizeof(param_name)) plen = sizeof(param_name) - 1;
            memcpy(param_name, pat, plen);
            param_name[plen] = '\0';

            /* Extract value from path */
            const char *path_end = path;
            while (*path_end && *path_end != '/') path_end++;
            size_t vlen = (size_t)(path_end - path);

            if (strcmp(param_name, name) == 0) {
                if (vlen >= len) vlen = len - 1;
                memcpy(value, path, vlen);
                value[vlen] = '\0';
                return true;
            }

            pat = pat_end;
            path = path_end;
        } else if (*pat == *path) {
            pat++;
            path++;
        } else {
            return false;
        }
    }

    return false;
}

/*
 * Response building
 */
void http_response_init(HttpResponse *resp) {
    memset(resp, 0, sizeof(HttpResponse));
    resp->status = 200;
    strcpy(resp->status_text, "OK");
    strcpy(resp->content_type, "application/json");
}

void http_response_status(HttpResponse *resp, int status, const char *text) {
    resp->status = status;
    strncpy(resp->status_text, text, sizeof(resp->status_text) - 1);
}

void http_response_json(HttpResponse *resp, JsonValue *json) {
    if (resp->body && resp->owned_body) free(resp->body);

    resp->body = json_stringify(json);
    resp->body_len = strlen(resp->body);
    resp->owned_body = true;
    strcpy(resp->content_type, "application/json");
}

void http_response_text(HttpResponse *resp, const char *text) {
    if (resp->body && resp->owned_body) free(resp->body);

    resp->body = strdup(text);
    resp->body_len = strlen(resp->body);
    resp->owned_body = true;
    strcpy(resp->content_type, "text/plain");
}

void http_response_error(HttpResponse *resp, int status,
                         const char *code, const char *message) {
    resp->status = status;

    switch (status) {
        case 400: strcpy(resp->status_text, "Bad Request"); break;
        case 404: strcpy(resp->status_text, "Not Found"); break;
        case 405: strcpy(resp->status_text, "Method Not Allowed"); break;
        case 500: strcpy(resp->status_text, "Internal Server Error"); break;
        default: strcpy(resp->status_text, "Error"); break;
    }

    JsonValue *err = json_response_error(code, message);
    http_response_json(resp, err);
    json_free(err);
}

char *http_build_response(HttpResponse *resp, size_t *out_len) {
    /* Build response string */
    size_t header_size = 512 + resp->body_len;
    char *output = malloc(header_size);

    int header_len = snprintf(output, header_size,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Connection: close\r\n"
        "\r\n",
        resp->status, resp->status_text,
        resp->content_type,
        resp->body_len);

    if (resp->body && resp->body_len > 0) {
        memcpy(output + header_len, resp->body, resp->body_len);
    }

    *out_len = (size_t)header_len + resp->body_len;
    return output;
}

void http_response_free(HttpResponse *resp) {
    if (resp->body && resp->owned_body) {
        free(resp->body);
        resp->body = NULL;
    }
}

/*
 * Route matching
 */
static bool route_matches(const char *pattern, const char *path) {
    while (*pattern && *path) {
        if (*pattern == ':') {
            /* Skip param in pattern */
            while (*pattern && *pattern != '/') pattern++;
            /* Skip value in path */
            while (*path && *path != '/') path++;
        } else if (*pattern == *path) {
            pattern++;
            path++;
        } else {
            return false;
        }
    }

    /* Both must be at end (or pattern ends with optional trailing /) */
    return (*pattern == '\0' || (*pattern == '/' && *(pattern+1) == '\0'))
           && (*path == '\0' || (*path == '/' && *(path+1) == '\0'));
}

static Route *find_route(ServerConfig *cfg, HttpMethod method, const char *path) {
    for (int i = 0; i < cfg->route_count; i++) {
        if (cfg->routes[i].method == method &&
            route_matches(cfg->routes[i].path, path)) {
            return &cfg->routes[i];
        }
    }
    return NULL;
}

/*
 * Request handling
 */
static void handle_request(int client_fd, ServerConfig *cfg) {
    char buffer[SERVER_MAX_REQUEST];
    ssize_t received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

    if (received <= 0) {
        close(client_fd);
        return;
    }
    buffer[received] = '\0';

    HttpRequest req;
    HttpResponse resp;
    http_response_init(&resp);

    if (!http_parse_request(buffer, (size_t)received, &req)) {
        http_response_error(&resp, 400, "ERR_BAD_REQUEST", "Invalid HTTP request");
        goto send_response;
    }

    /* Handle OPTIONS for CORS */
    if (req.method == HTTP_OPTIONS) {
        http_response_status(&resp, 204, "No Content");
        goto send_response;
    }

    /* Find matching route */
    Route *route = find_route(cfg, req.method, req.path);
    if (!route) {
        /* Try to find any route with same path (for 405) */
        for (int i = 0; i < cfg->route_count; i++) {
            if (route_matches(cfg->routes[i].path, req.path)) {
                http_response_error(&resp, 405, "ERR_METHOD_NOT_ALLOWED",
                                    "Method not allowed");
                goto send_response;
            }
        }
        http_response_error(&resp, 404, "ERR_NOT_FOUND", "Route not found");
        goto send_response;
    }

    /* Call handler with locked context */
    cmd_lock(cfg->cmd_ctx);
    error_enable_longjmp(false);  /* Disable longjmp in server mode */
    route->handler(&req, &resp, cfg->cmd_ctx);
    error_enable_longjmp(true);
    cmd_unlock(cfg->cmd_ctx);

send_response:
    {
        size_t resp_len;
        char *resp_data = http_build_response(&resp, &resp_len);
        send(client_fd, resp_data, resp_len, 0);
        free(resp_data);
    }

    http_response_free(&resp);
    close(client_fd);
}

/*
 * Thread pool worker
 */
static void *worker_thread(void *arg) {
    WorkerContext *wctx = (WorkerContext *)arg;
    handle_request(wctx->client_fd, wctx->cfg);
    free(wctx);
    return NULL;
}

/*
 * Server lifecycle
 */
void server_init(ServerConfig *cfg, uint16_t port) {
    memset(cfg, 0, sizeof(ServerConfig));
    cfg->port = port;
    cfg->thread_pool_size = SERVER_THREAD_POOL_SIZE;
    cfg->server_fd = -1;
    cfg->routes = NULL;
    cfg->route_count = 0;
}

void server_add_route(ServerConfig *cfg, HttpMethod method,
                      const char *path, RouteHandler handler) {
    cfg->routes = realloc(cfg->routes, (size_t)(cfg->route_count + 1) * sizeof(Route));
    cfg->routes[cfg->route_count].method = method;
    cfg->routes[cfg->route_count].path = path;
    cfg->routes[cfg->route_count].handler = handler;
    cfg->route_count++;
}

int server_start(ServerConfig *cfg, CommandContext *cmd_ctx) {
    cfg->cmd_ctx = cmd_ctx;

    /* Set up signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);  /* Ignore broken pipe */

    /* Create socket */
    cfg->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (cfg->server_fd < 0) {
        perror("socket");
        return -1;
    }

    /* Allow address reuse */
    int opt = 1;
    setsockopt(cfg->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* Bind */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(cfg->port);

    if (bind(cfg->server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(cfg->server_fd);
        return -1;
    }

    /* Listen */
    if (listen(cfg->server_fd, SERVER_BACKLOG) < 0) {
        perror("listen");
        close(cfg->server_fd);
        return -1;
    }

    cfg->running = true;
    printf("xBase3 server listening on port %d\n", cfg->port);
    printf("Press Ctrl+C to stop\n");

    /* Accept loop */
    while (cfg->running && !g_shutdown) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(cfg->server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;  /* Interrupted by signal */
            if (!cfg->running) break;
            perror("accept");
            continue;
        }

        /* Spawn worker thread */
        WorkerContext *wctx = malloc(sizeof(WorkerContext));
        wctx->client_fd = client_fd;
        wctx->cfg = cfg;

        pthread_t thread;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        if (pthread_create(&thread, &attr, worker_thread, wctx) != 0) {
            perror("pthread_create");
            close(client_fd);
            free(wctx);
        }

        pthread_attr_destroy(&attr);
    }

    printf("\nShutting down server...\n");
    close(cfg->server_fd);
    cfg->server_fd = -1;
    cfg->running = false;

    return 0;
}

void server_shutdown(ServerConfig *cfg) {
    cfg->running = false;
    g_shutdown = 1;
    if (cfg->server_fd >= 0) {
        shutdown(cfg->server_fd, SHUT_RDWR);
    }
}

void server_cleanup(ServerConfig *cfg) {
    if (cfg->server_fd >= 0) {
        close(cfg->server_fd);
        cfg->server_fd = -1;
    }
    if (cfg->routes) {
        free(cfg->routes);
        cfg->routes = NULL;
    }
    cfg->route_count = 0;
}
