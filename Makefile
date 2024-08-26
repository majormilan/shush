# Makefile for Simple Humane Shell (shush)

# Compiler
CC = musl-gcc

# Compilation flags
CFLAGS = -Wall -static -O2 -ffunction-sections -fdata-sections -Ilinenoise
LDFLAGS = -Wl,--gc-sections

# Target executable
TARGET = shush

# Source files
SRCS = shush.c builtins.c parse.c linenoise/linenoise.c init.c

# Object files
OBJS = $(SRCS:.c=.o)

# Installation prefix and directory
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

# Default rule to build the target
all: $(TARGET)

# Rule to compile and link the target
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJS)
	strip --strip-unneeded $(TARGET)

# Rule to compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to install the target
install: $(TARGET)
	install -d $(BINDIR)
	install -m 0755 $(TARGET) $(BINDIR)

# Rule to uninstall the target
uninstall:
	rm -f $(BINDIR)/$(TARGET)

# Clean rule to remove compiled files
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets
.PHONY: all clean install uninstall
