#pragma once

#define SELECT_TIMEOUT_MS 100
#define SELECT_TIMEOUT_US (NET_SELECT_TIMEOUT_MS * 1000)

int socket_set_nonblocking(int fd);

int socket_init_server(int domain, int type);

int socket_bind(int listen_fd, int port);

int socket_select(int fd, int timeout_ms);

int socket_check_connection(int fd);
