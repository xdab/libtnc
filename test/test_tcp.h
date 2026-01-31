#ifndef TEST_TCP_H
#define TEST_TCP_H

#include "test.h"
#include "tcp.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void test_tcp_server_init_valid(void)
{
    tcp_server_t server;
    int result = tcp_server_init(&server, 0);
    assert_equal_int(result, 0, "init returns 0");
    assert_true(server.listen_fd > 2, "listen_fd valid");
    assert_equal_int(server.num_clients, 0, "num_clients 0");
    tcp_server_free(&server);
}

void test_tcp_server_listen_timeout(void)
{
    tcp_server_t server;
    tcp_server_init(&server, 0);
    char buf_data[1];
    buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 1, .size = 0};
    int result = tcp_server_listen(&server, &buf);
    assert_equal_int(result, 0, "timeout returns 0");
    tcp_server_free(&server);
}

void test_tcp_server_accept_client(void)
{
    tcp_server_t server;
    tcp_server_init(&server, 0);

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getsockname(server.listen_fd, (struct sockaddr *)&addr, &len);
    uint16_t port = ntohs(addr.sin_port);

    pid_t pid = fork();
    if (pid == 0)
    {
        usleep(200000);
        int client_fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in caddr;
        memset(&caddr, 0, sizeof(caddr));
        caddr.sin_family = AF_INET;
        caddr.sin_port = htons(port);
        caddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        connect(client_fd, (struct sockaddr *)&caddr, sizeof(caddr));
        close(client_fd);
        exit(0);
    }

    char buf_data[1];
    buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 1, .size = 0};
    int attempts = 0;
    while (server.num_clients == 0 && attempts++ < 20)
        tcp_server_listen(&server, &buf);

    assert_equal_int(server.num_clients, 1, "client accepted");
    assert_true(server.clients[0].fd >= 3, "client fd valid");

    waitpid(pid, NULL, 0);
    tcp_server_free(&server);
}

void test_tcp_server_read_data(void)
{
    tcp_server_t server;
    tcp_server_init(&server, 0);

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getsockname(server.listen_fd, (struct sockaddr *)&addr, &len);
    uint16_t port = ntohs(addr.sin_port);

    pid_t pid = fork();
    if (pid == 0)
    {
        usleep(200000);
        int client_fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in caddr;
        memset(&caddr, 0, sizeof(caddr));
        caddr.sin_family = AF_INET;
        caddr.sin_port = htons(port);
        caddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        connect(client_fd, (struct sockaddr *)&caddr, sizeof(caddr));
        write(client_fd, "test", 4);
        close(client_fd);
        exit(0);
    }

    char buf_data[16];
    buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 16, .size = 0};
    int n = 0;
    int attempts = 0;
    while (n <= 0 && attempts++ < 20)
        n = tcp_server_listen(&server, &buf);

    assert_equal_int(n, 4, "read 4 bytes");
    assert_memory(buf_data, (void *)"test", 4, "data matches");

    waitpid(pid, NULL, 0);
    tcp_server_free(&server);
}

void test_tcp_server_client_disconnect(void)
{
    tcp_server_t server;
    tcp_server_init(&server, 0);

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getsockname(server.listen_fd, (struct sockaddr *)&addr, &len);
    uint16_t port = ntohs(addr.sin_port);

    pid_t pid = fork();
    if (pid == 0)
    {
        usleep(200000);
        int client_fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in caddr;
        memset(&caddr, 0, sizeof(caddr));
        caddr.sin_family = AF_INET;
        caddr.sin_port = htons(port);
        caddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        connect(client_fd, (struct sockaddr *)&caddr, sizeof(caddr));
        close(client_fd);
        exit(0);
    }

    char buf_data[1];
    buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 1, .size = 0};
    int attempts = 0;
    while (server.num_clients == 0 && attempts++ < 20)
        tcp_server_listen(&server, &buf);
    assert_equal_int(server.num_clients, 1, "client connected");

    attempts = 0;
    while (server.num_clients == 1 && attempts++ < 20)
        tcp_server_listen(&server, &buf);
    assert_equal_int(server.num_clients, 0, "client disconnected");

    waitpid(pid, NULL, 0);
    tcp_server_free(&server);
}

