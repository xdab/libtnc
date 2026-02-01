#pragma once

#include <sys/select.h>

#define SELECT_TIMEOUT_MS 100
#define SELECT_TIMEOUT_US (NET_SELECT_TIMEOUT_MS * 1000)
#define SELECT_MAX_FDS 32

#define SELECT_READ (1 << 0)
#define SELECT_WRITE (1 << 1)
#define SELECT_ERROR (1 << 2)

typedef struct socket_selector
{
    int fds[SELECT_MAX_FDS];
    int events[SELECT_MAX_FDS];
    int num_fds;
    int max_fd;
} socket_selector_t;

int socket_set_nonblocking(int fd);

int socket_init_server(int domain, int type);

int socket_bind(int listen_fd, int port);

int socket_select(int fd, int timeout_ms);

int socket_check_connection(int fd);

//

void socket_selector_init(socket_selector_t *sel);

int socket_selector_add(socket_selector_t *sel, int fd, int events);

int socket_selector_remove(socket_selector_t *sel, int fd);

int socket_selector_wait(socket_selector_t *sel, int timeout_ms);

int socket_selector_is_ready(socket_selector_t *sel, int fd);
