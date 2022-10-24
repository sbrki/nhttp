#ifndef NHTTP_HANDLER_H
#define NHTTP_HANDLER_H

#include "nhttp_ctx.h"

typedef int (*nhttp_handler_func)(const struct nhttp_ctx *);

#endif /* NHTTP_HANDLER_H */
