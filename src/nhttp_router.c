#include "nhttp_router.h"
#include "nhttp_handler.h"
#include "nhttp_req_type.h"
#include "nhttp_server.h" /* NHTTP_SERVER_LINE_SIZE, TODO: fix this circ dep */
#include "nhttp_util.h"
#include <stdarg.h> /* uint32_t, */
#include <stdlib.h> /* malloc, */

struct _nhttp_route_node *_nhttp_route_node_create(const char *name) {
  struct _nhttp_route_node *node;

  if (strlen(name) > NHTTP_ROUTER_NAME_SIZE - 1) {
    _nhttp_panic("passed route name is larger than NHTTP_ROUTER_NAME_SIZE");
  }

  node = malloc(sizeof(struct _nhttp_route_node));
  memset(node, 0, sizeof(struct _nhttp_route_node));
  strcpy(node->name, name);
  return node;
}

void _nhttp_route_node_free(struct _nhttp_route_node *node) {
  uint32_t i;

  for (i = 0; i < NHTTP_ROUTER_MAX_CHILDREN && node->static_children[i] != NULL;
       i++) {
    _nhttp_route_node_free(node->static_children[i]);
  }

  if (node->var_child != NULL) {
    _nhttp_route_node_free(node->var_child);
  }

  free(node);
}

void _nhttp_route_register(struct _nhttp_route_node *root, char **path,
                           enum _nhttp_req_type rt,
                           nhttp_handler_func   handler) {
  uint32_t                  i;
  char                     *next_path_element;
  char                      var_name[NHTTP_ROUTER_NAME_SIZE] = {0};
  struct _nhttp_route_node *next_node                        = NULL;

  next_path_element = strsep(path, "/");

  /* break condition: */
  /* if there are no more next path elements, we are at the node where */
  /* the passed handler function must be registered. */
  if (next_path_element == NULL) {
    switch (rt) {
    case GET:
      root->get_handler = handler;
      break;
    case HEAD:
      root->head_handler = handler;
      break;
    case POST:
      root->post_handler = handler;
      break;
    case PUT:
      root->put_handler = handler;
      break;
    case DELETE:
      root->delete_handler = handler;
      break;
    default:
      _nhttp_panic("HTTP method not recognized while registering route");
    }
    root->handler_count++;
    return;
  }

  /* edge case: root node */
  /* recurse with the same arguments, next call to strsep will return NULL*/
  if (strlen(next_path_element) == 0) {
    _nhttp_route_register(root, path, rt, handler);
    return;
  }

  /* next path element for a var child? */
  if (strlen(next_path_element) >= 3 && next_path_element[0] == '{' &&
      next_path_element[strlen(next_path_element) - 1] == '}') {
    /* 1: parse var name */
    if (strlen(next_path_element) - 2 > NHTTP_ROUTER_NAME_SIZE - 1) {
      _nhttp_panic("Var path element can't fit in NHTTP_ROUTER_NAME_SIZE");
    }
    memcpy(var_name, next_path_element + 1, strlen(next_path_element + 1) - 1);
    /* 2: create and fill node if it doesn't exist */
    if (root->var_child) {
      if (strcmp(root->var_child->name, var_name) != 0) {
        _nhttp_panic("Var path element mismatch");
      }
    } else {
      root->var_child = _nhttp_route_node_create(var_name);
    }
    /* 3: recurse into it */
    _nhttp_route_register(root->var_child, path, rt, handler);
    return;
  } else { /* next path element is for a static child */
    /* find if the static child already exists */
    for (i = 0;
         i < NHTTP_ROUTER_MAX_CHILDREN && root->static_children[i] != NULL;
         i++) {
      if (strcmp(next_path_element, root->static_children[i]->name) == 0) {
        next_node = root->static_children[i];
        break;
      }
    } /* for */

    /* if it doesn't exist, create it */
    if (!next_node) {
      next_node = _nhttp_route_node_create(next_path_element);
      for (i = 0;
           i < NHTTP_ROUTER_MAX_CHILDREN && root->static_children[i] != NULL;
           i++) {
      }
      if (i == NHTTP_ROUTER_MAX_CHILDREN) {
        _nhttp_panic("Exceeded NHTTP_ROUTER_MAX_CHILDREN");
      }
      root->static_children[i] = next_node;
    }
    /* 3: recurse into it */
    _nhttp_route_register(next_node, path, rt, handler);
    return;
  }
}

