// clang-format off
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <cmocka.h>
#include <stdio.h> 
#include <stdlib.h> 

#include "../src/nhttp_util.h"
// clang-format on

ssize_t __wrap_write(int fd, const void *buf, size_t n) {
  check_expected(n);
  return mock_type(ssize_t);
}

ssize_t __wrap_read(int fd, void *buf, size_t n) {
  check_expected(n);
  return mock_type(ssize_t);
}

static void test_write_all(void **state) {
  char data[] = "hello";
  {
    expect_value(__wrap_write, n, 5);
    will_return(__wrap_write, -1);
    size_t result = _nhttp_util_write_all(1, data, 5);
    assert_int_equal(result, -1);
  }
  {
    expect_value(__wrap_write, n, 5);
    expect_value(__wrap_write, n, 4);
    expect_value(__wrap_write, n, 3);
    expect_value(__wrap_write, n, 2);
    expect_value(__wrap_write, n, 1);
    will_return_count(__wrap_write, 1, 5);
    size_t result = _nhttp_util_write_all(1, data, 5);
    assert_int_equal(result, 1);
  }
  {
    expect_value(__wrap_write, n, 5);
    will_return(__wrap_write, 5);
    size_t result = _nhttp_util_write_all(1, data, 5);
    assert_int_equal(result, 1);
  }
}

static void test_buf_read_eof(void **state) {
  char dest[2 * NHTTP_UTIL_BUF_READER_SIZE]; /* twice the buf capacity, */
                                             /* just in case. */
  struct _nhttp_buf_reader *r = _nhttp_util_buf_reader_create(0xbeef);
  r->head = r->tail = 20;
  {
    expect_any(__wrap_read, n);
    will_return(__wrap_read, 0);
    size_t ret = _nhttp_util_buf_read(r, dest, 10);
    assert_int_equal(ret, 0);
  }
  _nhttp_util_buf_reader_free(r);
}

static void test_buf_read_error(void **state) {
  char dest[2 * NHTTP_UTIL_BUF_READER_SIZE];
  struct _nhttp_buf_reader *r = _nhttp_util_buf_reader_create(0xbeef);
  /* there is buffered data available in the reader */
  r->head = 10;
  r->tail = 20;
  {
    expect_any(__wrap_read, n);
    will_return(__wrap_read, -1);
    size_t ret =
        _nhttp_util_buf_read(r, dest, 100); /* should force read() call */
    assert_int_equal(ret, -1);
  }
  _nhttp_util_buf_reader_free(r);
}

static void test_buf_read_normal(void **state) {
  char dest[2 * NHTTP_UTIL_BUF_READER_SIZE];
  struct _nhttp_buf_reader *r = _nhttp_util_buf_reader_create(0xbeef);
  /* test "normal" usage of the reader, with as many edge cases covered as */
  /* possible. */
  {
    /* head=0, tail=0 */
    /* reader tries to read into all of its buf, but read returns only 100b. */
    expect_value(__wrap_read, n, NHTTP_UTIL_BUF_READER_SIZE);
    will_return(__wrap_read, 100);
    size_t ret = _nhttp_util_buf_read(r, dest, NHTTP_UTIL_BUF_READER_SIZE);
    assert_int_equal(ret, 100);
    assert_int_equal(r->head, 100);
    assert_int_equal(r->tail, 100);
  }
  {
    /* head=100, tail=100 */
    /* assure that buf reader attempts to read into all of its remaining buf, */
    /* i.e. into [tail, end] */
    expect_value(__wrap_read, n, NHTTP_UTIL_BUF_READER_SIZE - 100);
    will_return(__wrap_read, NHTTP_UTIL_BUF_READER_SIZE - 100);
    size_t ret = _nhttp_util_buf_read(r, dest, 10);
    assert_int_equal(ret, 10);
    assert_int_equal(r->head, 110);
    assert_int_equal(r->tail, NHTTP_UTIL_BUF_READER_SIZE);
  }
  {
    /* head=110, tail=NHTTP_UTIL_BUF_READER_SIZE */
    /* try to read more bytes than there are ready in the buf. */
    /* reader should not call read() as its tail is already at the end of its */
    /* buf. It should return only the bytes it has in its buf. */
    size_t ret = _nhttp_util_buf_read(r, dest, NHTTP_UTIL_BUF_READER_SIZE);
    assert_int_equal(ret, NHTTP_UTIL_BUF_READER_SIZE - 110);
    assert_int_equal(r->head, NHTTP_UTIL_BUF_READER_SIZE);
    assert_int_equal(r->tail, NHTTP_UTIL_BUF_READER_SIZE);
  }
  {
    /* head=NHTTP_UTIL_BUF_READER_SIZE, tail=NHTTP_UTIL_BUF_READER_SIZE */
    /* both head and tail are at the end, we expect them to reset to start, */
    /* and for reader to attempt to read into all of buf at the next call */
    expect_value(__wrap_read, n, NHTTP_UTIL_BUF_READER_SIZE);
    will_return(__wrap_read, NHTTP_UTIL_BUF_READER_SIZE);
    size_t ret = _nhttp_util_buf_read(r, dest, 1);
    assert_int_equal(ret, 1);
    assert_int_equal(r->head, 1);
    assert_int_equal(r->tail, NHTTP_UTIL_BUF_READER_SIZE);
  }
  _nhttp_util_buf_reader_free(r);
}

