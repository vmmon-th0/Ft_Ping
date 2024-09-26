#ifndef FT_PING_H
#define FT_PING_H

#include <arpa/inet.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/ip_icmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define PACKET_SIZE 64
#define TIMEOUT 1

struct ping_packet
{
    struct icmphdr hdr;
    char data[PACKET_SIZE - sizeof (struct icmphdr)];
};

// To Do implement stats

struct s_options
{
    _Bool ttl;
    _Bool verbose;
    _Bool help;
    _Bool deadline;
    _Bool packet_size;
    _Bool count;
};

struct s_sock_info
{
    struct sockaddr_in addr;
    char *hostname;
    char ip_addr[INET_ADDRSTRLEN];
    int sock_fd;
};

struct s_ping
{
    struct s_options options;
    struct s_sock_info sock_info;
};

extern struct s_ping g_ping;

#endif
