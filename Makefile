CC = gcc
CFLAGS = -Wall -Wextra -Werror -pthread -Ilib -Isrc -O2 -g -fsanitize=thread
TARGET = proxy
OBJDIR = obj

SOURCES = src/main.c \
          src/cache.c \
          src/list.c \
          src/hashtable.c \
          src/http_parser.c \
          src/thread_pool.c \
          src/server_fetch.c \
          src/client_handler.c \
          lib/picohttpparser.c

OBJECTS = $(SOURCES:%.c=$(OBJDIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(TARGET) $(OBJDIR) results *.dat

.PHONY: all clean
