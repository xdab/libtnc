/**
 * libtnc_echo - Multi-protocol echo server demonstrating libtnc networking utilities.
 *
 * Listens on TCP, UDP, and Unix Domain Socket, echoing received data back.
 * Uses epoll poller for efficient I/O multiplexing without busywaiting.
 *
 * Hardcoded configuration (testing executable):
 *   TCP port: 8001
 *   UDP port: 8002
 *   UDS path: /tmp/libtnc_echo.sock
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>

#include "common.h"
#include "buffer.h"
#include "poller.h"
#include "socket.h"

#define ECHO_TCP_PORT 8001
#define ECHO_UDP_PORT 8002
#define ECHO_UDS_PATH "/tmp/libtnc_echo.sock"
#define ECHO_UDS_DGRAM_PATH "/tmp/libtnc_echo_dgram.sock"
#define ECHO_BUF_SIZE 2048
#define ECHO_MAX_TCP_CLIENTS 16

typedef enum
{
    SOCK_TYPE_TCP_LISTEN,
    SOCK_TYPE_TCP_CLIENT,
    SOCK_TYPE_UDP,
    SOCK_TYPE_UDS_LISTEN,
    SOCK_TYPE_UDS_CLIENT,
    SOCK_TYPE_UDS_DGRAM
} sock_type_e;

typedef struct echo_server echo_server_t;

typedef struct client_info
{
    int fd;
    sock_type_e type;
} client_info_t;

struct echo_server
{
    socket_poller_t poller;

    int tcp_listen_fd;
    client_info_t tcp_clients[ECHO_MAX_TCP_CLIENTS];
    int tcp_num_clients;

    int udp_fd;
    struct sockaddr_in udp_client_addr;
    socklen_t udp_client_addr_len;

    int uds_listen_fd;
    client_info_t uds_clients[ECHO_MAX_TCP_CLIENTS];
    int uds_num_clients;

    int uds_dgram_fd;
    struct sockaddr_un uds_dgram_client_addr;
    socklen_t uds_dgram_client_addr_len;

    unsigned char buffer[ECHO_BUF_SIZE];
    volatile int running;
};

static echo_server_t g_server;

static void signal_handler(int sig)
{
    (void)sig;
    g_server.running = 0;
}

static int tcp_server_init(echo_server_t *srv)
{
    srv->tcp_listen_fd = socket_init_server(AF_INET, SOCK_STREAM);
    if (srv->tcp_listen_fd < 0)
        return -1;

    if (socket_bind(srv->tcp_listen_fd, ECHO_TCP_PORT) < 0)
        goto fail;

    if (listen(srv->tcp_listen_fd, 5) < 0)
        goto fail;

    if (socket_set_nonblocking(srv->tcp_listen_fd) < 0)
        goto fail;

    if (socket_poller_add(&srv->poller, srv->tcp_listen_fd, POLLER_EV_IN) < 0)
        goto fail;

    LOG("tcp server listening on port %d", ECHO_TCP_PORT);
    return 0;

fail:
    close(srv->tcp_listen_fd);
    srv->tcp_listen_fd = -1;
    return -1;
}

static void tcp_remove_client(echo_server_t *srv, int idx)
{
    if (idx < 0 || idx >= srv->tcp_num_clients)
        return;

    int fd = srv->tcp_clients[idx].fd;
    socket_poller_remove(&srv->poller, fd);
    close(fd);

    for (int i = idx; i < srv->tcp_num_clients - 1; i++)
        srv->tcp_clients[i] = srv->tcp_clients[i + 1];

    srv->tcp_num_clients--;
    LOG("tcp client disconnected (total: %d)", srv->tcp_num_clients);
}

static int tcp_accept_client(echo_server_t *srv)
{
    if (srv->tcp_num_clients >= ECHO_MAX_TCP_CLIENTS)
    {
        LOG("tcp max clients reached, rejecting connection");
        int fd = accept(srv->tcp_listen_fd, NULL, NULL);
        if (fd >= 0)
            close(fd);
        return -1;
    }

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    int client_fd = accept(srv->tcp_listen_fd, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_fd < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            LOGV("tcp accept failed: %s", strerror(errno));
        return -1;
    }

    if (socket_set_nonblocking(client_fd) < 0)
    {
        close(client_fd);
        return -1;
    }

    if (socket_poller_add(&srv->poller, client_fd, POLLER_EV_IN) < 0)
    {
        close(client_fd);
        return -1;
    }

    srv->tcp_clients[srv->tcp_num_clients].fd = client_fd;
    srv->tcp_clients[srv->tcp_num_clients].type = SOCK_TYPE_TCP_CLIENT;
    srv->tcp_num_clients++;

    char ipbuf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ipbuf, sizeof(ipbuf));
    LOG("tcp client connected from %s (total: %d)", ipbuf, srv->tcp_num_clients);

    return 0;
}

static int tcp_handle_client(echo_server_t *srv, int idx)
{
    int fd = srv->tcp_clients[idx].fd;

    ssize_t n = recv(fd, srv->buffer, sizeof(srv->buffer), 0);
    if (n <= 0)
    {
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return 0;

        LOGV("tcp client recv error: %s", strerror(errno));
        tcp_remove_client(srv, idx);
        return -1;
    }

    LOGV("tcp received %zd bytes from client %d", n, idx);

    ssize_t sent = send(fd, srv->buffer, n, MSG_NOSIGNAL | MSG_DONTWAIT);
    if (sent < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            LOGV("tcp client send error: %s", strerror(errno));
            tcp_remove_client(srv, idx);
            return -1;
        }
    }
    else
    {
        LOGV("tcp echoed %zd bytes to client %d", sent, idx);
    }

    return (int)n;
}

static int udp_server_init(echo_server_t *srv)
{
    srv->udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (srv->udp_fd < 0)
    {
        LOGV("udp socket failed: %s", strerror(errno));
        return -1;
    }

    int reuse = 1;
    if (setsockopt(srv->udp_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        LOGV("udp setsockopt failed: %s", strerror(errno));
        goto fail;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(ECHO_UDP_PORT);

    if (bind(srv->udp_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        LOG("udp bind failed on port %d: %s", ECHO_UDP_PORT, strerror(errno));
        goto fail;
    }

    if (socket_set_nonblocking(srv->udp_fd) < 0)
        goto fail;

    if (socket_poller_add(&srv->poller, srv->udp_fd, POLLER_EV_IN) < 0)
        goto fail;

    LOG("udp server listening on port %d", ECHO_UDP_PORT);
    return 0;

fail:
    close(srv->udp_fd);
    srv->udp_fd = -1;
    return -1;
}

static int udp_handle(echo_server_t *srv)
{
    srv->udp_client_addr_len = sizeof(srv->udp_client_addr);

    ssize_t n = recvfrom(srv->udp_fd, srv->buffer, sizeof(srv->buffer), 0,
                         (struct sockaddr *)&srv->udp_client_addr, &srv->udp_client_addr_len);
    if (n < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;

        LOG("udp recvfrom error: %s", strerror(errno));
        return -1;
    }

    char ipbuf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &srv->udp_client_addr.sin_addr, ipbuf, sizeof(ipbuf));
    LOGV("udp received %zd bytes from %s:%d", n, ipbuf, ntohs(srv->udp_client_addr.sin_port));

    ssize_t sent = sendto(srv->udp_fd, srv->buffer, n, 0,
                          (struct sockaddr *)&srv->udp_client_addr, srv->udp_client_addr_len);
    if (sent < 0)
    {
        LOG("udp sendto error: %s", strerror(errno));
        return -1;
    }

    LOGV("udp echoed %zd bytes to %s:%d", sent, ipbuf, ntohs(srv->udp_client_addr.sin_port));
    return (int)n;
}

static int uds_server_init(echo_server_t *srv)
{
    srv->uds_listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (srv->uds_listen_fd < 0)
    {
        LOGV("uds socket failed: %s", strerror(errno));
        return -1;
    }

    unlink(ECHO_UDS_PATH);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, ECHO_UDS_PATH, sizeof(addr.sun_path) - 1);

    if (bind(srv->uds_listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        LOG("uds bind failed on %s: %s", ECHO_UDS_PATH, strerror(errno));
        goto fail;
    }

    if (listen(srv->uds_listen_fd, 5) < 0)
    {
        LOGV("uds listen failed: %s", strerror(errno));
        goto fail;
    }

    if (socket_set_nonblocking(srv->uds_listen_fd) < 0)
        goto fail;

    if (socket_poller_add(&srv->poller, srv->uds_listen_fd, POLLER_EV_IN) < 0)
        goto fail;

    LOG("uds server listening on %s", ECHO_UDS_PATH);
    return 0;

fail:
    close(srv->uds_listen_fd);
    srv->uds_listen_fd = -1;
    return -1;
}

static void uds_remove_client(echo_server_t *srv, int idx)
{
    if (idx < 0 || idx >= srv->uds_num_clients)
        return;

    int fd = srv->uds_clients[idx].fd;
    socket_poller_remove(&srv->poller, fd);
    close(fd);

    for (int i = idx; i < srv->uds_num_clients - 1; i++)
        srv->uds_clients[i] = srv->uds_clients[i + 1];

    srv->uds_num_clients--;
    LOG("uds client disconnected (total: %d)", srv->uds_num_clients);
}

static int uds_accept_client(echo_server_t *srv)
{
    if (srv->uds_num_clients >= ECHO_MAX_TCP_CLIENTS)
    {
        LOG("uds max clients reached, rejecting connection");
        int fd = accept(srv->uds_listen_fd, NULL, NULL);
        if (fd >= 0)
            close(fd);
        return -1;
    }

    int client_fd = accept(srv->uds_listen_fd, NULL, NULL);
    if (client_fd < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            LOGV("uds accept failed: %s", strerror(errno));
        return -1;
    }

    if (socket_set_nonblocking(client_fd) < 0)
    {
        close(client_fd);
        return -1;
    }

    if (socket_poller_add(&srv->poller, client_fd, POLLER_EV_IN) < 0)
    {
        close(client_fd);
        return -1;
    }

    srv->uds_clients[srv->uds_num_clients].fd = client_fd;
    srv->uds_clients[srv->uds_num_clients].type = SOCK_TYPE_UDS_CLIENT;
    srv->uds_num_clients++;

    LOG("uds client connected (total: %d)", srv->uds_num_clients);
    return 0;
}

static int uds_handle_client(echo_server_t *srv, int idx)
{
    int fd = srv->uds_clients[idx].fd;

    ssize_t n = recv(fd, srv->buffer, sizeof(srv->buffer), 0);
    if (n <= 0)
    {
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return 0;

        LOGV("uds client recv error: %s", strerror(errno));
        uds_remove_client(srv, idx);
        return -1;
    }

    LOGV("uds received %zd bytes from client %d", n, idx);

    ssize_t sent = send(fd, srv->buffer, n, MSG_NOSIGNAL | MSG_DONTWAIT);
    if (sent < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            LOGV("uds client send error: %s", strerror(errno));
            uds_remove_client(srv, idx);
            return -1;
        }
    }
    else
    {
        LOGV("uds echoed %zd bytes to client %d", sent, idx);
    }

    return (int)n;
}

static int uds_dgram_server_init(echo_server_t *srv)
{
    srv->uds_dgram_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (srv->uds_dgram_fd < 0)
    {
        LOGV("uds dgram socket failed: %s", strerror(errno));
        return -1;
    }

    unlink(ECHO_UDS_DGRAM_PATH);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, ECHO_UDS_DGRAM_PATH, sizeof(addr.sun_path) - 1);

    if (bind(srv->uds_dgram_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        LOG("uds dgram bind failed on %s: %s", ECHO_UDS_DGRAM_PATH, strerror(errno));
        goto fail;
    }

    if (socket_set_nonblocking(srv->uds_dgram_fd) < 0)
        goto fail;

    if (socket_poller_add(&srv->poller, srv->uds_dgram_fd, POLLER_EV_IN) < 0)
        goto fail;

    LOG("uds dgram server listening on %s", ECHO_UDS_DGRAM_PATH);
    return 0;

fail:
    close(srv->uds_dgram_fd);
    srv->uds_dgram_fd = -1;
    return -1;
}

static int uds_dgram_handle(echo_server_t *srv)
{
    srv->uds_dgram_client_addr_len = sizeof(srv->uds_dgram_client_addr);

    ssize_t n = recvfrom(srv->uds_dgram_fd, srv->buffer, sizeof(srv->buffer), 0,
                         (struct sockaddr *)&srv->uds_dgram_client_addr, &srv->uds_dgram_client_addr_len);
    if (n < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;

        LOG("uds dgram recvfrom error: %s", strerror(errno));
        return -1;
    }

    LOGV("uds dgram received %zd bytes from %s", n, srv->uds_dgram_client_addr.sun_path);

    ssize_t sent = sendto(srv->uds_dgram_fd, srv->buffer, n, 0,
                          (struct sockaddr *)&srv->uds_dgram_client_addr, srv->uds_dgram_client_addr_len);
    if (sent < 0)
    {
        LOG("uds dgram sendto error: %s", strerror(errno));
        return -1;
    }

    LOGV("uds dgram echoed %zd bytes to %s", sent, srv->uds_dgram_client_addr.sun_path);
    return (int)n;
}

static void server_init(echo_server_t *srv)
{
    memset(srv, 0, sizeof(echo_server_t));
    srv->tcp_listen_fd = -1;
    srv->udp_fd = -1;
    srv->uds_listen_fd = -1;
    srv->uds_dgram_fd = -1;
    srv->running = 1;

    for (int i = 0; i < ECHO_MAX_TCP_CLIENTS; i++)
    {
        srv->tcp_clients[i].fd = -1;
        srv->uds_clients[i].fd = -1;
    }
}

static void server_free(echo_server_t *srv)
{
    for (int i = 0; i < srv->tcp_num_clients; i++)
        if (srv->tcp_clients[i].fd >= 0)
            close(srv->tcp_clients[i].fd);

    for (int i = 0; i < srv->uds_num_clients; i++)
        if (srv->uds_clients[i].fd >= 0)
            close(srv->uds_clients[i].fd);

    if (srv->tcp_listen_fd >= 0)
        close(srv->tcp_listen_fd);

    if (srv->udp_fd >= 0)
        close(srv->udp_fd);

    if (srv->uds_listen_fd >= 0)
    {
        close(srv->uds_listen_fd);
        unlink(ECHO_UDS_PATH);
    }

    if (srv->uds_dgram_fd >= 0)
    {
        close(srv->uds_dgram_fd);
        unlink(ECHO_UDS_DGRAM_PATH);
    }

    socket_poller_free(&srv->poller);
}

static int server_run(echo_server_t *srv)
{
    socket_poller_init(&srv->poller);

    if (tcp_server_init(srv) < 0)
        return -1;

    if (udp_server_init(srv) < 0)
        return -1;

    if (uds_server_init(srv) < 0)
        return -1;

    if (uds_dgram_server_init(srv) < 0)
        return -1;

    LOG("echo server running, press Ctrl+C to stop");

    while (srv->running)
    {
        int n = socket_poller_wait(&srv->poller, 100);
        if (n < 0)
        {
            LOG("poller wait failed");
            break;
        }

        if (n == 0)
            continue;

        if (socket_poller_is_ready(&srv->poller, srv->tcp_listen_fd))
            tcp_accept_client(srv);

        if (socket_poller_is_ready(&srv->poller, srv->udp_fd))
            udp_handle(srv);

        if (socket_poller_is_ready(&srv->poller, srv->uds_listen_fd))
            uds_accept_client(srv);

        if (socket_poller_is_ready(&srv->poller, srv->uds_dgram_fd))
            uds_dgram_handle(srv);

        for (int i = 0; i < srv->tcp_num_clients; i++)
        {
            if (socket_poller_is_ready(&srv->poller, srv->tcp_clients[i].fd))
            {
                tcp_handle_client(srv, i);
                break;
            }
        }

        for (int i = 0; i < srv->uds_num_clients; i++)
        {
            if (socket_poller_is_ready(&srv->poller, srv->uds_clients[i].fd))
            {
                uds_handle_client(srv, i);
                break;
            }
        }
    }

    LOG("shutting down");
    return 0;
}

int main(void)
{
    _func_pad = 12;
    _log_level = LOG_LEVEL_VERBOSE;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    server_init(&g_server);
    int ret = server_run(&g_server);
    server_free(&g_server);

    return ret < 0 ? 1 : 0;
}
