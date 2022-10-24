#ifndef NHTTP_UTIL_H
#define NHTTP_UTIL_H

#include <stdint.h>    /* uint32_t, */
#include <sys/types.h> /* size_t, ssize_t, */

/* _nhttp_util_write_all is a wrapper around write(2) that makes sure */
/* that the entire buffer has been written to the passed fd, */
/* by continuing where it left off if a call to write(2) didn't write the */
/* entire buffer (if it returned a value smaller than the passed n). */
/* Returns -1 if it encounters an error while calling write(2), */
/* otherwise it returns 1 . */
ssize_t _nhttp_util_write_all(int fd, const void *buf, size_t n);

#define NHTTP_UTIL_BUF_READER_SIZE 4096

/* _nhttp_buf_reader is a buffered fd reader. */
struct _nhttp_buf_reader {
  int      fd;
  char     buf[NHTTP_UTIL_BUF_READER_SIZE];
  uint32_t head, tail;
};

/* _nhttp_util_buf_reader_create creates a new buffered redaer. */
struct _nhttp_buf_reader *_nhttp_util_buf_reader_create(int fd);

/* _nhttp_util_buf_reader_free frees the buffered reader. */
/* Does not close the fd. */
void _nhttp_util_buf_reader_free(struct _nhttp_buf_reader *r);

/* _nhttp_util_buf_read reads count bytes from buffered reader r. */
/* Returns number of read bytes. */
/* Behaves same as read(2) call, i.e. it can return a value smaller than the */
/* passed count (due to data/device not being available at the moment).*/
/* Returns 0 on EOF, and -1 on error. */
/* Under the hood it calls read(2) in blocking mode - calls will block when */
/* there is no available data whatsoever. */
ssize_t _nhttp_util_buf_read(struct _nhttp_buf_reader *r, void *buf,
                             size_t count);

/* _nhttp_util_buf_read_until_crlf reads from the passed buffered reader */
/* into passed `buf` until it encouters CR LF byte sequence */
/* (HTTP end-of-line marker) (including it), or when it reads `maxcount` bytes*/
/* Returns 0 if CR LF sequence was encountered, even if `maxcount` was reached*/
/* (buf[maxcount-2] = CR, buf[maxcount-1] = LF ). */
/* Returns -1 if CR LF sequence was not encountered AND total of `maxcount` */
/* bytes were read from `r` and written to `buf`. */
/* It calls `_nhttp_util_buf_read` in a loop which can block. */
int _nhttp_util_buf_read_until_crlf(struct _nhttp_buf_reader *r, char *buf,
                                    size_t maxcount);

/* _nhttp_util_sendfile_all calls sendfile() in a loop until count bytes have */
/* been successfully read and written. Returns -1 if sendfile returned an err */
/* and 0 otherwise. */
ssize_t _nhttp_util_sendfile_all(int out_fd, int in_fd, off_t offset,
                                 size_t count);

/* _nhttp_util_remove_trailing_slash removes the trailing slash from passed */
/* string, not changing the starting point of the str. It terminates the str */
/* early if last character is a slash. */
/* Note that it causes a SIGSEGV if the passed string is not on heap or stack.*/
/* Panics if the passed str is NULL. */
void _nhttp_util_remove_trailing_slash(char *str);

/* _nhttp_util_remove_leading_slash removes the leading slash from passed */
/* string if the string starts with a slash, not changing the starting point */
/* of the str. It moves all the caracters left by one place and terminates */
/* the string one place earlier than the passed str. */
/* Note that it causes a SIGSEGV if the passed string is not on heap or stack.*/
/* Panics if the passed str is NULL. */
void _nhttp_util_remove_leading_slash(char *str);

/* TODO(sbrki): This also (forever!) wipes out fragment (aka URL Anchor) */

/* _nhttp_util_cut_path_query_params removes query params and anchor from */
/* path (zeroes it out), and copies the query params into dest. */
/* passed `path` has to be url encoded per RFC1738, i.e. question mark char */
/* (?) has to be escaped if it is part of the path, as its verbatim occurance */
/* is interpreted as start of the query params. Same goes for ladder character*/
/* (#) which has to be escaped if it is part of the path or query arguments. */
void _nhttp_util_cut_path_query_params(char *dest, char *path);

/*  _nhttp_util_str_escape escapes unsafe and reserved characters from the */
/* passed `str`, per RFC1738. Unsafe and reserved characters are escaped using*/
/* hex encoding trigrams (%XX) of character ASCII value. */
/* Returned string has to be freed by the caller. */
/* List of characters escaped by this function: */
/* * Unsafe characters: <,>,space,",#,%,{,},|,\,^,~,[,],`  */
/* * Reserved characters: ;,/,?,:,@,=,&   */
char *_nhttp_util_str_escape(const char *str);

/* _nhttp_util_str_unescape unescapes hex encoded trigrams from the passed */
/* `str`. Returned string has to be freed by the caller. */
/* Passed str is expected to be properly encoded per RFC1738, in particular */
/* percent sign must denote a start of encoded triple -> if a percent sign is */
/* a part of the content, it must be escaped (to %25). */
/* Passed str should be validated via call to */
/* `_nhttp_util_str_triplets_validate` prior to calling this function. */
char *_nhttp_util_str_unescape(const char *str);

/* _nhttp_util_str_triplets_validate validates that encoding triplets (%XX) */
/* are valid hexadecimal numbers (either lowercase or uppercase, if they  */
/* contain alpha chars). Also checks that there are no unencoded percent signs*/
/* per RFC1738. Returns 0 if string is valid, otherwise -1 . */
int _nhttp_util_str_triplets_validate(const char *str);

/* _nhttp_util_str_triplets_to_upper transforms encoding triplets (%XX) into */
/* uppercase letters if they are lowercase, e.g.: %a1%B2 -> %A1%B2 . */
/* The transformations are done in-place. */
/* Passed str is expected to be properly encoded per RFC1738, in particular */
/* percent sign must denote a start of encoded triple -> if a percent sign is */
/* a part of the content, it must be escaped (to %25). */
/* Passed str should be validated via call to */
/* `_nhttp_util_str_triplets_validate` prior to calling this function. */
void _nhttp_util_str_triplets_to_upper(char *str);

/* _nhttp_panic prints the passed message and terminates the process with */
/* exit code 1. */
void _nhttp_panic(const char *msg);

/* _nhttp_panicf prints the passed message and terminates the process with */
/* exit code 1. */
void _nhttp_panicf(const char *fmt, ...);

/* _nhttp_util_file_size returns the size of the file, in bytes. */
/* Returns -1 if file does not exist. */
ssize_t _nhttp_util_file_size(const char *path);

#endif /* NHTTP_UTIL_H */
