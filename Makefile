CFLAGS=-O3 -Wall -std=c89
LIB_SOURCES=libssh2-tunnel-example.c
LIB_DEPS=-lssh2	# libssh2

example:
	gcc $(CFLAGS) -o libssh2-tunnel-example $(LIB_DEPS) libssh2-tunnel-example.c

all: example
