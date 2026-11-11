#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>

typedef struct {
    int server_fd;
    int epoll_fd;
    uint16_t port;
} server_t;

int server_init(server_t *server, uint16_t port);
void server_close(server_t * server);
int make_socket_non_blocking(int sfd);

#endif
