// clang-format off
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <cmocka.h>
#include <stdio.h> 
#include <unistd.h> /* close,lseek */
#include <sys/mman.h>
#include <errno.h>
#include <string.h> /* strerror, */
#include <fcntl.h>  /* O_* */

#include "../src/nhttp_util.h"
// clang-format on

int mk_tmpfile() {
  remove("/tmp/cmocka_test");
  int fd = open("/tmp/cmocka_test", O_RDWR | O_CREAT);
  if (fd == -1) {
    printf("%s\n", strerror(errno));
    fail_msg("open failed");
  }
  return fd;
}

void rm_tmpfile() { remove("/tmp/cmocka_test"); }

static void test_buf_read_until_crlf(void **state) {
  {
    int fd = mk_tmpfile();

    char data[4] = {'a', '\r', '\n', 'b'};
    int count = write(fd, data, 4);
    if (count != 4) {
      printf("%s\n", strerror(errno));
      fail_msg("failed to write test data to fd");
    }
    lseek(fd, SEEK_SET, 0);

    struct _nhttp_buf_reader *br = _nhttp_util_buf_reader_create(fd);
    char buf[4] = {0};
    int res = _nhttp_util_buf_read_until_crlf(br, buf, 4);

    assert_int_equal(buf[0], 'a');
    assert_int_equal(buf[1], '\r');
    assert_int_equal(buf[2], '\n');
    assert_int_equal(buf[3], 0x0);
    assert_int_equal(res, 0);

    _nhttp_util_buf_reader_free(br);
    close(fd);
    rm_tmpfile();
  }
  {
    int fd = mk_tmpfile();

    char data[4] = {'\r', '\n', 'a', 'b'};
    int count = write(fd, data, 4);
    if (count != 4) {
      printf("%s\n", strerror(errno));
      fail_msg("failed to write test data to fd");
    }
    lseek(fd, SEEK_SET, 0);

    struct _nhttp_buf_reader *br = _nhttp_util_buf_reader_create(fd);
    char buf[4] = {0};
    int res = _nhttp_util_buf_read_until_crlf(br, buf, 4);

    assert_int_equal(buf[0], '\r');
    assert_int_equal(buf[1], '\n');
    assert_int_equal(buf[2], 0x0);
    assert_int_equal(buf[3], 0x0);
    assert_int_equal(res, 0);

    _nhttp_util_buf_reader_free(br);
    close(fd);
    rm_tmpfile();
  }
  {
    int fd = mk_tmpfile();

    char data[4] = {'a', 'b', '\r', '\n'};
    int count = write(fd, data, 4);
    if (count != 4) {
      printf("%s\n", strerror(errno));
      fail_msg("failed to write test data to fd");
    }
    lseek(fd, SEEK_SET, 0);

    struct _nhttp_buf_reader *br = _nhttp_util_buf_reader_create(fd);
    char buf[4] = {0};
    int res = _nhttp_util_buf_read_until_crlf(br, buf, 4);

    assert_int_equal(buf[0], 'a');
    assert_int_equal(buf[1], 'b');
    assert_int_equal(buf[2], '\r');
    assert_int_equal(buf[3], '\n');
    assert_int_equal(res, 0);

    _nhttp_util_buf_reader_free(br);
    close(fd);
    rm_tmpfile();
  }
  {
    int fd = mk_tmpfile();

    char data[4] = {'a', 'b', 'c', '\r'};
    int count = write(fd, data, 4);
    if (count != 4) {
      printf("%s\n", strerror(errno));
      fail_msg("failed to write test data to fd");
    }
    lseek(fd, SEEK_SET, 0);

    struct _nhttp_buf_reader *br = _nhttp_util_buf_reader_create(fd);
    char buf[4] = {0};
    int res = _nhttp_util_buf_read_until_crlf(br, buf, 4);

    assert_int_equal(buf[0], 'a');
    assert_int_equal(buf[1], 'b');
    assert_int_equal(buf[2], 'c');
    assert_int_equal(buf[3], '\r');
    assert_int_equal(res, -1);

    _nhttp_util_buf_reader_free(br);
    close(fd);
    rm_tmpfile();
  }
  {
    int fd = mk_tmpfile();

    char data[4] = {'\r', '\n', '\r', '\n'};
    int count = write(fd, data, 4);
    if (count != 4) {
      printf("%s\n", strerror(errno));
      fail_msg("failed to write test data to fd");
    }
    lseek(fd, SEEK_SET, 0);

    struct _nhttp_buf_reader *br = _nhttp_util_buf_reader_create(fd);
    char buf[4] = {0};
    int res = _nhttp_util_buf_read_until_crlf(br, buf, 4);

    assert_int_equal(buf[0], '\r');
    assert_int_equal(buf[1], '\n');
    assert_int_equal(buf[2], 0x0);
    assert_int_equal(buf[3], 0x0);
    assert_int_equal(res, 0);

    _nhttp_util_buf_reader_free(br);
    close(fd);
    rm_tmpfile();
  }
  {
    int fd = mk_tmpfile();

    char data[4] = {'\n', '\r', '\r', '\n'};
    int count = write(fd, data, 4);
    if (count != 4) {
      printf("%s\n", strerror(errno));
      fail_msg("failed to write test data to fd");
    }
    lseek(fd, SEEK_SET, 0);

    struct _nhttp_buf_reader *br = _nhttp_util_buf_reader_create(fd);
    char buf[4] = {0};
    int res = _nhttp_util_buf_read_until_crlf(br, buf, 4);

    assert_int_equal(buf[0], '\n');
    assert_int_equal(buf[1], '\r');
    assert_int_equal(buf[2], '\r');
    assert_int_equal(buf[3], '\n');
    assert_int_equal(res, 0);

    _nhttp_util_buf_reader_free(br);
    close(fd);
    rm_tmpfile();
  }
}

int main(void) {
  const struct CMUnitTest util_tests[] = {
      cmocka_unit_test(test_buf_read_until_crlf),
  };
  return cmocka_run_group_tests(util_tests, NULL, NULL);
}
