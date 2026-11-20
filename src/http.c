#include "http.h"
#include <stdio.h>
#include <string.h>

#define MAX_REQUEST_LINE 512

int http_parse_request(const char *raw_request, http_request_t *req){
    memset(req, 0, sizeof(http_request_t));

    const char *eol = strstr(raw_request, "\r\n");
    if (!eol || (size_t)(eol - raw_request) > MAX_REQUEST_LINE){
        return -1;
    }

    if (sscanf(raw_request, "%15s %255s %15s", req->method, req->uri, req->version) != 3){
        return -1;
    }

    if (strlen(req->method)  >= sizeof(req->method)  - 1 ||
        strlen(req->uri)     >= sizeof(req->uri)     - 1 ||
        strlen(req->version) >= sizeof(req->version) - 1){
        return -1;
    }

    const char *line = strstr(raw_request, "\r\n");
    while(line){
        line += 2;

        if (strncmp(line, "\r\n", 2)== 0) break;

        if(strncmp(line, "Host: ", 6) == 0){
            sscanf(line + 6, "%255[^\r\n]", req->host);
        } else if (strncmp(line, "Connection: ", 12) == 0) {
            sscanf(line + 12, "%63[^\r\n]", req->connection);      
        } else if (strncmp(line, "User-Agent: ", 12) == 0){
            sscanf(line + 12, "%255[^\r\n]", req->user_agent);
        }

        line = strstr(line, "\r\n");
    }

    return 0;
}