#ifndef NHTTP_MAP_H
#define NHTTP_MAP_H

#include "nhttp_util.h"
#include <stdint.h>
#include <stdio.h>  /* sprintf, */
#include <unistd.h> /* write, */

#define NHTTP_MAP_KEY_SIZE 1024
#define NHTTP_MAP_VALUE_SIZE 1024
#define NHTTP_MAP_BINS 256

struct _nhttp_map_entry {
  char                     key[NHTTP_MAP_KEY_SIZE];
  char                     val[NHTTP_MAP_VALUE_SIZE];
  struct _nhttp_map_entry *next;
};

struct _nhttp_map {
  struct _nhttp_map_entry *bins[NHTTP_MAP_BINS];
};

/* _nhttp_map_create allocates and initializes a new map */
struct _nhttp_map *_nhttp_map_create(void);

/* _nhttp_map_store set a (key,value) pair in the passed map. */
/* It copies the passed key and value strings into the map, making it safe */
/* for the caller to modify any of those two strings after the call. */
/* Maximum length of key is defined by NHTTP_MAP_KEY_SIZE */
/* Maximum length of value is defined by NHTTP_MAP_VALUE_SIZE */
void _nhttp_map_set(struct _nhttp_map *map, const char *key, const char *val);

/* _nhttp_map_get returns a value corresponding to the passed key. */
/* If the key is not found in the map, NULL is returned. */
/* The returned char* is pointing to the value that is internal to the map. */
/* Caller should copy the returned string before modifying it. */
const char *_nhttp_map_get(struct _nhttp_map *map, const char *key);

/* _nhttp_map_remove removes a (key,value) pair from the map. */
/* If passed key is not found in the map, it does nothing. */
void _nhttp_map_remove(struct _nhttp_map *map, const char *key);

/* _nhttp_map_free deallocates all the (key,value) pairs in the map, as well */
/* as the map itself. */
void _nhttp_map_free(struct _nhttp_map *map);

/* _nhttp_map_write_as_http_header is a utility that serializes all the map */
/* entries as response headers (HTTP/1.0 - RTF1945) and writes them to the */
/* passed file descriptor.*/
void _nhttp_map_write_as_http_header(struct _nhttp_map *map, int fd);

/* _nhttp_map_create_from_http_headers is a utility that initializes a nhttp */
/* map and fills it with values parsed from http headers. It reads the passed */
/* buffered reader until it encounters empty line (inclusive), ie until it */
/* reads two subsequent CRLF sequences, latter one representing the empty line*/
/* Returns NULL if either line size is exceeded (NHTTP_SERVER_LINE_SIZE) */
/* when reading from the fd, or if the request headers are malformed */
/* (e.g. misplaced CR and/or LF octets). */
struct _nhttp_map *
_nhttp_map_create_from_http_headers(struct _nhttp_buf_reader *br);

/* _nhttp_map_create_from_urlencoded initializes a nhttp map and fills it with*/
/* values parsed from a urlencoded string. String has to be properly escaped */
/* per RFC1738. Also unencodes keys and values before returning. */
/* Passed string should not be larger than NHTTP_SERVER_LINE_SIZE. */
/* Returns NULL if an error has occured.*/
struct _nhttp_map *_nhttp_map_create_from_urlencoded(char *str);

void _nhttp_map_debug_print(struct _nhttp_map *map);
#endif /* NHTTP_MAP_H */
