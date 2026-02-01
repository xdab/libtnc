#include "udp.h"
#include "common.h"
#include "buffer.h"
#include "socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

int udp_sender_init(udp_sender_t *sender, const char *addr, int port)
{
    nonnull(sender, "sender");
    nonnull(addr, "addr");
    EXITIF(port < 0, -1, "port must be positive");
    EXITIF(port > 65535, -1, "port must be less than 65536");

    size_t addr_len = strlen(addr);
    if (addr_len == 0 || addr_len >= INET_ADDRSTRLEN)
    {
        LOG("invalid address length: %s", addr);
        return -1;
    }

    memset(sender, 0, sizeof(udp_sender_t));

    sender->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sender->fd < 0)
    {
        LOGV("socket() failed: %s (errno=%d)", strerror(errno), errno);
        return -1;
    }

    memset(&sender->dest_addr, 0, sizeof(sender->dest_addr));
    sender->dest_addr.sin_family = AF_INET;
    sender->dest_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, addr, &sender->dest_addr.sin_addr) <= 0)
    {
        LOG("invalid address: %s", addr);
        close(sender->fd);
        return -1;
    }

    uint32_t addr_int = ntohl(sender->dest_addr.sin_addr.s_addr);
    if ((addr_int & 0xFF) == 0xFF)
    {
        int broadcast = 1;
        if (setsockopt(sender->fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0)
        {
            LOGV("setsockopt(SO_BROADCAST) failed: %s (errno=%d)", strerror(errno), errno);
            close(sender->fd);
            return -1;
        }
    }

    LOG("udp sender initialized for %s:%d", addr, port);
    return 0;
}

int udp_sender_send(udp_sender_t *sender, const buffer_t *buf)
{
    nonnull(sender, "sender");
    assert_buffer_valid(buf);

    ssize_t sent = sendto(sender->fd, buf->data, buf->size, 0, (struct sockaddr *)&sender->dest_addr, sizeof(sender->dest_addr));
    if (sent < 0)
    {
        LOG("sendto failed: %s (errno=%d)", strerror(errno), errno);
        return -1;
    }
    else if ((size_t)sent != (size_t)buf->size)
    {
        LOG("partial send: %zd/%d bytes: %s (errno=%d)", sent, buf->size, strerror(errno), errno);
        return -2;
    }
    else
    {
        LOGV("sent %d bytes to %s:%d", buf->size,
             inet_ntoa(sender->dest_addr.sin_addr), ntohs(sender->dest_addr.sin_port));
        return 0;
    }
}

void udp_sender_free(udp_sender_t *sender)
{
    nonnull(sender, "sender");

    if (sender->fd >= 0)
    {
        close(sender->fd);
        sender->fd = -1;
    }
}

int udp_server_init(udp_server_t *server, int port, int timeout_ms)
{
    nonnull(server, "server");
    EXITIF(port < 0, -1, "port must be positive");
    EXITIF(port > 65535, -1, "port must be less than 65536");

    memset(server, 0, sizeof(udp_server_t));
    server->timeout_ms = timeout_ms;

    server->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server->fd < 0)
    {
        LOGV("socket() failed: %s (errno=%d)", strerror(errno), errno);
        return -1;
    }

    int reuse = 1;
    if (setsockopt(server->fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        LOGV("setsockopt(SO_REUSEADDR) failed: %s (errno=%d)", strerror(errno), errno);
        close(server->fd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(server->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        LOG("bind() failed on port %d: %s (errno=%d)", port, strerror(errno), errno);
        close(server->fd);
        return -1;
    }

    LOG("udp server listening on port %d", port);
    return 0;
}

void udp_server_free(udp_server_t *server)
{
    nonnull(server, "server");

    if (server->fd >= 0)
    {
        close(server->fd);
        server->fd = -1;
    }
}

int udp_server_listen(udp_server_t *server, buffer_t *buf)
{
    nonnull(server, "server");
    assert_buffer_valid(buf);

    int ret = socket_select(server->fd, server->timeout_ms);
    if (ret <= 0)
        return ret;

    ssize_t n = recvfrom(server->fd, buf->data, buf->capacity, 0, NULL, NULL);
    if (n < 0)
    {
        LOG("recvfrom failed: %s (errno=%d)", strerror(errno), errno);
        return -1;
    }

    buf->size = (int)n;
    return n;
}