# nhttp
Nano HTTP library writen in ANSI C (C89).
------

<p align="center"><img width=100 src ="https://github.com/sbrki/nhttp/raw/master/extras/nhttp-transparent-big.png" /></p>

nhttp is a HTTP library that supports a solid subset of HTTP/1.0 
([RFC 1945](https://www.rfc-editor.org/rfc/rfc1945)), and even some HTTP/1.1
features ([RFC 2616](https://www.rfc-editor.org/rfc/rfc2616)) such as byte ranges.

It's focus is on having a clean and modern API, good readbility, solid performance & low
memory footprint, and is intended for use in constrained computing environments.

# Example
```c
#include "nhttp.h"

int html_handler(const struct nhttp_ctx *ctx) {
  char *html =
      "<html>"
      "  <head>"
      "    <title>nhttp server</title>"
      "  </head>"
      "  <body>"
      "    <h1>nhttp server</h1>"
      "  </body>"
      "</html>";
  return nhttp_send_html(ctx, html, 200);
}

int var_handler(const struct nhttp_ctx *ctx) {
  char buf[1024] = {0};
  sprintf(buf, "Hey %s, how are you?",
          nhttp_get_request_header(ctx, "name"));
  return nhttp_send_string(ctx, buf, 200);
}

int blob_handler(const struct nhttp_ctx *ctx) {

  unsigned char data[152] = {
      137, 80,  78,  71,  13,  10,  26,  10,  0,   0,   0,   13,  73,  72,
      68,  82,  0,   0,   0,   15,  0,   0,   0,   7,   8,   6,   0,   0,
      0,   215, 133, 23,  39,  0,   0,   0,   1,   115, 82,  71,  66,  0,
      174, 206, 28,  233, 0,   0,   0,   82,  73,  68,  65,  84,  24,  149,
      99,  96,  64,  3,   25,  25,  25,  255, 177, 241, 209, 197, 25,  24,
      24,  24,  152, 208, 5,   240, 105, 68,  55,  128, 241, 126, 161, 218,
      127, 6,   6,   6,   6,   197, 254, 91,  140, 232, 10,  102, 204, 152,
      193, 152, 145, 145, 241, 31,  157, 70,  177, 89,  177, 255, 22,  220,
      16,  152, 38,  124, 46,  130, 201, 227, 117, 54,  33,  239, 80,  164,
      145, 44,  155, 97,  0,   0,   152, 73,  43,  139, 166, 119, 206, 31,
      0,   0,   0,   0,   73,  69,  78,  68,  174, 66,  96,  130,
  };

  return nhttp_send_blob(ctx, data, 152, "image/png", 200);
}

/* also supports byte ranges out of the box */
int file_handler(const struct nhttp_ctx *ctx) {
  nhttp_set_response_header(ctx, "Content-Type", "image/png");
  return nhttp_send_file(ctx, "./image.png");
}

int main(void) {
  struct nhttp_server *s = nhttp_server_create();

  nhttp_on_get(s, "/html", html_handler);
  nhttp_on_get(s, "/name/{name}/", var_handler);
  nhttp_on_get(s, "/blob", blob_handler);
  nhttp_on_get(s, "/image.png", file_handler);

  nhttp_server_run(s, 8080);
}

```

# Status
This is alpha quality software! Expect bugs, breaking interface changes,
and other issues.
Even though a solid proportion of nhttp is covered by tests,
it is not production-ready software.
Due to its stripped-down nature performance on modern machines is solid and
nhttp can handle about 18k req/s. Currently the server uses <1KiB of memory (RSS)
while idle.

# Limitations
nhttp is single threaded (for now). Currently it has the benefit of keeping
the code simple enough. For multicore use multiple application
processes should be run.

Routing capabilities are also somewhat limited, see `nhttp_router.h` for more
information.

# Installation
## Dependencies
None.

If you want to run the tests, you will need:
* `cmocka` for unit tests (which don't cover all functionality)
* `python3` and `requests` library for E2E tests. (TODO:sbrki)

# Usage

# Security
Little thought was given into attack prevention aside from most basic attacks
such as buffer overflows, and it may be voulnerable even against those.
No effort was given into preventing even basic application level attacks
(e.g.: slow lorris). 
Usage behing a robust and secure reverse proxy (such as Nginx) is a must.
