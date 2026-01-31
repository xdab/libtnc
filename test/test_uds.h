#ifndef TEST_UDS_H
#define TEST_UDS_H

#include "test.h"
#include "uds.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define TEST_SOCKET_PATH "/tmp/libtnc_test.sock"

void test_uds_server_init_valid(void)
{
    uds_server_t server;
    int result = uds_server_init(&server, TEST_SOCKET_PATH);
    assert_equal_int(result, 0, "init returns 0");
    assert_true(server.listen_fd > 2, "listen_fd valid");
    assert_equal_int(server.num_clients, 0, "num_clients 0");
    uds_server_free(&server);
}

void test_uds_server_listen_timeout(void)
{
    uds_server_t server;
    uds_server_init(&server, TEST_SOCKET_PATH);
    char buf_data[1];
    buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 1, .size = 0};
    int result = uds_server_listen(&server, &buf);
    assert_equal_int(result, 0, "timeout returns 0");
    uds_server_free(&server);
}

void test_uds_server_accept_client(void)
{
    uds_server_t server;
    uds_server_init(&server, TEST_SOCKET_PATH);

    pid_t pid = fork();
    if (pid == 0)
    {
        usleep(200000);
        int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, TEST_SOCKET_PATH, sizeof(addr.sun_path) - 1);
        connect(client_fd, (struct sockaddr *)&addr, sizeof(addr));
        close(client_fd);
        exit(0);
    }

    char buf_data[1];
    buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 1, .size = 0};
    int attempts = 0;
    while (server.num_clients == 0 && attempts++ < 20)
        uds_server_listen(&server, &buf);

    assert_equal_int(server.num_clients, 1, "client accepted");
    assert_true(server.clients[0].fd >= 3, "client fd valid");

    waitpid(pid, NULL, 0);
    uds_server_free(&server);
}

void test_uds_server_read_data(void)
{
    uds_server_t server;
    uds_server_init(&server, TEST_SOCKET_PATH);

    pid_t pid = fork();
    if (pid == 0)
    {
        usleep(200000);
        int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, TEST_SOCKET_PATH, sizeof(addr.sun_path) - 1);
        connect(client_fd, (struct sockaddr *)&addr, sizeof(addr));
        write(client_fd, "test", 4);
        close(client_fd);
        exit(0);
    }

    char buf_data[16];
    buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 16, .size = 0};
    int n = 0;
    int attempts = 0;
    while (n <= 0 && attempts++ < 20)
        n = uds_server_listen(&server, &buf);

    assert_equal_int(n, 4, "read 4 bytes");
    assert_memory(buf_data, (void *)"test", 4, "data matches");

    waitpid(pid, NULL, 0);
    uds_server_free(&server);
}

void test_uds_server_client_disconnect(void)
{
    uds_server_t server;
    uds_server_init(&server, TEST_SOCKET_PATH);

    pid_t pid = fork();
    if (pid == 0)
    {
        usleep(200000);
        int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, TEST_SOCKET_PATH, sizeof(addr.sun_path) - 1);
        connect(client_fd, (struct sockaddr *)&addr, sizeof(addr));
        close(client_fd);
        exit(0);
    }

    char buf_data[1];
    buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 1, .size = 0};
    int attempts = 0;
    while (server.num_clients == 0 && attempts++ < 20)
        uds_server_listen(&server, &buf);
    assert_equal_int(server.num_clients, 1, "client connected");

    attempts = 0;
    while (server.num_clients == 1 && attempts++ < 20)
        uds_server_listen(&server, &buf);
    assert_equal_int(server.num_clients, 0, "client disconnected");

    waitpid(pid, NULL, 0);
    uds_server_free(&server);
}

void test_uds_server_broadcast(void)
{
    uds_server_t server;
    uds_server_init(&server, TEST_SOCKET_PATH);

    int pipefd[2];
    pipe(pipefd);

    pid_t pid = fork();
    if (pid == 0)
    {
        close(pipefd[0]);
        usleep(200000);
        int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, TEST_SOCKET_PATH, sizeof(addr.sun_path) - 1);
        connect(client_fd, (struct sockaddr *)&addr, sizeof(addr));
        char rbuf[16];
        int n = read(client_fd, rbuf, sizeof(rbuf));
        write(pipefd[1], rbuf, n);
        close(client_fd);
        close(pipefd[1]);
        exit(0);
    }

    close(pipefd[1]);

    char buf_data[1];
    buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 1, .size = 0};
    int attempts = 0;
    while (server.num_clients == 0 && attempts++ < 20)
        uds_server_listen(&server, &buf);
    assert_equal_int(server.num_clients, 1, "client connected");

    char msg_data[] = "hello";
    buffer_t msg_buf = {.data = (unsigned char *)msg_data, .capacity = 5, .size = 5};
    uds_server_broadcast(&server, &msg_buf);

    char received[16];
    int n = read(pipefd[0], received, sizeof(received));
    assert_equal_int(n, 5, "received 5 bytes");
    assert_memory(received, (void *)"hello", 5, "broadcast message received");

    close(pipefd[0]);
    waitpid(pid, NULL, 0);
    uds_server_free(&server);
}

