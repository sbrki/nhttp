#ifndef NHTTP_SERVER_H
#define NHTTP_SERVER_H

#include "nhttp_handler.h"
#include "nhttp_router.h"

/* as nhttp parses data using sscanf, neither one of the elements */
/* of the string that is being parsed is not allowed to be smaller */
/* than the entire string, as that is the only way to guarantee an overflow */
/* won't happen. */
#define NHTTP_SERVER_LINE_SIZE 4096

struct nhttp_server {
  struct _nhttp_route_node *router_root;
};

/* basics */

/* nhttp_server_create creates a nhttp server */
struct nhttp_server *nhttp_server_create(void);
/* nhttp_server_run starts the passed server on the specified port */
void nhttp_server_run(struct nhttp_server *s, int port);

/* registering routes */

/* nhttp_on_get registeres the passed `handler` to handle GET requests */
/* on the specified `path`. */
void nhttp_on_get(struct nhttp_server *s, const char *path,
                  nhttp_handler_func handler);

/* nhttp_on_head registeres the passed `handler` to handle HEAD requests */
/* on the specified `path`. */
void nhttp_on_head(struct nhttp_server *s, const char *path,
                   nhttp_handler_func handler);

/* nhttp_on_post registeres the passed `handler` to handle POST requests */
/* on the specified `path`. */
void nhttp_on_post(struct nhttp_server *s, const char *path,
                   nhttp_handler_func handler);

/* nhttp_on_put registeres the passed `handler` to handle PUT requests */
/* on the specified `path`. */
void nhttp_on_put(struct nhttp_server *s, const char *path,
                  nhttp_handler_func handler);

/* nhttp_on_delete registeres the passed `handler` to handle DELETE requests */
/* on the specified `path`. */
void nhttp_on_delete(struct nhttp_server *s, const char *path,
                     nhttp_handler_func handler);

/* delivery */
int nhttp_send_string(const struct nhttp_ctx *ctx, const char *str,
                      int status_code);

int nhttp_send_html(const struct nhttp_ctx *ctx, const char *html,
                    int status_code);

int nhttp_send_blob(const struct nhttp_ctx *ctx, const unsigned char *data,
                    int count, const char *ctype, int status_code);

/* supports byte ranges */
int nhttp_send_file(const struct nhttp_ctx *ctx, const char *path);

/* misc. delivery */

/* nhttp_redirect sends HTTP 302 (temporary redirect) if argument `permanent` */
/* is 0, and HTTP 301 (permanent redirect) otherwise. */
int nhttp_redirect(const struct nhttp_ctx *ctx, const char *to, int permanent);

/* header manipulation */

/* nhttp_get_request_header returns a char* to HTTP request header value if */
/* the provided key exists, otherwise NULL. Returned char* points to the */
/* internal string in map data structure, therefore it is const and string it */
/* is pointing to must not be changed after the call. */
const char *nhttp_get_request_header(const struct nhttp_ctx *ctx,
                                     const char             *key);

/* nhttp_set_response_header sets the HTTP response header. */
/* The value string argument gets copied under the hood and can be safely */
/* changed in the caller after the call. */
void nhttp_set_response_header(const struct nhttp_ctx *ctx, const char *key,
                               const char *value);

/* path parameters */

/* nhttp_get_path_param returns a char* to URL parameter if the provided name */
/* is registered for the route, otherwise NULL. Returned char* points to the */
/* internal string in map data structure, therefore it is const and string it */
/* is pointing to must not be changed after the call. */
const char *nhttp_get_path_param(const struct nhttp_ctx *ctx, const char *name);

/* query parameters */

/* nhttp_get_path_param returns a char* to query parameter if the provided */
/* name exists in URL parameters, otherwise NULL. Returned char* points to the*/
/* internal string in map data structure, therefore it is const and string it */
/* is pointing to must not be changed after the call. */
const char *nhttp_get_query_param(const struct nhttp_ctx *ctx,
                                  const char             *name);

#endif /* NHTTP_SERVER_H */
