#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "server.h"
#include "http.h"
#include <string.h>
#include "mime.h"
#include "files.h"

#define DEFAULT_PORT 8080
#define MAX_EVENTS 64

int main(void) {
    printf("Iniciando MiniHTTPd...\n");

    server_t server = {-1, -1, DEFAULT_PORT};

    if (server_init(&server, DEFAULT_PORT) < 0) {
        fprintf(stderr, "Fallo fatal al inicializar el servidor.\n");
        return EXIT_FAILURE;
    }

    // Crear la instancia de epoll
    server.epoll_fd = epoll_create1(0);
    if (server.epoll_fd == -1) {
        perror("Error: epoll_create1");
        server_close(&server);
        return EXIT_FAILURE;
    }

    // Registrar el socket del servidor en epoll
    struct epoll_event event;
    event.events = EPOLLIN; 
    event.data.fd = server.server_fd;

    if (epoll_ctl(server.epoll_fd, EPOLL_CTL_ADD, server.server_fd, &event) == -1) {
        perror("Error: epoll_ctl");
        server_close(&server);
        return EXIT_FAILURE;
    }

    struct epoll_event events[MAX_EVENTS];
    printf("Entrando al bucle de eventos (Presiona Ctrl+C para apagar)...\n");

    // Bucle infinito de eventos
    while (1) {
        int num_events = epoll_wait(server.epoll_fd, events, MAX_EVENTS, -1);
        if (num_events == -1) {
            perror("Error en epoll_wait");
            break;
        }

        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == server.server_fd) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(server.server_fd, (struct sockaddr *)&client_addr, &client_len);
                
                if (client_fd == -1) {
                    perror("Error al aceptar cliente");
                    continue;
                }

                make_socket_non_blocking(client_fd);

                struct epoll_event client_event;
                client_event.events = EPOLLIN | EPOLLET; 
                client_event.data.fd = client_fd;
                
                if (epoll_ctl(server.epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event) == -1) {
                    perror("Error al añadir cliente a epoll");
                    close(client_fd);
                } else {
                    printf("¡Cliente conectado y registrado!\n");
                }

            } else {
                int client_fd = events[i].data.fd;
                char buffer[2048] = {0}; 
                
                ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
                
                if (bytes_read > 0) {
                    printf("--- Petición HTTP recibida ---\n");
                    
                    http_request_t req;
                    if (http_parse_request(buffer, &req) == 0) {
                        printf("Método extraído: [%s]\n", req.method);
                        printf("URI extraída:    [%s]\n", req.uri);
                        printf("Versión:         [%s]\n", req.version);
                        
                        // El proyecto exige rechazar todo lo que no sea GET con un 405
                        if (strcmp(req.method, "GET") == 0) {
                            // 1. Buscar y leer el archivo de forma segura
                            file_content_t file;
                            file_read_secure(req.uri, &file);

                            char header_buffer[512];

                            // 2. Responder según el código de estado devuelto por files.c
                            if (file.status_code == 200) {
                                // Determinar el tipo MIME (si es la raíz "/", forzamos text/html para index.html)
                                const char *mime = (strcmp(req.uri, "/") == 0) ? "text/html" : mime_get_type(req.uri);
                                
                                // Construir los encabezados HTTP dinámicos
                                snprintf(header_buffer, sizeof(header_buffer),
                                         "HTTP/1.1 200 OK\r\n"
                                         "Content-Type: %s\r\n"
                                         "Content-Length: %zu\r\n"
                                         "Connection: keep-alive\r\n"
                                         "\r\n", 
                                         mime, file.size);
                                
                                // Enviar encabezados y luego el contenido binario del archivo
                                write(client_fd, header_buffer, strlen(header_buffer));
                                write(client_fd, file.data, file.size);
                                
                                printf("-> Enviado: %s (%zu bytes) [MIME: %s]\n", req.uri, file.size, mime);
                            } 
                            else if (file.status_code == 404) {
                                snprintf(header_buffer, sizeof(header_buffer), 
                                         "HTTP/1.1 404 Not Found\r\nConnection: keep-alive\r\n\r\n<h1>404 Archivo no encontrado</h1>");
                                write(client_fd, header_buffer, strlen(header_buffer));
                                printf("-> Error 404: %s no encontrado.\n", req.uri);
                            } 
                            else if (file.status_code == 403) {
                                snprintf(header_buffer, sizeof(header_buffer), 
                                         "HTTP/1.1 403 Forbidden\r\nConnection: keep.alive\r\n\r\n<h1>403 Acceso Denegado</h1>");
                                write(client_fd, header_buffer, strlen(header_buffer));
                                printf("-> Error 403: Intento de acceso denegado a %s.\n", req.uri);
                            } 
                            else {
                                snprintf(header_buffer, sizeof(header_buffer), 
                                         "HTTP/1.1 500 Internal Server Error\r\nConnection: keep-alive\r\n\r\n<h1>500 Error Interno</h1>");
                                write(client_fd, header_buffer, strlen(header_buffer));
                                printf("-> Error 500: Fallo interno al leer %s.\n", req.uri);
                            }

                            // 3. Liberar la memoria reservada para el archivo
                            file_free(&file);

                        } else {
                            // Rechazar métodos como POST, PUT, etc. (Código 405)
                            const char *error_response = 
                                "HTTP/1.1 405 Method Not Allowed\r\n"
                                "Connection: close\r\n\r\n";
                            write(client_fd, error_response, strlen(error_response));
                            printf("¡Método %s rechazado (405)!\n", req.method);
                        }
                    } else {
                        printf("Error: Petición mal formada (400 Bad Request).\n");
                    }
                    
                    printf("------------------------------\n");
                    //close(client_fd);
                }
            }
        }
    }

    server_close(&server);
    return EXIT_SUCCESS;
}