void test_uds_server_free(void)
{
    uds_server_t server;
    uds_server_init(&server, TEST_SOCKET_PATH);
    uds_server_free(&server);
    assert_equal_int(server.listen_fd, -1, "listen_fd reset");
    assert_equal_int(server.num_clients, 0, "num_clients reset");
}

void test_uds_server_init_invalid_path(void)
{
    uds_server_t server;

    int result = uds_server_init(&server, "");
    assert_equal_int(result, -1, "empty path rejected");

    char long_path[200];
    memset(long_path, 'a', sizeof(long_path) - 1);
    long_path[sizeof(long_path) - 1] = '\0';
    result = uds_server_init(&server, long_path);
    assert_equal_int(result, -1, "too long path rejected");
}

void test_uds_client_init_valid(void)
{
    uds_server_t server;
    uds_server_init(&server, TEST_SOCKET_PATH);

    uds_client_t client;
    int result = uds_client_init(&client, TEST_SOCKET_PATH);
    assert_equal_int(result, 0, "init returns 0");
    assert_true(client.fd >= 0, "socket valid");

    uds_client_free(&client);
    uds_server_free(&server);
}

void test_uds_client_init_invalid_path(void)
{
    uds_client_t client;

    int result = uds_client_init(&client, "");
    assert_equal_int(result, -1, "empty path rejected");

    char long_path[200];
    memset(long_path, 'a', sizeof(long_path) - 1);
    long_path[sizeof(long_path) - 1] = '\0';
    result = uds_client_init(&client, long_path);
    assert_equal_int(result, -1, "too long path rejected");

    result = uds_client_init(&client, "/nonexistent/path.sock");
    assert_equal_int(result, -1, "nonexistent path rejected");
}

void test_uds_client_listen_timeout(void)
{
    uds_server_t server;
    uds_server_init(&server, TEST_SOCKET_PATH);

    uds_client_t client;
    uds_client_init(&client, TEST_SOCKET_PATH);

    char buf_data[1];
    buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 1, .size = 0};

    usleep(200000);

    int result = uds_client_listen(&client, &buf);
    assert_equal_int(result, 0, "timeout returns 0");

    uds_client_free(&client);
    uds_server_free(&server);
}

void test_uds_client_connect_and_read(void)
{
    uds_server_t server;
    uds_server_init(&server, TEST_SOCKET_PATH);

    pid_t pid = fork();
    if (pid == 0)
    {
        uds_client_t client;
        int client_init = uds_client_init(&client, TEST_SOCKET_PATH);
        assert_equal_int(client_init, 0, "client init successful");

        char buf_data[16];
        buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 16, .size = 0};
        int n = 0;
        int attempts = 0;
        while (n <= 0 && attempts++ < 100)
        {
            n = uds_client_listen(&client, &buf);
            if (n == 0)
                usleep(10000);
        }

        assert_equal_int(n, 4, "client read 4 bytes");
        assert_memory(buf_data, (void *)"test", 4, "data matches");

        uds_client_free(&client);
        exit(0);
    }

    usleep(200000);

    char dummy_buf_data[1];
    buffer_t dummy_buf = {.data = (unsigned char *)dummy_buf_data, .capacity = 1, .size = 0};
    int attempts = 0;
    while (server.num_clients == 0 && attempts++ < 50)
    {
        uds_server_listen(&server, &dummy_buf);
        usleep(10000);
    }

    assert_equal_int(server.num_clients, 1, "server accepted client");

    char msg_data[] = "test";
    buffer_t msg_buf = {.data = (unsigned char *)msg_data, .capacity = 4, .size = 4};
    uds_server_broadcast(&server, &msg_buf);

    waitpid(pid, NULL, 0);
    uds_server_free(&server);
}