ssize_t __wrap_sendfile(int out_fd, int in_fd, off_t *offset, size_t count) {
  check_expected(out_fd);
  check_expected(in_fd);
  check_expected(count);
  return mock_type(ssize_t);
}

static void test_sendfile_all(void **state) {
  {
    expect_value(__wrap_sendfile, out_fd, 1);
    expect_value(__wrap_sendfile, in_fd, 2);
    expect_value(__wrap_sendfile, count, 20);
    will_return(__wrap_sendfile, 20);
    int ret = _nhttp_util_sendfile_all(1, 2, 10, 20);
    assert_int_equal(ret, 0);
  }
  {
    expect_value(__wrap_sendfile, out_fd, 1);
    expect_value(__wrap_sendfile, in_fd, 2);
    expect_value(__wrap_sendfile, count, 20);
    will_return(__wrap_sendfile, -1);
    int ret = _nhttp_util_sendfile_all(1, 2, 10, 20);
    assert_int_equal(ret, -1);
  }
  {
    expect_value(__wrap_sendfile, out_fd, 1);
    expect_value(__wrap_sendfile, in_fd, 2);
    expect_value(__wrap_sendfile, count, 20);
    will_return(__wrap_sendfile, 10);

    expect_value(__wrap_sendfile, out_fd, 1);
    expect_value(__wrap_sendfile, in_fd, 2);
    expect_value(__wrap_sendfile, count, 10);
    will_return(__wrap_sendfile, 10);
    int ret = _nhttp_util_sendfile_all(1, 2, 10, 20);
    assert_int_equal(ret, 0);
  }
}

static void test_remove_trailing_slash(void **state) {
  {
    char input[] = "/hello/world/";
    _nhttp_util_remove_trailing_slash(input);
    assert_string_equal(input, "/hello/world");
  }
  {
    char input[] = "/hello/world";
    _nhttp_util_remove_trailing_slash(input);
    assert_string_equal(input, "/hello/world");
  }
  {
    char input[] = "/";
    _nhttp_util_remove_trailing_slash(input);
    assert_string_equal(input, "");
  }
  {
    char input[] = "x";
    _nhttp_util_remove_trailing_slash(input);
    assert_string_equal(input, "x");
  }
  {
    char input[] = "";
    _nhttp_util_remove_trailing_slash(input);
    assert_string_equal(input, "");
  }
}

static void test_remove_leading_slash(void **state) {
  {
    char input[] = "/hello/world/";
    _nhttp_util_remove_leading_slash(input);
    assert_string_equal(input, "hello/world/");
  }
  {
    char input[] = "hello/world/";
    _nhttp_util_remove_leading_slash(input);
    assert_string_equal(input, "hello/world/");
  }
  {
    char input[] = "/";
    _nhttp_util_remove_leading_slash(input);
    assert_string_equal(input, "");
  }
  {
    char input[] = "x";
    _nhttp_util_remove_leading_slash(input);
    assert_string_equal(input, "x");
  }
  {
    char input[] = "";
    _nhttp_util_remove_leading_slash(input);
    assert_string_equal(input, "");
  }
}

