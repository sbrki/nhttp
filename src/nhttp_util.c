#include "nhttp_util.h"
#include <stdio.h>        /* printf, */
#include <stdlib.h>       /* malloc, exit */
#include <string.h>       /* memcpy, strlen */
#include <sys/sendfile.h> /* sendfile */
#include <sys/stat.h>     /* stat, */
#include <unistd.h>       /* write, */

ssize_t _nhttp_util_write_all(int fd, const void *buf, size_t n) {
  size_t  remaining;
  ssize_t sent;
  for (remaining = n; remaining; remaining -= (size_t)sent) {
    if ((sent = write(fd, (char *)buf + (n - remaining), remaining)) == -1)
      return -1;
  }
  return 1;
}

struct _nhttp_buf_reader *_nhttp_util_buf_reader_create(int fd) {
  struct _nhttp_buf_reader *r = malloc(sizeof(struct _nhttp_buf_reader));
  {
    r->fd   = fd;
    r->head = r->tail = 0;
  }
  return r;
}

void _nhttp_util_buf_reader_free(struct _nhttp_buf_reader *r) { free(r); }

ssize_t _nhttp_util_buf_read(struct _nhttp_buf_reader *r, void *buf,
                             size_t count) {
  /* NOTE: tail is the index of the first "FREE" byte (i.e. index */
  /* of first byte that is *NOT* buffered content) -- *NOT* the index of the */
  /* last byte of buffered content. */
  size_t ready;
  size_t bytes_to_copy;

  /* special case: reached end of buffer */
  if (r->tail == NHTTP_UTIL_BUF_READER_SIZE &&
      r->head == NHTTP_UTIL_BUF_READER_SIZE) {
    r->head = r->tail = 0;
  }

  ready = r->tail - r->head;
  if (ready < count && r->tail != NHTTP_UTIL_BUF_READER_SIZE) {
    /* attempt to read into [tail, end of buffer] */
    uint32_t free       = NHTTP_UTIL_BUF_READER_SIZE - r->tail;
    ssize_t  bytes_read = read(r->fd, &(r->buf[r->tail]), free);
    if (bytes_read < 0)
      return bytes_read;
    r->tail += bytes_read;
    ready = r->tail - r->head; /* TODO(sbrki): avail += bytes_read; */
  }

  bytes_to_copy = count < ready ? count : ready;
  memcpy(buf, &(r->buf[r->head]), bytes_to_copy);
  r->head += bytes_to_copy;

  return (ssize_t)bytes_to_copy;
}

int _nhttp_util_buf_read_until_crlf(struct _nhttp_buf_reader *r, char *buf,
                                    size_t maxcount) {
  uint32_t i;
  ssize_t  read;
  char     c, last_char_was_cr;
  last_char_was_cr = 0;
  for (i = 0; i < maxcount; i++) {
    read = _nhttp_util_buf_read(r, &c, 1);
    while (read == 0) {
      /* TODO(sbrki): this is incorrect, handle EOF properly */
      read = _nhttp_util_buf_read(r, &c, 1);
    }
    *buf++ = c;

    if (last_char_was_cr && c == '\n') {
      return 0;
    }

    if (c == '\r') {
      last_char_was_cr = 1;
    } else {
      last_char_was_cr = 0;
    }
  }
  return -1;
}

ssize_t _nhttp_util_sendfile_all(int out_fd, int in_fd, off_t offset,
                                 size_t count) {
  size_t remaining = count;
  while (remaining) {
    ssize_t sent = sendfile(out_fd, in_fd, &offset, remaining);
    if (sent == -1) {
      return -1;
    }
    remaining -= (size_t)sent;
  }
  return 0;
}

void _nhttp_util_remove_trailing_slash(char *str) {
  size_t len;
  if (!str) {
    _nhttp_panic(
        "_nhttp_util_remove_trailing_slash called with NULL str as argument");
  }
  len = strlen(str);
  if (len > 0 && str[len - 1] == '/') {
    str[len - 1] = 0; /* terminate early */
  }
}

