#include "tcp.h"
#include "common.h"
#include "buffer.h"
#include "socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

int tcp_server_init(tcp_server_t *server, int port, int timeout_ms)
{
    nonnull(server, "server");
    EXITIF(port < 0, -1, "port must be positive");
    EXITIF(port > 65535, -1, "port must be less than 65536");

    memset(server, 0, sizeof(tcp_server_t));
    server->timeout_ms = timeout_ms;

    for (int i = 0; i < TCP_MAX_CLIENTS; i++)
        server->clients[i].fd = -1;

    server->listen_fd = socket_init_server(AF_INET, SOCK_STREAM);
    if (server->listen_fd < 0)
        return -1;

    if (socket_bind(server->listen_fd, port) < 0)
        return -1;

    LOG("server listening on port %d", port);
    return 0;
}

void tcp_server_free(tcp_server_t *server)
{
    nonnull(server, "server");

    for (int i = 0; i < server->num_clients; i++)
    {
        if (server->clients[i].fd >= 0)
            close(server->clients[i].fd);
    }

    if (server->listen_fd >= 0)
        close(server->listen_fd);

    for (int i = 0; i < TCP_MAX_CLIENTS; i++)
        server->clients[i].fd = -1;
    server->listen_fd = -1;
    server->num_clients = 0;
}

static void tcp_remove_client(tcp_server_t *server, int idx)
{
    if (idx < 0 || idx >= server->num_clients)
        return;

    if (server->on_client_disconnect)
        server->on_client_disconnect(server->clients[idx].fd, server->user_data);

    close(server->clients[idx].fd);

    for (int i = idx; i < server->num_clients - 1; i++)
        server->clients[i] = server->clients[i + 1];

    server->num_clients--;
}

int tcp_server_listen(tcp_server_t *server, buffer_t *out_buf)
{
    nonnull(server, "server");
    assert_buffer_valid(out_buf);

    fd_set fds;
    FD_ZERO(&fds);

    FD_SET(server->listen_fd, &fds);
    int max_fd = server->listen_fd;

    for (int i = 0; i < server->num_clients; i++)
        if (server->clients[i].fd >= 0)
        {
            FD_SET(server->clients[i].fd, &fds);
            max_fd = max(max_fd, server->clients[i].fd);
        }

    int ret;
    if (server->timeout_ms > 0)
    {
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = server->timeout_ms * 1000;
        ret = select(max_fd + 1, &fds, NULL, NULL, &tv);
    }
    else
    {
        struct timeval tv = {0, 0};
        ret = select(max_fd + 1, &fds, NULL, NULL, &tv);
    }
    if (ret < 0)
    {
        LOG("select() failed: %s (errno=%d)", strerror(errno), errno);
        return -1;
    }

    if (ret == 0)
        return 0;

    if (FD_ISSET(server->listen_fd, &fds))
    {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int client_fd = accept(server->listen_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd < 0)
            goto handle_connected_clients;

        if (server->num_clients >= TCP_MAX_CLIENTS)
        {
            LOG("max clients reached, rejecting incoming connection");
            close(client_fd);
            goto handle_connected_clients;
        }

        if (socket_set_nonblocking(client_fd) < 0)
        {
            LOG("could not set incoming connection to nonblocking mode");
            close(client_fd);
            goto handle_connected_clients;
        }

        server->clients[server->num_clients].fd = client_fd;
        server->num_clients++;

        if (server->on_client_connect)
            server->on_client_connect(client_fd, server->user_data);

        char ipbuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ipbuf, sizeof(ipbuf));
        LOG("client connected from %s (total: %d)", ipbuf, server->num_clients);
    }

