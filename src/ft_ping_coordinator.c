#include "ft_ping.h"

/**
 * @brief This function resolves a hostname that will be the ping destination
 * target, we are dealing with IPV4 & 6, The getaddrinfo() function allocates
 * and initializes a linked list of addrinfo structures. There are several
 * reasons why the linked list may have more than one addrinfo structure :
 * - Multihoming (A network host can be reached via multiple IP addresses)
 * - Multiple protocols (AF_INET (IPv4) and AF_INET6 (IPv6))
 * - Various socket types (The same service can be reached via different socket
 * types, such as SOCK_STREAM (TCP) and SOCK_DGRAM (UDP))
 * @param hostname destination hostname
 */

static int
resolve_hostname (const char *hostname)
{
    struct addrinfo hints, *res, *p;
    int status;
    ip_version ipv = UNSPEC;

    memset (&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_RAW;
    hints.ai_flags = AI_CANONNAME;

    if ((status = getaddrinfo (hostname, NULL, &hints, &res)) != 0)
    {
        fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (status));
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next)
    {
        if (p->ai_family == AF_INET)
        {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            void *addr = &(ipv4->sin_addr);

            if (inet_ntop (p->ai_family, addr, g_ping.sock_info.ip_addr,
                           sizeof (g_ping.sock_info.ip_addr))
                == NULL)
            {
                perror ("inet_ntop IPv4");
                continue;
            }
            memcpy (&g_ping.sock_info.ai, p, sizeof (struct addrinfo));
            PING_DEBUG ("IPv4 addr for %s: %s\n", hostname,
                        g_ping.sock_info.ip_addr);

            ipv = IPV4;

            if (g_ping.options.ipv == UNSPEC || g_ping.options.ipv == IPV4)
            {
                break;
            }
        }
        else if (p->ai_family == AF_INET6)
        {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            void *addr = &(ipv6->sin6_addr);

            if (inet_ntop (p->ai_family, addr, g_ping.sock_info.ip_addr,
                           sizeof (g_ping.sock_info.ip_addr))
                == NULL)
            {
                perror ("inet_ntop IPv6");
                continue;
            }
            memcpy (&g_ping.sock_info.ai, p, sizeof (struct addrinfo));
            PING_DEBUG ("IPv6 addr for %s: %s\n", hostname,
                        g_ping.sock_info.ip_addr);

            ipv = IPV6;

            if (g_ping.options.ipv == UNSPEC || g_ping.options.ipv == IPV6)
            {
                break;
            }
        }
    }

    g_ping.options.ipv = ipv;

    if (res->ai_canonname)
    {
        memcpy (g_ping.sock_info.ai_canonname, res->ai_canonname,
                sizeof (g_ping.sock_info.ai_canonname));
        g_ping.sock_info.ai.ai_canonname = g_ping.sock_info.ai_canonname;
    }

    freeaddrinfo (res);

    if (p == NULL && ipv == UNSPEC)
    {
        fprintf (stderr, "No valid IPv4 or IPv6 address found for %s\n",
                 hostname);
        return -1;
    }

    return 0;
}

static void
send_icmp_packet_v4 ()
{
    struct ping_packet_v4 ping_pkt;

    fill_icmp_packet_v4 (&ping_pkt);
    start_rtt_metrics ();

    if (sendto (g_ping.sock_info.sock_fd, &ping_pkt,
                sizeof (struct ping_packet_v4), 0,
                (struct sockaddr *)&g_ping.sock_info.addr_4,
                sizeof (g_ping.sock_info.addr_4))
        == -1)
    {
        perror ("sendto");
        release_resources ();
        exit (EXIT_FAILURE);
    }
    ++g_ping.ping_stats.nb_snd;
    PING_DEBUG ("Ping sent to %s\n", g_ping.sock_info.ip_addr);
}

static void
send_icmp_packet_v6 ()
{
    struct ping_packet_v6 ping_pkt;

    fill_icmp_packet_v6 (&ping_pkt);
    start_rtt_metrics ();

    if (sendto (g_ping.sock_info.sock_fd, &ping_pkt,
                sizeof (struct ping_packet_v6), 0,
                (struct sockaddr *)&g_ping.sock_info.addr_6,
                sizeof (g_ping.sock_info.addr_6))
        == -1)
    {
        perror ("sendto");
        release_resources ();
        exit (EXIT_FAILURE);
    }

    ++g_ping.ping_stats.nb_snd;
    PING_DEBUG ("Ping sent to %s\n", g_ping.sock_info.ip_addr);
}

