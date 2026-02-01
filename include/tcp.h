#ifndef TCP_H
#define TCP_H

#include <sys/select.h>
#include <stddef.h>
#include "buffer.h"

#define TCP_MAX_CLIENTS 16
#define TCP_READ_BUF_SIZE 2048
#define TCP_DEF_TIMEOUT_MS 50

typedef struct tcp_client
{
    int fd;
    int timeout_ms;
} tcp_client_t;

typedef struct tcp_server
{
    int listen_fd;
    tcp_client_t clients[TCP_MAX_CLIENTS];
    int num_clients;
    int timeout_ms;
} tcp_server_t;

int tcp_server_init(tcp_server_t *server, int port, int timeout_ms);

void tcp_server_free(tcp_server_t *server);

int tcp_server_listen(tcp_server_t *server, buffer_t *out_buf);

void tcp_server_broadcast(tcp_server_t *server, const buffer_t *buf);

//

int tcp_client_init(tcp_client_t *client, const char *addr, int port, int timeout_ms);

void tcp_client_free(tcp_client_t *client);

int tcp_client_listen(tcp_client_t *client, buffer_t *out_buf);

int tcp_client_send(tcp_client_t *client, const buffer_t *buf);

#endif
