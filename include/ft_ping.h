#ifndef FT_PING_H
#define FT_PING_H

#include <arpa/inet.h>
#include <errno.h>
#include <float.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <netdb.h>
#include <netinet/icmp6.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip_icmp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define PACKET_SIZE 64
#define CONTROL_BUFFER_SIZE 1024

#define ICMPV4_PACKET_SIZE PACKET_SIZE + sizeof (struct iphdr)
#define ICMPV4_PAYLOAD_SIZE PACKET_SIZE - sizeof (struct icmphdr)

#define ICMPV6_PACKET_SIZE PACKET_SIZE + sizeof (struct ip6_hdr)
#define ICMPV6_PAYLOAD_SIZE PACKET_SIZE - sizeof (struct icmp6_hdr)

#define RTT_WEIGHT_FACTOR 0.125
#define RTT_DEVIATION_FACTOR 0.25

#define TIMEOUT 1

typedef enum
{
    START,
    PING,
    END
} message;

typedef enum
{
    UNSPEC,
    IPV4,
    IPV6,
} ip_version;

#ifdef DEBUG
#define PING_DEBUG(fmt, ...)                                                   \
    fprintf (stderr, "DEBUG: %s:%d:%s(): " fmt, __FILE__, __LINE__, __func__,  \
             ##__VA_ARGS__)
#else
#define PING_DEBUG(fmt, ...)
#endif

struct ping_packet_v4
{
    struct icmphdr hdr;
    char data[ICMPV4_PAYLOAD_SIZE];
};

struct ping_packet_v6
{
    struct icmp6_hdr hdr;
    char data[ICMPV6_PAYLOAD_SIZE];
};

struct s_options
{
    _Bool verbose;
    _Bool help;
    ip_version ipv;
    uint8_t ttl;
    uint32_t count;
};

struct s_sock_info
{
    struct addrinfo ai;
    struct sockaddr_in addr_4;
    struct sockaddr_in6 addr_6;
    const char *hostname;
    char ai_canonname[NI_MAXHOST];
    char ip_addr[INET6_ADDRSTRLEN];
    int sock_fd;
};

struct s_rtt
{
    struct timespec start;
    struct timespec end;
    double rtt;
    struct s_rtt *next;
};

struct s_ping_stats
{
    int sequence;
    ssize_t bytes_recv;
    _Bool ready_send;

    uint8_t hopli;

    uint16_t nb_snd;
    uint16_t nb_res;

    double pkt_loss;
    double ping_session;
    double dev_rtt;
    double estimated_rtt;
    double timeout_threshold;

    double min;
    double max;
    double avg;
};

struct s_ping
{
    struct s_options options;
    struct s_rtt *rtt_metrics;
    struct s_rtt *rtt_metrics_beg;
    struct s_sock_info sock_info;
    struct s_ping_stats ping_stats;
};

extern struct s_ping g_ping;

void ping_coord (const char *hostname);
void fill_icmp_packet_v4 (struct ping_packet_v4 *ping_pkt);
void fill_icmp_packet_v6 (struct ping_packet_v6 *ping_pkt);
void start_rtt_metrics ();
void end_rtt_metrics ();
void ping_messages_handler (message type);
void release_resources ();
void compute_rtt_stats ();
void ping_socket_init ();
_Bool rtt_timeout ();
_Bool verify_checksum (struct ping_packet_v4 *ping_pkt);

#endif
