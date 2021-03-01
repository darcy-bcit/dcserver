#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H


#include <dc_application/config.h>
#include <netinet/in.h>
#include <stdbool.h>


struct dc_server_config
{
    struct dc_config parent;
    in_port_t port;
    bool force_connection;
    int backlog;
    bool verbose;
};


#endif //SERVER_ECHO_SERVER_CONFIG_H
