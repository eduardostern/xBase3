/*
 * xBase3 - dBASE III+ Compatible Database System
 * server.h - Minimal HTTP server header
 */

#ifndef XBASE3_SERVER_H
#define XBASE3_SERVER_H

#include "commands.h"
#include "json.h"
#include <stdbool.h>
#include <stdint.h>

/* Default settings */
#define SERVER_DEFAULT_PORT     8080
#define SERVER_BACKLOG          10
#define SERVER_MAX_REQUEST      65536
#define SERVER_MAX_HEADERS      50
#define SERVER_THREAD_POOL_SIZE 4

/* HTTP methods */
typedef enum {
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
    HTTP_OPTIONS,
    HTTP_UNKNOWN
} HttpMethod;

/* HTTP request */
typedef struct {
    HttpMethod method;
    char path[512];
    char query[512];          /* Query string (after ?) */
    char headers[SERVER_MAX_HEADERS][2][256];  /* Header name/value pairs */
    int header_count;
    char *body;               /* Request body (for POST/PUT) */
    size_t body_len;
    char content_type[128];
} HttpRequest;

/* HTTP response */
typedef struct {
    int status;
    char status_text[64];
    char content_type[128];
    char *body;
    size_t body_len;
    bool owned_body;          /* If true, body will be freed */
} HttpResponse;

/* Route handler function type */
typedef void (*RouteHandler)(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);

/* Route entry */
typedef struct {
    HttpMethod method;
    const char *path;         /* Path pattern (may include params like /records/:id) */
    RouteHandler handler;
} Route;

/* Server configuration */
typedef struct {
    uint16_t port;
    int thread_pool_size;
    bool running;
    int server_fd;
    CommandContext *cmd_ctx;
    Route *routes;
    int route_count;
} ServerConfig;

/*
 * Server lifecycle
 */

/* Initialize server configuration */
void server_init(ServerConfig *cfg, uint16_t port);

/* Add route to server */
void server_add_route(ServerConfig *cfg, HttpMethod method,
                      const char *path, RouteHandler handler);

/* Start server (blocks until shutdown) */
int server_start(ServerConfig *cfg, CommandContext *cmd_ctx);

/* Request server shutdown */
void server_shutdown(ServerConfig *cfg);

/* Free server resources */
void server_cleanup(ServerConfig *cfg);

/*
 * Request/Response helpers
 */

/* Parse HTTP request from socket data */
bool http_parse_request(const char *data, size_t len, HttpRequest *req);

/* Get request header value */
const char *http_get_header(HttpRequest *req, const char *name);

/* Get query parameter */
const char *http_get_param(HttpRequest *req, const char *name);

/* Get path parameter (e.g., :id from /records/:id) */
bool http_get_path_param(HttpRequest *req, const char *pattern,
                         const char *name, char *value, size_t len);

/* Initialize response */
void http_response_init(HttpResponse *resp);

/* Set response status */
void http_response_status(HttpResponse *resp, int status, const char *text);

/* Set JSON response body from JsonValue */
void http_response_json(HttpResponse *resp, JsonValue *json);

/* Set plain text response */
void http_response_text(HttpResponse *resp, const char *text);

/* Set error response */
void http_response_error(HttpResponse *resp, int status,
                         const char *code, const char *message);

/* Build raw HTTP response to send */
char *http_build_response(HttpResponse *resp, size_t *out_len);

/* Free response resources */
void http_response_free(HttpResponse *resp);

/*
 * Utility
 */

/* URL decode string in-place */
void url_decode(char *str);

/* Parse method string to enum */
HttpMethod http_parse_method(const char *str);

#endif /* XBASE3_SERVER_H */
