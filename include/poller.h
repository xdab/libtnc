#pragma once

#include <stdint.h>
#include <sys/epoll.h>

#define POLLER_EV_IN EPOLLIN
#define POLLER_EV_OUT EPOLLOUT
#define POLLER_EV_ERR EPOLLERR

#define POLLER_MAX_EVENTS 64

typedef struct socket_poller
{
    int epoll_fd;
    struct epoll_event events[POLLER_MAX_EVENTS];
    int num_events;
} socket_poller_t;

void socket_poller_init(socket_poller_t *pol);
void socket_poller_free(socket_poller_t *pol);

int socket_poller_add(socket_poller_t *pol, int fd, uint32_t events);
int socket_poller_remove(socket_poller_t *pol, int fd);

int socket_poller_wait(socket_poller_t *pol, int timeout_ms);
int socket_poller_is_ready(socket_poller_t *pol, int fd);
