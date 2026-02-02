#include "poller.h"
#include "common.h"
#include <string.h>
#include <unistd.h>
#include <errno.h>

void socket_poller_init(socket_poller_t *pol)
{
    memset(pol, 0, sizeof(socket_poller_t));
    pol->epoll_fd = -1;

    pol->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (pol->epoll_fd < 0)
    {
        LOG("epoll_create1 failed: %s", strerror(errno));
        pol->epoll_fd = -1;
    }
}

void socket_poller_free(socket_poller_t *pol)
{
    if (pol->epoll_fd >= 0)
    {
        close(pol->epoll_fd);
        pol->epoll_fd = -1;
    }
    pol->num_events = 0;
}

int socket_poller_add(socket_poller_t *pol, int fd, uint32_t events)
{
    if (pol->epoll_fd < 0)
        return -1;

    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;

    if (epoll_ctl(pol->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0)
    {
        LOGV("epoll_ctl ADD failed for fd %d: %s", fd, strerror(errno));
        return -1;
    }

    return 0;
}

int socket_poller_remove(socket_poller_t *pol, int fd)
{
    if (pol->epoll_fd < 0)
        return -1;

    if (epoll_ctl(pol->epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0)
    {
        LOGV("epoll_ctl DEL failed for fd %d: %s", fd, strerror(errno));
        return -1;
    }

    return 0;
}

int socket_poller_wait(socket_poller_t *pol, int timeout_ms)
{
    if (pol->epoll_fd < 0)
        return -1;

    pol->num_events = 0;

    int ret = epoll_wait(pol->epoll_fd, pol->events, POLLER_MAX_EVENTS, timeout_ms);
    if (ret < 0)
    {
        if (errno == EINTR)
            return 0;
        LOG("epoll_wait failed: %s", strerror(errno));
        return -1;
    }

    pol->num_events = ret;
    return ret;
}

int socket_poller_is_ready(socket_poller_t *pol, int fd)
{
    for (int i = 0; i < pol->num_events; i++)
    {
        if (pol->events[i].data.fd == fd)
            return (pol->events[i].events & EPOLLIN) ? 1 : 0;
    }
    return 0;
}
