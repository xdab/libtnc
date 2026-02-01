#ifndef TEST_SELECTOR_H
#define TEST_SELECTOR_H

#include "test.h"
#include "socket.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void test_selector_create(void)
{
    socket_selector_t *sel = socket_selector_create();
    assert_true(sel != NULL, "create returns non-null");
    socket_selector_free(sel);
}

void test_selector_add_single(void)
{
    socket_selector_t *sel = socket_selector_create();

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    assert_true(fd >= 0, "socket created");

    int result = socket_selector_add(sel, fd, SELECT_READ);
    assert_equal_int(result, 0, "add returns 0");

    socket_selector_free(sel);
    close(fd);
}

void test_selector_add_multiple(void)
{
    socket_selector_t *sel = socket_selector_create();

    int fd1 = socket(AF_INET, SOCK_DGRAM, 0);
    int fd2 = socket(AF_INET, SOCK_DGRAM, 0);
    int fd3 = socket(AF_INET, SOCK_DGRAM, 0);

    assert_equal_int(socket_selector_add(sel, fd1, SELECT_READ), 0, "add fd1");
    assert_equal_int(socket_selector_add(sel, fd2, SELECT_WRITE), 0, "add fd2");
    assert_equal_int(socket_selector_add(sel, fd3, SELECT_READ | SELECT_WRITE), 0, "add fd3");

    socket_selector_free(sel);
    close(fd1);
    close(fd2);
    close(fd3);
}

void test_selector_add_max(void)
{
    socket_selector_t *sel = socket_selector_create();

    int fds[SELECT_MAX_FDS];
    for (int i = 0; i < SELECT_MAX_FDS; i++)
    {
        fds[i] = socket(AF_INET, SOCK_DGRAM, 0);
        assert_equal_int(socket_selector_add(sel, fds[i], SELECT_READ), 0, "add within limit");
    }

    int extra_fd = socket(AF_INET, SOCK_DGRAM, 0);
    assert_equal_int(socket_selector_add(sel, extra_fd, SELECT_READ), -1, "add beyond limit fails");

    socket_selector_free(sel);
    for (int i = 0; i < SELECT_MAX_FDS; i++)
        close(fds[i]);
    close(extra_fd);
}

void test_selector_remove(void)
{
    socket_selector_t *sel = socket_selector_create();

    int fd1 = socket(AF_INET, SOCK_DGRAM, 0);
    int fd2 = socket(AF_INET, SOCK_DGRAM, 0);

    socket_selector_add(sel, fd1, SELECT_READ);
    socket_selector_add(sel, fd2, SELECT_READ);

    assert_equal_int(socket_selector_remove(sel, fd1), 0, "remove returns 0");

    socket_selector_free(sel);
    close(fd1);
    close(fd2);
}

void test_selector_remove_nonexistent(void)
{
    socket_selector_t *sel = socket_selector_create();

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    socket_selector_add(sel, fd, SELECT_READ);

    int fake_fd = 9999;
    assert_equal_int(socket_selector_remove(sel, fake_fd), -1, "remove non-existent returns -1");

    socket_selector_free(sel);
    close(fd);
}

void test_selector_wait_timeout(void)
{
    socket_selector_t *sel = socket_selector_create();

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    socket_selector_add(sel, fd, SELECT_READ);

    int result = socket_selector_wait(sel, 10);
    assert_equal_int(result, 0, "timeout returns 0");

    socket_selector_free(sel);
    close(fd);
}

void test_selector_wait_ready(void)
{
    socket_selector_t *sel = socket_selector_create();

    int fds[2];
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, fds) < 0)
    {
        assert_true(0, "socketpair failed");
        return;
    }

    socket_selector_add(sel, fds[0], SELECT_READ);

    write(fds[1], "x", 1);

    int result = socket_selector_wait(sel, 100);
    assert_true(result > 0, "wait returns > 0 when data ready");
    assert_true(socket_selector_is_ready(sel, fds[0]), "fd is ready");

    socket_selector_free(sel);
    close(fds[0]);
    close(fds[1]);
}

void test_selector_is_ready(void)
{
    socket_selector_t *sel = socket_selector_create();

    int fds[2];
    socketpair(AF_UNIX, SOCK_DGRAM, 0, fds);

    socket_selector_add(sel, fds[0], SELECT_READ);

    assert_equal_int(socket_selector_is_ready(sel, fds[0]), 0, "not ready before data");

    write(fds[1], "x", 1);
    socket_selector_wait(sel, 100);

    assert_equal_int(socket_selector_is_ready(sel, fds[0]), 1, "ready after data");

    socket_selector_free(sel);
    close(fds[0]);
    close(fds[1]);
}

void test_selector_is_ready_not_ready(void)
{
    socket_selector_t *sel = socket_selector_create();

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    socket_selector_add(sel, fd, SELECT_READ);

    assert_equal_int(socket_selector_is_ready(sel, fd), 0, "unready fd returns 0");

    socket_selector_free(sel);
    close(fd);
}

void test_selector_free(void)
{
    socket_selector_t *sel = socket_selector_create();
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    socket_selector_add(sel, fd, SELECT_READ);

    socket_selector_free(sel);
    close(fd);
}

void test_selector_mixed_events(void)
{
    socket_selector_t *sel = socket_selector_create();

    int fds[2];
    socketpair(AF_UNIX, SOCK_DGRAM, 0, fds);

    int result = socket_selector_add(sel, fds[0], SELECT_READ | SELECT_WRITE | SELECT_ERROR);
    assert_equal_int(result, 0, "add with mixed events");

    write(fds[1], "x", 1);
    int ready = socket_selector_wait(sel, 100);
    assert_true(ready > 0, "wait returns ready");

    socket_selector_free(sel);
    close(fds[0]);
    close(fds[1]);
}

void test_selector_max_fd_tracking(void)
{
    socket_selector_t *sel = socket_selector_create();

    int fd1 = socket(AF_INET, SOCK_DGRAM, 0);
    int fd2 = socket(AF_INET, SOCK_DGRAM, 0);

    socket_selector_add(sel, fd1, SELECT_READ);
    socket_selector_add(sel, fd2, SELECT_READ);

    assert_equal_int(socket_selector_remove(sel, fd2), 0, "remove higher fd");
    assert_equal_int(socket_selector_remove(sel, fd1), 0, "remove lower fd");

    socket_selector_free(sel);
    close(fd1);
    close(fd2);
}

void test_selector_wait_error(void)
{
    socket_selector_t *sel = socket_selector_create();

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    socket_selector_add(sel, fd, SELECT_READ);

    close(fd);

    int result = socket_selector_wait(sel, 10);
    assert_equal_int(result, -1, "wait on closed fd returns -1");

    socket_selector_free(sel);
}

#endif
