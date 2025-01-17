# Makefile for Simple Humane Shell (shush)

# Compiler
CC = diet gcc

# Compilation flags
CFLAGS = -Wall -static -Os -ffunction-sections -fdata-sections
LDFLAGS = -Wl,--gc-sections -Llibtline -ltline -Llibtinyio -ltinyio

# Target executable
TARGET = shush

# Source files
SRCS = shush.c builtins.c parse.c terminal.c init.c lexer.c

# Object files
OBJS = $(SRCS:.c=.o)

# Library and source for libtline
LIBTLINE_DIR = libtline
LIBTLINE_SRCS = $(LIBTLINE_DIR)/readline.c $(LIBTLINE_DIR)/utf8.c
LIBTLINE_OBJS = $(LIBTLINE_SRCS:.c=.o)
LIBTLINE_LIB = $(LIBTLINE_DIR)/libtline.a

# Library and source for libtinyio
LIBTINYIO_DIR = libtinyio
LIBTINYIO_SRCS = $(LIBTINYIO_DIR)/stdio.c $(LIBTINYIO_DIR)/string.c $(LIBTINYIO_DIR)/signal.c
LIBTINYIO_OBJS = $(LIBTINYIO_SRCS:.c=.o)
LIBTINYIO_LIB = $(LIBTINYIO_DIR)/libtinyio.a

# Installation prefix and directory
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

# Default rule to build the target
all: $(TARGET)

# Rule to compile and link the target
$(TARGET): $(OBJS) $(LIBTLINE_LIB) $(LIBTINYIO_LIB)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(TARGET)
	strip --strip-unneeded $(TARGET)

# Rule to compile libtline
$(LIBTLINE_LIB): $(LIBTLINE_OBJS)
	ar rcs $@ $^

$(LIBTLINE_DIR)/%.o: $(LIBTLINE_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to compile libtinyio
$(LIBTINYIO_LIB): $(LIBTINYIO_OBJS)
	ar rcs $@ $^

$(LIBTINYIO_DIR)/%.o: $(LIBTINYIO_DIR)/%.c
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
	rm -f $(OBJS) $(TARGET) $(LIBTLINE_OBJS) $(LIBTLINE_LIB) $(LIBTINYIO_OBJS) $(LIBTINYIO_LIB)

# Phony targets
.PHONY: all clean install uninstall
