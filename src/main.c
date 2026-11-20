#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "server.h"
#include "http.h"
#include "mime.h"
#include "files.h"

#define DEFAULT_PORT 9999
#define MAX_EVENTS 64
#define MAX_FDS 4096

typedef struct {
    char *buf;      
    size_t len;     
    size_t sent;    
} pending_t;

static pending_t pending[MAX_FDS];
static char close_after[MAX_FDS];

static void pending_clear(int fd) {
    free(pending[fd].buf);
    pending[fd].buf = NULL;
    pending[fd].len = 0;
    pending[fd].sent = 0;
}

static void close_conn(int fd) {
    pending_clear(fd);
    close_after[fd] = 0;
    close(fd);
}

static int flush_pending(int fd) {
    pending_t *p = &pending[fd];
    while (p->sent < p->len) {
        ssize_t n = write(fd, p->buf + p->sent, p->len - p->sent);
        if (n > 0) {
            p->sent += (size_t)n;
        } else if (n == -1 && errno == EINTR) {
            continue;
        } else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return 0;
        } else {
            return -1;
        }
    }
    return 1;
}

static void epoll_set_events(int epoll_fd, int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
}

static const char *conn_header(int fd) {
    return close_after[fd] ? "close" : "keep-alive";
}

static int send_response(int epoll_fd, int fd, const char *header, const char *body, size_t body_len) {
    size_t header_len = strlen(header);
    char *buf = malloc(header_len + body_len);
    if (!buf) {
        perror("Error: malloc en send_response");
        close_conn(fd);
        return -1;
    }
    memcpy(buf, header, header_len);
    memcpy(buf + header_len, body, body_len);

    pending_clear(fd);
    pending[fd].buf = buf;
    pending[fd].len = header_len + body_len;

    int r = flush_pending(fd);
    if (r == 0) {
        epoll_set_events(epoll_fd, fd, EPOLLIN | EPOLLOUT | EPOLLET);
        return 0;
    }
    if (r == -1 || close_after[fd]) {
        close_conn(fd);
        return -1;
    }
    pending_clear(fd);
    return 0;
}

static int send_error(int epoll_fd, int fd, int code, const char *reason) {
    char body[96], header[256];
    int blen = snprintf(body, sizeof(body), "<h1>%d %s</h1>", code, reason);
    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\nContent-Type: text/html\r\nContent-Length: %d\r\nConnection: %s\r\n\r\n",
             code, reason, blen, conn_header(fd));
    return send_response(epoll_fd, fd, header, body, (size_t)blen);
}

static int handle_request(int epoll_fd, int nsd, const char *buffer) {
    http_request_t req;
    int ret;

    if (http_parse_request(buffer, &req) != 0) {
        close_after[nsd] = 1;   
        printf("Petición mal formada (400)\n");
        return send_error(epoll_fd, nsd, 400, "Bad Request");
    }

    printf("Petición: [%s] [%s] [%s]\n", req.method, req.uri, req.version);

    int is_http10 = (strcmp(req.version, "HTTP/1.0") == 0);
    close_after[nsd] = (strncasecmp(req.connection, "close", 5) == 0) ||
                       (is_http10 && strncasecmp(req.connection, "keep-alive", 10) != 0);

    if (strcmp(req.method, "GET") != 0) {
        printf("Método %s rechazado (405)\n", req.method);
        return send_error(epoll_fd, nsd, 405, "Method Not Allowed");
    }

    file_content_t file;
    file_read_secure(req.uri, &file);

    if (file.status_code == 200) {
        const char *mime = (strcmp(req.uri, "/") == 0) ? "text/html" : mime_get_type(req.uri);
        char header[512];
        snprintf(header, sizeof(header),
                 "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: %s\r\n\r\n",
                 mime, file.size, conn_header(nsd));
        ret = send_response(epoll_fd, nsd, header, file.data, file.size);
        printf("-> Enviado: %s (%zu bytes) [MIME: %s]\n", req.uri, file.size, mime);
    } else {    
        int code = file.status_code;
        ret = send_error(epoll_fd, nsd, code,
                         code == 404 ? "Not Found" : code == 403 ? "Forbidden" : "Internal Server Error");
        printf("-> Error %d: %s\n", code, req.uri);
    }

    file_free(&file);
    return ret;
}

int main(void) {
    printf("Iniciando MiniHTTPd...\n");

    signal(SIGPIPE, SIG_IGN);

    server_t server = {-1, -1, DEFAULT_PORT};

    if (server_init(&server, DEFAULT_PORT) < 0) {
        fprintf(stderr, "Fallo fatal al inicializar el servidor.\n");
        server_close(&server);
        return EXIT_FAILURE;
    }

    server.epoll_fd = epoll_create1(0);
    if (server.epoll_fd == -1) {
        perror("Error: epoll_create1");
        server_close(&server);
        return EXIT_FAILURE;
    }

    struct epoll_event event = { .events = EPOLLIN, .data.fd = server.sd };

    if (epoll_ctl(server.epoll_fd, EPOLL_CTL_ADD, server.sd, &event) == -1) {
        perror("Error: epoll_ctl");
        server_close(&server);
        return EXIT_FAILURE;
    }

    struct epoll_event events[MAX_EVENTS];
    printf("Entrando al bucle de eventos (Presiona Ctrl+C para apagar)...\n");

    while (1) {
        int num_events = epoll_wait(server.epoll_fd, events, MAX_EVENTS, -1);
        if (num_events == -1) {
            if (errno == EINTR) continue;
            perror("Error en epoll_wait");
            break;
        }

        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == server.sd) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int nsd = accept(server.sd, (struct sockaddr *)&client_addr, &client_len);

                if (nsd == -1) {
                    perror("Error al aceptar cliente");
                    continue;
                }

                make_socket_non_blocking(nsd);
                close_after[nsd] = 0;

                struct epoll_event client_event = { .events = EPOLLIN | EPOLLET, .data.fd = nsd };

                if (nsd >= MAX_FDS || epoll_ctl(server.epoll_fd, EPOLL_CTL_ADD, nsd, &client_event) == -1) {
                    perror("Error al registrar cliente");
                    close(nsd);
                } else {
                    printf("¡Cliente conectado y registrado!\n");
                }

            } else {
                int nsd = events[i].data.fd;
                uint32_t ev = events[i].events;

                if ((ev & EPOLLOUT) && pending[nsd].buf) {
                    int r = flush_pending(nsd);
                    if (r == -1 || (r == 1 && close_after[nsd])) {
                        close_conn(nsd);
                        continue;
                    }
                    if (r == 1) {
                        pending_clear(nsd);
                        epoll_set_events(server.epoll_fd, nsd, EPOLLIN | EPOLLET);
                    }
                }

                if (!(ev & EPOLLIN)) {
                    if (ev & (EPOLLERR | EPOLLHUP)) close_conn(nsd);
                    continue;
                }

                while (1) {
                    char buffer[2048];
                    ssize_t bytes_read = read(nsd, buffer, sizeof(buffer) - 1);

                    if (bytes_read > 0) {
                        buffer[bytes_read] = '\0';
                        if (handle_request(server.epoll_fd, nsd, buffer) == -1) break;
                    } else if (bytes_read == 0) {
                        close_conn(nsd);    
                        break;
                    } else if (errno == EINTR) {
                        continue;
                    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        break;              
                    } else {
                        close_conn(nsd);
                        break;
                    }
                }
            }
        }
    }

    server_close(&server);
    return EXIT_SUCCESS;
}