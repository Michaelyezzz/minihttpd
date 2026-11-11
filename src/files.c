#include "files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#define BASE_DIR "./www"

void file_read_secure(const char *uri, file_content_t *content) {
    content->data = NULL;
    content->size = 0;
    content->status_code = 404;

    char file_path[512];
    if (strcmp(uri, "/") == 0) {
        snprintf(file_path, sizeof(file_path), "%s/index.html", BASE_DIR);
    } else {
        snprintf(file_path, sizeof(file_path), "%s%s", BASE_DIR, uri);
    }

    // 1. Resolver ruta del archivo
    char resolved_path[PATH_MAX];
    if (realpath(file_path, resolved_path) == NULL) {
        printf("[DEBUG] 404: No se encontró el archivo en %s\n", file_path);
        content->status_code = 404;
        return;
    }

    // 2. Resolver ruta de la carpeta base (Posible 500)
    char resolved_base[PATH_MAX];
    if (realpath(BASE_DIR, resolved_base) == NULL) {
        perror("[DEBUG] 500: Fallo realpath en BASE_DIR");
        content->status_code = 500;
        return;
    }

    // 3. Prevenir Directory Traversal
    if (strncmp(resolved_base, resolved_path, strlen(resolved_base)) != 0) {
        printf("[DEBUG] 403: Intento de evasión hacia %s\n", resolved_path);
        content->status_code = 403;
        return;
    }

    // 4. Abrir archivo
    FILE *f = fopen(resolved_path, "rb");
    if (!f) {
        perror("[DEBUG] 403: Fallo fopen (sin permisos)");
        content->status_code = 403;
        return;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    // 5. Reservar memoria (Posible 500)
    content->data = malloc(fsize + 1);
    if (!content->data) {
        perror("[DEBUG] 500: Fallo malloc (memoria insuficiente)");
        fclose(f);
        content->status_code = 500;
        return;
    }

    // 6. Leer archivo (Posible 500)
    size_t bytes_read = fread(content->data, 1, fsize, f);
    if (fsize > 0 && bytes_read != (size_t)fsize) {
        printf("[DEBUG] 500: Fallo fread. Esperaba %ld bytes, leyó %zu\n", fsize, bytes_read);
        free(content->data);
        content->data = NULL;
        fclose(f);
        content->status_code = 500;
        return;
    }

    content->data[fsize] = '\0';
    content->size = (size_t)fsize;
    content->status_code = 200;
    
    fclose(f);
}

void file_free(file_content_t *content) {
    if (content->data) {
        free(content->data);
        content->data = NULL;
        content->size = 0;
    }
}