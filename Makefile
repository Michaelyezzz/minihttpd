CC = gcc
CFLAGS = -Wall -Wextra -I./include

# Lista de todos los archivos fuente requeridos
SRCS = src/main.c src/server.c src/http.c src/mime.c src/files.c
OBJS = $(SRCS:.c=.o)
TARGET = minihttpd

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

clean:
	rm -f src/*.o $(TARGET)