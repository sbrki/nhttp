// clang-format off
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <cmocka.h>

#include <stdlib.h>
#include "../src/nhttp_server.h"
#include "../src/nhttp_map.h"
// clang-format on

static void test_get_request_header(void **state) {
  struct nhttp_ctx *ctx = malloc(sizeof(struct nhttp_ctx));
  ctx->req_headers      = _nhttp_map_create();
  _nhttp_map_set(ctx->req_headers, "foo", "bar");

  assert_string_equal(nhttp_get_request_header(ctx, "foo"), "bar");
  assert_ptr_equal(nhttp_get_request_header(ctx, "baz"), NULL);

  _nhttp_map_free(ctx->req_headers);
  free(ctx);
}

static void test_set_response_header(void **state) {
  struct nhttp_ctx *ctx = malloc(sizeof(struct nhttp_ctx));
  ctx->resp_headers     = _nhttp_map_create();

  nhttp_set_response_header(ctx, "foo", "bar");

  assert_string_equal(_nhttp_map_get(ctx->resp_headers, "foo"), "bar");
  assert_ptr_equal(_nhttp_map_get(ctx->resp_headers, "baz"), NULL);

  _nhttp_map_free(ctx->resp_headers);
  free(ctx);
}

static void test_get_path_param(void **state) {
  struct nhttp_ctx *ctx = malloc(sizeof(struct nhttp_ctx));
  ctx->path_params      = _nhttp_map_create();
  _nhttp_map_set(ctx->path_params, "foo", "bar");

  assert_string_equal(nhttp_get_path_param(ctx, "foo"), "bar");
  assert_ptr_equal(nhttp_get_path_param(ctx, "baz"), NULL);

  _nhttp_map_free(ctx->path_params);
  free(ctx);
}

static void test_get_query_param(void **state) {
  struct nhttp_ctx *ctx = malloc(sizeof(struct nhttp_ctx));
  ctx->query_params     = _nhttp_map_create();
  _nhttp_map_set(ctx->query_params, "foo", "bar");

  assert_string_equal(nhttp_get_query_param(ctx, "foo"), "bar");
  assert_ptr_equal(nhttp_get_query_param(ctx, "baz"), NULL);

  _nhttp_map_free(ctx->query_params);
  free(ctx);
}

int main(void) {
  const struct CMUnitTest map_tests[] = {
      cmocka_unit_test(test_get_request_header),
      cmocka_unit_test(test_set_response_header),
      cmocka_unit_test(test_get_path_param),
      cmocka_unit_test(test_get_query_param),
  };
  return cmocka_run_group_tests(map_tests, NULL, NULL);
}
