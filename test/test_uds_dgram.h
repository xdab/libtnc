#ifndef TEST_UDS_DGRAM_H
#define TEST_UDS_DGRAM_H

#include "test.h"
#include "uds.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define TEST_DGRAM_SOCKET_PATH "/tmp/libtnc_test_dgram.sock"

void test_uds_dgram_sender_init_valid(void)
{
    uds_dgram_sender_t sender;
    int result = uds_dgram_sender_init(&sender, TEST_DGRAM_SOCKET_PATH);
    assert_equal_int(result, 0, "init returns 0");
    assert_true(sender.fd >= 0, "socket valid");
    assert_string(sender.dest_addr.sun_path, TEST_DGRAM_SOCKET_PATH, "dest path set");
    uds_dgram_sender_free(&sender);
}

void test_uds_dgram_sender_init_invalid_path(void)
{
    uds_dgram_sender_t sender;

    int result = uds_dgram_sender_init(&sender, "");
    assert_equal_int(result, -1, "empty path rejected");

    char long_path[200];
    memset(long_path, 'a', sizeof(long_path) - 1);
    long_path[sizeof(long_path) - 1] = '\0';
    result = uds_dgram_sender_init(&sender, long_path);
    assert_equal_int(result, -1, "too long path rejected");
}

void test_uds_dgram_sender_send(void)
{
    uds_dgram_server_t server;
    int result = uds_dgram_server_init(&server, TEST_DGRAM_SOCKET_PATH, UDS_DEF_TIMEOUT_MS);
    assert_equal_int(result, 0, "server init successful");

    pid_t pid = fork();
    if (pid == 0)
    {
        usleep(100000);
        uds_dgram_sender_t sender;
        int sender_init = uds_dgram_sender_init(&sender, TEST_DGRAM_SOCKET_PATH);
        assert_equal_int(sender_init, 0, "sender init successful");

        char msg_data[] = "hello dgram";
        buffer_t msg_buf = {.data = (unsigned char *)msg_data, .capacity = 11, .size = 11};
        int sent = uds_dgram_sender_send(&sender, &msg_buf);
        assert_equal_int(sent, 11, "sent 11 bytes");

        uds_dgram_sender_free(&sender);
        exit(0);
    }

    char buf_data[32];
    buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 32, .size = 0};
    int n = 0;
    int attempts = 0;
    while (n <= 0 && attempts++ < 50)
    {
        n = uds_dgram_server_listen(&server, &buf);
        usleep(10000);
    }

    assert_equal_int(n, 11, "server received 11 bytes");
    assert_memory(buf_data, (void *)"hello dgram", 11, "data matches");

    waitpid(pid, NULL, 0);
    uds_dgram_server_free(&server);
}

void test_uds_dgram_sender_send_empty(void)
{
    uds_dgram_sender_t sender;
    int result = uds_dgram_sender_init(&sender, TEST_DGRAM_SOCKET_PATH);
    assert_equal_int(result, 0, "init successful");

    buffer_t empty_buf = {.data = (unsigned char *)"x", .capacity = 1, .size = 0};
    int sent = uds_dgram_sender_send(&sender, &empty_buf);
    assert_equal_int(sent, 0, "empty buffer returns 0");

    uds_dgram_sender_free(&sender);
}

void test_uds_dgram_sender_free(void)
{
    uds_dgram_sender_t sender;
    uds_dgram_sender_init(&sender, TEST_DGRAM_SOCKET_PATH);
    uds_dgram_sender_free(&sender);
    assert_equal_int(sender.fd, -1, "socket reset");
}

void test_uds_dgram_server_init_valid(void)
{
    uds_dgram_server_t server;
    int result = uds_dgram_server_init(&server, TEST_DGRAM_SOCKET_PATH, UDS_DEF_TIMEOUT_MS);
    assert_equal_int(result, 0, "init returns 0");
    assert_true(server.fd >= 0, "socket valid");
    assert_string(server.socket_path, TEST_DGRAM_SOCKET_PATH, "path set");
    assert_equal_int(server.timeout_ms, UDS_DEF_TIMEOUT_MS, "timeout_ms set correctly");
    uds_dgram_server_free(&server);
}

void test_uds_dgram_server_init_timeout_ms_zero(void)
{
    uds_dgram_server_t server;
    int result = uds_dgram_server_init(&server, TEST_DGRAM_SOCKET_PATH, 0);
    assert_equal_int(result, 0, "init with timeout_ms=0 returns 0");
    assert_true(server.fd >= 0, "socket valid");
    assert_equal_int(server.timeout_ms, 0, "timeout_ms is 0");
    uds_dgram_server_free(&server);
}

