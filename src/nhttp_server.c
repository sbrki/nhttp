#include "nhttp_server.h"
#include "nhttp_map.h"
#include "nhttp_req_type.h"
#include "nhttp_router.h"
#include "nhttp_util.h"
#include <errno.h>
#include <fcntl.h> /* O_* */
#include <netinet/in.h>
#include <signal.h>     /* signal, SIG* */
#include <sys/socket.h> /* socket, */

#include <stdlib.h> /* malloc,strcpy, */

static void _nhttp_server_dispatch(struct nhttp_server *s, int connfd);
static void _nhttp_server_send_status_line(int connfd, int status_code);
static int  _nhttp_send_generic(const struct nhttp_ctx *ctx,
                                const unsigned char *data, size_t count,
                                const char *ctype, int status_code);
static void _nhttp_server_assert_path_len(const char *path);
static int _nhttp_send_byte_range(const struct nhttp_ctx *ctx, const char *path,
                                  long range_start, long range_end,
                                  size_t filelen);
static int _nhttp_send_file(const struct nhttp_ctx *ctx, const char *path,
                            size_t filelen);

enum _nhttp_req_type _nhttp_server_parse_method(const char *method);

struct nhttp_server *nhttp_server_create() {
  struct nhttp_server *s = malloc(sizeof(struct nhttp_server));
  memset(s, 0, sizeof(struct nhttp_server));
  s->router_root = _nhttp_route_node_create("");
  return s;
}

void nhttp_server_run(struct nhttp_server *s, int port) {
  /* TODO(sbrki): register sig handlers for gracefully shutting down the serv */

  int                sockfd, connfd;
  struct sockaddr_in serv_info;
  int                one = 1; /* for setsockopt */

  /* ignore SIGPIPE which is raised when client closes the conn */
  if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
    _nhttp_panicf("could not set ignoring SIGPIPE: %s", strerror(errno));
  }

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    _nhttp_panicf("could not create socket: %s", strerror(errno));
    exit(1);
  }

  /* set SO_REUSEADDR socket option to allow rapid restart of server proc with*/
  /* call to bind on the same port. Makes the OS ignore any previous socket on*/
  /* the same port that is in TIME_WAIT state (dying). */
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int))) {
    _nhttp_panicf("could not set SO_REUSEADDR: %s", strerror(errno));
    exit(1);
  }

  serv_info.sin_family      = AF_INET;
  serv_info.sin_addr.s_addr = ntohl(INADDR_ANY);
  serv_info.sin_port        = htons((uint16_t)port);

  if (bind(sockfd, (struct sockaddr *)&serv_info, sizeof(struct sockaddr_in)) !=
      0) {
    _nhttp_panicf("bind on port %d failed: %s", port, strerror(errno));
    exit(1);
  }

  if (listen(sockfd, 128) != 0) {
    _nhttp_panicf("listen failed: %s", strerror(errno));
    exit(1);
  }

  printf("server listening!\n");

  while (1) {
    connfd =
        accept(sockfd, NULL, 0); /* TODO(sbrki): get IP and set it to req.IP */
    if (connfd < 0) {
      printf("accept failed: %s\n", strerror(errno));
      continue;
    }
    _nhttp_server_dispatch(s, connfd);
    printf("dispatch returned\n");
  }
}

enum _nhttp_req_type _nhttp_server_parse_method(const char *method) {
  if (!strcmp(method, "GET")) {
    return GET;
  } else if (!strcmp(method, "HEAD")) {
    return HEAD;
  } else if (!strcmp(method, "POST")) {
    return POST;
  } else if (!strcmp(method, "PUT")) {
    return PUT;
  } else if (!strcmp(method, "DELETE")) {
    return DELETE;
  }
  return X_UNKNOWN;
}

