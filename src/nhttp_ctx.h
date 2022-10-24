#ifndef NHTTP_CTX_H
#define NHTTP_CTX_H

#include "nhttp_map.h"
#include "nhttp_util.h"

struct nhttp_ctx {
  /* both connfd and bufr share the same file descriptor. connfd should be */
  /* used for writing (preferably via `_nhttp_util_write_all`), and bufr */
  /* should be used for reading as it is a read-only buffer above the connfd.*/
  int                       connfd;
  struct _nhttp_buf_reader *bufr;
  struct _nhttp_map        *path_params;
  struct _nhttp_map        *query_params;
  struct _nhttp_map        *req_headers;
  struct _nhttp_map        *resp_headers;
};

#endif /* NHTTP_CTX_H */