static void test_cut_path_query_params(void **state) {
  {
    char dest[20] = {0};
    char path[] = "";
    _nhttp_util_cut_path_query_params(dest, path);
    assert_int_equal(dest[0], 0);
    assert_string_equal(path, "");
  }
  {
    char dest[20] = {0};
    char path[] = "foo";
    _nhttp_util_cut_path_query_params(dest, path);
    assert_int_equal(dest[0], 0);
    assert_string_equal(path, "foo");
  }
  {
    char dest[20] = {0};
    char path[] = "foo?bar=baz";
    _nhttp_util_cut_path_query_params(dest, path);
    assert_string_equal(dest, "bar=baz");
    assert_string_equal(path, "foo");
  }
  {
    char dest[20] = {0};
    char path[] = "foo?bar=baz#frag";
    _nhttp_util_cut_path_query_params(dest, path);
    assert_string_equal(dest, "bar=baz");
    assert_string_equal(path, "foo");
  }
  /* edge case */
  {
    char dest[20] = {0};
    char path[] = "foo?";
    _nhttp_util_cut_path_query_params(dest, path);
    assert_int_equal(dest[0], 0);
    assert_string_equal(path, "foo");
  }
  /* edge case */
  {
    char dest[20] = {0};
    char path[] = "foo?bar=baz#";
    _nhttp_util_cut_path_query_params(dest, path);
    assert_string_equal(dest, "bar=baz");
    assert_string_equal(path, "foo");
  }
  /* edge case */
  {
    char dest[20] = {0};
    char path[] = "foo?#";
    _nhttp_util_cut_path_query_params(dest, path);
    assert_int_equal(dest[0], 0);
    assert_string_equal(path, "foo");
  }
  /* incorrect input */
  {
    char dest[20] = {0};
    char path[] = "foo#?";
    _nhttp_util_cut_path_query_params(dest, path);
    assert_int_equal(dest[0], 0);
    assert_string_equal(path, "foo#");
  }
}

static void test_str_triplets_validate(void **state) {
  {
    char *input = "";
    int res = _nhttp_util_str_triplets_validate(input);
    assert_int_equal(res, 0);
  }
  {
    char *input = "foobar";
    int res = _nhttp_util_str_triplets_validate(input);
    assert_int_equal(res, 0);
  }
  {
    char *input = "%20";
    int res = _nhttp_util_str_triplets_validate(input);
    assert_int_equal(res, 0);
  }
  {
    char *input = "%aa%bb";
    int res = _nhttp_util_str_triplets_validate(input);
    assert_int_equal(res, 0);
  }
  {
    char *input = "%AA%BB";
    int res = _nhttp_util_str_triplets_validate(input);
    assert_int_equal(res, 0);
  }

  /* percent sign not encoded (out-of-bounds prevention) */
  {
    char *input = "%2";
    int res = _nhttp_util_str_triplets_validate(input);
    assert_int_equal(res, -1);
  }
  {
    char *input = "foo%20%2";
    int res = _nhttp_util_str_triplets_validate(input);
    assert_int_equal(res, -1);
  }
  {
    char *input = "foo%";
    int res = _nhttp_util_str_triplets_validate(input);
    assert_int_equal(res, -1);
  }

  /* invalid hex number */
  {
    char *input = "%0z";
    int res = _nhttp_util_str_triplets_validate(input);
    assert_int_equal(res, -1);
  }
  {
    char *input = "%20%0z";
    int res = _nhttp_util_str_triplets_validate(input);
    assert_int_equal(res, -1);
  }

  /* just a mixed-bag stress test */
  {
    char *input = "foo%20bar/baz%30@";
    int res = _nhttp_util_str_triplets_validate(input);
    assert_int_equal(res, 0);
  }
  {
    char *input = "foo%%20bar";
    int res = _nhttp_util_str_triplets_validate(input);
    assert_int_equal(res, -1);
  }
}