void test_tcp_server_broadcast(void)
{
    tcp_server_t server;
    tcp_server_init(&server, 0);

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getsockname(server.listen_fd, (struct sockaddr *)&addr, &len);
    uint16_t port = ntohs(addr.sin_port);

    int pipefd[2];
    pipe(pipefd);

    pid_t pid = fork();
    if (pid == 0)
    {
        close(pipefd[0]); // Close read end
        usleep(200000);
        int client_fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in caddr;
        memset(&caddr, 0, sizeof(caddr));
        caddr.sin_family = AF_INET;
        caddr.sin_port = htons(port);
        caddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        connect(client_fd, (struct sockaddr *)&caddr, sizeof(caddr));
        char rbuf[16];
        int n = read(client_fd, rbuf, sizeof(rbuf));
        write(pipefd[1], rbuf, n);
        close(client_fd);
        close(pipefd[1]);
        exit(0);
    }

    close(pipefd[1]); // Close write end

    char buf_data[1];
    buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 1, .size = 0};
    int attempts = 0;
    while (server.num_clients == 0 && attempts++ < 20)
        tcp_server_listen(&server, &buf);
    assert_equal_int(server.num_clients, 1, "client connected");

    char msg_data[] = "hello";
    buffer_t msg_buf = {.data = (unsigned char *)msg_data, .capacity = 5, .size = 5};
    tcp_server_broadcast(&server, &msg_buf);

    char received[16];
    int n = read(pipefd[0], received, sizeof(received));
    assert_equal_int(n, 5, "received 5 bytes");
    assert_memory(received, (void *)"hello", 5, "broadcast message received");

    close(pipefd[0]);
    waitpid(pid, NULL, 0);
    tcp_server_free(&server);
}

void test_tcp_server_free(void)
{
    tcp_server_t server;
    tcp_server_init(&server, 0);
    tcp_server_free(&server);
    assert_equal_int(server.listen_fd, -1, "listen_fd reset");
    assert_equal_int(server.num_clients, 0, "num_clients reset");
}

void test_tcp_client_init_valid(void)
{
    tcp_client_t client;
    int result = tcp_client_init(&client, "127.0.0.1", 12345);
    assert_equal_int(result, 0, "init returns 0");
    assert_true(client.fd >= 0, "socket valid");
    tcp_client_free(&client);
}

void test_tcp_client_init_invalid_address(void)
{
    tcp_client_t client;

    // Empty address
    int result = tcp_client_init(&client, "", 12345);
    assert_equal_int(result, -1, "empty address rejected");

    // Too long address
    char long_addr[INET_ADDRSTRLEN + 10];
    memset(long_addr, '1', sizeof(long_addr) - 1);
    long_addr[sizeof(long_addr) - 1] = '\0';
    result = tcp_client_init(&client, long_addr, 12345);
    assert_equal_int(result, -1, "too long address rejected");

    // Malformed addresses (inet_pton will reject these)
    result = tcp_client_init(&client, "192.168.1", 12345);
    assert_equal_int(result, -1, "malformed address rejected");

    result = tcp_client_init(&client, "192.168.abc.1", 12345);
    assert_equal_int(result, -1, "non-numeric address rejected");

    result = tcp_client_init(&client, "999.999.999.999", 12345);
    assert_equal_int(result, -1, "invalid octet values rejected");
}

void test_tcp_client_listen_timeout(void)
{
    tcp_client_t client;
    tcp_client_init(&client, "127.0.0.1", 12345);
    char buf_data[1];
    buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 1, .size = 0};

    // Give connection time to fail (connecting to non-existent server)
    usleep(200000);

    int result = tcp_client_listen(&client, &buf);
    assert_equal_int(result, -1, "connection failure returns -1");
    tcp_client_free(&client);
}

