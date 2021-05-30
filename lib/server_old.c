// #include <sys/socket.h>
// #include <unistd.h>
// #include <arpa/inet.h>
// #include <netdb.h>
// #include <string.h>
// #include <errno.h>
// #include <stdlib.h>
// #include "../include/logger.h"
// #include "../include/address.h"
// #include "../include/server.h"
// #include "../include/client.h"
// #include "../include/io.h"

// #define MAX_PENDING_CONN 5
// #define MAX_ADDR_BUFFER 128

// static char addrBuffer[MAX_ADDR_BUFFER];

// /* --------------------------------------------------
//    Resolves address and creates passive socket
// -------------------------------------------------- */

// int setupServerSocket(const char *service) {

// 	// Create address criteria

// 	struct addrinfo addrCriteria;
// 	memset(&addrCriteria, 0, sizeof(addrCriteria));

// 	addrCriteria.ai_family = AF_INET;
// 	addrCriteria.ai_flags = AI_PASSIVE;             // Accept on any address/port
// 	addrCriteria.ai_socktype = SOCK_STREAM;
// 	addrCriteria.ai_protocol = IPPROTO_TCP;

// 	// Resolve service string for posible addresses

// 	struct addrinfo *servAddr;
// 	int getaddr = getaddrinfo(NULL, service, &addrCriteria, &servAddr);
// 	if (getaddr != 0) {
// 		log(FATAL, "getaddrinfo() failed %s", gai_strerror(getaddr));
// 	}

// 	// Try to bind to an address and to start listening on it

// 	int servSock = -1;
// 	for (struct addrinfo *addr = servAddr; addr != NULL && servSock == -1; addr = addr->ai_next) {

// 		// Create socket and make it reusable

// 		servSock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
// 		if (servSock < 0){
// 			log(ERROR, "Creating passive socket");
// 			continue;
// 		}

// 		int opt = 1;
//     	setsockopt(servSock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

// 		// Bind and listen

// 		int bindRes = bind(servSock, addr->ai_addr, addr->ai_addrlen);
// 		if(bindRes < 0){
// 			log(ERROR, "Binding to server socket");
// 			close(servSock);
// 			servSock = -1;
// 		}

// 		int listenRes = listen(servSock, MAX_PENDING_CONN);
// 		if(listenRes < 0){
// 			log(ERROR, "Listening to server socket");
// 			close(servSock);
// 			servSock = -1;
// 		}

// 		// Print local address

// 		struct sockaddr_storage localAddr;
// 		socklen_t addrSize = sizeof(localAddr);

// 		int getname = getsockname(servSock, (struct sockaddr *) &localAddr, &addrSize);
// 		if (getname >= 0) {
// 			printSocketAddress((struct sockaddr *) &localAddr, addrBuffer);
// 			log(INFO, "Binding to %s", addrBuffer);
// 		}
// 	}

// 	freeaddrinfo(servAddr);

// 	return servSock;

// }


// /* --------------------------------------------------
//    Recieves passive socket and creates active socket
//    when a new connection is available
// -------------------------------------------------- */

// void handleConnections(int masterSocket, int (*read_handler)(int), int (*write_handler)(int)) {

//     connection *connections = calloc(MAX_CONNECTIONS, sizeof(connection));

//     // Accept the incoming connection

//     struct sockaddr_in address;
//     int addrlen = sizeof(address);

//     fd_set readfds;
//     fd_set writefds;

//     FD_ZERO(&writefds);
     
//     while (1) {

//         // Add master socket to read set

//         FD_ZERO(&readfds);
//         FD_SET(masterSocket, &readfds);
         
//         // Add child sockets to read set

//         int maxSocket = masterSocket;
//         for (int i = 0; i < MAX_CONNECTIONS; i++) {
//             connection conn = connections[i];

//             // This represents an empty space
//             if(conn.src_socket == 0)
//                 continue;
                        
//             FD_SET(conn.src_socket, &readfds);
//             FD_SET(conn.dst_socket, &readfds);

//             int localMax = conn.src_socket > conn.dst_socket ? conn.src_socket : conn.dst_socket;
//             maxSocket = localMax > maxSocket ? localMax : maxSocket;
//         }
  
//         // Wait for activity on one of the sockets

//         int activity = select(maxSocket + 1, &readfds, &writefds, NULL, NULL);
//         if ((activity < 0) && (errno != EINTR)) {
//             log(FATAL, "On select")
//         }
          
