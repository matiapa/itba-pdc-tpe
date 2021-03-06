#include <doh_client.h>
#include <http.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h> //printf
#include <args.h>
#include <logger.h>
#include <buffer.h>
#include <http_response_parser.h>
#include <http_message_parser.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

#define A 1
#define AAAA 28
#define BUFF_SIZE 6000

//DNS header structure
struct DNS_HEADER {
    unsigned short id; // identification number

    unsigned char rd :1; // recursion desired
    unsigned char tc :1; // truncated message
    unsigned char aa :1; // authoritive answer
    unsigned char opcode :4; // purpose of message
    unsigned char qr :1; // query/response flag

    unsigned char rcode :4; // response code
    unsigned char cd :1; // checking disabled
    unsigned char ad :1; // authenticated data
    unsigned char z :1; // its z! reserved
    unsigned char ra :1; // recursion available

    uint16_t q_count; // number of question entries
    uint16_t ans_count; // number of answer entries
    uint16_t auth_count; // number of authority entries
    uint16_t add_count; // number of resource entries
};

//Constant sized fields of query structure
struct QUESTION {
    uint16_t qtype;
    uint16_t qclass;
};

//Constant sized fields of the resource record structure
#pragma pack(push, 1)
struct R_DATA {
    uint16_t type;
    uint16_t _class;
    uint32_t ttl;
    uint16_t data_len;
};
#pragma pack(pop)

//Structure for ipv4 or ipv6
union sa {
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;
};

//Addrinfo structure
struct aibuf {
    struct addrinfo ai;
    union sa sa;
};


/*------------------- FUNCIONES -------------------*/
int change_to_dns_format(char* dns, const char * host);
unsigned char * get_body(unsigned char * str);
int get_name(unsigned char * body);
int create_post(int length, char * body, char * write_buffer, int space);
int send_doh_request(const char * target, int s, int type);
int read_response(struct aibuf * out, int sin_port, int family, int ans_count, int initial_size);
int resolve_string(struct addrinfo ** addrinfo, const char * target, int port);

/*------------------- VARIABLES GLOBALES -------------------*/
struct doh configurations;
buffer buff;
int id = 0; // TODO: hacerlo dinamico para manejar varios pedidos doh



void initialize_doh_client(struct doh * args) {
    memcpy(&configurations, args, sizeof(struct doh));
}

// https://git.musl-libc.org/cgit/musl/tree/src/network/getaddrinfo.c
int doh_client(const char * target, const int sin_port, struct addrinfo ** restrict addrinfo, int family) {

    /*--------- Chequeo si el target esta en formato IP o es Localhost ---------*/
    if (resolve_string(addrinfo, target, sin_port) >= 0) return 0; // El target esta en formato IP


    if (family != AF_INET && family != AF_INET6) // pedido invalido
        return -1;

    struct sockaddr_in dest;
    buffer_init(&buff, BUFF_SIZE, calloc(1, BUFF_SIZE));

    int s = socket(AF_INET , SOCK_STREAM , IPPROTO_TCP); // Socket para conectarse al DOH Server
    if (s < 0) {
        log(ERROR, "Socket failed to create")
        free_buffer(&buff);
        return -1;
    }

    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&timeout, sizeof(int)) < 0) {
        log(ERROR, "set DoH socket options SO_REUSEADDR failed %s ", strerror(errno));
    }

    /*--------- Establece la conex??n con el servidor ---------*/
    dest.sin_family = AF_INET;
    dest.sin_port = htons(configurations.port);
    dest.sin_addr.s_addr = inet_addr(configurations.ip);

    if (connect(s, (struct sockaddr*)&dest,sizeof(dest)) < 0) { // Establece la conexi??n con el servidor DOH
        log(ERROR, "Error in connect")
        free_buffer(&buff);
        close(s);
        return -1;
    }

    struct aibuf * out = NULL;
    int cant = 0;
    int type = family == AF_INET ? A : AAAA;

    buffer_reset(&buff);
    memset(buff.data, 0, BUFF_SIZE); // Limpio el buffer

    /*----------- Mando el request DOH para IPv4 -----------*/
    if (send_doh_request(target, s, type) < 0) {
        free_buffer(&buff);
        close(s);
        return -1;
    }
    
    buffer_reset(&buff);
    memset(buff.data, 0, BUFF_SIZE); // Limpio el buffer

    /*----------- Recivo el response DOH -----------*/
    int dim = sizeof(struct sockaddr_in);
    size_t nbyte;
    char * aux_buff = (char *)buffer_write_ptr(&buff, &nbyte);

    ssize_t read_bytes;
    if ((read_bytes = recvfrom (s, aux_buff, nbyte , 0 , (struct sockaddr*)&dest , (socklen_t*)&dim )) < 0) {
        log(ERROR, "Getting response from DOH server")
        free_buffer(&buff);
        close(s);
        return -1;
    }

    buffer_write_adv(&buff, read_bytes);

    http_response_parser parser = {0};
    http_response_parser_init(&parser);

    http_response_parser_parse(&parser, &buff, false);
    http_response response = parser.response;

    buff.read = (unsigned char *)response.message.body;

    struct DNS_HEADER * dns = calloc(1, sizeof(struct DNS_HEADER));
    if (dns == NULL) {
        log(ERROR, "Doing calloc of dns header");
        free_buffer(&buff);
        close(s);
        return -1;
    }
    memcpy(dns, buffer_read_ptr(&buff, &nbyte), sizeof(struct DNS_HEADER)); // Obtengo el DNS_HEADER
    // struct DNS_HEADER * dns = (struct DNS_HEADER *)buffer_read_ptr(&buff, &nbyte);
    buffer_read_adv(&buff, sizeof(struct DNS_HEADER));

    int ans_count = ntohs(dns->ans_count); // Cantidad de respuestas
    free(dns);

    if (ans_count == 0) {
        free_buffer(&buff);
        close(s);
        return 0;
    }

    out = calloc(1, ans_count * sizeof(*out) + 1); // Se usa para llenar la estructura de las answers
    if (out == NULL) {
        log(ERROR, "Doing calloc of out");
        free_buffer(&buff);
        close(s);
        return -1;
    }

    int n = get_name(buffer_read_ptr(&buff, &nbyte)) + sizeof(struct QUESTION);
    buffer_read_adv(&buff, n); // comienzo de las answers, me salteo la estructura QUESTION porque no me interesa

    /*----------- Lectura del response DOH -----------*/
    read_response(out, sin_port, family, ans_count, cant);

    // Termine
    *addrinfo = &out->ai;
    free_buffer(&buff);
    close(s);
    return 0;
}

