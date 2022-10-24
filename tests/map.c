// clang-format off
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <cmocka.h>

#include "../src/nhttp_map.h"
#include "./sockmock.h"
// clang-format on

static void test_nhttp_map_set_single_element(void **state) {
  struct _nhttp_map *map = _nhttp_map_create();
  _nhttp_map_set(map, "key", "value");
  // djb2 hash of string "key" is 193496974, and with 256 bins
  // the entry should end up in bin 142.
  for (int i = 0; i < NHTTP_MAP_BINS; i++) {
    if (i == 142) {
      assert_non_null(map->bins[i]);
    } else {
      assert_null(map->bins[i]);
    }
  }

  assert_string_equal(map->bins[142]->key, "key");
  assert_string_equal(map->bins[142]->val, "value");
  assert_null(map->bins[142]->next);
}

static void test_nhttp_map_set_colission(void **state) {
  struct _nhttp_map *map = _nhttp_map_create();
  // strings "key" and "    key" produce a collision ->
  // they both end up in bin 142.
  _nhttp_map_set(map, "key", "value");
  _nhttp_map_set(map, "    key", "value2");

  for (int i = 0; i < NHTTP_MAP_BINS; i++) {
    if (i == 142) {
      assert_non_null(map->bins[i]);
    } else {
      assert_null(map->bins[i]);
    }
  }

  assert_string_equal(map->bins[142]->key, "key");
  assert_string_equal(map->bins[142]->val, "value");
  assert_non_null(map->bins[142]->next);

  assert_string_equal(map->bins[142]->next->key, "    key");
  assert_string_equal(map->bins[142]->next->val, "value2");
  assert_null(map->bins[142]->next->next);
}

static void test_nhttp_map_set_overwrite(void **state) {
  struct _nhttp_map *map = _nhttp_map_create();
  // strings "key" and "    key" produce a collision ->
  // they both end up in bin 142.
  _nhttp_map_set(map, "key", "value");
  _nhttp_map_set(map, "    key", "value2");
  _nhttp_map_set(map, "    key", "value3");

  assert_string_equal(map->bins[142]->key, "key");
  assert_string_equal(map->bins[142]->val, "value");
  assert_non_null(map->bins[142]->next);

  assert_string_equal(map->bins[142]->next->key, "    key");
  assert_string_equal(map->bins[142]->next->val, "value3");
  assert_null(map->bins[142]->next->next);
}

static void test_nhttp_map_get(void **state) {
  struct _nhttp_map *map = _nhttp_map_create();

  assert_null(_nhttp_map_get(map, "non existent key!"));

  // strings "key" and "    key" produce a collision ->
  // they both end up in bin 142.
  _nhttp_map_set(map, "key", "value");
  _nhttp_map_set(map, "    key", "value2");

  assert_string_equal(_nhttp_map_get(map, "key"), "value");
  assert_string_equal(_nhttp_map_get(map, "    key"), "value2");
}

static void test_nhttp_map_remove(void **state) {
  struct _nhttp_map *map = _nhttp_map_create();

  // strings "key" and "    key" produce a collision ->
  // they both end up in bin 142.
  _nhttp_map_set(map, "key", "value");
  _nhttp_map_set(map, "    key", "value2");

  // removing non existent key should have no effect
  {
    _nhttp_map_remove(map, "non existent key!");

    assert_string_equal(map->bins[142]->key, "key");
    assert_string_equal(map->bins[142]->val, "value");
    assert_non_null(map->bins[142]->next);

    assert_string_equal(map->bins[142]->next->key, "    key");
    assert_string_equal(map->bins[142]->next->val, "value2");
    assert_null(map->bins[142]->next->next);
  }
  // removing head entry
  {
    _nhttp_map_remove(map, "    key");

    assert_string_equal(map->bins[142]->key, "key");
    assert_string_equal(map->bins[142]->val, "value");
    assert_null(map->bins[142]->next);

    // revert for next test case
    _nhttp_map_set(map, "    key", "value2");
  }
  // removing tail entry
  {
    _nhttp_map_remove(map, "key");

    assert_string_equal(map->bins[142]->key, "    key");
    assert_string_equal(map->bins[142]->val, "value2");
    assert_null(map->bins[142]->next);
  }
  // TODO(sbrki): add test case for removing middle entry
}