void test_tcp_client_connect_and_read(void)
{
    tcp_server_t server;
    tcp_server_init(&server, 0);

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getsockname(server.listen_fd, (struct sockaddr *)&addr, &len);
    uint16_t port = ntohs(addr.sin_port);

    pid_t pid = fork();
    if (pid == 0)
    {
        // Child process: create client and connect
        tcp_client_t client;
        int client_init = tcp_client_init(&client, "127.0.0.1", port);
        assert_equal_int(client_init, 0, "client init successful");

        char buf_data[16];
        buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 16, .size = 0};
        int n = 0;
        int attempts = 0;
        // Wait for connection and data
        while (n <= 0 && attempts++ < 100)
        {
            n = tcp_client_listen(&client, &buf);
            if (n == 0) // Connection in progress or no data
                usleep(10000);
        }

        assert_equal_int(n, 4, "client read 4 bytes");
        assert_memory(buf_data, (void *)"test", 4, "data matches");

        tcp_client_free(&client);
        exit(0);
    }

    // Parent process: wait for client to connect, then send data
    usleep(200000); // Give child time to start connecting

    char dummy_buf_data[1];
    buffer_t dummy_buf = {.data = (unsigned char *)dummy_buf_data, .capacity = 1, .size = 0};
    int attempts = 0;
    while (server.num_clients == 0 && attempts++ < 50)
    {
        tcp_server_listen(&server, &dummy_buf);
        usleep(10000);
    }

    assert_equal_int(server.num_clients, 1, "server accepted client");

    // Send data to connected client
    char msg_data[] = "test";
    buffer_t msg_buf = {.data = (unsigned char *)msg_data, .capacity = 4, .size = 4};
    tcp_server_broadcast(&server, &msg_buf);

    waitpid(pid, NULL, 0);
    tcp_server_free(&server);
}

void test_tcp_client_server_disconnect(void)
{
    tcp_server_t server;
    tcp_server_init(&server, 0);

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getsockname(server.listen_fd, (struct sockaddr *)&addr, &len);
    uint16_t port = ntohs(addr.sin_port);

    pid_t pid = fork();
    if (pid == 0)
    {
        // Child process: create client and connect
        tcp_client_t client;
        int client_init = tcp_client_init(&client, "127.0.0.1", port);
        assert_equal_int(client_init, 0, "client init successful");

        char buf_data[16];
        buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 16, .size = 0};

        // Wait for connection to be fully established (SO_ERROR != EINPROGRESS)
        int attempts = 0;
        while (attempts++ < 100)
        {
            int error;
            socklen_t len = sizeof(error);
            if (getsockopt(client.fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0)
                break; // Connection established
            usleep(10000);
        }

        // Now try to read - should eventually detect disconnect
        int n = 0;
        attempts = 0;
        while (client.fd >= 0 && attempts++ < 50)
        {
            n = tcp_client_listen(&client, &buf);
            usleep(10000);
        }

        assert_equal_int(n, -1, "client detected disconnect");
        assert_equal_int(client.fd, -1, "client fd reset on disconnect");

        tcp_client_free(&client);
        exit(0);
    }

    // Parent process: accept client, then disconnect server
    usleep(200000); // Give child time to start connecting

    char dummy_buf_data[1];
    buffer_t dummy_buf = {.data = (unsigned char *)dummy_buf_data, .capacity = 1, .size = 0};
    int attempts = 0;
    while (server.num_clients == 0 && attempts++ < 50)
    {
        tcp_server_listen(&server, &dummy_buf);
        usleep(10000);
    }

    assert_equal_int(server.num_clients, 1, "server accepted client");

    // Disconnect the server (this should cause client to detect disconnect)
    tcp_server_free(&server);

    waitpid(pid, NULL, 0);
    tcp_server_free(&server); // Already freed, but safe
}

void test_tcp_client_read_error(void)
{
    tcp_client_t client;
    tcp_client_init(&client, "127.0.0.1", 12345);

    // Close socket to force read error
    close(client.fd);
    client.fd = -1;

    char buf_data[1];
    buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 1, .size = 0};
    int result = tcp_client_listen(&client, &buf);
    assert_equal_int(result, -1, "read with invalid socket returns error");
    tcp_client_free(&client);
}

void test_tcp_client_free(void)
{
    tcp_client_t client;
    tcp_client_init(&client, "127.0.0.1", 12345);
    tcp_client_free(&client);
    assert_equal_int(client.fd, -1, "socket reset");
}

