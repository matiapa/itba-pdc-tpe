PHONY = clean all

CFLAGS = -I include/ -Wall -Wextra -pedantic -pedantic-errors -std=c11 -g -D_POSIX_C_SOURCE=200112L -Wno-gnu-zero-variadic-macro-arguments

all: httpd

PROXY_OBJ = lib/address.o lib/args.o lib/buffer.o lib/client.o lib/http.o lib/io.o lib/logger.o lib/selector.o\
 lib/tcp_utils.o lib/udp_utils.o lib/stm.o httpd/main.o httpd/monitor.o httpd/proxy_stm.o httpd/doh_client.o

httpd: $(PROXY_OBJ)
	$(CC) -pthread $(CFLAGS) $(PROXY_OBJ) -o bin/httpd

clean:
	rm -rf $(PROXY_OBJ) bin/httpd