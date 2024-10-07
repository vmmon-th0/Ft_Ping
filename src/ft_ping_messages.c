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
        if (g_ping.options.verbose == true)
        {
            printf ("ping: sock4.fd: %d (socktype: SOCK_RAW), sock6.fd: %d "
                    "(socktype: SOCK_RAW), hints.ai_family: AF_UNSPEC\n",
                    g_ping.sock_info.sock_fd, 0);
            printf ("ai.ai_family: %s, ai.ai_canonname: '%s'",
                    g_ping.sock_info.ai.ai_family == AF_INET ? "AF_INET"
                                                             : "AF_INET6",
                    g_ping.sock_info.ai.ai_canonname);

            printf ("Selected options: Verbose: %s, IPv4: %s, IPv6: %s, Count: "
                    "%u, Deadline: %u, TTL: %u\n\n",
                    g_ping.options.verbose ? "Yes" : "No",
                    g_ping.options.ipv4 ? "Yes" : "No",
                    g_ping.options.ipv6 ? "Yes" : "No", g_ping.options.count,
                    g_ping.options.deadline, g_ping.options.ttl);
        }
        printf (START_MESSAGE_FORMAT, g_ping.sock_info.hostname,
                g_ping.sock_info.ip_addr,
                g_ping.options.ipv6 ? ICMPV6_PAYLOAD_SIZE : ICMPV4_PAYLOAD_SIZE,
                g_ping.options.ipv6 ? ICMPV6_PACKET_SIZE : ICMPV4_PACKET_SIZE);
    }
    else if (type == PING)
    {
        printf (PING_MESSAGE_FORMAT, g_ping.sock_info.hostname,
                g_ping.sock_info.ip_addr, g_ping.ping_state.sequence,
                g_ping.ping_state.hopli, g_ping.rtt_metrics->std_rtt);
    }
    else if (type == END)
    {
        printf (END_MESSAGE_HEADER_FORMAT, g_ping.sock_info.hostname);
        printf ("N/A\n");
        printf ("N/A\n");
    }
}