static void
recv_icmp_packet_v4 ()
{
    char recv_packet[PACKET_SIZE + sizeof (struct iphdr)];
    struct sockaddr_in r_addr;
    socklen_t addr_len;

    addr_len = sizeof (r_addr);

    if (recvfrom (g_ping.sock_info.sock_fd, recv_packet, sizeof (recv_packet),
                  0, (struct sockaddr *)&r_addr, &addr_len)
        == -1)
    {
        perror ("recvfrom");
        release_resources ();
        exit (EXIT_FAILURE);
    }

    end_rtt_metrics ();

    /* The response includes the IP header followed by the ICMP header. It is
     * necessary to extract the IP header to access the ICMP header. */

    struct iphdr *ip_hdr = (struct iphdr *)recv_packet;
    struct icmphdr *icmp_hdr
        = (struct icmphdr *)(recv_packet + ip_hdr->ihl * 4);

    g_ping.ping_stats.hopli = ip_hdr->ttl;
    g_ping.ping_stats.sequence = ntohs (icmp_hdr->un.echo.sequence);

    PING_DEBUG ("Received ICMP packet:\n");
    PING_DEBUG ("Type: %d\n", icmp_hdr->type);
    PING_DEBUG ("Code: %d\n", icmp_hdr->code);
    PING_DEBUG ("Checksum: %d\n", icmp_hdr->checksum);
    PING_DEBUG ("Sequence: %d\n", g_ping.ping_stats.sequence);
    PING_DEBUG ("Identifier: %d\n", ntohs (icmp_hdr->un.echo.id));

    if (verify_checksum ((struct ping_packet_v4 *)icmp_hdr) == false)
    {
        fprintf (stderr, "Received corrupted ICMPv4 packet\n");
        release_resources ();
        exit (EXIT_FAILURE);
    }

    switch (icmp_hdr->type)
    {
        case ICMP_ECHOREPLY:
            if (icmp_hdr->un.echo.id == htons (getpid ())
                && icmp_hdr->un.echo.sequence
                       == htons (g_ping.ping_stats.sequence))
            {
                ping_messages_handler (PING);
                ++g_ping.ping_stats.nb_res;
            }
            break;
        case ICMP_DEST_UNREACH:
            switch (icmp_hdr->code)
            {
                case ICMP_NET_UNREACH:
                    printf ("Destination Unreachable: Network unreachable.\n");
                    break;
                case ICMP_HOST_UNREACH:
                    printf ("Destination Unreachable: Host unreachable.\n");
                    break;
                case ICMP_PORT_UNREACH:
                    printf ("Destination Unreachable: Port unreachable.\n");
                    break;
                case ICMP_FRAG_NEEDED:
                    printf ("Destination Unreachable: Fragmentation needed but "
                            "DF flag set.\n");
                    break;
                default:
                    printf ("Destination Unreachable: Code %d.\n",
                            icmp_hdr->code);
            }
            break;
        case ICMP_TIME_EXCEEDED:
            printf ("Time Exceeded: TTL expired for %s.\n",
                    g_ping.sock_info.ip_addr);
            break;
        default:
            printf ("Unhandled ICMP type %d received from %s.\n",
                    icmp_hdr->type, g_ping.sock_info.ip_addr);
            break;
    }
}