void test_uds_dgram_server_init_invalid_path(void)
{
    uds_dgram_server_t server;

    int result = uds_dgram_server_init(&server, "", UDS_DEF_TIMEOUT_MS);
    assert_equal_int(result, -1, "empty path rejected");

    char long_path[200];
    memset(long_path, 'a', sizeof(long_path) - 1);
    long_path[sizeof(long_path) - 1] = '\0';
    result = uds_dgram_server_init(&server, long_path, UDS_DEF_TIMEOUT_MS);
    assert_equal_int(result, -1, "too long path rejected");
}

void test_uds_dgram_server_listen_timeout(void)
{
    uds_dgram_server_t server;
    uds_dgram_server_init(&server, TEST_DGRAM_SOCKET_PATH, UDS_DEF_TIMEOUT_MS);
    char buf_data[1];
    buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 1, .size = 0};
    int result = uds_dgram_server_listen(&server, &buf);
    assert_equal_int(result, 0, "timeout returns 0");
    uds_dgram_server_free(&server);
}

void test_uds_dgram_server_listen_no_block(void)
{
    uds_dgram_server_t server;
    uds_dgram_server_init(&server, TEST_DGRAM_SOCKET_PATH, 0); // Non-blocking mode
    assert_equal_int(server.timeout_ms, 0, "timeout_ms is 0");

    char buf_data[1];
    buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 1, .size = 0};

    // With timeout_ms=0, should return immediately without blocking
    int result = uds_dgram_server_listen(&server, &buf);
    assert_equal_int(result, 0, "no data returns 0");

    uds_dgram_server_free(&server);
}

void test_uds_dgram_server_free(void)
{
    uds_dgram_server_t server;
    uds_dgram_server_init(&server, TEST_DGRAM_SOCKET_PATH, UDS_DEF_TIMEOUT_MS);
    uds_dgram_server_free(&server);
    assert_equal_int(server.fd, -1, "socket reset");
    assert_string(server.socket_path, "", "path cleared");
}

void test_uds_dgram_server_multiple_messages(void)
{
    uds_dgram_server_t server;
    int result = uds_dgram_server_init(&server, TEST_DGRAM_SOCKET_PATH, UDS_DEF_TIMEOUT_MS);
    assert_equal_int(result, 0, "server init successful");

    pid_t pid = fork();
    if (pid == 0)
    {
        usleep(100000);
        uds_dgram_sender_t sender;
        int sender_init = uds_dgram_sender_init(&sender, TEST_DGRAM_SOCKET_PATH);
        assert_equal_int(sender_init, 0, "sender init successful");

        // Send multiple messages
        char msg1[] = "msg1";
        buffer_t buf1 = {.data = (unsigned char *)msg1, .capacity = 4, .size = 4};
        int sent1 = uds_dgram_sender_send(&sender, &buf1);
        assert_equal_int(sent1, 4, "sent first message");

        char msg2[] = "msg2";
        buffer_t buf2 = {.data = (unsigned char *)msg2, .capacity = 4, .size = 4};
        int sent2 = uds_dgram_sender_send(&sender, &buf2);
        assert_equal_int(sent2, 4, "sent second message");

        char msg3[] = "msg3";
        buffer_t buf3 = {.data = (unsigned char *)msg3, .capacity = 4, .size = 4};
        int sent3 = uds_dgram_sender_send(&sender, &buf3);
        assert_equal_int(sent3, 4, "sent third message");

        uds_dgram_sender_free(&sender);
        exit(0);
    }

    char buf_data[16];
    buffer_t buf = {.data = (unsigned char *)buf_data, .capacity = 16, .size = 0};

    // Receive first message
    int n1 = 0;
    int attempts = 0;
    while (n1 <= 0 && attempts++ < 50)
    {
        n1 = uds_dgram_server_listen(&server, &buf);
        usleep(10000);
    }
    assert_equal_int(n1, 4, "received first message");
    assert_memory(buf_data, (void *)"msg1", 4, "first message matches");

    // Receive second message
    int n2 = 0;
    attempts = 0;
    while (n2 <= 0 && attempts++ < 50)
    {
        n2 = uds_dgram_server_listen(&server, &buf);
        usleep(10000);
    }
    assert_equal_int(n2, 4, "received second message");
    assert_memory(buf_data, (void *)"msg2", 4, "second message matches");

    // Receive third message
    int n3 = 0;
    attempts = 0;
    while (n3 <= 0 && attempts++ < 50)
    {
        n3 = uds_dgram_server_listen(&server, &buf);
        usleep(10000);
    }
    assert_equal_int(n3, 4, "received third message");
    assert_memory(buf_data, (void *)"msg3", 4, "third message matches");

    waitpid(pid, NULL, 0);
    uds_dgram_server_free(&server);
}

#endif
