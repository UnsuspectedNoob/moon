CC = gcc
CFLAGS = -g -std=c99 -Wall -Wextra -I./src
SRC = src/chunk.c src/compiler.c src/debug.c src/main.c src/memory.c src/object.c src/scanner.c src/table.c src/value.c src/vm.c
OBJ = $(SRC:.c=.o)
TARGET = moon

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET) $(OBJ)
