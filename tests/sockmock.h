// clang-format off
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <cmocka.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
// clang-format on

#define SOCK_PATH "/tmp/nhttp.sock"

struct sockmock {
  int serv_sock_fd;
  int serv_conn_fd;
  int client_sock_fd;
};

struct connectordata {
  struct sockaddr_un client_addr;
  int client_sock_fd;
};

void *_connector(void *arg) {
  struct connectordata cd = *((struct connectordata *)arg);

  if (connect(cd.client_sock_fd, (struct sockaddr *)&(cd.client_addr),
              sizeof(struct sockaddr_un)) == -1) {

    printf("Error: %s\n", strerror(errno));
    fail_msg("client connect() failed");
  }
  return NULL;
}

struct sockmock sockmock_create(const char *clientmsg) {
  remove(SOCK_PATH);

  /* server */
  struct sockaddr_un serv_addr;
  int serv_sock_fd;
  if ((serv_sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    fail_msg("could not create server socket");
  }

  memset(&serv_addr, 0, sizeof(struct sockaddr_un));
  serv_addr.sun_family = AF_UNIX;
  strcpy(serv_addr.sun_path, SOCK_PATH);

  if (bind(serv_sock_fd, (struct sockaddr *)&serv_addr,
           sizeof(struct sockaddr_un)) == -1) {
    fail_msg("server could not bind UNIX domain socket");
  }

  if (listen(serv_sock_fd, 0) == -1) {
    fail_msg("server could not listen on UNIX domain socket");
  }

  /* client */
  struct sockaddr_un client_addr;
  int client_sock_fd;
  if ((client_sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    fail_msg("could not create client socket");
  }

  memset(&client_addr, 0, sizeof(struct sockaddr_un));
  client_addr.sun_family = AF_UNIX;
  strcpy(client_addr.sun_path, SOCK_PATH);

  /* connect to the server socket from another thread */

  pthread_t connect_thr;
  struct connectordata cd;
  cd.client_addr = client_addr;
  cd.client_sock_fd = client_sock_fd;
  int res = pthread_create(&connect_thr, NULL, _connector, (void *)&cd);

  int serv_conn_fd = accept(serv_sock_fd, NULL, NULL);

  pthread_join(connect_thr, NULL);

  if (write(client_sock_fd, clientmsg, strlen(clientmsg)) !=
      strlen(clientmsg)) {
    fail_msg("client write() failed");
  }

  struct sockmock result;
  result.serv_sock_fd = serv_sock_fd;
  result.serv_conn_fd = serv_conn_fd;
  result.client_sock_fd = client_sock_fd;
  return result;
}

char *sockmock_free(struct sockmock s, int count) {
  char *serv_resp = NULL;
  if (count > 0) {
    serv_resp = (char *)malloc(count);
    if (read(s.client_sock_fd, serv_resp, count) != count) {
      fail_msg("client could not read all count bytes from socket");
    }
  }

  /* make sure that there is no more data to be read (EOF) */
  char byte;
  if (read(s.client_sock_fd, &byte, 1) > 0) {
    fail_msg("there are unread bytes in client socket fd");
  }
  close(s.client_sock_fd);
  if (close(s.serv_conn_fd) != -1)
    fail_msg("server conn fd left open");
  close(s.serv_sock_fd);
  remove(SOCK_PATH);
  return serv_resp;
}
