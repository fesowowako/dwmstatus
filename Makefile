SRCS=$(shell find -name "*.c")

DEPS=x11

#DEBUG=-fsanitize=address,undefined,leak -ggdb

LDFLAGS=-lpthread $(shell pkgconf --libs $(DEPS))
CFLAGS=-std=c99 -pthread -Wpedantic -Wall -Wextra $(shell pkgconf --cflags $(DEPS)) $(DEBUG)
BIN=dwmstatus
CC=clang

PREFIX=/usr/local

all:
	$(CC) $(SRCS) $(CFLAGS) $(LDFLAGS) -o $(BIN)

run:
	./$(BIN)

install: $(BIN)
	install -m 755 $(BIN) $(PREFIX)/bin/$(BIN)