void test_tcp_client_partial_read(void)
{
    tcp_server_t server;
    tcp_server_init(&server, 0);

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getsockname(server.listen_fd, (struct sockaddr *)&addr, &len);
    uint16_t port = ntohs(addr.sin_port);

    pid_t pid = fork();
    if (pid == 0)
    {
        usleep(200000);
        tcp_client_t client;
        int client_init = tcp_client_init(&client, "127.0.0.1", port);
        assert_equal_int(client_init, 0, "client init successful");

        char buf_data[8];
        buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 8, .size = 0};
        int n1 = 0;
        int attempts = 0;
        while (n1 <= 0 && attempts++ < 50)
        {
            n1 = tcp_client_listen(&client, &buf);
            usleep(10000);
        }
        assert_equal_int(n1, 8, "first read 8 bytes");
        assert_memory(buf_data, (void *)"hello wo", 8, "first chunk matches");

        int n2 = tcp_client_listen(&client, &buf);
        assert_equal_int(n2, 8, "second read 8 bytes");
        assert_memory(buf_data, (void *)"rld! tes", 8, "second chunk matches");

        int n3 = tcp_client_listen(&client, &buf);
        assert_equal_int(n3, 8, "third read 8 bytes");
        assert_memory(buf_data, (void *)"t data", 6, "third chunk matches");

        tcp_client_free(&client);
        exit(0);
    }

    char dummy_buf_data[1];
    buffer_t dummy_buf = {.data = (unsigned char *)dummy_buf_data, .capacity = 1, .size = 0};
    int attempts = 0;
    while (server.num_clients == 0 && attempts++ < 50)
    {
        tcp_server_listen(&server, &dummy_buf);
        usleep(10000);
    }
    assert_equal_int(server.num_clients, 1, "server accepted client");

    char msg_data[] = "hello world! test data";
    buffer_t msg_buf = {.data = (unsigned char *)msg_data, .capacity = 24, .size = 24};
    tcp_server_broadcast(&server, &msg_buf);

    waitpid(pid, NULL, 0);
    tcp_server_free(&server);
}

void test_tcp_client_connection_in_progress(void)
{
    tcp_server_t server;
    tcp_server_init(&server, 0);

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getsockname(server.listen_fd, (struct sockaddr *)&addr, &len);
    uint16_t port = ntohs(addr.sin_port);

    pid_t pid = fork();
    if (pid == 0)
    {
        tcp_client_t client;
        int client_init = tcp_client_init(&client, "127.0.0.1", port);
        assert_equal_int(client_init, 0, "client init successful");

        char buf_data[16];
        buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 16, .size = 0};
        int n = tcp_client_listen(&client, &buf);
        assert_equal_int(n, 0, "connection in progress, no data");

        usleep(200000); // Allow connection to establish

        n = tcp_client_listen(&client, &buf);
        assert_equal_int(n, 4, "read 4 bytes after connection");
        assert_memory(buf_data, (void *)"test", 4, "data matches");

        tcp_client_free(&client);
        exit(0);
    }

    usleep(100000); // Let child start connecting

    char dummy_buf_data[1];
    buffer_t dummy_buf = {.data = (unsigned char *)dummy_buf_data, .capacity = 1, .size = 0};
    int attempts = 0;
    while (server.num_clients == 0 && attempts++ < 50)
    {
        tcp_server_listen(&server, &dummy_buf);
        usleep(10000);
    }
    assert_equal_int(server.num_clients, 1, "server accepted client");

    char msg_data[] = "test";
    buffer_t msg_buf = {.data = (unsigned char *)msg_data, .capacity = 4, .size = 4};
    tcp_server_broadcast(&server, &msg_buf);

    waitpid(pid, NULL, 0);
    tcp_server_free(&server);
}

