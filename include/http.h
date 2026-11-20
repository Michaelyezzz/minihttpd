#ifndef HTTP_H
#define HTTP_H

#define MAX_REQUEST_LINE 512   // tamaño máximo de la línea "GET /ruta HTTP/1.1"

typedef struct{
    char method[16];
    char uri[256];
    char version[16];

    char host[256];
    char connection[64];
    char user_agent[256];
} http_request_t;

int http_parse_request(const char *raw_request, http_request_t *req);

#endif