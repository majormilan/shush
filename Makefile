# Makefile for Simple Humane Shell (shush)

# Compiler
CC = diet gcc

# Version string (optional, override with make VERSION=your_version)
VERSION ?=

# Build directory
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
LIBTLINE_BUILD_DIR = $(BUILD_DIR)/libtline
LIBTINYIO_BUILD_DIR = $(BUILD_DIR)/libtinyio

# Compilation flags
CFLAGS = -Wall -Os -static -ffunction-sections -fdata-sections
# Conditionally add VERSION_STRING if VERSION is set
ifneq ($(VERSION),)
CFLAGS += -DVERSION_STRING=\"$(VERSION)\"
endif
LDFLAGS = -Wl,--gc-sections -L$(LIBTLINE_BUILD_DIR) -ltline -L$(LIBTINYIO_BUILD_DIR) -ltinyio

# Target executable
TARGET = $(BUILD_DIR)/shush

# Source files
SRCS = shush.c builtins.c parse.c terminal.c init.c lexer.c session.c

# Object files
OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRCS))

# Library and source for libtline
LIBTLINE_DIR = libtline
LIBTLINE_SRCS = $(LIBTLINE_DIR)/readline.c $(LIBTLINE_DIR)/utf8.c $(LIBTLINE_DIR)/tab.c
LIBTLINE_OBJS = $(patsubst $(LIBTLINE_DIR)/%.c,$(LIBTLINE_BUILD_DIR)/%.o,$(LIBTLINE_SRCS))
LIBTLINE_LIB = $(LIBTLINE_BUILD_DIR)/libtline.a

# Library and source for libtinyio
LIBTINYIO_DIR = libtinyio
LIBTINYIO_SRCS = $(LIBTINYIO_DIR)/stdio.c $(LIBTINYIO_DIR)/string.c $(LIBTINYIO_DIR)/signal.c $(LIBTINYIO_DIR)/pwd.c
LIBTINYIO_OBJS = $(patsubst $(LIBTINYIO_DIR)/%.c,$(LIBTINYIO_BUILD_DIR)/%.o,$(LIBTINYIO_SRCS))
LIBTINYIO_LIB = $(LIBTINYIO_BUILD_DIR)/libtinyio.a

# Installation prefix and directory
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

# Default rule to build the target (dynamic version)
all: $(TARGET)

# Rule for custom version
custom: CFLAGS += -DVERSION_STRING=\"$(VERSION)\"
custom: $(TARGET)

# Rule to compile and link the target
$(TARGET): $(OBJS) $(LIBTLINE_LIB) $(LIBTINYIO_LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(TARGET)
	strip --strip-unneeded $(TARGET)

# Rule to compile libtline
$(LIBTLINE_LIB): $(LIBTLINE_OBJS) | $(LIBTLINE_BUILD_DIR)
	ar rcs $@ $^

$(LIBTLINE_BUILD_DIR)/%.o: $(LIBTLINE_DIR)/%.c | $(LIBTLINE_BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to compile libtinyio
$(LIBTINYIO_LIB): $(LIBTINYIO_OBJS) | $(LIBTINYIO_BUILD_DIR)
	ar rcs $@ $^

$(LIBTINYIO_BUILD_DIR)/%.o: $(LIBTINYIO_DIR)/%.c | $(LIBTINYIO_BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to compile source files into object files
$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create build directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(LIBTLINE_BUILD_DIR):
	mkdir -p $(LIBTLINE_BUILD_DIR)

$(LIBTINYIO_BUILD_DIR):
	mkdir -p $(LIBTINYIO_BUILD_DIR)

# Rule to install the target
install: $(TARGET)
	install -d $(BINDIR)
	install -m 0755 $(TARGET) $(BINDIR)

# Rule to uninstall the target
uninstall:
	rm -f $(BINDIR)/shush

# Clean rule to remove compiled files
clean:
	rm -rf $(BUILD_DIR)

# Phony targets
.PHONY: all clean install uninstall custom
