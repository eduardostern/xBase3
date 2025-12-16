/*
 * xBase3 - dBASE III+ Compatible Database System
 * handlers.h - REST API handlers header
 */

#ifndef XBASE3_HANDLERS_H
#define XBASE3_HANDLERS_H

#include "server.h"

/* Register all API routes with server */
void handlers_register(ServerConfig *cfg);

/*
 * Database endpoints
 */
void handle_database_open(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);
void handle_database_close(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);
void handle_database_info(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);
void handle_database_create(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);

/*
 * Navigation endpoints
 */
void handle_navigate_goto(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);
void handle_navigate_skip(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);
void handle_navigate_top(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);
void handle_navigate_bottom(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);
void handle_navigate_position(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);

/*
 * Record endpoints
 */
void handle_records_list(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);
void handle_records_get(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);
void handle_records_append(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);
void handle_records_update(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);
void handle_records_delete(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);
void handle_records_recall(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);

/*
 * Query endpoints
 */
void handle_query_locate(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);
void handle_query_count(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);
void handle_query_seek(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);

/*
 * Index endpoints
 */
void handle_index_create(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);
void handle_index_open(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);
void handle_index_close(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);
void handle_index_reindex(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);

/*
 * Execute endpoints
 */
void handle_execute(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);
void handle_eval(HttpRequest *req, HttpResponse *resp, CommandContext *ctx);

#endif /* XBASE3_HANDLERS_H */
