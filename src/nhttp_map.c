#include "nhttp_map.h"
#include "nhttp_server.h"
#include "nhttp_util.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h> /* bzero, */

/* _nhttp_djb2 returns djb2 hash of the passed string */
/* See http://www.cse.yorku.ca/~oz/hash.html for more info. */
uint32_t _nhttp_djb2(const char *str) {
  uint32_t hash = 5381;
  uint32_t c;
  while ((c = (uint32_t)*str++)) {
    hash = ((hash << 5) + hash) + c;
  }
  return hash;
}

struct _nhttp_map *_nhttp_map_create() {
  struct _nhttp_map *m = (struct _nhttp_map *)malloc(sizeof(struct _nhttp_map));
  memset(m, 0, sizeof(struct _nhttp_map));
  return m;
}

void _nhttp_map_set(struct _nhttp_map *map, const char *key, const char *val) {
  struct _nhttp_map_entry *loc, *last;
  uint32_t                 bin;

  /* create new map entry */
  struct _nhttp_map_entry *entry =
      (struct _nhttp_map_entry *)malloc(sizeof(struct _nhttp_map_entry));
  strcpy(entry->key, key);
  strcpy(entry->val, val);
  entry->next = NULL;

  bin = _nhttp_djb2(key) % NHTTP_MAP_BINS;

  if (map->bins[bin] == NULL) {
    map->bins[bin] = entry;
    return;
  }

  /* iterate through all entries (collisions) in the bin */
  for (loc = map->bins[bin]; loc != NULL; loc = loc->next) {
    /* overwrite existing entry */
    if (strcmp(loc->key, key) == 0) {
      strcpy(loc->val, val);
      return;
    }
    last = loc;
  }
  last->next = entry;
}

const char *_nhttp_map_get(struct _nhttp_map *map, const char *key) {
  uint32_t                 bin = _nhttp_djb2(key) % NHTTP_MAP_BINS;
  struct _nhttp_map_entry *loc;
  for (loc = map->bins[bin]; loc != NULL; loc = loc->next) {
    if (strcmp(loc->key, key) == 0) {
      return loc->val;
    }
  }
  return NULL;
}

void _nhttp_map_remove(struct _nhttp_map *map, const char *key) {
  uint32_t                 bin = _nhttp_djb2(key) % NHTTP_MAP_BINS;
  struct _nhttp_map_entry *loc, *prev = NULL;

  for (loc = map->bins[bin]; loc != NULL; loc = loc->next) {
    if (strcmp(loc->key, key) == 0) {
      if (loc == map->bins[bin]) {
        map->bins[bin] = loc->next;
        free(loc);
        return;
      } else {
        prev->next = loc->next;
        free(loc);
        return;
      }
    }
    prev = loc;
  }
}

void _nhttp_map_free(struct _nhttp_map *map) {
  uint32_t                 bin;
  struct _nhttp_map_entry *loc, *tmp;
  for (bin = 0; bin < NHTTP_MAP_BINS; bin++) {
    if (map->bins[bin] == NULL)
      continue;
    for (loc = map->bins[bin]; loc != NULL;) {
      tmp = loc->next;
      free(loc);
      loc = tmp;
    }
  }
  free(map);
}

void _nhttp_map_write_as_http_header(struct _nhttp_map *map, int fd) {
  /* NOTE: RFC 1945: HTTP-header = field-name ":" [ field-value ] CRLF */
  char buf[NHTTP_MAP_KEY_SIZE + NHTTP_MAP_VALUE_SIZE + 4]; /* 4 -> */
  /* ":" + CRLF + '\0' */
  uint32_t                 bin;
  struct _nhttp_map_entry *loc;

  for (bin = 0; bin < NHTTP_MAP_BINS; bin++) {
    for (loc = map->bins[bin]; loc != NULL; loc = loc->next) {
      bzero(buf, NHTTP_MAP_KEY_SIZE + NHTTP_MAP_VALUE_SIZE + 4);
      sprintf(buf, "%s:%s\r\n", loc->key, loc->val);
      _nhttp_util_write_all(fd, buf, strlen(buf));
    }
  }
}

struct _nhttp_map *
_nhttp_map_create_from_http_headers(struct _nhttp_buf_reader *br) {
  struct _nhttp_map *m                             = _nhttp_map_create();
  char               line[NHTTP_SERVER_LINE_SIZE]  = {0};
  char               key[NHTTP_SERVER_LINE_SIZE]   = {0};
  char               value[NHTTP_SERVER_LINE_SIZE] = {0};

  while (1) {
    memset(line, 0, NHTTP_SERVER_LINE_SIZE);
    memset(key, 0, NHTTP_SERVER_LINE_SIZE);
    memset(value, 0, NHTTP_SERVER_LINE_SIZE);

    if (_nhttp_util_buf_read_until_crlf(br, line, NHTTP_SERVER_LINE_SIZE - 1) ==
        -1) {
      _nhttp_map_free(m);
      return NULL;
    }

    /* break condition = empty line (CRLF) --> start of req body is next */
    if (strlen(line) <= 2) {
      return m;
    }

    sscanf(line, "%[^:]: %[^\r]\r\n", key, value); /* also strips leading */
                                                   /* whitespace from value */

    _nhttp_map_set(m, key, value);
  }

  return m;
}

/* TODO(sbrki): rewrite this more elegantly */
/* Also, in current impl. empty keys are undefined behaviour, but don't */
/* cause segfaults. For example, foo= and foo=&bar=2 have different outcomes, */
/* in the second one foo is set to empty string, while it is NULL in first. */
/* This should be fixed at some point, but is not a burning issue atm. */
struct _nhttp_map *_nhttp_map_create_from_urlencoded(char *str) {
  struct _nhttp_map *m   = _nhttp_map_create();
  size_t             len = strlen(str);
  size_t             i;
  int                mode = 0; /* 0 = in key, 1 = in value, 2 = invalid */
  char               key[NHTTP_SERVER_LINE_SIZE] = {0};
  char               val[NHTTP_SERVER_LINE_SIZE] = {0};
  char              *kp                          = key;
  char              *vp                          = val;
  char              *unesc_key;
  char              *unesc_val;

  if (_nhttp_util_str_triplets_validate(str)) {
    _nhttp_map_free(m);
    return NULL;
  }
  _nhttp_util_str_triplets_to_upper(str);

  for (i = 0; i < len; i++) {
    if (mode == 0) {
      if (str[i] == '=') {
        mode = 1;
        continue;
      } else {
        *kp++ = str[i];
      }
    }

    if (mode == 1) {
      if (str[i] == '&' || i == len - 1) {

        if (i == (len - 1)) {
          *vp = str[i];
        }
        /* save k,v pair, cleanup */

        /* unescape hex triplets */
        unesc_key = _nhttp_util_str_unescape(key);
        unesc_val = _nhttp_util_str_unescape(val);

        /* check lengths */
        if ((strlen(unesc_key) + 1 > NHTTP_MAP_KEY_SIZE) ||
            (strlen(unesc_val) + 1 > NHTTP_MAP_VALUE_SIZE)) {
          free(unesc_key);
          free(unesc_val);
          _nhttp_map_free(m);
          return NULL;
        }

        /* save */
        _nhttp_map_set(m, unesc_key, unesc_val);

        /* cleanup */
        free(unesc_key);
        free(unesc_val);
        memset(key, 0, NHTTP_SERVER_LINE_SIZE);
        memset(val, 0, NHTTP_SERVER_LINE_SIZE);
        kp = key;
        vp = val;

        mode = 0;
        continue;
      } else {
        *vp++ = str[i];
      }
    }
  }
  return m;
}
