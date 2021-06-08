#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <address.h>

const char *
printFamily(struct addrinfo *aip)
{
	switch (aip->ai_family) {
	case AF_INET:
		return "inet";
	case AF_INET6:
		return "inet6";
	case AF_UNIX:
		return "unix";
	case AF_UNSPEC:
		return "unspecified";
	default:
		return "unknown";
	}
}

const char *
printType(struct addrinfo *aip)
{
	switch (aip->ai_socktype) {
	case SOCK_STREAM:
		return "stream";
	case SOCK_DGRAM:
		return "datagram";
	case SOCK_SEQPACKET:
		return "seqpacket";
	case SOCK_RAW:
		return "raw";
	default:
		return "unknown ";
	}
}

const char *
printProtocol(struct addrinfo *aip)
{
	switch (aip->ai_protocol) {
	case 0:
		return "default";
	case IPPROTO_TCP:
		return "TCP";
	case IPPROTO_UDP:
		return "UDP";
	case IPPROTO_RAW:
		return "raw";
	default:
		return "unknown ";
	}
}

void
printFlags(struct addrinfo *aip)
{
	printf("flags");
	if (aip->ai_flags == 0) {
		printf(" 0");
	} else {
		if (aip->ai_flags & AI_PASSIVE)
			printf(" passive");
		if (aip->ai_flags & AI_CANONNAME)
			printf(" canon");
		if (aip->ai_flags & AI_NUMERICHOST)
			printf(" numhost");
		if (aip->ai_flags & AI_NUMERICSERV)
			printf(" numserv");
		if (aip->ai_flags & AI_V4MAPPED)
			printf(" v4mapped");
		if (aip->ai_flags & AI_ALL)
			printf(" all");
	}
}

char *
printAddressPort( const struct addrinfo *aip, char addr[]) 
{
	char abuf[INET6_ADDRSTRLEN];
	const char *addrAux ;
	if (aip->ai_family == AF_INET) {
		struct sockaddr_in	*sinp;
		sinp = (struct sockaddr_in *)aip->ai_addr;
		addrAux = inet_ntop(AF_INET, &sinp->sin_addr, abuf, INET_ADDRSTRLEN);
		if ( addrAux == NULL )
			addrAux = "unknown";
		strcpy(addr, addrAux);
		if ( sinp->sin_port != 0) {
			sprintf(addr + strlen(addr), ": %d", ntohs(sinp->sin_port));
		}
	} else if ( aip->ai_family ==AF_INET6) {
		struct sockaddr_in6	*sinp;
		sinp = (struct sockaddr_in6 *)aip->ai_addr;
		addrAux = inet_ntop(AF_INET6, &sinp->sin6_addr, abuf, INET6_ADDRSTRLEN);
		if ( addrAux == NULL )
			addrAux = "unknown";
		strcpy(addr, addrAux);			
		if ( sinp->sin6_port != 0)
			sprintf(addr + strlen(addr), ": %d", ntohs(sinp->sin6_port));
	} else
		strcpy(addr, "unknown");
	return addr;
}


int 
printSocketAddress(const struct sockaddr *address, char *addrBuffer) {

	void *numericAddress; 

	in_port_t port;

	switch (address->sa_family) {
		case AF_INET:
			numericAddress = &((struct sockaddr_in *) address)->sin_addr;
			port = ntohs(((struct sockaddr_in *) address)->sin_port);
			break;
		case AF_INET6:
			numericAddress = &((struct sockaddr_in6 *) address)->sin6_addr;
			port = ntohs(((struct sockaddr_in6 *) address)->sin6_port);
			break;
		default:
			strcpy(addrBuffer, "[unknown type]");    // Unhandled type
			return 0;
	}
	// Convert binary to printable address
	if (inet_ntop(address->sa_family, numericAddress, addrBuffer, INET6_ADDRSTRLEN) == NULL)
		strcpy(addrBuffer, "[invalid address]"); 
	else {
		if (port != 0)
			sprintf(addrBuffer + strlen(addrBuffer), ":%u", port);
	}
	return 1;
}

int sockAddrsEqual(const struct sockaddr *addr1, const struct sockaddr *addr2) {
	if (addr1 == NULL || addr2 == NULL)
		return addr1 == addr2;
	else if (addr1->sa_family != addr2->sa_family)
		return 0;
	else if (addr1->sa_family == AF_INET) {
		struct sockaddr_in *ipv4Addr1 = (struct sockaddr_in *) addr1;
		struct sockaddr_in *ipv4Addr2 = (struct sockaddr_in *) addr2;
		return ipv4Addr1->sin_addr.s_addr == ipv4Addr2->sin_addr.s_addr && ipv4Addr1->sin_port == ipv4Addr2->sin_port;
	} else if (addr1->sa_family == AF_INET6) {
		struct sockaddr_in6 *ipv6Addr1 = (struct sockaddr_in6 *) addr1;
		struct sockaddr_in6 *ipv6Addr2 = (struct sockaddr_in6 *) addr2;
		return memcmp(&ipv6Addr1->sin6_addr, &ipv6Addr2->sin6_addr, sizeof(struct in6_addr)) == 0 
			&& ipv6Addr1->sin6_port == ipv6Addr2->sin6_port;
	} else
		return 0;
}


int get_machine_fqdn(char * fqdn) {
	struct addrinfo * p;
	
	// Get unqualified hostname

	char hostname[1024] = {0};
	gethostname(hostname, 1023);

	// Get FQDN if available

	struct addrinfo hints = {0};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;

	struct addrinfo * info;
	int gai_result;
	if ((gai_result = getaddrinfo(hostname, "http", &hints, &info)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_result));
		return -1;
	}

	strcpy(fqdn, info->ai_canonname);

	freeaddrinfo(info);
}


int is_number(const char * str) {
    while(*str != '\0') {
        if (*str > '9' || *str < '0') return 0;
        str++;
    }
    return 1;
}

int parse_url(char * text, struct url * url) {

    memset(url, 0, sizeof(*url));
    char * token = NULL;
    char * rest = text;
    url->port = 0;
    int flag = 0, num_flag;

    if (rest[0] == '/') { // esta en formato origin
        strcpy(url->path, rest);
        return 0;
    }

    while (strchr(rest, ':') != NULL && (token = strtok_r(rest, ":", &rest))) {
        num_flag = is_number(token);
        if (!num_flag && !flag) {
            if (strcmp(token, "http") == 0 || strcmp(token, "https") == 0) {
                strcpy(url->protocol, token);
                rest += 2; // por ://
            } else {
                strcpy(url->hostname, token);
                flag = 1;
            }
        } else if (num_flag) {
            url->port = atoi(token);
        } else break;
    }

    token = NULL;
    while ((token = strtok_r(rest, "/", &rest))) {
        num_flag = is_number(token);
        if (!num_flag) {
            if (!flag) {
                strcpy(url->hostname, token);
                flag = 1;
            } else {
                if (rest != NULL){
                    snprintf(url->path, PATH_LENGTH, "/%s/%s", token, rest);
                    rest = NULL;
                } else {
                    snprintf(url->path, PATH_LENGTH, "/%s", token);
                }
            }
        } else {
            url->port = atoi(token);
        }
    }

    if (url->port == 0) url->port = 80;

    return 0;
}
