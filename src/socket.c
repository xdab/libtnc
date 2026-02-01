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

//

struct socket_selector
{
    int fds[SELECT_MAX_FDS];
    int events[SELECT_MAX_FDS];
    int num_fds;
    int max_fd;
};

socket_selector_t *socket_selector_create(void)
{
    socket_selector_t *sel = malloc(sizeof(socket_selector_t));
    if (!sel)
        return NULL;

    sel->num_fds = 0;
    sel->max_fd = -1;

    return sel;
}

int socket_selector_add(socket_selector_t *sel, int fd, int events)
{
    if (sel->num_fds >= SELECT_MAX_FDS)
        return -1;

    if (fd > sel->max_fd)
        sel->max_fd = fd;

    sel->fds[sel->num_fds] = fd;
    sel->events[sel->num_fds] = events;
    sel->num_fds++;

    return 0;
}

int socket_selector_remove(socket_selector_t *sel, int fd)
{
    for (int i = 0; i < sel->num_fds; i++)
    {
        if (sel->fds[i] == fd)
        {
            sel->fds[i] = sel->fds[sel->num_fds - 1];
            sel->events[i] = sel->events[sel->num_fds - 1];
            sel->num_fds--;

            if (fd == sel->max_fd)
            {
                sel->max_fd = -1;
                for (int j = 0; j < sel->num_fds; j++)
                    if (sel->fds[j] > sel->max_fd)
                        sel->max_fd = sel->fds[j];
            }

            return 0;
        }
    }

    return -1;
}

int socket_selector_wait(socket_selector_t *sel, int timeout_ms)
{
    fd_set read_fds;
    fd_set write_fds;
    fd_set error_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&error_fds);

    for (int i = 0; i < sel->num_fds; i++)
    {
        int fd = sel->fds[i];
        int events = sel->events[i];
        if (events & SELECT_READ)
            FD_SET(fd, &read_fds);
        if (events & SELECT_WRITE)
            FD_SET(fd, &write_fds);
        if (events & SELECT_ERROR)
            FD_SET(fd, &error_fds);
    }

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(sel->max_fd + 1, &read_fds, &write_fds, &error_fds, &tv);
    if (ret < 0)
    {
        LOG("select() failed: %s (errno=%d)", strerror(errno), errno);
        return -1;
    }

    if (ret == 0)
        return 0;

    for (int i = 0; i < sel->num_fds; i++)
    {
        int fd = sel->fds[i];
        int events = sel->events[i];
        int ready = 0;
        if ((events & SELECT_READ) && FD_ISSET(fd, &read_fds))
            ready = 1;
        else if ((events & SELECT_WRITE) && FD_ISSET(fd, &write_fds))
            ready = 1;
        else if ((events & SELECT_ERROR) && FD_ISSET(fd, &error_fds))
            ready = 1;
        sel->events[i] = ready ? (sel->events[i] | 0x80000000) : (sel->events[i] & 0x7FFFFFFF);
    }

    return ret;
}

int socket_selector_is_ready(socket_selector_t *sel, int fd)
{
    for (int i = 0; i < sel->num_fds; i++)
    {
        if (sel->fds[i] == fd)
            return (sel->events[i] & 0x80000000) ? 1 : 0;
    }
    return 0;
}

void socket_selector_free(socket_selector_t *sel)
{
    free(sel);
}