static void _nhttp_server_dispatch(struct nhttp_server *s, int connfd) {
  char                             request_line[NHTTP_SERVER_LINE_SIZE] = {0};
  char                             method[NHTTP_SERVER_LINE_SIZE]       = {0};
  char                             path[NHTTP_SERVER_LINE_SIZE]         = {0};
  char                             proto[NHTTP_SERVER_LINE_SIZE]        = {0};
  char                             query_params[NHTTP_SERVER_LINE_SIZE] = {0};
  char                            *pp                                   = path;
  struct _nhttp_buf_reader        *bufr;
  enum _nhttp_req_type             method_enum;
  struct _nhttp_route_match_result rmr;
  struct nhttp_ctx                *ctx;

  bufr = _nhttp_util_buf_reader_create(connfd);
  if (_nhttp_util_buf_read_until_crlf(bufr, request_line,
                                      NHTTP_SERVER_LINE_SIZE - 1)) {
    _nhttp_server_send_status_line(connfd, 413);
    close(connfd);
    _nhttp_util_buf_reader_free(bufr);
    return;
  }
  sscanf(request_line, "%s %s %s", method, path, proto);
  printf("<%s> <%s> <%s>\n", method, path, proto);

  /* match path against the router */
  _nhttp_util_cut_path_query_params(query_params, path);
  _nhttp_util_remove_trailing_slash(path);
  _nhttp_util_remove_leading_slash(path);

  method_enum = _nhttp_server_parse_method(method);
  if (method_enum == X_UNKNOWN) {
    _nhttp_server_send_status_line(connfd, 400);
    close(connfd);
    _nhttp_util_buf_reader_free(bufr);
    return;
  }
  rmr = _nhttp_route_match(s->router_root, &pp, method_enum, NULL);

  if (rmr.found == -2) {
    _nhttp_server_send_status_line(connfd, 404);
    close(connfd);
    _nhttp_util_buf_reader_free(bufr);
    return;
  } else if (rmr.found == -1) {
    _nhttp_server_send_status_line(connfd, 405);
    close(connfd);
    _nhttp_util_buf_reader_free(bufr);
    return;
  }

  /* prepare context */
  ctx              = malloc(sizeof(struct nhttp_ctx));
  ctx->connfd      = connfd;
  ctx->bufr        = bufr;
  ctx->path_params = rmr.vars;
  if ((ctx->req_headers = _nhttp_map_create_from_http_headers(bufr)) == NULL) {
    /* TODO(sbrki): consider checking if we should return 413 */
    _nhttp_map_free(ctx->path_params);
    _nhttp_map_free(ctx->resp_headers);
    _nhttp_util_buf_reader_free(bufr);
    free(ctx);
    _nhttp_server_send_status_line(connfd, 400);
    close(connfd);
    return;
  }
  if (!(ctx->query_params = _nhttp_map_create_from_urlencoded(query_params))) {
    _nhttp_map_free(ctx->path_params);
    _nhttp_map_free(ctx->resp_headers);
    _nhttp_map_free(ctx->req_headers);
    _nhttp_util_buf_reader_free(bufr);
    free(ctx);
    _nhttp_server_send_status_line(connfd, 400);
    close(connfd);
    return;
  }

  ctx->resp_headers = _nhttp_map_create();

  /* execute handler */
  rmr.handler(ctx);

  /* cleanup */
  _nhttp_map_free(ctx->path_params);
  _nhttp_map_free(ctx->req_headers);
  _nhttp_map_free(ctx->resp_headers);
  _nhttp_map_free(ctx->query_params);
  _nhttp_util_buf_reader_free(bufr);
  free(ctx);
  close(connfd);
  return;
}

static void _nhttp_server_assert_path_len(const char *path) {
  if (strlen(path) + 1 > NHTTP_SERVER_LINE_SIZE) {
    _nhttp_panicf("passed router path <%s> can't fit in NHTTP_SERVER_LINE_SIZE",
                  path);
  }
}

void nhttp_on_get(struct nhttp_server *s, const char *path,
                  nhttp_handler_func handler) {
  char  processed_path[NHTTP_SERVER_LINE_SIZE] = {0};
  char *pp                                     = processed_path;
  _nhttp_server_assert_path_len(path);
  strcpy(pp, path);
  _nhttp_util_remove_leading_slash(pp);
  _nhttp_util_remove_trailing_slash(pp);
  _nhttp_route_register(s->router_root, &pp, GET, handler);
}

/* delivery */

static int _nhttp_send_generic(const struct nhttp_ctx *ctx,
                               const unsigned char *data, size_t count,
                               const char *ctype, int status_code) {
  char clen_buf[32] = {0};
  _nhttp_server_send_status_line(ctx->connfd, status_code);
  if (!_nhttp_map_get(ctx->resp_headers, "Content-Length")) {
    sprintf(clen_buf, "%lu", count);
    _nhttp_map_set(ctx->resp_headers, "Content-Length", clen_buf);
  }
  if (!_nhttp_map_get(ctx->resp_headers, "Content-Type")) {
    _nhttp_map_set(ctx->resp_headers, "Content-Type", ctype);
  }
  _nhttp_map_write_as_http_header(ctx->resp_headers, ctx->connfd);
  _nhttp_util_write_all(ctx->connfd, "\r\n", 2);
  _nhttp_util_write_all(ctx->connfd, data, count);
  return 0;
}

