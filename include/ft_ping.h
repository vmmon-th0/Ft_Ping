#ifndef FT_PING_H
#define FT_PING_H

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
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
#define ICMPV6_PACKET_SIZE PACKET_SIZE + sizeof (struct ip6_hdr)

#define ICMPV4_PAYLOAD_SIZE PACKET_SIZE - sizeof (struct icmphdr)
#define ICMPV6_PAYLOAD_SIZE PACKET_SIZE - sizeof (struct icmp6_hdr)

#define RTT_WEIGHT_FACTOR 0.125

#define TIMEOUT 1

typedef enum
{
    START,
    PING,
    END
} MessageType;

#ifdef DEBUG
#define PING_DEBUG(fmt, ...)                                                   \
    fprintf (stderr, "DEBUG: %s:%d:%s(): " fmt, __FILE__, __LINE__, __func__,  \
             __VA_ARGS__)
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
    _Bool ipv6;
    _Bool ipv4;
    uint8_t ttl;
    uint32_t deadline;
    uint32_t count;
};

/* Split it later src/dst sockets */

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
    double std_rtt;
    struct s_rtt *next;
};

struct s_ping_state
{
    int sequence;
    uint8_t hopli;
    uint64_t nb_res;
    double estimated_rtt;
};

struct s_ping
{
    struct s_options options;
    struct s_rtt *rtt_metrics;
    struct s_rtt *rtt_metrics_beg;
    struct s_sock_info sock_info;
    struct s_ping_state ping_state;
};

extern struct s_ping g_ping;

void ping_coord (const char *hostname);

void fill_icmp_packet_v4 (struct ping_packet_v4 *ping_pkt);

void fill_icmp_packet_v6 (struct ping_packet_v6 *ping_pkt);

void start_rtt_metrics ();
void end_rtt_metrics ();

void ping_messages_handler (MessageType type);

void release_rtt_resources ();

#endif