void test_uds_client_server_disconnect(void)
{
    uds_server_t server;
    uds_server_init(&server, TEST_SOCKET_PATH);

    pid_t pid = fork();
    if (pid == 0)
    {
        uds_client_t client;
        int client_init = uds_client_init(&client, TEST_SOCKET_PATH);
        assert_equal_int(client_init, 0, "client init successful");

        char buf_data[16];
        buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 16, .size = 0};

        int attempts = 0;
        while (attempts++ < 100)
        {
            int error;
            socklen_t len = sizeof(error);
            if (getsockopt(client.fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0)
                break;
            usleep(10000);
        }

        int n = 0;
        attempts = 0;
        while (client.fd >= 0 && attempts++ < 50)
        {
            n = uds_client_listen(&client, &buf);
            usleep(10000);
        }

        assert_equal_int(n, -1, "client detected disconnect");
        assert_equal_int(client.fd, -1, "client fd reset on disconnect");

        uds_client_free(&client);
        exit(0);
    }

    usleep(200000);

    char dummy_buf_data[1];
    buffer_t dummy_buf = {.data = (unsigned char *)dummy_buf_data, .capacity = 1, .size = 0};
    int attempts = 0;
    while (server.num_clients == 0 && attempts++ < 50)
    {
        uds_server_listen(&server, &dummy_buf);
        usleep(10000);
    }

    assert_equal_int(server.num_clients, 1, "server accepted client");

    uds_server_free(&server);

    waitpid(pid, NULL, 0);
}

void test_uds_client_read_error(void)
{
    uds_server_t server;
    uds_server_init(&server, TEST_SOCKET_PATH);
    uds_client_t client;
    uds_client_init(&client, TEST_SOCKET_PATH);

    close(client.fd);
    client.fd = -1;

    char buf_data[1];
    buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 1, .size = 0};
    int result = uds_client_listen(&client, &buf);
    assert_equal_int(result, -1, "read with invalid socket returns error");
    uds_client_free(&client);
}

void test_uds_client_free(void)
{
    uds_server_t server;
    uds_server_init(&server, TEST_SOCKET_PATH);

    uds_client_t client;
    uds_client_init(&client, TEST_SOCKET_PATH);
    uds_client_free(&client);
    assert_equal_int(client.fd, -1, "socket reset");

    uds_server_free(&server);
}

void test_uds_client_partial_read(void)
{
    uds_server_t server;
    uds_server_init(&server, TEST_SOCKET_PATH);

    pid_t pid = fork();
    if (pid == 0)
    {
        usleep(200000);
        uds_client_t client;
        int client_init = uds_client_init(&client, TEST_SOCKET_PATH);
        assert_equal_int(client_init, 0, "client init successful");

        char buf_data[8];
        buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 8, .size = 0};
        int n1 = 0;
        int attempts = 0;
        while (n1 <= 0 && attempts++ < 50)
        {
            n1 = uds_client_listen(&client, &buf);
            usleep(10000);
        }
        assert_equal_int(n1, 8, "first read 8 bytes");
        assert_memory(buf_data, (void *)"hello wo", 8, "first chunk matches");

        int n2 = uds_client_listen(&client, &buf);
        assert_equal_int(n2, 8, "second read 8 bytes");
        assert_memory(buf_data, (void *)"rld! tes", 8, "second chunk matches");

        int n3 = uds_client_listen(&client, &buf);
        assert_equal_int(n3, 8, "third read 8 bytes");
        assert_memory(buf_data, (void *)"t data", 6, "third chunk matches");

        uds_client_free(&client);
        exit(0);
    }

    char dummy_buf_data[1];
    buffer_t dummy_buf = {.data = (unsigned char *)dummy_buf_data, .capacity = 1, .size = 0};
    int attempts = 0;
    while (server.num_clients == 0 && attempts++ < 50)
    {
        uds_server_listen(&server, &dummy_buf);
        usleep(10000);
    }
    assert_equal_int(server.num_clients, 1, "server accepted client");

    char msg_data[] = "hello world! test data";
    buffer_t msg_buf = {.data = (unsigned char *)msg_data, .capacity = 24, .size = 24};
    uds_server_broadcast(&server, &msg_buf);

    waitpid(pid, NULL, 0);
    uds_server_free(&server);
}

void test_uds_client_connection_in_progress(void)
{
    uds_server_t server;
    uds_server_init(&server, TEST_SOCKET_PATH);

    pid_t pid = fork();
    if (pid == 0)
    {
        uds_client_t client;
        int client_init = uds_client_init(&client, TEST_SOCKET_PATH);
        assert_equal_int(client_init, 0, "client init successful");

        char buf_data[16];
        buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 16, .size = 0};
        int n = uds_client_listen(&client, &buf);
        assert_equal_int(n, 0, "connection in progress, no data");

        usleep(200000);

        n = uds_client_listen(&client, &buf);
        assert_equal_int(n, 4, "read 4 bytes after connection");
        assert_memory(buf_data, (void *)"test", 4, "data matches");

        uds_client_free(&client);
        exit(0);
    }

    usleep(100000);

    char dummy_buf_data[1];
    buffer_t dummy_buf = {.data = (unsigned char *)dummy_buf_data, .capacity = 1, .size = 0};
    int attempts = 0;
    while (server.num_clients == 0 && attempts++ < 50)
    {
        uds_server_listen(&server, &dummy_buf);
        usleep(10000);
    }
    assert_equal_int(server.num_clients, 1, "server accepted client");

    char msg_data[] = "test";
    buffer_t msg_buf = {.data = (unsigned char *)msg_data, .capacity = 4, .size = 4};
    uds_server_broadcast(&server, &msg_buf);

    waitpid(pid, NULL, 0);
    uds_server_free(&server);
}

