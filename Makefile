CC = gcc
CFLAGS = -Wall -Wextra -pthread -O1 -Ilib -Isrc
TARGET = proxy
SRCDIR = src
LIBDIR = lib
OBJDIR = obj

SOURCES = $(SRCDIR)/main.c \
          $(SRCDIR)/cache.c \
          $(SRCDIR)/http_parser.c \
          $(SRCDIR)/thread_pool.c \
          $(SRCDIR)/server_fetch.c \
          $(SRCDIR)/client_handler.c \
          $(LIBDIR)/picohttpparser.c

OBJECTS = $(SOURCES:%.c=$(OBJDIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(TARGET) $(OBJDIR) results

test: $(TARGET)
	./test_proxy.sh

.PHONY: all clean test