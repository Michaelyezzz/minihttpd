#ifndef FILES_H
#define FILES_H

#include <stddef.h>

//Estructura para almacenar el resultado de la lectura
typedef struct{
    char *data;         // Puntero al contenido del archivo en memoria
    size_t size;        // Tamaño en bytes
    int status_code;    // Codigo HTTP (200, 403, 404, 500)
} file_content_t;

// Lee el archivo de forma segura previniendo "Directory Traversal"
void file_read_secure(const char *uri, file_content_t *content);

// Libera la memoria reservada dinamicamente
void file_free(file_content_t *content);

#endif