#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

// Función auxiliar para ahcer que el socket nos e bloquee

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

    // 1. Craer el socket TPC IPv4
    server->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server->server_fd < 0){
        perror("Error: socket");
        return -1;
    }

    // 2. Configurar el SO_REUSEADDR
    int opt = 1;
    if (setsockopt(server->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))<0){
        perror("Error: setsockopt");
        return -1;
    }

    // 3. Asignar IP y puerto (bind)

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Escucha todas las interfaces
    server_addr.sin_port = htons(port);

    if (bind(server->server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr))<0){
        perror("Error: bind");
        return -1;
    }

    // 4. Hacer el socket no bloqueante para prepararlo para epoll
    if (make_socket_non_blocking(server->server_fd) == 1){
        perror("Error: fcntl non-blocking");
        return -1;
    }

    // 5. Poner en modo escucha (listen)
    if (listen(server->server_fd, SOMAXCONN) < 0){
        perror("Error: listen");
        return -1;
    }

    printf("Servidor HTTP escuchando en el puerto %d...\n", port);
    return 0;
}

void server_close(server_t *server){
    if(server->server_fd >= 0) close(server->server_fd);
    if(server->epoll_fd >= 0) close(server->epoll_fd);
}