static void test_nhttp_map_create_from_http_headers(void **state) {
  /* edge case: no headers */
  {
    struct sockmock           s = sockmock_create("\r\n");
    struct _nhttp_buf_reader *br =
        _nhttp_util_buf_reader_create(s.serv_conn_fd);

    struct _nhttp_map *m = _nhttp_map_create_from_http_headers(br);
    assert_non_null(m);

    _nhttp_map_free(m);
    free(br);
    close(s.serv_conn_fd);
    sockmock_free(s, 0);
  }
  {
    struct sockmock           s = sockmock_create("Key1:Val1\r\n"
                                                            "Key2: Val2\r\n"
                                                            "key-3: val-3\r\n"
                                                            "Key4: val with spaces\r\n"
                                                            "\r\n");
    struct _nhttp_buf_reader *br =
        _nhttp_util_buf_reader_create(s.serv_conn_fd);

    struct _nhttp_map *m = _nhttp_map_create_from_http_headers(br);
    assert_non_null(m);
    assert_string_equal(_nhttp_map_get(m, "Key1"), "Val1");
    assert_string_equal(_nhttp_map_get(m, "Key2"), "Val2");
    assert_string_equal(_nhttp_map_get(m, "key-3"), "val-3");
    assert_string_equal(_nhttp_map_get(m, "Key4"), "val with spaces");

    _nhttp_map_free(m);
    free(br);
    close(s.serv_conn_fd);
    sockmock_free(s, 0);
  }
}

static void test_nhttp_map_create_from_urlencoded(void **state) {
  /* TODO(sbrki): write more tests */
  {
    char              *input = "";
    struct _nhttp_map *map   = _nhttp_map_create_from_urlencoded(input);
    assert_non_null(map);
    /* assert empty */
    for (int i = 0; i < NHTTP_MAP_BINS; i++) {
      assert_null(map->bins[i]);
    }
    _nhttp_map_free(map);
  }
  {
    char               input[] = "foo=1";
    struct _nhttp_map *map     = _nhttp_map_create_from_urlencoded(input);
    assert_non_null(map);
    assert_string_equal(_nhttp_map_get(map, "foo"), "1");
    _nhttp_map_free(map);
  }
  {
    char               input[] = "foo=1&bar=2";
    struct _nhttp_map *map     = _nhttp_map_create_from_urlencoded(input);
    assert_non_null(map);
    assert_string_equal(_nhttp_map_get(map, "foo"), "1");
    assert_string_equal(_nhttp_map_get(map, "bar"), "2");
    _nhttp_map_free(map);
  }
  {
    char               input[] = "foo=1&bar=2&baz=3";
    struct _nhttp_map *map     = _nhttp_map_create_from_urlencoded(input);
    assert_non_null(map);
    assert_string_equal(_nhttp_map_get(map, "foo"), "1");
    assert_string_equal(_nhttp_map_get(map, "bar"), "2");
    assert_string_equal(_nhttp_map_get(map, "baz"), "3");
    _nhttp_map_free(map);
  }
  {
    char               input[] = "foo%2bbar=1%202";
    struct _nhttp_map *map     = _nhttp_map_create_from_urlencoded(input);
    assert_non_null(map);
    assert_string_equal(_nhttp_map_get(map, "foo+bar"), "1 2");
    _nhttp_map_free(map);
  }
  {
    char               input[] = "foo%2Bbar=1%202";
    struct _nhttp_map *map     = _nhttp_map_create_from_urlencoded(input);
    assert_non_null(map);
    assert_string_equal(_nhttp_map_get(map, "foo+bar"), "1 2");
    _nhttp_map_free(map);
  }
  /* edge case */
  {
    char               input[] = "foo";
    struct _nhttp_map *map     = _nhttp_map_create_from_urlencoded(input);
    assert_non_null(map);
    assert_null(_nhttp_map_get(map, "foo"));
    _nhttp_map_free(map);
  }
  /* edge case, undefined behaviour */
  {
    char               input[] = "foo=";
    struct _nhttp_map *map     = _nhttp_map_create_from_urlencoded(input);
    assert_non_null(map);
    _nhttp_map_free(map);
  }
  /* edge case, undefined behaviour */
  {
    char               input[] = "foo=&bar=baz";
    struct _nhttp_map *map     = _nhttp_map_create_from_urlencoded(input);
    assert_non_null(map);
    assert_string_equal(_nhttp_map_get(map, "bar"), "baz");
    _nhttp_map_free(map);
  }
}

int main(void) {
  const struct CMUnitTest map_tests[] = {
      cmocka_unit_test(test_nhttp_map_set_single_element),
      cmocka_unit_test(test_nhttp_map_set_colission),
      cmocka_unit_test(test_nhttp_map_set_overwrite),
      cmocka_unit_test(test_nhttp_map_get),
      cmocka_unit_test(test_nhttp_map_remove),
      cmocka_unit_test(test_nhttp_map_create_from_http_headers),
      cmocka_unit_test(test_nhttp_map_create_from_urlencoded),
  };
  return cmocka_run_group_tests(map_tests, NULL, NULL);
}
