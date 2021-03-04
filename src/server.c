#include "server.h"
#include "sys/socket.h"
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/select.h>
#include <errno.h>


struct dc_server_environment
{
    struct dc_fsm_environment common;
    const struct dc_server_config *config;
    struct dc_server_lifecycle *lifecycle;
    int server_fd;
    connection_handler handler_func;
    struct sockaddr_in addr;
    void *data;
    int max_fd;
    fd_set read_fd_set;
};


typedef enum
{
    INIT = FSM_APP_STATE_START, // 2
    BIND,                       // 3
    LISTEN,                     // 4
    ACCEPT,                     // 5
    ERROR,                      // 6
} States;


static void *run_fsm(struct dc_server_environment *env);
static int server_init(struct dc_server_environment *env);
static int server_bind(struct dc_server_environment *env);
static int server_listen(struct dc_server_environment *env);
static int server_accept_serial(struct dc_server_environment *env);
static int server_accept_select(struct dc_server_environment *env);
static int server_error(struct dc_server_environment *env);


struct dc_server_lifecycle *dc_server_lifecycle_create(Lifecycle type,
                                                       struct timeval *timeout)
{
    struct dc_server_lifecycle *lifecycle;

    lifecycle = malloc(sizeof(struct dc_server_lifecycle));
    lifecycle->init   = server_init;
    lifecycle->bind   = server_bind;
    lifecycle->listen = server_listen;

        lifecycle->timeout = timeout;

    if(type == SEQUENTIAL)
    {
        lifecycle->accept = server_accept_serial;
    }
    else if(type == SELECT)
    {
        lifecycle->accept = server_accept_select;
    }

    lifecycle->error  = server_error;

    return lifecycle;
}


void dc_server_lifecycle_destroy(struct dc_server_lifecycle **plifecycle)
{
    free(*plifecycle);
    *plifecycle = NULL;
}

pthread_t dc_server_run(const struct dc_server_config *config, struct dc_server_lifecycle *lifecycle, int server_fd, connection_handler handler, void *data)
{
    pthread_t                    thread;
    pthread_attr_t               attr;
    struct dc_server_environment *env;
    int                          result;

    env = malloc(sizeof(struct dc_server_environment));
    env->common.name  = "Server";
    env->config       = config;
    env->lifecycle    = lifecycle;
    env->server_fd    = server_fd;
    env->handler_func = handler;
    env->data         = data;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    result = pthread_create(&thread, &attr, (void * (*)(void *))run_fsm, (void *)env);

    if(result != 0)
    {
    }

    pthread_attr_destroy(&attr);

    return thread;
}


static void *run_fsm(struct dc_server_environment *env)
{
    int                        code;
    int                        start_state;
    int                        end_state;
    struct state_transition    transitions[] =
    {
        { FSM_INIT,   INIT,       (state_func)env->lifecycle->init   },
        { INIT,       BIND,       (state_func)env->lifecycle->bind   },
        { BIND,       LISTEN,     (state_func)env->lifecycle->listen },
        { LISTEN,     ACCEPT,     (state_func)env->lifecycle->accept },
        { INIT,       ERROR,      (state_func)env->lifecycle->error  },
        { BIND,       ERROR,      (state_func)env->lifecycle->error  },
        { LISTEN,     ERROR,      (state_func)env->lifecycle->error  },
        { ACCEPT,     ERROR,      (state_func)env->lifecycle->error  },
        { ERROR,      FSM_EXIT,   NULL                               },
        { FSM_IGNORE, FSM_IGNORE, NULL                               },
    };

    start_state = FSM_INIT;
    end_state   = INIT;
    code        = dc_fsm_run((struct dc_fsm_environment *)env, &start_state, &end_state, transitions, env->config->verbose);
    pthread_exit(NULL);
}

static int server_init(struct dc_server_environment *env)
{
    memset(&env->addr, 0, sizeof(struct sockaddr_in));
    env->addr.sin_family      = AF_INET;
    env->addr.sin_port        = htons(env->config->port);
    env->addr.sin_addr.s_addr = htonl(INADDR_ANY);

    return BIND;
}

static int server_bind(struct dc_server_environment *env)
{
    int result;

    if(env->config->force_connection)
    {
        int enable = 1;

        result = setsockopt(env->server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

        if(result < 0)
        {
            return ERROR;
        }
    }

    result = bind(env->server_fd, (struct sockaddr *)&env->addr, sizeof(struct sockaddr_in));

    if(result < 0)
    {
        return ERROR;
    }

    return LISTEN;
}

static int server_listen(struct dc_server_environment *env)
{
    int result;

    result = listen(env->server_fd, env->config->backlog);

    if(result < 0)
    {
        return ERROR;
    }

    return ACCEPT;
}


static int server_accept_serial(struct dc_server_environment *env)
{
    for(;;)
    {
        int client_fd;

        client_fd = accept(env->server_fd, NULL, NULL);

        if(client_fd < 0)
        {
            break;
        }

        env->handler_func(client_fd, env->config, env->data);
    }

    return ERROR;
}

// inspired by this: https://www.ibm.com/support/knowledgecenter/ssw_ibm_i_72/rzab6/xnonblock.htm
static int server_accept_select(struct dc_server_environment *env)
{
    env->max_fd = env->server_fd;
    FD_ZERO(&env->read_fd_set);
    FD_SET(env->server_fd, &env->read_fd_set);

    // need a way to exit
    for(;;)
    {
        fd_set read_fd_set;
        int nfds;

        memcpy(&read_fd_set, &env->read_fd_set, sizeof(env->read_fd_set));

        // we use a timeout so we can check for exit periodically
        // not sure if this is a good idea or not... will need to experiment
        nfds = select(env->max_fd + 1, &read_fd_set, NULL, NULL, env->lifecycle->timeout);

        if(nfds < 0)
        {
            perror("select");
            exit(EXIT_FAILURE);
        }

        for(int fd = 0; fd <= env->max_fd && nfds > 0; fd++)
        {
            if(FD_ISSET(fd, &read_fd_set))
            {
                if(fd == env->server_fd)
                {
                    int client_fd;

                    client_fd = accept(env->server_fd, NULL, NULL);

                    if(client_fd < 0)
                    {
                        perror("accept");
                        exit(EXIT_FAILURE);
                    }

                    FD_SET(client_fd, &env->read_fd_set);

                    if(client_fd > env->max_fd)
                    {
                        env->max_fd = client_fd;
                    }
                }
                else
                {
                    bool remove;

                    remove = env->handler_func(fd, env->config, env->data);

                    if(remove)
                    {
                        FD_CLR(fd, &env->read_fd_set);

                        while(FD_ISSET(env->max_fd, &env->read_fd_set) == false)
                        {
                            env->max_fd -= 1;
                        }
                    }
                }
            }
        }
    }

    return ERROR;
}

static int server_error(struct dc_server_environment *env)
{
    char *message;
    char *str;

    message = strerror(errno);

    // 1 is the # of digits for the states +
    // 3 is " - " +
    // strlen is the length of the system message
    // 1 is null +
    str = malloc(1 + strlen(message) + 3 + 1);
    sprintf(str, "%d - %s", env->common.current_state_id, str);
    perror(str);

    return FSM_EXIT;
}

