#ifndef SERVER_H
#define SERVER_H


/*
 * Copyright 2021 D'Arcy Smith + the BCIT CST Datacommunications Option students.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


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