int create_post(int length, char * body, char * write_buffer, int space) {
    int header_count = 4;
    http_request request = {
        .method = POST,
        .message.header_count = header_count,
        .message.body_length = length,
        .message.body = body
    };
    strcpy(request.message.headers[0][0], "Accept");
    strcpy(request.message.headers[0][1], "application/dns-message");
    strcpy(request.message.headers[1][0], "Content-Type");
    strcpy(request.message.headers[1][1], "application/dns-message");
    strcpy(request.message.headers[2][0], "Host");
    snprintf(request.message.headers[2][1], HEADER_LENGTH, "%s:%d", configurations.host, configurations.port);
    strcpy(request.message.headers[3][0], "Content-Length");
    snprintf(request.message.headers[3][1], 4, "%d", length);
    strcpy(request.url, configurations.path);

    return write_request(&request, write_buffer, space, true);
}

int send_doh_request(const char * target, int s, int type) {
    unsigned char * qname;
    struct QUESTION * qinfo = NULL;
    size_t nbyte;

    struct DNS_HEADER *dns;
    dns = (struct DNS_HEADER *)(char *)buffer_write_ptr(&buff, &nbyte);
    dns->id = id;
    dns->qr = 0; //This is a query
    dns->opcode = 0; //This is a standard query
    dns->aa = 0; //Not Authoritative
    dns->tc = 0; //This message is not truncated
    dns->rd = 1; //Recursion Desired
    dns->ra = 0; //Recursion not available! hey we dont have it (lol)
    dns->z = 0;
    dns->ad = 0;
    dns->cd = 0;
    dns->rcode = 0;
    dns->q_count = htons(1); //we have only 1 question
    dns->ans_count = 0;
    dns->auth_count = 0;
    dns->add_count = 0;
    buffer_write_adv(&buff, sizeof(struct DNS_HEADER)); // muevo el buffer despues de haber escrito DNS_HEADER

    qname = (unsigned char*)buffer_write_ptr(&buff, &nbyte);

    int len = change_to_dns_format((char *)qname, target); // Mete el host en la posici??n del qname
    buffer_write_adv(&buff, len);


    qinfo = (struct QUESTION*)buffer_write_ptr(&buff, &nbyte);
    qinfo->qtype = htons(type);
    qinfo->qclass = htons(1); // Its internet
    buffer_write_adv(&buff, sizeof(struct QUESTION));

    char * aux_buff = (char *)buffer_read_ptr(&buff, &nbyte);

    char * write_buffer = malloc(1024); // TODO: Choose a number
    if (write_buffer == NULL) {
        log(ERROR, "Doing malloc of write buffer");
        return -1;
    }

    int written = create_post((int)nbyte, aux_buff, write_buffer, 1024); // crea el http request

    if( send(s, write_buffer, written, 0) < 0) { // manda el paquete al servidor DOH
        log(ERROR, "Sending DOH request")
        return -1;
    }

    free(write_buffer); // Libera el request http
    return 0;
}

