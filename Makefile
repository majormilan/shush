# Makefile for Simple Humane Shell (shush)

# Compiler
CC = musl-gcc

# Compilation flags
CFLAGS = -Wall -static

# Target executable
TARGET = shush

# Source files
SRCS = shush.c builtins.c

# Object files
OBJS = $(SRCS:.c=.o)

# Default rule to build the target
all: $(TARGET)

# Rule to compile and link the target
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Rule to compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule to remove compiled files
clean:
	rm -f $(OBJS) $(TARGET)

