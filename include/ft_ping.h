#ifndef FT_PING_H
#define FT_PING_H

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define PACKET_SIZE 64
#define IP_PACKET_SIZE PACKET_SIZE + sizeof (struct iphdr)
#define PAYLOAD_SIZE PACKET_SIZE - sizeof (struct icmphdr)
#define TIMEOUT 1

typedef enum
{
    START,
    PING,
    END
} MessageType;

struct ping_packet
{
    struct icmphdr hdr;
    char data[PAYLOAD_SIZE];
};

// To Do implement stats

struct s_options
{
    _Bool verbose;
    _Bool help;
    uint8_t ttl;
    uint32_t deadline;
    uint32_t count;
};

struct s_sock_info
{
    struct sockaddr_in addr;
    const char *hostname;
    char ip_addr[INET_ADDRSTRLEN];
    int sock_fd;
};

struct s_rtt
{
    struct timeval start;
    struct timeval end;
    double std_rtt;
};

struct s_ping
{
    struct s_options options;
    struct s_sock_info sock_info;
    struct s_rtt rtt_metrics;

    int sequence;
};

extern struct s_ping g_ping;

void ping_coord (const char *hostname);

void fill_icmp_packet (struct ping_packet *ping_pkt);

void compute_std_rtt ();

void ping_messages_handler (MessageType type);

#endif