int nhttp_send_string(const struct nhttp_ctx *ctx, const char *str,
                      int status_code) {
  return _nhttp_send_generic(ctx, (unsigned char *)str, strlen(str),
                             "text/plain", status_code);
}

int nhttp_send_html(const struct nhttp_ctx *ctx, const char *html,
                    int status_code) {
  return _nhttp_send_generic(ctx, (unsigned char *)html, strlen(html),
                             "text/html", status_code);
}

int nhttp_send_blob(const struct nhttp_ctx *ctx, const unsigned char *data,
                    int count, const char *ctype, int status_code) {
  return _nhttp_send_generic(ctx, data, (unsigned long)count, ctype,
                             status_code);
}

static int _nhttp_send_byte_range(const struct nhttp_ctx *ctx, const char *path,
                                  long range_start, long range_end,
                                  size_t filelen) {
  char buf[128] = {0};
  int  filefd;

  if ((filefd = open(path, O_RDONLY)) == -1) {
    _nhttp_server_send_status_line(ctx->connfd, 500);
    return 0;
  }

  sprintf(buf, "%ld", range_end - range_start + 1);
  _nhttp_map_set(ctx->resp_headers, "Content-Length", buf);

  sprintf(buf, "bytes %ld-%ld/%ld", range_start, range_end, filelen);
  _nhttp_map_set(ctx->resp_headers, "Content-Range", buf);

  _nhttp_server_send_status_line(ctx->connfd, 206);
  _nhttp_map_write_as_http_header(ctx->resp_headers, ctx->connfd);
  _nhttp_util_write_all(ctx->connfd, "\r\n", 2);
  _nhttp_util_sendfile_all(ctx->connfd, filefd, range_start,
                           (size_t)(range_end - range_start + 1));
  close(filefd);
  return 0;
}

static int _nhttp_send_file(const struct nhttp_ctx *ctx, const char *path,
                            size_t filelen) {
  char buf[128] = {0};
  int  filefd;

  if ((filefd = open(path, O_RDONLY)) == -1) {
    _nhttp_server_send_status_line(ctx->connfd, 500);
    return 0;
  }

  sprintf(buf, "%ld", filelen);
  _nhttp_map_set(ctx->resp_headers, "Content-Length", buf);

  _nhttp_server_send_status_line(ctx->connfd, 200);
  _nhttp_map_write_as_http_header(ctx->resp_headers, ctx->connfd);
  _nhttp_util_write_all(ctx->connfd, "\r\n", 2);
  _nhttp_util_sendfile_all(ctx->connfd, filefd, 0, filelen);
  close(filefd);
  return 0;
}

/* TODO(sbrki): parse mime from extension */
int nhttp_send_file(const struct nhttp_ctx *ctx, const char *path) {
  ssize_t     len         = _nhttp_util_file_size(path);
  long        range_start = 0, range_end = 0;
  int         matched;
  const char *r;

  if (len == -1) {
    _nhttp_server_send_status_line(ctx->connfd, 500);
    return 0;
  }

  if ((r = _nhttp_map_get(ctx->req_headers, "Range"))) {
    /* TODO(sbrki): support multiple byte ranges */
    /* this implementation only looks at the first byte range if there are */
    /* multiple present. */
    matched = sscanf(r, "bytes=%ld-%ld", &range_start, &range_end);
    if (matched == 1) { /* Range: <int>-  */
      range_end = len - 1;
    }
    if (range_start >= range_end || range_start >= len) {
      _nhttp_server_send_status_line(ctx->connfd, 416);
      return 0;
    }
    if (range_end >= len) {
      range_end = len - 1;
    }
    return _nhttp_send_byte_range(ctx, path, range_start, range_end,
                                  (size_t)len);
  }
  return _nhttp_send_file(ctx, path, (size_t)len);
}

/* misc. delivery */

int nhttp_redirect(const struct nhttp_ctx *ctx, const char *to, int permanent) {
  _nhttp_map_set(ctx->resp_headers, "Content-Length", "0");
  _nhttp_map_set(ctx->resp_headers, "Location", to);
  if (permanent) {
    _nhttp_server_send_status_line(ctx->connfd, 301);
  } else {
    _nhttp_server_send_status_line(ctx->connfd, 302);
  }
  _nhttp_map_write_as_http_header(ctx->resp_headers, ctx->connfd);
  return 0;
}

