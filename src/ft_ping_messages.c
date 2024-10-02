#include "ft_ping.h"

static const char *START_MESSAGE_FORMAT
    = "PING %s (%s) %lu(%lu) bytes of data.\n";
static const char *PING_MESSAGE_FORMAT
    = "64 bytes from %s (%s): icmp_seq=%d ttl=%hhu time=%.2fms\n";

static const char *END_MESSAGE_HEADER_FORMAT = "--- %s ping statistics ---\n";
static const char *END_MESSAGE_STATS_FORMAT
    = "%d packets transmitted, %d received, %.0f%% packet loss, time %ldms\n";
static const char *END_MESSAGE_RTT_FORMAT
    = "rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n";

void
ping_messages_handler (MessageType type)
{
    if (type == START)
    {
        printf (START_MESSAGE_FORMAT, g_ping.sock_info.hostname,
                g_ping.sock_info.ip_addr, PAYLOAD_SIZE, IP_PACKET_SIZE);
    }
    else if (type == PING)
    {
        printf (PING_MESSAGE_FORMAT, g_ping.sock_info.hostname,
                g_ping.sock_info.ip_addr, g_ping.sequence, g_ping.options.ttl,
                g_ping.rtt_metrics.std_rtt);
    }
    else if (type == END)
    {
        printf (END_MESSAGE_HEADER_FORMAT, g_ping.sock_info.hostname);
        printf ("N/A");
        printf ("N/A");
    }
}