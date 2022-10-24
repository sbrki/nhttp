#ifndef NHTTP_ROUTER_H
#define NHTTP_ROUTER_H

#include "nhttp_handler.h"
#include "nhttp_map.h"
#include "nhttp_req_type.h"

#define NHTTP_ROUTER_NAME_SIZE 512
#define NHTTP_ROUTER_MAX_CHILDREN 128

/* nhttp router is a simplistic router with a couple of limitations. */
/* It is good enough for most use cases, and relatively performant (>1M op/s) */
/* on modern hardware. Every path is split into "path elements" using */
/* slash (/) as a separator. The router consists of a trie-like structure */
/* of all registered paths, where every node in the trie represents a path */
/* element. A node can be one of two kinds: */
/* 1: static (e.g. "/foo") */
/* 2: variable (e.g. "/{bar}") */
/* Path matching works by splitting the incoming path into path elements, */
/* and recursively descending down in the trie, picking a child node that */
/* corresponds to the next path element. */
/* LIMITATION 1: While searching for the next child node, static children */
/* nodes have the priority over the variable children nodes. */
/* LIMITATION 2: A node can have only one variable child, meaning that */
/* the name of the variable has to be consistent, i.e. following routes */
/* could not be registered at the same time: */
/* - "/foo/{bar}" */
/* - "/foo/{baz}" */
/* LIMITATION 3: The router doesn't implement longest-matching-route semantics*/

struct _nhttp_route_node {
  /* name is either the literal name of the path element if node is static, */
  /* or name of the variable if node is a var node. */
  char                      name[NHTTP_ROUTER_NAME_SIZE];
  struct _nhttp_route_node *static_children[NHTTP_ROUTER_MAX_CHILDREN];
  struct _nhttp_route_node *var_child; /* only one var child node is allowed */
  uint32_t handler_count; /* used to quickly decide between 404 or 405 */
  nhttp_handler_func get_handler;
  nhttp_handler_func head_handler;
  nhttp_handler_func post_handler;
  nhttp_handler_func put_handler;
  nhttp_handler_func delete_handler;
};

/* _nhttp_route_node_create mallocs the route and copies passed name. */
/* Zeroes out static and var children arrays. */
/* Panics if passed name string is larger than NHTTP_ROUTER_NAME_SIZE . */
struct _nhttp_route_node *_nhttp_route_node_create(const char *name);

/* _nhttp_route_node_free frees the passed route and all of its children. */
void _nhttp_route_node_free(struct _nhttp_route_node *r);

/* _nhttp_route_register registers the passed path. */
/* If path variables are used and there are multiple paths registered, */
/* all paths must have the same variable at the same path element, or */
/* the function panics. It modifies contents of **path, copy it before */
/* calling. Passed **path must not have a leading and trailing slash - */
/* call _nhttp_util_remove_trailing_slash and _nhttp_util_remove_leading_slash*/
/* before passing the **path . */
void _nhttp_route_register(struct _nhttp_route_node *root, char **path,
                           enum _nhttp_req_type rt, nhttp_handler_func handler);

/* _nhttp_route_match_result is the result of trying to match an incoming */
/* path against the router (i.e. route trie). */
/* Field `found` has following semantics: */
/*   -2 -> not found (404) */
/*   -1 -> method not allowed (405) */
/*    0 -> found */
/* In case of a successful match (`found`==0), `vars` ptr is set (!=NULL) */
/* and has to be freed by the caller. */
struct _nhttp_route_match_result {
  char               found;
  nhttp_handler_func handler;
  struct _nhttp_map *vars;
};

/* _nhttp_route_match tries to match the incoming path against the router. */
/* It modifies contents of **path, copy it before calling. */
/* Passed **path must not have a leading and trailing slash - */
/* call _nhttp_util_remove_trailing_slash and _nhttp_util_remove_leading_slash*/
/* before passing the **path . */
/* In case of a successful match, `vars` field of the returned */
/* `_nhttp_route_match_result` has to be freed after use. */
struct _nhttp_route_match_result
_nhttp_route_match(struct _nhttp_route_node *node, char **path,
                   enum _nhttp_req_type rt, struct _nhttp_map *vars);

#endif /* NHTTP_ROUTER_H */
