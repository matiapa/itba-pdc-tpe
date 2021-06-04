#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdbool.h>

#define VIA_PROXY_NAME_SIZE 128
#define BLACKLIST_SIZE 1024

typedef struct Config {
    int maxClients;                             // Max allowed clients (up to 1000). Default is 1000.
    int connectionTimeout;                      // Max inactivity time before disconnection, or -1 to disable it. Default is -1.

    bool statisticsEnabled;                     // Whether to log connection statistics. Default is 1.
    bool disectorsEnabled;                      // Whether to extract plain text credentials. Default is 1.

    char viaProxyName[VIA_PROXY_NAME_SIZE];     // Host name to use on RFC 2616 required 'Via' header. Default is proxy hostname.
    char clientBlacklist[BLACKLIST_SIZE];       // Comma separated list of client IPs to which service must be denied.
    char targetBlacklist[BLACKLIST_SIZE];       // Comma separated list of target IPs to which connection must be denied.

    int logLevel;                               // Minimum log level to display of [DEBUG, INFO, ERROR, FATAL]. Default is DEBUG.
} Config;

extern Config proxy_conf;

#endif