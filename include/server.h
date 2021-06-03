#ifndef SERVER_H_
#define SERVER_H_

#include <selector.h>

#define MAX_CLIENTS 500
#define MAX_CONNECTIONS MAX_CLIENTS * 2 + 1

char *targetHost, *targetPort;

// Create, bind, and listen a new TCP server socket
int create_server_socket(const char *service);

// Accept a new TCP connection on a server socket
int handle_connections(
    int server,
    void (*handle_creates) (struct selector_key *key),
    void (*handle_reads) (struct selector_key *key),
    void (*handle_writes) (struct selector_key *key)
);

#endif 
