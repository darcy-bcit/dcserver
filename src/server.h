#ifndef SERVER_H
#define SERVER_H


#include "server_config.h"
#include <pthread.h>
#include <sys/time.h>
#include <dc_fsm/fsm.h>


struct dc_server_environment;


typedef int (*dc_server_lifecycle_func)(struct dc_server_environment *env);
typedef bool (*connection_handler)(int client_fd, const struct dc_server_config *config, void *data);


struct dc_server_lifecycle
{
    dc_server_lifecycle_func init;
    dc_server_lifecycle_func bind;
    dc_server_lifecycle_func listen;
    dc_server_lifecycle_func accept;
    dc_server_lifecycle_func error;
    struct timeval *timeout;
};


typedef enum
{
    SEQUENTIAL,
    SELECT,
} Lifecycle;


struct dc_server_lifecycle *dc_server_lifecycle_create(Lifecycle type,
                                                       struct timeval *timeout);
void dc_server_lifecycle_destroy(struct dc_server_lifecycle **plifecycle);
pthread_t dc_server_run(const struct dc_server_config *config, struct dc_server_lifecycle *lifecycle, int server_fd, connection_handler handler, void *data);


#endif
