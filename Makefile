# Makefile for Simple Humane Shell (shush)

# Compiler
CC = diet gcc
#CC = musl-gcc
#CC = gcc

# Compilation flags
CFLAGS = -Wall -static -O2 -ffunction-sections -fdata-sections
LDFLAGS = -Wl,--gc-sections -Llibtline -ltline

# Target executable
TARGET = shush

# Source files
SRCS = shush.c builtins.c parse.c terminal.c init.c

# Object files
OBJS = $(SRCS:.c=.o)

# Library and source for libtline
LIBTLINE_DIR = libtline
LIBTLINE_SRCS = $(LIBTLINE_DIR)/readline.c
LIBTLINE_OBJS = $(LIBTLINE_SRCS:.c=.o)
LIBTLINE_LIB = $(LIBTLINE_DIR)/libtline.a

# Installation prefix and directory
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

# Default rule to build the target
all: $(TARGET)

# Rule to compile and link the target
$(TARGET): $(OBJS) $(LIBTLINE_LIB)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(TARGET)
	strip --strip-unneeded $(TARGET)

# Rule to compile libtline
$(LIBTLINE_LIB): $(LIBTLINE_OBJS)
	ar rcs $@ $^

$(LIBTLINE_DIR)/%.o: $(LIBTLINE_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

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
	rm -f $(OBJS) $(TARGET) $(LIBTLINE_OBJS) $(LIBTLINE_LIB)

# Phony targets
.PHONY: all clean install uninstall
