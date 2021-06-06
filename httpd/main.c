#include <signal.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>

#include <tcp_utils.h>
#include <logger.h>
#include <client.h>
#include <io.h>
#include <args.h>
#include <monitor.h>
#include <http_parser.h>


static void sigterm_handler(const int signal);

void handle_writes(struct selector_key *key);

void handle_reads(struct selector_key *key);

void handle_creates(struct selector_key *key);


int main(int argc, char **argv) {

    // Read arguments and close stdin

    struct proxy_args args;
    parse_args(argc, argv, &args);
    
    targetHost = "localhost";
    targetPort = "8081";

    close(0);

    // Register handlers for closing program appropiately

    signal(SIGTERM, sigterm_handler);
    signal(SIGINT,  sigterm_handler);

    // Start monitor on another thread

    char mng_port[6] = {0};
    snprintf(mng_port, 6, "%d", args.mng_port);

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, start_monitor, mng_port);

    // Start accepting connections

    char proxy_port[6] = {0};
    snprintf(proxy_port, 6, "%d", args.proxy_port);
      
    int serverSocket = create_tcp_server(proxy_port);
    if(serverSocket < 0) {
        log(ERROR, "Creating passive socket");
        exit(EXIT_FAILURE);
    }

    // Start handling connections

    int res = handle_connections(serverSocket, handle_creates, handle_reads, handle_writes);
    if(res < 0) {
        log(ERROR, "Handling connections");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    close(serverSocket);

    return EXIT_SUCCESS;
    
}


void handle_writes(struct selector_key *key) {

    log(DEBUG, "Entered handle_writes with src %d", key->src_socket);

    if (buffer_can_read(&(key->item->conn_buffer))) {

        size_t len;
        uint8_t *bytes = buffer_read_ptr(&(key->item->conn_buffer), &len);

        int sentBytes = bsend(key->dst_socket, bytes, len);

        buffer_read_adv(&(key->item->conn_buffer), sentBytes);
        FD_CLR(key->dst_socket, &(key->s)->slave_w);

        log(DEBUG, "Sent %d bytes to socket %d\n", sentBytes, key->dst_socket);

    }

}


void handle_reads(struct selector_key *key) {

    if (buffer_can_write(&(key->item->conn_buffer))) {

        size_t space;
        uint8_t *ptr = buffer_write_ptr(&(key->item->conn_buffer), &space);

        int readBytes = read(key->src_socket, ptr, space);

        buffer_write_adv(&(key->item->conn_buffer), readBytes);
        FD_SET(key->dst_socket, &(key->s)->slave_w);

        log(DEBUG, "Received %d bytes from socket %d\n", readBytes, key->src_socket);

        if (readBytes <= 0)
            item_kill(key->s, key->item);
        
        // item_state s = key->item->state;

        size_t availableBytes;
        buffer_read_ptr(&(key->item->conn_buffer), &availableBytes);
         struct request req;
         parserData p;
         parse_http_request(ptr , &req,&p, availableBytes);

    }

}


void handle_creates(struct selector_key *key) {

    struct sockaddr_in address;
    int addrlen = sizeof(struct sockaddr_in);

    int masterSocket = key->s->fds[0].client_socket;


    // Accept the client connection

    int clientSocket = accept(masterSocket, (struct sockaddr *) &address, (socklen_t *) &addrlen);
    if (clientSocket < 0) {
        log(FATAL, "Accepting new connection")
        exit(EXIT_FAILURE);
    }

    log(INFO, "New connection - FD: %d - IP: %s - Port: %d\n", clientSocket, inet_ntoa(address.sin_addr),
        ntohs(address.sin_port));

    if (strstr(proxy_conf.clientBlacklist, inet_ntoa(address.sin_addr)) != NULL) {
        log(INFO, "Kicking %s due to blacklist", inet_ntoa(address.sin_addr));
        item_kill(key->s, key->item);
        log(INFO, "Kicked %s due to blacklist", inet_ntoa(address.sin_addr));
    }


    // Initiate a connection to target

    int targetSocket = setupClientSocket(key->s->targetHost, key->s->targetPort);
    if (targetSocket < 0) {
        log(ERROR, "Failed to connect to target");
    }

    key->item->client_socket = clientSocket;
    key->item->target_socket = targetSocket;

    key->item->client_interest = OP_READ | OP_WRITE;    // OP_READ
    key->item->target_interest = OP_READ | OP_WRITE;    // OP_NOOP
    key->item->state = CONNECT;

    buffer_init(&(key->item->conn_buffer), CONN_BUFFER, malloc(CONN_BUFFER));
}


static void sigterm_handler(const int signal) {

    printf("signal %d, cleaning up and exiting\n",signal);
    exit(EXIT_SUCCESS);

}