handle_connected_clients:
    for (int i = 0; i < server->num_clients; i++)
    {
        if (server->clients[i].fd < 0)
            continue;

        if (!FD_ISSET(server->clients[i].fd, &fds))
            continue;

        int n = read(server->clients[i].fd, out_buf->data, out_buf->capacity);
        if (n > 0)
        {
            out_buf->size = n;
            return n;
        }

        if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK))
        {
            LOG("client disconnected: %s (errno=%d)", strerror(errno), errno);
            tcp_remove_client(server, i);
            i--;
        }
    }

    return 0;
}

void tcp_server_broadcast(tcp_server_t *server, const buffer_t *buf)
{
    nonnull(server, "server");
    assert_buffer_valid(buf);

    if (buf->size == 0)
        return;

    for (int i = 0; i < server->num_clients; i++)
    {
        if (server->clients[i].fd < 0)
            continue;

        ssize_t sent = send(server->clients[i].fd, buf->data, buf->size, MSG_NOSIGNAL | MSG_DONTWAIT);
        if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            LOGV("send failed, removing client: %s (errno=%d)", strerror(errno), errno);
            tcp_remove_client(server, i);
            i--;
            continue;
        }

        LOGV("sent %zd bytes to client %d (fd %d)", sent, i, server->clients[i].fd);
    }
}

int tcp_client_init(tcp_client_t *client, const char *addr, int port, int timeout_ms)
{
    nonnull(client, "client");
    nonnull(addr, "addr");
    EXITIF(port < 0, -1, "port must be positive");
    EXITIF(port > 65535, -1, "port must be less than 65536");

    memset(client, 0, sizeof(tcp_client_t));
    client->timeout_ms = timeout_ms;

    client->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->fd < 0)
    {
        LOGV("socket() failed: %s (errno=%d)", strerror(errno), errno);
        return -1;
    }

    if (socket_set_nonblocking(client->fd) < 0)
    {
        LOGV("net_set_nonblocking() failed: %s (errno=%d)", strerror(errno), errno);
        close(client->fd);
        client->fd = -1;
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, addr, &server_addr.sin_addr) <= 0)
    {
        LOG("invalid address: %s", addr);
        close(client->fd);
        client->fd = -1;
        return -1;
    }

    if (connect(client->fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        if (errno != EINPROGRESS)
        {
            LOG("connect() failed to %s:%d: %s (errno=%d)", addr, port, strerror(errno), errno);
            close(client->fd);
            client->fd = -1;
            return -1;
        }
    }

    LOG("connected to %s:%d", addr, port);
    return 0;
}

void tcp_client_free(tcp_client_t *client)
{
    nonnull(client, "client");

    if (client->fd >= 0)
        close(client->fd);
    client->fd = -1;
}

int tcp_client_listen(tcp_client_t *client, buffer_t *out_buf)
{
    nonnull(client, "client");
    assert_buffer_valid(out_buf);

    if (client->fd < 0)
        return -1;

    if (socket_check_connection(client->fd) < 0)
    {
        tcp_client_free(client);
        return -1;
    }

    int ret = socket_select(client->fd, client->timeout_ms);
    if (ret <= 0)
        return ret;

    int n = read(client->fd, out_buf->data, out_buf->capacity);
    if (n > 0)
        return n;

    if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK))
    {
        LOG("remote disconnected: %s (errno=%d)", strerror(errno), errno);
        tcp_client_free(client);
        return -1;
    }

    return 0;
}

int tcp_client_send(tcp_client_t *client, const buffer_t *buf)
{
    nonnull(client, "client");
    assert_buffer_valid(buf);

    if (client->fd < 0)
        return -1;

    if (buf->size == 0)
        return 0;

    if (socket_check_connection(client->fd) < 0)
    {
        tcp_client_free(client);
        return -1;
    }

    ssize_t sent = send(client->fd, buf->data, buf->size, MSG_NOSIGNAL | MSG_DONTWAIT);
    if (sent < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;

        LOGV("send failed: %s (errno=%d)", strerror(errno), errno);
        tcp_client_free(client);
        return -1;
    }

    LOGV("sent %zd bytes", sent);
    return (int)sent;
}