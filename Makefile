# Makefile for Simple Humane Shell (shush)

# Compiler
CC = musl-gcc

# Compilation flags
CFLAGS = -Wall -static -O2 -ffunction-sections -fdata-sections
LDFLAGS = -Wl,--gc-sections

# Target executable
TARGET = shush

# Source files
SRCS = shush.c builtins.c parse.c

# Object files
OBJS = $(SRCS:.c=.o)

# Default rule to build the target
all: $(TARGET)

# Rule to compile and link the target
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJS)
	strip --strip-unneeded $(TARGET)

# Rule to compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule to remove compiled files
clean:
	rm -f $(OBJS) $(TARGET)
