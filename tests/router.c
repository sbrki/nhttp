// clang-format off
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <cmocka.h>

#include "../src/nhttp_router.h"
#include "../src/nhttp_util.h"
// clang-format on

void __wrap_free(void *ptr) {
  check_expected(ptr);
  return;
}

static void test_nhttp_router_node_create_and_free(void **state) {
  struct _nhttp_route_node *root = _nhttp_route_node_create("root");
  struct _nhttp_route_node *root_a = _nhttp_route_node_create("root_a");
  struct _nhttp_route_node *root_a_b = _nhttp_route_node_create("root_a_b");
  struct _nhttp_route_node *root_a_c = _nhttp_route_node_create("root_a_c");
  struct _nhttp_route_node *root_a_d = _nhttp_route_node_create("root_a_d");
  struct _nhttp_route_node *root_e = _nhttp_route_node_create("root_e");

  /* check if _nhttp_route_node_create() works as expected */
  /* check just on root node, creation of others behaves the same */
  uint32_t i;
  for (i = 0; i < NHTTP_ROUTER_MAX_CHILDREN; i++) {
    assert_ptr_equal(root->static_children[i], NULL);
  }
  assert_ptr_equal(root->var_child, NULL);
  assert_string_equal(root->name, "root");
  assert_int_equal(root->handler_count, 0);
  assert_ptr_equal(root->get_handler, NULL);
  assert_ptr_equal(root->head_handler, NULL);
  assert_ptr_equal(root->post_handler, NULL);
  assert_ptr_equal(root->put_handler, NULL);
  assert_ptr_equal(root->delete_handler, NULL);

  /* check if _nhttp_route_node_free() works as expected */
  root->static_children[0] = root_a;
  root->static_children[1] = root_e;
  root_a->static_children[0] = root_a_b;
  root_a->static_children[1] = root_a_c;
  root_a->var_child = root_a_d;

  expect_value(__wrap_free, ptr, root_a_b);
  expect_value(__wrap_free, ptr, root_a_c);
  expect_value(__wrap_free, ptr, root_a_d);
  expect_value(__wrap_free, ptr, root_a);

  expect_value(__wrap_free, ptr, root_e);
  expect_value(__wrap_free, ptr, root);

  _nhttp_route_node_free(root);
}

