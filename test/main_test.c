#include "test.h"
#include "test_ax25.h"
#include "test_tnc2.h"
#include "test_hldc.h"
#include "test_kiss.h"
#include "test_line.h"
#include "test_tcp.h"
#include "test_udp.h"
#include "test_uds.h"
#include "test_poller.h"

int main(void)
{
    begin_suite();

    begin_module("Address");
    test_addr_init();
    test_addr_init_with();
    test_addr_pack();
    test_addr_unpack();
    end_module();

    begin_module("Packet");
    test_packet_init();
    test_packet_pack();
    test_packet_pack_unpack();
    end_module();

    begin_module("TNC2");
    test_tnc2_string_to_packet_with_repeated();
    test_tnc2_roundtrip_simple();
    test_tnc2_roundtrip_complex();
    test_tnc2_invalid_chars_in_callsign();
    test_tnc2_control_char();
    test_tnc2_ssid_overflow();
    test_tnc2_missing_greater_than();
    test_tnc2_missing_colon();
    test_tnc2_empty_callsign();
    test_tnc2_too_many_digis();
    test_tnc2_callsign_too_long();
    test_tnc2_space_in_callsign();
    test_tnc2_info_too_large();
    test_tnc2_null_termination();
    test_tnc2_packet_roundtrip_with_null_term();
    test_tnc2_edge_case_mixed_valid_invalid_chars();
    test_tnc2_edge_case_boundary_digits();
    test_tnc2_edge_case_callsign_padding();
    end_module();

    begin_module("HLDC");
    test_hldc_framer_init();
    test_hldc_framer_flag_scaling();
    test_hldc_framer_bit_stuffing();
    test_hldc_deframer_init();
    end_module();

    begin_module("KISS");
    test_kiss_decoder_init();
    test_kiss_decoder_empty_frames();
    test_kiss_decoder_data_frame();
    test_kiss_encode_basic();
    test_kiss_encode_with_escaping();
    test_kiss_read_frame_escaped_characters();
    test_kiss_read_invalid_escape_sequence();
    test_kiss_read_incomplete_frame();
    test_kiss_read_consecutive_empty_frames();
    test_kiss_read_multiple_consecutive_escape();
    test_kiss_read_back_to_back_frames();
    end_module();

    begin_module("Line Reader");
    test_lr_simple_line();
    test_lr_crlf_handling();
    test_lr_empty_lines_ignored();
    test_lr_embedded_cr();
    test_lr_multiple_lines();
    test_lr_binary_data();
    test_lr_line_too_long();
    end_module();

    begin_module("TCP");
    test_tcp_server_init_valid();
    test_tcp_server_listen_timeout();
    test_tcp_server_accept_client();
    test_tcp_server_read_data();
    test_tcp_server_client_disconnect();
    test_tcp_server_broadcast();
    test_tcp_server_free();
    test_tcp_client_init_valid();
    test_tcp_client_init_invalid_address();
    test_tcp_client_listen_timeout();
    test_tcp_client_connect_and_read();
    test_tcp_client_server_disconnect();
    test_tcp_client_read_error();
    test_tcp_client_partial_read();
    test_tcp_client_connection_in_progress();
    test_tcp_client_write_error();
    test_tcp_client_free();
    test_tcp_client_send();
    end_module();

    begin_module("UDP");
    test_udp_sender_init_valid();
    test_udp_sender_init_broadcast();
    test_udp_sender_send_unicast();
    test_udp_sender_free();
    test_udp_sender_init_invalid_address();
    test_udp_sender_send_error();
    test_udp_sender_init_broadcast_config();
    test_udp_server_init_valid();
    test_udp_server_listen_timeout();
    test_udp_server_receive_data();
    test_udp_server_init_invalid_address();
    test_udp_server_free();
    end_module();

    begin_module("UDS");
    test_uds_server_init_valid();
    test_uds_server_listen_timeout();
    test_uds_server_accept_client();
    test_uds_server_read_data();
    test_uds_server_client_disconnect();
    test_uds_server_broadcast();
    test_uds_server_free();
    test_uds_server_init_invalid_path();
    test_uds_client_init_valid();
    test_uds_client_init_invalid_path();
    test_uds_client_listen_timeout();
    test_uds_client_connect_and_read();
    test_uds_client_server_disconnect();
    test_uds_client_read_error();
    test_uds_client_free();
    test_uds_client_partial_read();
    test_uds_client_connection_in_progress();
    test_uds_server_broadcast_two_clients();
    test_uds_client_send();
    end_module();

    begin_module("Poller");
    test_poller_create();
    test_poller_add_single();
    test_poller_add_multiple();
    test_poller_remove();
    test_poller_remove_nonexistent();
    test_poller_wait_timeout();
    test_poller_wait_ready();
    test_poller_is_ready();
    test_poller_is_ready_not_ready();
    test_poller_free();
    test_poller_mixed_events();
    end_module();

    int failed = end_suite();

    return failed ? 1 : 0;
}