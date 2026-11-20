#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

int make_socket_non_blocking(int sfd){
    int flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1) return -1;

    flags |= O_NONBLOCK;
    if(fcntl(sfd, F_SETFL, flags) == -1) return -1;

    return 0;
}

int server_init(server_t *server, uint16_t port) {
    server->port = port;
    server->epoll_fd = -1;

    server->sd = socket(AF_INET, SOCK_STREAM, 0);
    if(server->sd < 0){
        perror("Error: socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(server->sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))<0){
        perror("Error: setsockopt");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; 
    server_addr.sin_port = htons(port);

    if (bind(server->sd, (struct sockaddr *)&server_addr, sizeof(server_addr))<0){
        perror("Error: bind");
        return -1;
    }

    if (make_socket_non_blocking(server->sd) == -1){
        perror("Error: fcntl non-blocking");
        return -1;
    }

    if (listen(server->sd, SOMAXCONN) < 0){
        perror("Error: listen");
        return -1;
    }

    printf("Servidor HTTP escuchando en el puerto %d...\n", port);
    return 0;
}

void server_close(server_t *server){
    if(server->sd >= 0) close(server->sd);
    if(server->epoll_fd >= 0) close(server->epoll_fd);
}