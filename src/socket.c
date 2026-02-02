#include "socket.h"
#include "common.h"
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/select.h>

int socket_set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int socket_init_server(int domain, int type)
{
    int fd = socket(domain, type, 0);
    if (fd < 0)
    {
        LOGV("socket() failed: %s (errno=%d)", strerror(errno), errno);
        return -1;
    }

    int reuse = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        LOGV("setsockopt(SO_REUSEADDR) failed: %s (errno=%d)", strerror(errno), errno);
        close(fd);
        return -1;
    }

    if (socket_set_nonblocking(fd) < 0)
    {
        LOGV("socket_set_nonblocking() failed: %s (errno=%d)", strerror(errno), errno);
        close(fd);
        return -1;
    }

    return fd;
}

int socket_bind(int listen_fd, int port)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        LOG("bind() failed on port %d: %s (errno=%d)", port, strerror(errno), errno);
        close(listen_fd);
        return -1;
    }

    if (listen(listen_fd, 5) < 0)
    {
        LOGV("listen() failed: %s (errno=%d)", strerror(errno), errno);
        close(listen_fd);
        return -1;
    }

    return 0;
}

int socket_select(int fd, int timeout_ms)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    int ret;
    if (timeout_ms > 0)
    {
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = timeout_ms * 1000;
        ret = select(fd + 1, &fds, NULL, NULL, &tv);
    }
    else
    {
        struct timeval tv = {0, 0};
        ret = select(fd + 1, &fds, NULL, NULL, &tv);
    }

    if (ret < 0)
    {
        LOG("select() failed: %s (errno=%d)", strerror(errno), errno);
        return -1;
    }

    if (ret == 0 || !FD_ISSET(fd, &fds))
        return 0;

    return 1;
}

int socket_check_connection(int fd)
{
    int error;
    socklen_t len = sizeof(error);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
    {
        LOG("getsockopt failed: %s (errno=%d)", strerror(errno), errno);
        return -1;
    }

    if (error != 0 && error != EINPROGRESS)
    {
        LOG("connection failed: %s", strerror(error));
        return -1;
    }

    return 0;
}
