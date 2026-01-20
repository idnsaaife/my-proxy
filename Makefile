CC = gcc
CFLAGS = -Wall -Wextra -Werror -pthread -Ilibs/picohttpparser -Isrc -O2 -g -fsanitize=thread
LDFLAGS = -lpthread -fsanitize=thread
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
          src/logger.c \
          libs/picohttpparser/picohttpparser.c

OBJECTS = $(SOURCES:%.c=$(OBJDIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(TARGET) $(OBJDIR) results *.dat

.PHONY: all clean
