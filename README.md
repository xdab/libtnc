# libtnc

TNC-related utilities library for AX.25 packet radio.
Contains utilities both intended for use **in** a TNC program as well as others **interfacing** with a TNC.

## Features

- **AX.25**: Packet and address structs and basic functions
- **HDLC**: Framing and deframing with NRZI, bit stuffing, checksums
- **KISS**: Binary protocol for TNC communication similar to SLIP
- **TNC2**: Human-readable packet representation (STATION>DEST,PATH:DATA)
- **CRC-CCITT**: 16-bit CRC calculation
- **TCP/UDP**: Network client/server support
- **Unix Domain Sockets**: Local inter-process communication
- **Socket Selector**: Multi-FD select() helper for managing multiple sockets
- **Line parsing**: Buffered line reader with callback

## Build

```sh
make build    # Debug build
make release  # Release build
make test     # Run unit tests
make install  # Install to system
make clean    # Clean build artifacts
```

## Usage

```c
#include "ax25.h"
#include "tnc2.h"
#include "kiss.h"
#include "hldc.h"
#include "crc.h"

// Build AX.25 packet
ax25_packet_t packet;
ax25_packet_init(&packet);
ax25_addr_init_with(&packet.destination, "NOCALL", 0, false);
ax25_addr_init_with(&packet.source, "MYCALL", 1, false);
packet.control = 0x03;  // UI frame
packet.protocol = 0xf0;  // No layer 3
packet.info_len = 5;
memcpy(packet.info, "HELLO", 5);

// Convert to TNC2 text format
buffer_t tnc2_buf;
buffer_init(&tnc2_buf, tnc2_out, sizeof(tnc2_out));
tnc2_packet_to_string(&packet, &tnc2_buf);

// Encode to KISS
kiss_message_t kiss;
kiss_encode(&kiss, kiss_out, sizeof(kiss_out));

// HDLC framing for radio transmission
hldc_framer_t framer;
hldc_framer_init(&framer, 16, 16);  // 16 flag bytes pre/postamble
hldc_framer_process(&framer, &kiss_buf, &hdlc_out, NULL);

// CRC-CCITT checksum
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

### Networking

```c
tcp_server_t server;
tcp_server_init(&server, 8000);

udp_sender_t sender;
udp_sender_init(&sender, "192.168.1.1", 9000);
udp_sender_send(&sender, data, len);

line_reader_t lr;
line_reader_init(&lr, my_line_callback);
line_reader_process(&lr, ch);
```

### Multi-Socket Selection

```c
socket_selector_t *sel = socket_selector_create();
socket_selector_add(sel, tcp_server.listen_fd, SELECT_READ);
socket_selector_add(sel, udp_server.fd, SELECT_READ);
socket_selector_add(sel, uds_client.fd, SELECT_READ | SELECT_WRITE);

while (1) {
    int ready = socket_selector_wait(sel, 100);
    if (ready > 0) {
        if (socket_selector_is_ready(sel, tcp_server.listen_fd))
            tcp_server_listen(&server, &buf);
        if (socket_selector_is_ready(sel, udp_server.fd))
            udp_server_listen(&udp_server, &buf);
        if (socket_selector_is_ready(sel, uds_client.fd))
            uds_client_listen(&uds_client, &buf);
    }
}
socket_selector_free(sel);
```

## Dependencies

- POSIX headers (pthread, socket, etc.)
- Standard C library