struct _nhttp_route_match_result
_nhttp_route_match(struct _nhttp_route_node *node, char **path,
                   enum _nhttp_req_type rt, struct _nhttp_map *vars) {
  uint32_t                         i;
  struct _nhttp_route_match_result res;
  char                            *next_path_element, *unesc_next_path_element;
  char                             buf[NHTTP_SERVER_LINE_SIZE + 1];

  if (vars == NULL) {
    vars = _nhttp_map_create();
  }

  /* strsep() semantics and edge cases with "/" as delimiter: */
  /* "/foo/bar" -> ["", "foo", "bar"] */
  /* "/" -> ["", ""] */
  /* "" -> [""] (and sets stringp to NULL)*/
  /* "foo///bar" -> ["foo", "", "", "bar"] */
  next_path_element = strsep(path, "/");

  /* if strsep returned empty string "", it means that it also got */
  /* an empty string as an input -> it is the root path ("GET / HTTP/1.0") */
  /* TODO(sbrki): add utility for removing consecutive slashes in the path */
  /*              as they would cause strsep to return "" for non-root paths */
  if (next_path_element != NULL && strlen(next_path_element) > 0) {

    /* unescape path */
    strcpy(buf, next_path_element);
    if (_nhttp_util_str_triplets_validate(buf)) {
      res.found   = -2; /* -> 404 */
      res.handler = NULL;
      res.vars    = NULL;
      _nhttp_map_free(vars);
      return res;
    }
    _nhttp_util_str_triplets_to_upper(buf);
    unesc_next_path_element = _nhttp_util_str_unescape(buf);

    /* iterate through static_children first */
    for (i = 0;
         i < NHTTP_ROUTER_MAX_CHILDREN && node->static_children[i] != NULL;
         i++) {
      if (!strcmp(unesc_next_path_element, node->static_children[i]->name)) {
        free(unesc_next_path_element);
        return _nhttp_route_match(node->static_children[i], path, rt, vars);
      }
    }

    /* if none of the static_children matched, continue with dynamic */
    /* child, if present */
    if (node->var_child) {
      _nhttp_map_set(vars, node->var_child->name, unesc_next_path_element);
      free(unesc_next_path_element);
      return _nhttp_route_match(node->var_child, path, rt, vars);
    }

    /* at this point we know there is a next element in the incoming */
    /* path, but it could not be matched */
    res.found   = -2; /* -> 404 */
    res.handler = NULL;
    _nhttp_map_free(vars);
    free(unesc_next_path_element);
    return res;
  } else {
    /* at this point there is no next element in the incoming path, */
    /* so return the handler for the current node (and rt matches) or 404 if */
    /* handler is not registered */
    nhttp_handler_func handler;
    switch (rt) {
    case GET:
      handler = node->get_handler;
      break;
    case HEAD:
      handler = node->head_handler;
      break;
    case POST:
      handler = node->post_handler;
      break;
    case PUT:
      handler = node->put_handler;
      break;
    case DELETE:
      handler = node->delete_handler;
      break;
    default:
      handler = NULL;
    }

    if (handler != NULL) {
      res.found   = 0;
      res.handler = handler;
      res.vars    = vars;
      return res;
    }

    /* route matched but handler for requested method is not registered -> 405*/
    if (node->handler_count) {
      res.found   = -1;
      res.handler = NULL;
      _nhttp_map_free(vars);
      res.vars = NULL;
      return res;
    }

    /* route matched but it has zero registered handlers -> 404 */
    res.found   = -2;
    res.handler = NULL;
    _nhttp_map_free(vars);
    return res;
  } /* else */
}
