#ifndef TEST_POLLER_H
#define TEST_POLLER_H

#include "test.h"
#include "poller.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void test_poller_create(void)
{
    socket_poller_t pol;
    socket_poller_init(&pol);
    assert_true(pol.epoll_fd >= 0, "poller epoll_fd is valid after init");
    socket_poller_free(&pol);
    assert_equal_int(pol.epoll_fd, -1, "epoll_fd is -1 after free");
}

void test_poller_add_single(void)
{
    socket_poller_t pol;
    socket_poller_init(&pol);

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    assert_true(fd >= 0, "socket created");

    int result = socket_poller_add(&pol, fd, POLLER_EV_IN);
    assert_equal_int(result, 0, "add returns 0");

    close(fd);
    socket_poller_free(&pol);
}

void test_poller_add_multiple(void)
{
    socket_poller_t pol;
    socket_poller_init(&pol);

    int fd1 = socket(AF_INET, SOCK_DGRAM, 0);
    int fd2 = socket(AF_INET, SOCK_DGRAM, 0);
    int fd3 = socket(AF_INET, SOCK_DGRAM, 0);

    assert_equal_int(socket_poller_add(&pol, fd1, POLLER_EV_IN), 0, "add fd1 with POLLER_EV_IN");
    assert_equal_int(socket_poller_add(&pol, fd2, POLLER_EV_OUT), 0, "add fd2 with POLLER_EV_OUT");
    assert_equal_int(socket_poller_add(&pol, fd3, POLLER_EV_IN | POLLER_EV_OUT), 0, "add fd3 with mixed events");

    close(fd1);
    close(fd2);
    close(fd3);
    socket_poller_free(&pol);
}

void test_poller_remove(void)
{
    socket_poller_t pol;
    socket_poller_init(&pol);

    int fd1 = socket(AF_INET, SOCK_DGRAM, 0);
    int fd2 = socket(AF_INET, SOCK_DGRAM, 0);

    socket_poller_add(&pol, fd1, POLLER_EV_IN);
    socket_poller_add(&pol, fd2, POLLER_EV_IN);

    assert_equal_int(socket_poller_remove(&pol, fd1), 0, "remove returns 0");

    close(fd1);
    close(fd2);
    socket_poller_free(&pol);
}

void test_poller_remove_nonexistent(void)
{
    socket_poller_t pol;
    socket_poller_init(&pol);

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    socket_poller_add(&pol, fd, POLLER_EV_IN);

    int fake_fd = 9999;
    assert_equal_int(socket_poller_remove(&pol, fake_fd), -1, "remove non-existent returns -1");

    close(fd);
    socket_poller_free(&pol);
}

void test_poller_wait_timeout(void)
{
    socket_poller_t pol;
    socket_poller_init(&pol);

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    socket_poller_add(&pol, fd, POLLER_EV_IN);

    int result = socket_poller_wait(&pol, 10);
    assert_equal_int(result, 0, "timeout returns 0");

    close(fd);
    socket_poller_free(&pol);
}

void test_poller_wait_ready(void)
{
    socket_poller_t pol;
    socket_poller_init(&pol);

    int fds[2];
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, fds) < 0)
    {
        assert_true(0, "socketpair failed");
        return;
    }

    socket_poller_add(&pol, fds[0], POLLER_EV_IN);

    write(fds[1], "x", 1);

    int result = socket_poller_wait(&pol, 100);
    assert_true(result > 0, "wait returns > 0 when data ready");
    assert_true(socket_poller_is_ready(&pol, fds[0]), "fd is ready");

    close(fds[0]);
    close(fds[1]);
    socket_poller_free(&pol);
}

void test_poller_is_ready(void)
{
    socket_poller_t pol;
    socket_poller_init(&pol);

    int fds[2];
    socketpair(AF_UNIX, SOCK_DGRAM, 0, fds);

    socket_poller_add(&pol, fds[0], POLLER_EV_IN);

    assert_equal_int(socket_poller_is_ready(&pol, fds[0]), 0, "not ready before data");

    write(fds[1], "x", 1);
    socket_poller_wait(&pol, 100);

    assert_equal_int(socket_poller_is_ready(&pol, fds[0]), 1, "ready after data");

    close(fds[0]);
    close(fds[1]);
    socket_poller_free(&pol);
}

void test_poller_is_ready_not_ready(void)
{
    socket_poller_t pol;
    socket_poller_init(&pol);

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    socket_poller_add(&pol, fd, POLLER_EV_IN);

    assert_equal_int(socket_poller_is_ready(&pol, fd), 0, "unready fd returns 0");

    close(fd);
    socket_poller_free(&pol);
}

void test_poller_free(void)
{
    socket_poller_t pol;
    socket_poller_init(&pol);
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    socket_poller_add(&pol, fd, POLLER_EV_IN);

    socket_poller_free(&pol);
    assert_equal_int(pol.epoll_fd, -1, "epoll_fd is -1 after free");
    assert_equal_int(pol.num_events, 0, "num_events is 0 after free");

    close(fd);
}

void test_poller_mixed_events(void)
{
    socket_poller_t pol;
    socket_poller_init(&pol);

    int fds[2];
    socketpair(AF_UNIX, SOCK_DGRAM, 0, fds);

    int result = socket_poller_add(&pol, fds[0], POLLER_EV_IN | POLLER_EV_OUT | POLLER_EV_ERR);
    assert_equal_int(result, 0, "add with mixed events");

    write(fds[1], "x", 1);
    int ready = socket_poller_wait(&pol, 100);
    assert_true(ready > 0, "wait returns ready");

    close(fds[0]);
    close(fds[1]);
    socket_poller_free(&pol);
}

#endif
