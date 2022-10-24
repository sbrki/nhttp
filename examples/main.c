/* clang main.c ../nhttp.o -o main */

#include "../src/nhttp.h"

/* route: /name/{name} */
int var_handler(const struct nhttp_ctx *ctx) {
  char buf[1024] = {0};
  sprintf(buf, "Hey %s, how are you?", nhttp_get_path_param(ctx, "name"));
  return nhttp_send_string(ctx, buf, 200);
}

int html_handler(const struct nhttp_ctx *ctx) {
  char *html =
      "<html>"
      "  <head>"
      "    <title>nhttp server</title>"
      "  </head>"
      "  <body>"
      "    <h1>nhttp server</h1>"
      "    <img src='/blob' style='image-rendering:pixelated; width: 20%;'/>"
      "    <p>how is it going</p>"
      "    <audio controls>"
      "      <source src='/song.mp3' type='audio/mpeg'>"
      "    Your browser does not support the audio element."
      "    </audio>"
      "  </body>"
      "</html>";
  return nhttp_send_html(ctx, html, 200);
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

int custom_err_handler(const struct nhttp_ctx *ctx) {
  return nhttp_send_string(ctx, "My custom error code!", 600);
}

int file_handler(const struct nhttp_ctx *ctx) {
  nhttp_set_response_header(ctx, "Content-Type", "audio/mpeg");
  return nhttp_send_file(ctx, "./song.mp3");
}

int temporary_redirect_handler(const struct nhttp_ctx *ctx) {
  return nhttp_redirect(ctx, "/blob", 0);
}

int permanent_redirect_handler(const struct nhttp_ctx *ctx) {
  return nhttp_redirect(ctx, "/blob", 1);
}

int query_param_handler(const struct nhttp_ctx *ctx) {
  char buf[4096] = {0};
  sprintf(buf, "foo = <%s>, bar = <%s>", nhttp_get_query_param(ctx, "foo"),
          nhttp_get_query_param(ctx, "bar"));
  return nhttp_send_string(ctx, buf, 200);
}

int main(void) {
  struct nhttp_server *s = nhttp_server_create();
  nhttp_on_get(s, "/name/{name}/", var_handler);
  nhttp_on_get(s, "/html", html_handler);
  nhttp_on_get(s, "/blob", blob_handler);
  nhttp_on_get(s, "/song.mp3", file_handler);
  nhttp_on_get(s, "/600", custom_err_handler);
  nhttp_on_get(s, "/temporary-redirect", temporary_redirect_handler);
  nhttp_on_get(s, "/permanent-redirect", permanent_redirect_handler);
  nhttp_on_get(s, "/query-param", query_param_handler);

  nhttp_server_run(s, 8080);
}