static void test_str_triplets_to_upper(void **state) {
  {
    char input[] = "";
    _nhttp_util_str_triplets_to_upper(input);
    assert_string_equal(input, "");
  }
  {
    char input[] = "foo";
    _nhttp_util_str_triplets_to_upper(input);
    assert_string_equal(input, "foo");
  }
  {
    char input[] = "%00 %a0 %0a %aa";
    _nhttp_util_str_triplets_to_upper(input);
    assert_string_equal(input, "%00 %A0 %0A %AA");
  }
  {
    char input[] = "%00 %A0 %0A %AA";
    _nhttp_util_str_triplets_to_upper(input);
    assert_string_equal(input, "%00 %A0 %0A %AA");
  }
  {
    char input[] = "%aF %Af %af %AF";
    _nhttp_util_str_triplets_to_upper(input);
    assert_string_equal(input, "%AF %AF %AF %AF");
  }
  {
    char input[] = "foo%afbar";
    _nhttp_util_str_triplets_to_upper(input);
    assert_string_equal(input, "foo%AFbar");
  }
}

static void test_str_escape(void **state) {
  {
    char *input = "";
    char *res = _nhttp_util_str_escape(input);
    assert_string_equal(res, "");
    free(res);
  }
  {
    char *input = "foo";
    char *res = _nhttp_util_str_escape(input);
    assert_string_equal(res, "foo");
    free(res);
  }
  {
    char *input = " ";
    char *res = _nhttp_util_str_escape(input);
    assert_string_equal(res, "%20");
    free(res);
  }
  {
    char *input = "foo bar";
    char *res = _nhttp_util_str_escape(input);
    assert_string_equal(res, "foo%20bar");
    free(res);
  }
  {
    char *input = "<foo bar baz>";
    char *res = _nhttp_util_str_escape(input);
    assert_string_equal(res, "%3Cfoo%20bar%20baz%3E");
    free(res);
  }
  {
    char *input = " <>\"#%{}|\\^~[]`;/?:@=&";
    char *res = _nhttp_util_str_escape(input);
    assert_string_equal(
        res,
        "%20%3C%3E%22%23%25%7B%7D%7C%5C%5E%7E%5B%5D%60%3B%2F%3F%3A%40%3D%26");
    free(res);
  }
}

static void test_str_unescape(void **state) {
  {
    char *input = "";
    char *res = _nhttp_util_str_unescape(input);
    assert_string_equal(res, "");
    free(res);
  }
  {
    char *input = "foo";
    char *res = _nhttp_util_str_unescape(input);
    assert_string_equal(res, "foo");
    free(res);
  }
  {
    char *input = "%20";
    char *res = _nhttp_util_str_unescape(input);
    assert_string_equal(res, " ");
    free(res);
  }
  {
    char *input = "foo%20bar";
    char *res = _nhttp_util_str_unescape(input);
    assert_string_equal(res, "foo bar");
    free(res);
  }
  {
    char *input = "%3Cfoo%20bar%20baz%3E";
    char *res = _nhttp_util_str_unescape(input);
    assert_string_equal(res, "<foo bar baz>");
    free(res);
  }
  {
    char *input =
        "%20%3C%3E%22%23%25%7B%7D%7C%5C%5E%7E%5B%5D%60%3B%2F%3F%3A%40%3D%26";
    char *res = _nhttp_util_str_unescape(input);
    assert_string_equal(res, " <>\"#%{}|\\^~[]`;/?:@=&");
    free(res);
  }
}

int main(void) {
  const struct CMUnitTest util_tests[] = {
      cmocka_unit_test(test_write_all),
      cmocka_unit_test(test_buf_read_eof),
      cmocka_unit_test(test_buf_read_error),
      cmocka_unit_test(test_buf_read_normal),
      cmocka_unit_test(test_sendfile_all),
      cmocka_unit_test(test_remove_trailing_slash),
      cmocka_unit_test(test_cut_path_query_params),
      cmocka_unit_test(test_remove_leading_slash),
      cmocka_unit_test(test_str_triplets_validate),
      cmocka_unit_test(test_str_triplets_to_upper),
      cmocka_unit_test(test_str_escape),
      cmocka_unit_test(test_str_unescape),
  };
  return cmocka_run_group_tests(util_tests, NULL, NULL);
}
