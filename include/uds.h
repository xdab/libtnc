#ifndef UDS_H
#define UDS_H

#include <sys/select.h>
#include <stddef.h>
#include "buffer.h"

#define UDS_MAX_CLIENTS 16
#define UDS_READ_BUF_SIZE 2048
#define UDS_SOCKET_PATH_MAX 108
#define UDS_DEF_TIMEOUT_MS 50

typedef struct uds_client
{
    int fd;
    int timeout_ms;
} uds_client_t;

typedef void (*uds_client_event_cb_t)(int client_fd, void *user_data);

typedef struct uds_server
{
    int listen_fd;
    uds_client_t clients[UDS_MAX_CLIENTS];
    int num_clients;
    char socket_path[UDS_SOCKET_PATH_MAX];
    int timeout_ms;
    uds_client_event_cb_t on_client_connect;
    uds_client_event_cb_t on_client_disconnect;
    void *user_data;
} uds_server_t;

int uds_server_init(uds_server_t *server, const char *socket_path, int timeout_ms);

void uds_server_free(uds_server_t *server);

int uds_server_listen(uds_server_t *server, buffer_t *out_buf);

void uds_server_broadcast(uds_server_t *server, const buffer_t *buf);

//

int uds_client_init(uds_client_t *client, const char *socket_path, int timeout_ms);

void uds_client_free(uds_client_t *client);

int uds_client_listen(uds_client_t *client, buffer_t *out_buf);

int uds_client_send(uds_client_t *client, const buffer_t *buf);

#endif