void _nhttp_util_remove_leading_slash(char *str) {
  size_t   len;
  uint32_t i;
  if (!str) {
    _nhttp_panic(
        "_nhttp_util_remove_leading_slash called with NULL str as argument");
  }
  len = strlen(str);

  /* edge case: "/" */
  if (len == 1 && str[0] == '/') {
    str[0] = 0;
    return;
  }

  if (len > 1 && str[0] == '/') {
    for (i = 0; i <= len - 2; i++) {
      str[i] = str[i + 1];
    }
    str[len - 1] = 0; /* terminate last char in original str */
  }
}

void _nhttp_util_cut_path_query_params(char *dest, char *path) {
  size_t len = strlen(path);
  size_t i;
  int    mode = 0; /* 0 = before query, 1 = in query, 2 = in fragment */
  for (i = 0; i < len; i++) {
    if (mode == 0) {
      if (path[i] == '?') {
        mode    = 1;
        path[i] = 0;
        continue;
      }
    } else if (mode == 1) {
      if (path[i] == '#') {
        mode    = 2;
        path[i] = 0;
        continue;
      } else {
        *dest++ = path[i];
        path[i] = 0;
      }
    } else if (mode == 2) {
      path[i] = 0;
    }
  }
}

int _nhttp_util_str_triplets_validate(const char *str) {
  size_t len = strlen(str);
  size_t i, j;
  for (i = 0; i < len; i++) {
    if (str[i] == '%') {
      /* check for out-of-bounds */
      if (i + 2 >= len) {
        return -1;
      }
      /* check next two chars */
      for (j = 1; j < 3; j++) {
        if (!((str[i + j] >= '0' && str[i + j] <= '9') ||
              (str[i + j] >= 'a' && str[i + j] <= 'f') ||
              (str[i + j] >= 'A' && str[i + j] <= 'F'))) {
          return -1;
        }
      }
    }
  }
  return 0;
}

char *_nhttp_util_str_escape(const char *str) {
  size_t i;
  size_t len = strlen(str);
  char  *res = malloc(3 * len + 1);
  char  *tmp = res;
  memset(res, 0, 3 * len + 1);

  for (i = 0; i < len; i++) {
    if (str[i] == ' ' || str[i] == '<' || str[i] == '>' || str[i] == '"' ||
        str[i] == '#' || str[i] == '%' || str[i] == '{' || str[i] == '}' ||
        str[i] == '|' || str[i] == '\\' || str[i] == '^' || str[i] == '~' ||
        str[i] == '[' || str[i] == ']' || str[i] == '`' || str[i] == ';' ||
        str[i] == '/' || str[i] == '?' || str[i] == ':' || str[i] == '@' ||
        str[i] == '=' || str[i] == '&') {
      *tmp++ = '%';
      sprintf(tmp, "%2X", str[i]);
      tmp += 2;
    } else {
      *tmp++ = str[i];
    }
  }

  return res;
}

char *_nhttp_util_str_unescape(const char *str) {
  size_t i;
  size_t len = strlen(str);
  char  *res = malloc(len + 1);
  char  *tmp = res;
  char   hex[2];
  int    c;
  memset(res, 0, len + 1);

  for (i = 0; i < len; i++) {
    if (str[i] == '%') {
      hex[0] = str[i + 1];
      hex[1] = str[i + 2];
      sscanf(hex, "%2X", &c);
      *tmp++ = (char)c;
      i += 2;
    } else {
      *tmp++ = str[i];
    }
  }
  return res;
}

void _nhttp_util_str_triplets_to_upper(char *str) {
  size_t len = strlen(str);
  size_t i, j;
  for (i = 0; i < len; i++) {
    if (str[i] == '%') {
      for (j = 1; j < 3; j++) {
        if (str[i + j] >= 'a' && str[i + j] <= 'f') {
          str[i + j] -= 'a' - 'A';
        }
      }
    }
  }
}

void _nhttp_panic(const char *msg) {
  printf("nhttp panic:\t%s\n", msg);
  exit(1);
}

void _nhttp_panicf(const char *fmt, ...) {
  va_list args;

  printf("nhttp panic:\t");

  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);

  printf("\n");
  exit(1);
}

ssize_t _nhttp_util_file_size(const char *path) {
  struct stat s;
  if (stat(path, &s)) {
    return -1;
  }
  return s.st_size;
}
