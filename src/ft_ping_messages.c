#include "ft_ping.h"

static const char *START_MESSAGE_FORMAT
    = "PING %s (%s) %lu(%lu) bytes of data.\n";
static const char *PING_MESSAGE_FORMAT
    = "%d bytes from %s (%s): icmp_seq=%d ttl=%hhu time=%.2fms\n";

static const char *END_MESSAGE_HEADER_FORMAT = "--- %s ping statistics ---\n";
static const char *END_MESSAGE_STATS_FORMAT
    = "%d packets transmitted, %d received, %.0f%% packet loss, time %.0f ms\n";
static const char *END_MESSAGE_RTT_FORMAT
    = "rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n";

void
ping_messages_handler (message type)
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
                    "%u, TTL: %u\n\n",
                    g_ping.options.verbose ? "Yes" : "No",
                    g_ping.options.ipv == IPV4 ? "Yes" : "No",
                    g_ping.options.ipv == IPV6 ? "Yes" : "No",
                    g_ping.options.count, g_ping.options.ttl);
        }
        printf (START_MESSAGE_FORMAT, g_ping.sock_info.hostname,
                g_ping.sock_info.ip_addr,
                g_ping.options.ipv == IPV6 ? ICMPV6_PAYLOAD_SIZE
                                           : ICMPV4_PAYLOAD_SIZE,
                g_ping.options.ipv == IPV6 ? ICMPV6_PACKET_SIZE
                                           : ICMPV4_PACKET_SIZE);
    }
    else if (type == PING)
    {
        printf (PING_MESSAGE_FORMAT,
                g_ping.options.ipv == IPV6
                    ? g_ping.info.bytes_recv
                    : g_ping.info.bytes_recv - sizeof (struct iphdr),
                g_ping.sock_info.hostname, g_ping.sock_info.ip_addr,
                g_ping.info.sequence, g_ping.info.hopli,
                g_ping.rtt_metrics->rtt);
    }
    else if (type == END)
    {
        compute_rtt_stats ();
        printf (END_MESSAGE_HEADER_FORMAT, g_ping.sock_info.hostname);
        printf (END_MESSAGE_STATS_FORMAT, g_ping.stats.nb_snd,
                g_ping.stats.nb_res, g_ping.stats.pkt_loss,
                g_ping.stats.ping_session);
        printf (END_MESSAGE_RTT_FORMAT, g_ping.stats.min,
                g_ping.stats.avg, g_ping.stats.max,
                g_ping.stats.dev_rtt);
    }
}