CC = gcc
CFLAGS = -g -std=c99 -Wall -Wextra -I./src

# 1. THE MAGIC: Automatically find all .c files in the src/ directory
SRC = $(wildcard src/*.c)

# 2. Automatically generate the corresponding .o object file names
OBJ = $(SRC:.c=.o)

# The final executable name
TARGET = moon

# Default rule when you just type 'make'
all: $(TARGET)

# 3. Linking the final executable
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lm
	@echo "Build complete! Run ./$(TARGET) to start Moon."

# 4. Compiling individual .c files into .o files
# $< means "the first dependency" (the .c file)
# $@ means "the target" (the .o file)
src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# 5. Clean up all compiled files
clean:
	rm -f $(TARGET) $(OBJ)
	@echo "Cleaned up executable and object files."

# .PHONY tells Make these aren't actual files on the disk, just commands
.PHONY: all clean