/* TODO(sbrki): add more strict tests, check that none of the handlers */
/* that shouldn't be registered are NULL */
static void test_nhttp_route_register(void **state) {
  {
    struct _nhttp_route_node *root = _nhttp_route_node_create("");
    nhttp_handler_func handler = (nhttp_handler_func)0x12345678;
    char path[] = "/";
    char *p = path;
    _nhttp_util_remove_trailing_slash(p);
    _nhttp_util_remove_leading_slash(p);
    _nhttp_route_register(root, &p, GET, handler);

    assert_string_equal(root->name, "");
    uint32_t i;
    for (i = 0; i < NHTTP_ROUTER_MAX_CHILDREN; i++) {
      assert_ptr_equal(root->static_children[i], NULL);
    }
    assert_int_equal(root->handler_count, 1);
    assert_ptr_equal(root->var_child, NULL);
    assert_ptr_equal(root->get_handler, handler);
  }
  {
    struct _nhttp_route_node *root = _nhttp_route_node_create("");
    nhttp_handler_func handler = (nhttp_handler_func)0x1;
    nhttp_handler_func handler2 = (nhttp_handler_func)0x2;
    nhttp_handler_func handler3 = (nhttp_handler_func)0x3;
    char path1[] = "/a/";
    char *p1 = path1;
    _nhttp_util_remove_trailing_slash(p1);
    _nhttp_util_remove_leading_slash(p1);
    _nhttp_route_register(root, &p1, GET, handler);

    char path2[] = "/a/";
    char *p2 = path2;
    _nhttp_util_remove_trailing_slash(p2);
    _nhttp_util_remove_leading_slash(p2);
    _nhttp_route_register(root, &p2, PUT, handler2);

    char path3[] = "/b/";
    char *p3 = path3;
    _nhttp_util_remove_trailing_slash(p3);
    _nhttp_util_remove_leading_slash(p3);
    _nhttp_route_register(root, &p3, POST, handler3);

    assert_string_equal(root->name, "");
    assert_int_equal(root->handler_count, 0);
    assert_ptr_not_equal(root->static_children[0], NULL);
    assert_ptr_not_equal(root->static_children[1], NULL);
    assert_ptr_equal(root->static_children[2], NULL);

    assert_string_equal(root->static_children[0]->name, "a");
    assert_int_equal(root->static_children[0]->handler_count, 2);
    assert_ptr_equal(root->static_children[0]->get_handler, handler);
    assert_ptr_equal(root->static_children[0]->put_handler, handler2);

    assert_string_equal(root->static_children[1]->name, "b");
    assert_int_equal(root->static_children[1]->handler_count, 1);
    assert_ptr_equal(root->static_children[1]->post_handler, handler3);
  }
  {
    struct _nhttp_route_node *root = _nhttp_route_node_create("");
    nhttp_handler_func handler = (nhttp_handler_func)0x1;
    nhttp_handler_func handler2 = (nhttp_handler_func)0x2;
    char path1[] = "/a/b/c";
    char *p1 = path1;
    _nhttp_util_remove_trailing_slash(p1);
    _nhttp_util_remove_leading_slash(p1);
    _nhttp_route_register(root, &p1, GET, handler);

    char path2[] = "/a/b/d";
    char *p2 = path2;
    _nhttp_util_remove_trailing_slash(p2);
    _nhttp_util_remove_leading_slash(p2);
    _nhttp_route_register(root, &p2, GET, handler2);

    assert_string_equal(root->name, "");
    assert_int_equal(root->handler_count, 0);
    assert_ptr_not_equal(root->static_children[0], NULL);
    assert_ptr_equal(root->static_children[1], NULL);
    assert_ptr_equal(root->var_child, NULL);

    assert_string_equal(root->static_children[0]->name, "a");
    assert_int_equal(root->static_children[0]->handler_count, 0);
    assert_ptr_not_equal(root->static_children[0]->static_children[0], NULL);
    assert_ptr_equal(root->static_children[0]->var_child, NULL);

    assert_string_equal(root->static_children[0]->static_children[0]->name,
                        "b");

    assert_int_equal(
        root->static_children[0]->static_children[0]->handler_count, 0);
    assert_ptr_not_equal(
        root->static_children[0]->static_children[0]->static_children[0], NULL);
    assert_ptr_equal(root->static_children[0]->static_children[0]->var_child,
                     NULL);

    assert_string_equal(
        root->static_children[0]->static_children[0]->static_children[0]->name,
        "c");
    assert_int_equal(root->static_children[0]
                         ->static_children[0]
                         ->static_children[0]
                         ->handler_count,
                     1);
    assert_ptr_equal(root->static_children[0]
                         ->static_children[0]
                         ->static_children[0]
                         ->static_children[0],
                     NULL);
    assert_ptr_equal(root->static_children[0]
                         ->static_children[0]
                         ->static_children[0]
                         ->var_child,
                     NULL);
    assert_ptr_equal(root->static_children[0]
                         ->static_children[0]
                         ->static_children[0]
                         ->get_handler,
                     handler);

    assert_string_equal(
        root->static_children[0]->static_children[0]->static_children[1]->name,
        "d");
    assert_int_equal(root->static_children[0]
                         ->static_children[0]
                         ->static_children[1]
                         ->handler_count,
                     1);
    assert_ptr_equal(root->static_children[0]
                         ->static_children[0]
                         ->static_children[1]
                         ->static_children[0],
                     NULL);
    assert_ptr_equal(root->static_children[0]
                         ->static_children[0]
                         ->static_children[1]
                         ->var_child,
                     NULL);
    assert_ptr_equal(root->static_children[0]
                         ->static_children[0]
                         ->static_children[1]
                         ->get_handler,
                     handler2);
  }
  /* var node tests */
  {
    struct _nhttp_route_node *root = _nhttp_route_node_create("");
    nhttp_handler_func handler = (nhttp_handler_func)0x1;
    nhttp_handler_func handler2 = (nhttp_handler_func)0x2;
    char path1[] = "/{foo}/";
    char *p1 = path1;
    _nhttp_util_remove_trailing_slash(p1);
    _nhttp_util_remove_leading_slash(p1);
    _nhttp_route_register(root, &p1, GET, handler);

    char path2[] = "/{foo}/{bar}/";
    char *p2 = path2;
    _nhttp_util_remove_trailing_slash(p2);
    _nhttp_util_remove_leading_slash(p2);
    _nhttp_route_register(root, &p2, PUT, handler2);

    assert_string_equal(root->name, "");
    assert_int_equal(root->handler_count, 0);
    assert_ptr_not_equal(root->var_child, NULL);
    assert_ptr_equal(root->static_children[0], NULL);

    assert_string_equal(root->var_child->name, "foo");
    assert_ptr_equal(root->var_child->static_children[0], NULL);
    assert_ptr_not_equal(root->var_child->var_child, NULL);
    assert_int_equal(root->var_child->handler_count, 1);
    assert_ptr_equal(root->var_child->get_handler, handler);

    assert_string_equal(root->var_child->var_child->name, "bar");
    assert_ptr_equal(root->var_child->var_child->static_children[0], NULL);
    assert_ptr_equal(root->var_child->var_child->var_child, NULL);
    assert_int_equal(root->var_child->handler_count, 1);
    assert_ptr_equal(root->var_child->var_child->put_handler, handler2);
  }
  {
    struct _nhttp_route_node *root = _nhttp_route_node_create("");
    nhttp_handler_func handler = (nhttp_handler_func)0x1;
    nhttp_handler_func handler2 = (nhttp_handler_func)0x2;
    char path1[] = "/{foo}/";
    char *p1 = path1;
    _nhttp_util_remove_trailing_slash(p1);
    _nhttp_util_remove_leading_slash(p1);
    _nhttp_route_register(root, &p1, GET, handler);

    char path2[] = "/{foo}/a/{bar}/";
    char *p2 = path2;
    _nhttp_util_remove_trailing_slash(p2);
    _nhttp_util_remove_leading_slash(p2);
    _nhttp_route_register(root, &p2, PUT, handler2);

    assert_string_equal(root->name, "");
    assert_int_equal(root->handler_count, 0);
    assert_ptr_not_equal(root->var_child, NULL);
    assert_ptr_equal(root->static_children[0], NULL);

    assert_string_equal(root->var_child->name, "foo");
    assert_ptr_not_equal(root->var_child->static_children[0], NULL);
    assert_ptr_equal(root->var_child->var_child, NULL);
    assert_int_equal(root->var_child->handler_count, 1);
    assert_ptr_equal(root->var_child->get_handler, handler);

    assert_string_equal(root->var_child->static_children[0]->name, "a");
    assert_ptr_equal(root->var_child->static_children[0]->static_children[0],
                     NULL);
    assert_ptr_not_equal(root->var_child->static_children[0]->var_child, NULL);
    assert_int_equal(root->var_child->static_children[0]->handler_count, 0);

    assert_string_equal(root->var_child->static_children[0]->var_child->name,
                        "bar");
    assert_ptr_equal(
        root->var_child->static_children[0]->var_child->static_children[0],
        NULL);
    assert_ptr_equal(root->var_child->static_children[0]->var_child->var_child,
                     NULL);
    assert_int_equal(
        root->var_child->static_children[0]->var_child->handler_count, 1);
  }
}

