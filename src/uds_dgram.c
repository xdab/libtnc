#include "uds.h"
#include "common.h"
#include "buffer.h"
#include "socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

int uds_dgram_sender_init(uds_dgram_sender_t *sender, const char *dest_socket_path)
{
    nonnull(sender, "sender");
    nonnull(dest_socket_path, "dest_socket_path");

    size_t path_len = strlen(dest_socket_path);
    if (path_len == 0)
    {
        LOG("destination socket path must not be empty");
        return -1;
    }
    if (path_len >= UDS_SOCKET_PATH_MAX)
    {
        LOG("destination socket path too long (max %d)", UDS_SOCKET_PATH_MAX - 1);
        return -1;
    }

    memset(sender, 0, sizeof(uds_dgram_sender_t));

    sender->fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sender->fd < 0)
    {
        LOGV("socket() failed: %s (errno=%d)", strerror(errno), errno);
        return -1;
    }

    memset(&sender->dest_addr, 0, sizeof(sender->dest_addr));
    sender->dest_addr.sun_family = AF_UNIX;
    memcpy(sender->dest_addr.sun_path, dest_socket_path, path_len);

    LOG("uds dgram sender initialized for %s", dest_socket_path);
    return 0;
}

int uds_dgram_sender_send(uds_dgram_sender_t *sender, const buffer_t *buf)
{
    nonnull(sender, "sender");
    assert_buffer_valid(buf);

    if (buf->size == 0)
        return 0;

    ssize_t sent = sendto(sender->fd, buf->data, buf->size, 0,
                          (struct sockaddr *)&sender->dest_addr, sizeof(sender->dest_addr));
    if (sent < 0)
    {
        LOG("sendto failed: %s (errno=%d)", strerror(errno), errno);
        return -1;
    }

    LOGV("sent %zd bytes to %s", sent, sender->dest_addr.sun_path);
    return (int)sent;
}

void uds_dgram_sender_free(uds_dgram_sender_t *sender)
{
    nonnull(sender, "sender");

    if (sender->fd >= 0)
    {
        close(sender->fd);
        sender->fd = -1;
    }
}

int uds_dgram_server_init(uds_dgram_server_t *server, const char *socket_path, int timeout_ms)
{
    nonnull(server, "server");
    nonnull(socket_path, "socket_path");

    size_t path_len = strlen(socket_path);
    if (path_len == 0)
    {
        LOG("socket path must not be empty");
        return -1;
    }
    if (path_len >= UDS_SOCKET_PATH_MAX)
    {
        LOG("socket path too long (max %d)", UDS_SOCKET_PATH_MAX - 1);
        return -1;
    }

    memset(server, 0, sizeof(uds_dgram_server_t));
    memcpy(server->socket_path, socket_path, path_len);
    server->timeout_ms = timeout_ms;

    server->fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (server->fd < 0)
    {
        LOGV("socket() failed: %s (errno=%d)", strerror(errno), errno);
        return -1;
    }

    unlink(socket_path);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, socket_path, path_len);

    if (bind(server->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        LOG("bind() failed on %s: %s (errno=%d)", socket_path, strerror(errno), errno);
        close(server->fd);
        return -1;
    }

    if (socket_set_nonblocking(server->fd) < 0)
    {
        LOGV("socket_set_nonblocking() failed: %s (errno=%d)", strerror(errno), errno);
        close(server->fd);
        unlink(socket_path);
        return -1;
    }

    LOG("uds dgram server listening on %s", socket_path);
    return 0;
}

int uds_dgram_server_listen(uds_dgram_server_t *server, buffer_t *out_buf)
{
    nonnull(server, "server");
    assert_buffer_valid(out_buf);

    int ret = socket_select(server->fd, server->timeout_ms);
    if (ret <= 0)
        return ret;

    ssize_t n = recvfrom(server->fd, out_buf->data, out_buf->capacity, 0, NULL, NULL);
    if (n < 0)
    {
        LOG("recvfrom failed: %s (errno=%d)", strerror(errno), errno);
        return -1;
    }

    out_buf->size = (int)n;
    return (int)n;
}

void uds_dgram_server_free(uds_dgram_server_t *server)
{
    nonnull(server, "server");

    if (server->fd >= 0)
    {
        close(server->fd);
        server->fd = -1;
    }

    if (server->socket_path[0] != '\0')
    {
        unlink(server->socket_path);
        server->socket_path[0] = '\0';
    }
}