int read_response(struct aibuf * out, int sin_port, int family, int ans_count, int initial_size) {

    int sin_family, cant = initial_size;
    size_t nbytes;
    for (int i = 0 + initial_size; i < ans_count + initial_size; i++) {
        buffer_read_adv(&buff, get_name(buffer_read_ptr(&buff, &nbytes)) + 1);

        struct R_DATA * data = calloc(1, sizeof(struct R_DATA));
        if (data == NULL) {
            log(ERROR, "Creating data calloc")
            return -1;
        }
        memcpy(data, buffer_read_ptr(&buff, &nbytes), sizeof(struct R_DATA));
        buffer_read_adv(&buff, sizeof(struct R_DATA));

        if (ntohs(data->type) == A) sin_family = AF_INET;
        else if (ntohs(data->type) == AAAA) sin_family = AF_INET6;
        else sin_family = -1;

        if (family == sin_family) {
            out[cant].ai = (struct addrinfo) {
                    .ai_family = sin_family,
                    .ai_socktype = SOCK_STREAM,
                    .ai_protocol = 0,
                    .ai_addrlen = sin_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
                    .ai_addr = (void *) &out[cant].sa,
                    .ai_canonname = NULL};

            if (cant) out[cant - 1].ai.ai_next = &out[cant].ai;

            switch (sin_family) {
                case AF_INET:
                    out[cant].sa.sin.sin_family = AF_INET;
                    out[cant].sa.sin.sin_port = htons(sin_port);
                    memcpy(&out[cant].sa.sin.sin_addr, buffer_read_ptr(&buff, &nbytes), 4);
                    break;
                case AF_INET6:
                    out[cant].sa.sin6.sin6_family = AF_INET6;
                    out[cant].sa.sin6.sin6_port = htons(sin_port);
                    out[cant].sa.sin6.sin6_scope_id = 0;
                    memcpy(&out[cant].sa.sin6.sin6_addr, buffer_read_ptr(&buff, &nbytes), 16);
                    break;
                default:
                    break;
            }
            cant++;
        }
        buffer_read_adv(&buff, ntohs(data->data_len));
        free(data);
    }

    return cant;
}

int resolve_string(struct addrinfo ** addrinfo, const char * target, int port) {
    struct aibuf * out = calloc(1, sizeof(*out) + 1);
    if (out == NULL) {
        log(ERROR, "Doing calloc of out");
        return -1;
    }
    if (inet_pton(AF_INET, target, &out->sa.sin.sin_addr)) { // Es una IPv4
        out->ai = (struct addrinfo) {
                .ai_family = AF_INET,
                .ai_socktype = SOCK_STREAM,
                .ai_protocol = 0,
                .ai_addrlen = sizeof(struct sockaddr_in),
                .ai_addr = (void *) &out->sa,
                .ai_canonname = NULL,
        };

        out->sa.sin.sin_family = AF_INET;
        out->sa.sin.sin_port = htons(port);
        *addrinfo = &out->ai;
        return 0;
    } else if (inet_pton(AF_INET6, target, &out->sa.sin6.sin6_addr)) { // Es una IPv6
        out->ai = (struct addrinfo) {
                .ai_family = AF_INET6,
                .ai_socktype = SOCK_STREAM,
                .ai_protocol = 0,
                .ai_addrlen = sizeof(struct sockaddr_in6),
                .ai_addr = (void *) &out->sa,
                .ai_canonname = NULL,
        };

        out->sa.sin6.sin6_family = AF_INET6;
        out->sa.sin6.sin6_port = htons(port);
        out->sa.sin6.sin6_scope_id = 0;
        *addrinfo = &out->ai;
        return 0;
    } else {
        return -1; // Es una URL
    }
}

int change_to_dns_format(char* dns, const char * host) {
    int len = (int)strlen(host);
    memcpy(dns, ".", 1);
    memcpy(dns + 1, host, len);

    char cant = 0;
    for (int i = (int)strlen(dns) - 1; i >= 0; i--) {
        if (dns[i] != '.') {
            cant++;
        } else {
            dns[i] = cant;
            cant = 0;
        }
    }
    dns[len + 1] = 0;
    return len + 2;
}

int get_name(unsigned char * body) {
    if (*body & 0xC0) { // Chequeo si no esta en formato comprimido
        return 2;
    }
    int cant = 1;
    body++; // para evitar el primer punto
    while (*((char *)body) != 0) {
        body++;
        cant++;
    }

    return cant;
}