//         // If something happened on the master socket, then its an incoming connection

//         if (FD_ISSET(masterSocket, &readfds)) {

//             // Accept the client connection

//             int clientSocket = accept(masterSocket, (struct sockaddr *) &address, (socklen_t*) &addrlen);
//             if (clientSocket < 0) {
//                 log(FATAL, "Accepting new connection")
//                 exit(EXIT_FAILURE);
//             }

//             log(INFO, "New connection - FD: %d - IP: %s - Port: %d\n", clientSocket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

//             // Initiate a connection to target

//             int targetSocket = setupClientSocket(targetHost, targetPort);
//             if (targetSocket < 0) {
//                 log(ERROR, "Failed to connect to target")
//             }

//             // Store the connection with its sockets

//             for (int i = 0; i < MAX_CONNECTIONS; i++) {
//                 if(connections[i].src_socket == 0 ) {
//                     connections[i].src_socket = clientSocket;
//                     connections[i].dst_socket = targetSocket;
//                     break;
//                 }
//             }
                                              
//         }

//         // Check for available writes

//         for (int i = 0; i < MAX_CONNECTIONS; i++) {
//             connection conn = connections[i];

//             if(conn.src_socket == 0)
//                 continue;

//             // For target

//             if (FD_ISSET(conn.dst_socket, &writefds) && conn.src_dst_buffer.size > 0) {
//                 int sentBytes = ssend(conn.dst_socket, conn.src_dst_buffer.data);
//                 conn.src_dst_buffer.size -= sentBytes;
//                 FD_CLR(conn.dst_socket, &writefds);
//             }

//             // For client

//             if (FD_ISSET(conn.src_socket, &writefds) && conn.dst_src_buffer.size > 0) {
//                 int sentBytes = ssend(conn.src_socket, conn.dst_src_buffer.data);
//                 conn.dst_src_buffer.size -= sentBytes;
//                 FD_CLR(conn.src_socket, &writefds);
//             }
//         }
          
//         // Check for available reads

//         for (int i = 0; i < MAX_CONNECTIONS; i++) {
//             connection *conn = connections + i;

//             // From client
              
//             if (FD_ISSET(conn->src_socket , &readfds)) {

//                 int readBytes = read(conn->src_socket, conn->src_dst_buffer.data, CONN_BUFFER);
//                 conn->src_dst_buffer.data[readBytes] = '\0';

//                 if (readBytes == 0) {

//                     // Client disconnected, close the client and target sockets

//                     getpeername(conn->src_socket, (struct sockaddr*) &address, (socklen_t*) &addrlen);
//                     log(INFO, "Closed connection - IP: %s - Port: %d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                                            
//                     close(conn->src_socket);
//                     close(conn->dst_socket);

//                     // Remove connection from the list

//                     memset((void *)(connections + i), 0, sizeof(connections[i]));
                    
//                     FD_CLR(conn->src_socket, &writefds);
//                     FD_CLR(conn->dst_socket, &writefds);

//                 } else {

//                     log(DEBUG, "Received %d bytes from socket %d\n", readBytes, conn->src_socket);

//                     FD_SET(conn->dst_socket, &writefds);

//                     conn->src_dst_buffer.size = readBytes;

//                 }
//             }

//             // From target

//             if (FD_ISSET(conn->dst_socket , &readfds)) {

//                 int readBytes = read(conn->dst_socket, conn->dst_src_buffer.data, CONN_BUFFER);
//                 conn->dst_src_buffer.data[readBytes] = '\0';

//                 if (readBytes == 0) {

//                     // Target disconnected, close the client and target sockets

//                     getpeername(conn->dst_socket, (struct sockaddr*) &address, (socklen_t*) &addrlen);
//                     log(INFO, "Closed connection - IP: %s - Port: %d\n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                                            
//                     close(conn->dst_socket);
//                     close(conn->src_socket);

//                     // Remove connection from the list

//                     memset((void *)(connections + i), 0, sizeof(connections[i]));

//                     FD_CLR(conn->dst_socket, &writefds);
//                     FD_CLR(conn->src_socket, &writefds);

//                 } else {

//                     log(DEBUG, "Received %d bytes from socket %d\n", readBytes, conn->dst_socket);

//                     FD_SET(conn->src_socket, &writefds);

//                     conn->dst_src_buffer.size = readBytes;

//                 }
//             }
//         }
//     }

// }