static void test_nhttp_route_match(void **state) {
  {
    expect_any_always(__wrap_free, ptr);
    struct _nhttp_route_node *root = _nhttp_route_node_create("");
    nhttp_handler_func handler = (nhttp_handler_func)0x1;
    nhttp_handler_func handler2 = (nhttp_handler_func)0x2;
    char path1[] = "/a/b/c";
    char *p1 = path1;
    _nhttp_util_remove_trailing_slash(p1);
    _nhttp_util_remove_leading_slash(p1);
    _nhttp_route_register(root, &p1, GET, handler);

    char path2[] = "/a/b";
    char *p2 = path2;
    _nhttp_util_remove_trailing_slash(p2);
    _nhttp_util_remove_leading_slash(p2);
    _nhttp_route_register(root, &p2, GET, handler2);

    char incoming[] = "/a/b/c";
    char *inc = incoming;
    _nhttp_util_remove_trailing_slash(inc);
    _nhttp_util_remove_leading_slash(inc);

    struct _nhttp_route_match_result res;
    res = _nhttp_route_match(root, &inc, GET, NULL);

    assert_int_equal(res.found, 0);
    assert_ptr_equal(res.handler, handler);
    assert_ptr_not_equal(res.vars, NULL);

    char incoming2[] = "/a/b";
    char *inc2 = incoming2;
    _nhttp_util_remove_trailing_slash(inc2);
    _nhttp_util_remove_leading_slash(inc2);
    res = _nhttp_route_match(root, &inc2, GET, NULL);

    assert_int_equal(res.found, 0);
    assert_ptr_equal(res.handler, handler2);
    assert_ptr_not_equal(res.vars, NULL);

    char incoming3[] = "/";
    char *inc3 = incoming3;
    _nhttp_util_remove_trailing_slash(inc3);
    _nhttp_util_remove_leading_slash(inc3);
    res = _nhttp_route_match(root, &inc3, GET, NULL);

    assert_int_equal(res.found, -2);
    assert_ptr_equal(res.handler, NULL);
    assert_ptr_equal(res.vars, NULL);

    char incoming4[] = "/a/b";
    char *inc4 = incoming4;
    _nhttp_util_remove_trailing_slash(inc4);
    _nhttp_util_remove_leading_slash(inc4);
    res = _nhttp_route_match(root, &inc4, POST, NULL);

    assert_int_equal(res.found, -1);
    assert_ptr_equal(res.handler, NULL);
    assert_ptr_equal(res.vars, NULL);
  }
  /* check if path vars are parsed correctly */
  {
    struct _nhttp_route_node *root = _nhttp_route_node_create("");
    nhttp_handler_func handler = (nhttp_handler_func)0x1;
    char path1[] = "/{foo}/baz/{bar}";
    char *p1 = path1;
    _nhttp_util_remove_trailing_slash(p1);
    _nhttp_util_remove_leading_slash(p1);
    _nhttp_route_register(root, &p1, GET, handler);

    char incoming[] = "/a/baz/b";
    char *inc = incoming;
    _nhttp_util_remove_trailing_slash(inc);
    _nhttp_util_remove_leading_slash(inc);

    struct _nhttp_route_match_result res;
    res = _nhttp_route_match(root, &inc, GET, NULL);

    assert_int_equal(res.found, 0);
    assert_ptr_equal(res.handler, handler);
    assert_ptr_not_equal(res.vars, NULL);
    assert_string_equal(_nhttp_map_get(res.vars, "foo"), "a");
    assert_string_equal(_nhttp_map_get(res.vars, "bar"), "b");

    char incoming2[] = "/hello--/baz/world--";
    char *inc2 = incoming2;
    _nhttp_util_remove_trailing_slash(inc2);
    _nhttp_util_remove_leading_slash(inc2);
    res = _nhttp_route_match(root, &inc2, GET, NULL);

    assert_int_equal(res.found, 0);
    assert_ptr_equal(res.handler, handler);
    assert_ptr_not_equal(res.vars, NULL);
    assert_string_equal(_nhttp_map_get(res.vars, "foo"), "hello--");
    assert_string_equal(_nhttp_map_get(res.vars, "bar"), "world--");
  } /* TODO(sbrki): write tests for escape sequences once they are impl */
  /* check if routing prioritizes static over var child(ren) */
  {
    struct _nhttp_route_node *root = _nhttp_route_node_create("");
    nhttp_handler_func handler = (nhttp_handler_func)0x1;
    nhttp_handler_func handler2 = (nhttp_handler_func)0x2;
    char path1[] = "/a/{foo}";
    char *p1 = path1;
    _nhttp_util_remove_trailing_slash(p1);
    _nhttp_util_remove_leading_slash(p1);
    _nhttp_route_register(root, &p1, GET, handler);

    char path2[] = "/a/bar";
    char *p2 = path2;
    _nhttp_util_remove_trailing_slash(p2);
    _nhttp_util_remove_leading_slash(p2);
    _nhttp_route_register(root, &p2, GET, handler2);

    char incoming[] = "/a/bar";
    char *inc = incoming;
    _nhttp_util_remove_trailing_slash(inc);
    _nhttp_util_remove_leading_slash(inc);

    struct _nhttp_route_match_result res;
    res = _nhttp_route_match(root, &inc, GET, NULL);
    assert_int_equal(res.found, 0);
    assert_ptr_equal(res.handler, handler2);
    assert_ptr_not_equal(res.vars, NULL);

    char incoming2[] = "/a/hello";
    char *inc2 = incoming2;
    _nhttp_util_remove_trailing_slash(inc2);
    _nhttp_util_remove_leading_slash(inc2);

    res = _nhttp_route_match(root, &inc2, GET, NULL);
    assert_int_equal(res.found, 0);
    assert_ptr_equal(res.handler, handler);
    assert_ptr_not_equal(res.vars, NULL);
  }
  /* check if incoming path elements are unencoded before matching */
  {
    struct _nhttp_route_node *root = _nhttp_route_node_create("");
    nhttp_handler_func handler = (nhttp_handler_func)0x1;
    char path1[] = "/foo bar/bar baz/";
    char *p1 = path1;
    _nhttp_util_remove_trailing_slash(p1);
    _nhttp_util_remove_leading_slash(p1);
    _nhttp_route_register(root, &p1, GET, handler);

    char incoming[] = "/foo%20bar/bar%20baz/";
    char *inc = incoming;
    _nhttp_util_remove_trailing_slash(inc);
    _nhttp_util_remove_leading_slash(inc);

    struct _nhttp_route_match_result res;
    res = _nhttp_route_match(root, &inc, GET, NULL);
    assert_int_equal(res.found, 0);
    assert_ptr_equal(res.handler, handler);
    assert_ptr_not_equal(res.vars, NULL);
  }
  /* check if incoming path variables are unencoded before matching */
  {
    struct _nhttp_route_node *root = _nhttp_route_node_create("");
    nhttp_handler_func handler = (nhttp_handler_func)0x1;
    char path1[] = "/foo bar/{baz}/";
    char *p1 = path1;
    _nhttp_util_remove_trailing_slash(p1);
    _nhttp_util_remove_leading_slash(p1);
    _nhttp_route_register(root, &p1, GET, handler);

    char incoming[] = "/foo%20bar/hello%20world/";
    char *inc = incoming;
    _nhttp_util_remove_trailing_slash(inc);
    _nhttp_util_remove_leading_slash(inc);

    struct _nhttp_route_match_result res;
    res = _nhttp_route_match(root, &inc, GET, NULL);
    assert_int_equal(res.found, 0);
    assert_ptr_equal(res.handler, handler);
    assert_ptr_not_equal(res.vars, NULL);
    assert_string_equal("hello world", _nhttp_map_get(res.vars, "baz"));
  }
}

int main(void) {
  const struct CMUnitTest router_tests[] = {
      cmocka_unit_test(test_nhttp_router_node_create_and_free),
      cmocka_unit_test(test_nhttp_route_register),
      cmocka_unit_test(test_nhttp_route_match),
  };
  return cmocka_run_group_tests(router_tests, NULL, NULL);
}
