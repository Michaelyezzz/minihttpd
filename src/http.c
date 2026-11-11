#include "http.h"
#include <stdio.h>
#include <string.h>

int http_parse_request(const char *raw_request, http_request_t *req){
    memset(req, 0, sizeof(http_request_t));

    // Extraemos método, uri y versión de la primera línea.
    // %15s significa: lee una cadena hasta el primer espacio, máximo 15 caracteres.
    // Esto previene los Buffer Overflows exigidos en el proyecto.

    // 1. Extraer la línea de la solicitud (request line)
    if (sscanf(raw_request, "%15s %25s %15s", req->method, req->uri, req->version) != 3){

        //Si pudo extraer los 3 elementos al petición esta mal formada
        return -1;
    }

    // 2. Extraer los encabezados buscando los saltos de línea (\r\n)
    const char *line = strstr(raw_request, "\r\n");
    while(line){
        line += 2;

        // Si hay doble salto de linea ya no hay encabezados
        if (strncmp(line, "\r\n", 2)== 0) break;

        //Buscar cada encabezado requerido
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