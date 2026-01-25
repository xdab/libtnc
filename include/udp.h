#ifndef UDP_H
#define UDP_H

#include <stddef.h>
#include <netinet/in.h>
#include "buffer.h"

#define UDP_MAX_PORT 65535

typedef struct udp_sender
{
    int fd;
    struct sockaddr_in dest_addr;
} udp_sender_t;

int udp_sender_init(udp_sender_t *sender, const char *addr, int port);

int udp_sender_send(udp_sender_t *sender, const buffer_t *buf);

void udp_sender_free(udp_sender_t *sender);

typedef struct udp_server
{
    int fd;
} udp_server_t;

int udp_server_init(udp_server_t *server, int port);

void udp_server_free(udp_server_t *server);

int udp_server_listen(udp_server_t *server, buffer_t *out_buf);

#endif
