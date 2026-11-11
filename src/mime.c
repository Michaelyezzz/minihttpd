#include "mime.h"
#include <string.h>

const char* mime_get_type(const char *filename){
    // Buscar el último punto en el nombre del archivo
    const char *dot = strrchr(filename, '.');

    // Sí no hay punto, asuminos que son datos binarios genericos
    if (!dot || dot == filename){
        return "application/octet-stream";
    }

    if (strcmp(dot, ".html") == 0) return "text/html";
    if (strcmp(dot, ".css") == 0) return "text/css";
    if (strcmp(dot, ".js") == 0) return "application/javascript";
    if (strcmp(dot, ".png") == 0) return "image/png";
    if (strcmp(dot, ".jpg") == 0) return "image/jpeg";

    //por defecto, texto plano
    return "text/plain";
}