static void
recv_icmp_packet_v6 ()
{
    char recv_packet[PACKET_SIZE];
    struct msghdr msg;
    struct iovec iov;
    char control_buf[CONTROL_BUFFER_SIZE];
    struct cmsghdr *cmsg;
    ssize_t len;

    iov.iov_base = recv_packet;
    iov.iov_len = sizeof (recv_packet);

    memset (&msg, 0, sizeof (msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control_buf;
    msg.msg_controllen = sizeof (control_buf);

    len = recvmsg (g_ping.sock_info.sock_fd, &msg, 0);
    if (len == -1)
    {
        perror ("recvmsg");
        release_resources ();
        exit (EXIT_FAILURE);
    }


    struct icmp6_hdr *icmp6_recv = (struct icmp6_hdr *)recv_packet;
    uint8_t type = icmp6_recv->icmp6_type;

    if (type == ICMP6_ECHO_REPLY)
    {
        end_rtt_metrics ();
    }

    g_ping.ping_stats.hopli = -1;
    for (cmsg = CMSG_FIRSTHDR (&msg); cmsg != NULL;
         cmsg = CMSG_NXTHDR (&msg, cmsg))
    {
        if (cmsg->cmsg_level == IPPROTO_IPV6
            && cmsg->cmsg_type == IPV6_HOPLIMIT)
        {
            uint8_t hoplimit;
            memcpy (&hoplimit, CMSG_DATA (cmsg), sizeof (hoplimit));
            g_ping.ping_stats.hopli = hoplimit;
            break;
        }
    }

    g_ping.ping_stats.sequence
        = ntohs (icmp6_recv->icmp6_dataun.icmp6_un_data16[1]);

    PING_DEBUG ("Received ICMPv6 packet:\n");
    PING_DEBUG ("Type: %d\n", type);
    PING_DEBUG ("Code: %d\n", icmp6_recv->icmp6_code);
    PING_DEBUG ("Checksum: %d\n", ntohs (icmp6_recv->icmp6_cksum));
    PING_DEBUG ("Sequence: %d\n", g_ping.ping_stats.sequence);
    PING_DEBUG ("Hop Limit: %d\n", g_ping.ping_stats.hopli);
    PING_DEBUG ("Identifier: %d\n", ntohs (icmp6_recv->icmp6_dataun.icmp6_un_data16[0]));

    if (type == ND_ROUTER_ADVERT || type == ND_NEIGHBOR_SOLICIT
        || type == ND_NEIGHBOR_ADVERT)
    {
        PING_DEBUG ("Ignored ICMPv6 type %d from %s.\n", type,
                    g_ping.sock_info.ip_addr);
        ssize_t n = recvmsg(g_ping.sock_info.sock_fd, &msg, MSG_PEEK);
        printf("len -> %d, type -> %d", n, type);
        if (n >= 1) {
            printf ("data is here...\n");
            recv_icmp_packet_v6 ();
        }
        return;
    }

    switch (type)
    {
        case ICMP6_ECHO_REPLY:
            if (icmp6_recv->icmp6_dataun.icmp6_un_data16[0] == htons (getpid ())
                && g_ping.ping_stats.sequence
                       == ntohs (icmp6_recv->icmp6_dataun.icmp6_un_data16[1]))
            {
                ping_messages_handler (PING);
                ++g_ping.ping_stats.nb_res;
            }
            break;

        case ICMP6_DST_UNREACH:
            switch (icmp6_recv->icmp6_code)
            {
                case ICMP6_DST_UNREACH_NOROUTE:
                    printf (
                        "Destination Unreachable: No route to destination.\n");
                    break;
                case ICMP6_DST_UNREACH_ADMIN:
                    printf ("Destination Unreachable: Administratively "
                            "prohibited.\n");
                    break;
                case ICMP6_DST_UNREACH_BEYONDSCOPE:
                    printf ("Destination Unreachable: Beyond scope of source "
                            "address.\n");
                    break;
                default:
                    printf ("Destination Unreachable: Code %d.\n",
                            icmp6_recv->icmp6_code);
            }
            break;

        case ICMP6_TIME_EXCEEDED:
            printf ("Time Exceeded: Hop limit exceeded in transit for %s.\n",
                    g_ping.sock_info.ip_addr);
            break;

        default:
            printf ("Unhandled ICMPv6 type %d received from %s.\n",
                    icmp6_recv->icmp6_type, g_ping.sock_info.ip_addr);
            break;
    }

    return;
}

/**
 * @brief Supervises the steps of the ping diagnosis.
 * This function is the central point regarding the supervision of the steps to
 * perform a ping diagnostic from the creation of sockets to the sending and
 * reception of ICMP packets.
 * @param hostname destination hostname
 */

void
ping_coord (const char *hostname)
{
    if (resolve_hostname (hostname) == -1)
    {
        fprintf (stderr, "Failed to resolve hostname\n");
        exit (EXIT_FAILURE);
    }

    g_ping.sock_info.hostname = hostname;

    ping_socket_init ();
    ping_messages_handler (START);

    while (1)
    {
        if (g_ping.options.ipv == IPV6)
        {
            send_icmp_packet_v6 ();
            recv_icmp_packet_v6 ();
        }
        else
        {
            send_icmp_packet_v4 ();
            recv_icmp_packet_v4 ();
        }

        if (g_ping.options.count
            && g_ping.ping_stats.nb_snd >= g_ping.options.count)
        {
            break;
        }

        if (rtt_timeout () == true)
        {
            fprintf (stderr, "ping reached timeout: %f\n",
                     g_ping.ping_stats.timeout_threshold);
            break;
        }
        sleep (1);
    }
    ping_messages_handler (END);
    release_resources ();
}