# libtnc

TNC-related utilities library for AX.25 packet radio.
Contains utilities both intended for use **in** a TNC program as well as others **interfacing** with a TNC.

## Features

- **AX.25**: Packet and address structs and basic functions
- **HDLC**: Framing and deframing with NRZI, bit stuffing, checksums
- **KISS**: Binary protocol for TNC communication similar to SLIP
- **TNC2**: Human-readable packet representation (STATION>DEST,PATH:DATA)
- **CRC-CCITT**: 16-bit CRC calculation
- **TCP**: Client and server support with event callbacks
- **UDP**: Sender and server (connectionless packet I/O)
- **Unix Domain Sockets**: Local inter-process communication (stream and datagram)
- **Socket Poller**: epoll-based multi-FD polling for efficient I/O multiplexing
- **Line parsing**: Buffered line reader with callback

## Build

```sh
make build    # Debug build
make release  # Release build
make test     # Run unit tests
make install  # Install to system
make clean    # Clean build artifacts
make echo     # Runs demo echo server
```

## Usage

```c
ax25_packet_t packet;
ax25_packet_init(&packet);
ax25_addr_init_with(&packet.destination, "NOCALL", 0, false);
ax25_addr_init_with(&packet.source, "MYCALL", 1, false);
packet.control = 0x03;  // UI frame
packet.protocol = 0xf0;  // No layer 3
packet.info_len = 5;
memcpy(packet.info, "HELLO", 5);

buffer_t tnc2_buf;
buffer_init(&tnc2_buf, tnc2_out, sizeof(tnc2_out));
tnc2_packet_to_string(&packet, &tnc2_buf);

kiss_message_t kiss;
kiss_encode(&kiss, kiss_out, sizeof(kiss_out));

hldc_framer_t framer;
hldc_framer_init(&framer, 16, 16);  // 16 flag bytes pre/postamble
hldc_framer_process(&framer, &kiss_buf, &hdlc_out, NULL);

crc_ccitt_t crc;
crc_ccitt_init(&crc);
crc_ccitt_update_buffer(&crc, data, len);
uint16_t checksum = crc_ccitt_get(&crc);
```

### Receiving

```c
hldc_deframer_t deframer;
hldc_deframer_init(&deframer);

for (int i = 0; i < bit_count; i++) {
    hldc_deframer_process(&deframer, bits[i], &frame_buf, NULL);
}

kiss_decoder_t decoder;
kiss_decoder_init(&decoder);
kiss_decoder_process(&decoder, byte, &kiss_msg);

ax25_packet_t packet;
ax25_packet_unpack(&packet, &kiss_msg.data_buf);
```

### Networking and IPC

```c
tcp_server_t server;
tcp_server_init(&server, 8000, 50);
tcp_server_listen(&server, &buf);

tcp_client_t client;
tcp_client_init(&client, "192.168.1.1", 8000, 50);
tcp_client_send(&client, &buf);
tcp_client_listen(&client, &buf);

udp_sender_t sender;
udp_sender_init(&sender, "192.168.1.1", 9000);
udp_sender_send(&sender, &buf);

udp_server_t udp_srv;
udp_server_init(&udp_srv, 9000, 50);
udp_server_listen(&udp_srv, &buf);

uds_server_t uds_srv;
uds_server_init(&uds_srv, "/tmp/my.sock", 50);
uds_server_listen(&uds_srv, &buf);

uds_dgram_sender_t uds_snd;
uds_dgram_sender_init(&uds_snd, "/tmp/server.sock");
uds_dgram_sender_send(&uds_snd, &buf);

uds_dgram_server_t uds_dgram_srv;
uds_dgram_server_init(&uds_dgram_srv, "/tmp/dgram.sock", 50);
uds_dgram_server_listen(&uds_dgram_srv, &buf);

line_reader_t lr;
line_reader_init(&lr, my_line_callback);
line_reader_process(&lr, ch);
```

### Multi-Socket Polling

```c
socket_poller_t pol;
socket_poller_init(&pol);
socket_poller_add(&pol, tcp_server.listen_fd, POLLER_EV_IN);
socket_poller_add(&pol, udp_server.fd, POLLER_EV_IN);
socket_poller_add(&pol, client_fd, POLLER_EV_IN | POLLER_EV_OUT);

while (1) {
    int ready = socket_poller_wait(&pol, 100);
    if (ready > 0) {
        if (socket_poller_is_ready(&pol, tcp_server.listen_fd))
            tcp_server_listen(&server, &buf);
        if (socket_poller_is_ready(&pol, udp_server.fd))
            udp_server_listen(&udp_server, &buf);
        if (socket_poller_is_ready(&pol, client_fd))
            handle_client(&client);
    }
}
socket_poller_free(&pol);
```

## Demo program

See `src/libtnc_echo.c` for a complete multi-protocol echo server demonstrating TCP/UDP clients and servers as well as all variants of Unix socket IPC (connected and connectionless).

## Dependencies

- POSIX headers (pthread, socket, etc.)
- Standard C library
