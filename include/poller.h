#pragma once

#include <stdint.h>
#include <sys/epoll.h>

#define POLLER_EV_IN  EPOLLIN
#define POLLER_EV_OUT EPOLLOUT
#define POLLER_EV_ERR EPOLLERR

#define POLLER_MAX_EVENTS 64

typedef struct socket_poller
{
    int epoll_fd;
    int audio_fd;
    int num_sockets;
    struct epoll_event events[POLLER_MAX_EVENTS];
    int num_events;
} socket_poller_t;

void socket_poller_init(socket_poller_t *pol);
void socket_poller_free(socket_poller_t *pol);

int socket_poller_add_socket(socket_poller_t *pol, int fd, uint32_t events);
int socket_poller_add_audio(socket_poller_t *pol, int fd);
int socket_poller_remove(socket_poller_t *pol, int fd);

int socket_poller_wait(socket_poller_t *pol, int timeout_ms);

int socket_poller_is_socket_ready(socket_poller_t *pol, int fd);
int socket_poller_is_audio_ready(socket_poller_t *pol);