void test_tcp_client_write_error(void)
{
    tcp_server_t server;
    tcp_server_init(&server, 0);

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getsockname(server.listen_fd, (struct sockaddr *)&addr, &len);
    uint16_t port = ntohs(addr.sin_port);

    pid_t pid = fork();
    if (pid == 0)
    {
        tcp_client_t client;
        int client_init = tcp_client_init(&client, "127.0.0.1", port);
        assert_equal_int(client_init, 0, "client init successful");

        char buf_data[16];
        buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 16, .size = 0};
        int n = 0;
        int attempts = 0;
        while (client.fd >= 0 && attempts++ < 50)
        {
            n = tcp_client_listen(&client, &buf);
            if (n == -1)
                break;
            usleep(10000);
        }

        assert_equal_int(n, -1, "client detected server disconnect");
        assert_equal_int(client.fd, -1, "client fd reset on disconnect");

        tcp_client_free(&client);
        exit(0);
    }

    usleep(200000); // Give child time to start connecting

    char dummy_buf_data[1];
    buffer_t dummy_buf = {.data = (unsigned char *)dummy_buf_data, .capacity = 1, .size = 0};
    int attempts = 0;
    while (server.num_clients == 0 && attempts++ < 50)
    {
        tcp_server_listen(&server, &dummy_buf);
        usleep(10000);
    }
    assert_equal_int(server.num_clients, 1, "server accepted client");

    tcp_server_free(&server); // Disconnect server immediately

    waitpid(pid, NULL, 0);
}

void test_tcp_server_broadcast_two_clients(void)
{
    tcp_server_t server;
    tcp_server_init(&server, 0);

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getsockname(server.listen_fd, (struct sockaddr *)&addr, &len);
    uint16_t port = ntohs(addr.sin_port);

    int pipefd1[2], pipefd2[2];
    pipe(pipefd1);
    pipe(pipefd2);

    pid_t pid1 = fork();
    if (pid1 == 0)
    {
        close(pipefd1[0]); // Close read end
        close(pipefd2[0]);
        close(pipefd2[1]);
        usleep(200000);
        int client_fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in caddr;
        memset(&caddr, 0, sizeof(caddr));
        caddr.sin_family = AF_INET;
        caddr.sin_port = htons(port);
        caddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        connect(client_fd, (struct sockaddr *)&caddr, sizeof(caddr));
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
        close(pipefd2[0]); // Close read end
        close(pipefd1[0]);
        close(pipefd1[1]);
        usleep(200000);
        int client_fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in caddr;
        memset(&caddr, 0, sizeof(caddr));
        caddr.sin_family = AF_INET;
        caddr.sin_port = htons(port);
        caddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        connect(client_fd, (struct sockaddr *)&caddr, sizeof(caddr));
        char rbuf[32];
        int n = read(client_fd, rbuf, sizeof(rbuf));
        write(pipefd2[1], rbuf, n);
        close(client_fd);
        close(pipefd2[1]);
        exit(0);
    }

    close(pipefd1[1]); // Close write ends
    close(pipefd2[1]);

    char buf_data[1];
    buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 1, .size = 0};
    int attempts = 0;
    while (server.num_clients < 2 && attempts++ < 50)
        tcp_server_listen(&server, &buf);
    assert_equal_int(server.num_clients, 2, "two clients connected");

    char msg_data[] = "broadcast_test";
    buffer_t msg_buf = {.data = (unsigned char *)msg_data, .capacity = 14, .size = 14};
    tcp_server_broadcast(&server, &msg_buf);

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
    tcp_server_free(&server);
}

void test_tcp_client_send(void)
{
    tcp_server_t server;
    tcp_server_init(&server, 0);

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getsockname(server.listen_fd, (struct sockaddr *)&addr, &len);
    uint16_t port = ntohs(addr.sin_port);

    pid_t pid = fork();
    if (pid == 0)
    {
        tcp_client_t client;
        int client_init = tcp_client_init(&client, "127.0.0.1", port);
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
        int sent = tcp_client_send(&client, &msg_buf);
        assert_equal_int(sent, 18, "sent 18 bytes");

        tcp_client_free(&client);
        exit(0);
    }

    usleep(200000);

    char buf_data[32];
    buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 32, .size = 0};
    int n = 0;
    int attempts = 0;
    while (n <= 0 && attempts++ < 50)
    {
        n = tcp_server_listen(&server, &buf);
        usleep(10000);
    }

    assert_equal_int(n, 18, "server received 18 bytes");
    assert_memory(buf_data, (void *)"hello from client", 18, "data matches");

    waitpid(pid, NULL, 0);
    tcp_server_free(&server);
}

#endif
