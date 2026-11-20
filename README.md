# MiniHTTPd

Servidor HTTP/1.1 minimalista escrito en C, sin bibliotecas HTTP externas. Proyecto integrador de Computación Distribuida — Escuela Politécnica Nacional.

## Características

- Arquitectura event-driven con `epoll` (single-thread, no `fork`)
- Sockets no bloqueantes con `EPOLLET` y lectura hasta `EAGAIN`
- Conexiones persistentes (keep-alive) por defecto
- Envío parcial con buffer pendiente y `EPOLLOUT` para archivos grandes
- Detección de tipo MIME por extensión del archivo
- Protección contra directory traversal (`realpath` + validación de prefijo → 403)
- Protección contra buffer overflow (`snprintf`, `sscanf` con ancho limitado)
- Códigos de respuesta: 200, 400, 403, 404, 405, 500

## Estructura

```
minihttpd/
├── include/          # Cabeceras (.h)
│   ├── files.h
│   ├── http.h
│   ├── mime.h
│   └── server.h
├── src/              # Código fuente
│   ├── main.c        # Bucle de eventos y lógica principal
│   ├── server.c      # Inicialización del socket servidor
│   ├── http.c        # Parsing de peticiones HTTP
│   ├── mime.c        # Tipos MIME por extensión
│   └── files.c       # Lectura segura de archivos
├── www/              # Archivos estáticos servidos
│   ├── index.html
│   ├── style.css
│   └── image.jpg
└── Makefile
```

## Compilación y ejecución

```bash
make
./minihttpd
```

El servidor escucha en el puerto **9999** por defecto. Acceder a `http://localhost:9999` desde un navegador.

## Pruebas

```bash
# Página principal
curl http://localhost:9999/

# Archivo inexistente → 404
curl http://localhost:9999/noexiste.html

# Directory traversal → 403
curl --path-as-is http://localhost:9999/../../../../etc/passwd

# Método no permitido → 405
curl -X POST http://localhost:9999/

# Solicitud mal formada → 400 (URI > 255 caracteres)
curl "http://localhost:9999/$(python3 -c 'print("A"*300)')"
```

## Autor

Michael Yánez — EPN, 2026