static void _nhttp_server_send_status_line(int connfd, int status_code) {
  /* TODO(sbrki): finish this */
  char *str;
  char  buf[64];
  switch (status_code) {
  case 200:
    str = "HTTP/1.0 200 OK\r\n";
    _nhttp_util_write_all(connfd, str, strlen(str));
    break;
  case 201:
    str = "HTTP/1.0 201 Created\r\n";
    _nhttp_util_write_all(connfd, str, strlen(str));
    break;
  case 202:
    str = "HTTP/1.0 202 Accepted\r\n";
    _nhttp_util_write_all(connfd, str, strlen(str));
    break;
  case 204:
    str = "HTTP/1.0 204 No Content\r\n";
    _nhttp_util_write_all(connfd, str, strlen(str));
    break;
  case 206:
    str = "HTTP/1.0 206 Partial Content\r\n";
    _nhttp_util_write_all(connfd, str, strlen(str));
    break;

  case 300:
    str = "HTTP/1.0 300 Multiple Choices\r\n";
    _nhttp_util_write_all(connfd, str, strlen(str));
    break;
  case 301:
    str = "HTTP/1.0 301 Moved Permanently\r\n";
    _nhttp_util_write_all(connfd, str, strlen(str));
    break;
  case 302:
    str = "HTTP/1.0 302 Moved Temporarily\r\n";
    _nhttp_util_write_all(connfd, str, strlen(str));
    break;
  case 304:
    str = "HTTP/1.0 304 Not Modified\r\n";
    _nhttp_util_write_all(connfd, str, strlen(str));
    break;

  case 400:
    str = "HTTP/1.0 400 Bad Request\r\n";
    _nhttp_util_write_all(connfd, str, strlen(str));
    break;
  case 401:
    str = "HTTP/1.401 Unauthorized\r\n";
    _nhttp_util_write_all(connfd, str, strlen(str));
    break;
  case 403:
    str = "HTTP/1.403 Forbidden\r\n";
    _nhttp_util_write_all(connfd, str, strlen(str));
    break;
  case 404:
    str = "HTTP/1.0 404 Not Found\r\n",
    _nhttp_util_write_all(connfd, str, strlen(str));
    break;
  case 405:
    str = "HTTP/1.0 405 method not allowed\r\n";
    _nhttp_util_write_all(connfd, str, strlen(str));
    break;
  case 413:
    str = "HTTP/1.0 413 Request Entity Too Large\r\n";
    _nhttp_util_write_all(connfd, str, strlen(str));
    break;
  case 416:
    str = "HTTP/1.0 416 Range Not Satisfiable\r\n";
    _nhttp_util_write_all(connfd, str, strlen(str));
    break;

  case 500:
    str = "HTTP/1.0 500 Internal Server Error\r\n";
    _nhttp_util_write_all(connfd, str, strlen(str));
    break;
  case 501:
    str = "HTTP/1.0 501 Not Implemented\r\n";
    _nhttp_util_write_all(connfd, str, strlen(str));
    break;
  case 502:
    str = "HTTP/1.0 502 Bad Gateway\r\n";
    _nhttp_util_write_all(connfd, str, strlen(str));
    break;
  case 503:
    str = "HTTP/1.0 503 Service Unavailable\r\n";
    _nhttp_util_write_all(connfd, str, strlen(str));
    break;
  default:
    sprintf(buf, "HTTP/1.0 %d\r\n", status_code);
    _nhttp_util_write_all(connfd, buf, strlen(buf));
    break;
  }
}

/* header manipulation */

const char *nhttp_get_request_header(const struct nhttp_ctx *ctx,
                                     const char             *key) {
  return _nhttp_map_get(ctx->req_headers, key);
}

void nhttp_set_response_header(const struct nhttp_ctx *ctx, const char *key,
                               const char *value) {
  _nhttp_map_set(ctx->resp_headers, key, value);
}

/* path parameters */

const char *nhttp_get_path_param(const struct nhttp_ctx *ctx,
                                 const char             *name) {
  return _nhttp_map_get(ctx->path_params, name);
}

/* query parameters */

const char *nhttp_get_query_param(const struct nhttp_ctx *ctx,
                                  const char             *name) {
  return _nhttp_map_get(ctx->query_params, name);
}