void test_uds_server_broadcast_two_clients(void)
{
    uds_server_t server;
    uds_server_init(&server, TEST_SOCKET_PATH);

    int pipefd1[2], pipefd2[2];
    pipe(pipefd1);
    pipe(pipefd2);

    pid_t pid1 = fork();
    if (pid1 == 0)
    {
        close(pipefd1[0]);
        close(pipefd2[0]);
        close(pipefd2[1]);
        usleep(200000);
        int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, TEST_SOCKET_PATH, sizeof(addr.sun_path) - 1);
        connect(client_fd, (struct sockaddr *)&addr, sizeof(addr));
        char rbuf[32];
        int n = read(client_fd, rbuf, sizeof(rbuf));
        write(pipefd1[1], rbuf, n);
        close(client_fd);
        close(pipefd1[1]);
        exit(0);
    }

    pid_t pid2 = fork();
    if (pid2 == 0)
    {
        close(pipefd2[0]);
        close(pipefd1[0]);
        close(pipefd1[1]);
        usleep(200000);
        int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, TEST_SOCKET_PATH, sizeof(addr.sun_path) - 1);
        connect(client_fd, (struct sockaddr *)&addr, sizeof(addr));
        char rbuf[32];
        int n = read(client_fd, rbuf, sizeof(rbuf));
        write(pipefd2[1], rbuf, n);
        close(client_fd);
        close(pipefd2[1]);
        exit(0);
    }

    close(pipefd1[1]);
    close(pipefd2[1]);

    char buf_data[1];
    buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 1, .size = 0};
    int attempts = 0;
    while (server.num_clients < 2 && attempts++ < 50)
        uds_server_listen(&server, &buf);
    assert_equal_int(server.num_clients, 2, "two clients connected");

    char msg_data[] = "broadcast_test";
    buffer_t msg_buf = {.data = (unsigned char *)msg_data, .capacity = 14, .size = 14};
    uds_server_broadcast(&server, &msg_buf);

    char received1[32], received2[32];
    int n1 = read(pipefd1[0], received1, sizeof(received1));
    int n2 = read(pipefd2[0], received2, sizeof(received2));
    assert_equal_int(n1, 14, "client1 received 14 bytes");
    assert_equal_int(n2, 14, "client2 received 14 bytes");
    assert_memory(received1, (void *)"broadcast_test", 14, "client1 broadcast message received");
    assert_memory(received2, (void *)"broadcast_test", 14, "client2 broadcast message received");

    close(pipefd1[0]);
    close(pipefd2[0]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
    uds_server_free(&server);
}

void test_uds_client_send(void)
{
    uds_server_t server;
    uds_server_init(&server, TEST_SOCKET_PATH);

    pid_t pid = fork();
    if (pid == 0)
    {
        uds_client_t client;
        int client_init = uds_client_init(&client, TEST_SOCKET_PATH);
        assert_equal_int(client_init, 0, "client init successful");

        // Wait for connection to establish
        int attempts = 0;
        while (attempts++ < 100)
        {
            int error;
            socklen_t errlen = sizeof(error);
            if (getsockopt(client.fd, SOL_SOCKET, SO_ERROR, &error, &errlen) == 0 && error == 0)
                break;
            usleep(10000);
        }

        // Send data to server
        char msg_data[] = "hello from client";
        buffer_t msg_buf = {.data = (unsigned char *)msg_data, .capacity = 18, .size = 18};
        int sent = uds_client_send(&client, &msg_buf);
        assert_equal_int(sent, 18, "sent 18 bytes");

        uds_client_free(&client);
        exit(0);
    }

    usleep(200000);

    char buf_data[32];
    buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 32, .size = 0};
    int n = 0;
    int attempts = 0;
    while (n <= 0 && attempts++ < 50)
    {
        n = uds_server_listen(&server, &buf);
        usleep(10000);
    }

    assert_equal_int(n, 18, "server received 18 bytes");
    assert_memory(buf_data, (void *)"hello from client", 18, "data matches");

    waitpid(pid, NULL, 0);
    uds_server_free(&server);
}

#endif
