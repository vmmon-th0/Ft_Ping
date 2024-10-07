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
        if (p->ai_family == AF_INET6)
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
            g_ping.options.ipv6 = true;
            break;
        }
    }

    if (p == NULL || g_ping.options.ipv4 == true)
    {
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
                g_ping.options.ipv6 = false;
                break;
            }
        }
    }

    if (res->ai_canonname)
    {
        memcpy (g_ping.sock_info.ai_canonname, res->ai_canonname,
                sizeof (g_ping.sock_info.ai_canonname));
        g_ping.sock_info.ai.ai_canonname = g_ping.sock_info.ai_canonname;
    }

    freeaddrinfo (res);

    if (p == NULL)
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
        close (g_ping.sock_info.sock_fd);
        release_rtt_resources ();
        exit (EXIT_FAILURE);
    }
    PING_DEBUG ("Ping sent to %s\n",
                inet_ntoa (g_ping.sock_info.addr_4.sin_addr));
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
        close (g_ping.sock_info.sock_fd);
        release_rtt_resources ();
        exit (EXIT_FAILURE);
    }
    PING_DEBUG ("Ping sent to %s\n",
                inet_ntoa (g_ping.sock_info.addr_6.sin_addr));
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
        close (g_ping.sock_info.sock_fd);
        release_rtt_resources ();
        exit (EXIT_FAILURE);
    }

    end_rtt_metrics ();

    /* The response includes the IP header followed by the ICMP header. It is
     * necessary to extract the IP header to access the ICMP header. */

    struct iphdr *ip_hdr = (struct iphdr *)recv_packet;
    struct icmphdr *icmp_hdr
        = (struct icmphdr *)(recv_packet + ip_hdr->ihl * 4);

    g_ping.ping_state.hopli = ip_hdr->ttl;
    g_ping.ping_state.sequence = ntohs (icmp_hdr->un.echo.sequence);

    PING_DEBUG ("Received ICMP packet:\n");
    PING_DEBUG ("Type: %d\n", icmp_hdr->type);
    PING_DEBUG ("Code: %d\n", icmp_hdr->code);
    PING_DEBUG ("Checksum: %d\n", icmp_hdr->checksum);
    PING_DEBUG ("ID: %d\n", ntohs (icmp_hdr->un.echo.id));
    PING_DEBUG ("Sequence: %d\n", g_ping.ping_state.sequence);

    switch (icmp_hdr->type)
    {
        case ICMP_ECHOREPLY:
            if (icmp_hdr->un.echo.id == htons (getpid ())
                && icmp_hdr->un.echo.sequence
                       == htons (g_ping.ping_state.sequence))
            {
                ping_messages_handler (PING);
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
        close (g_ping.sock_info.sock_fd);
        release_rtt_resources ();
        exit (EXIT_FAILURE);
    }

    end_rtt_metrics ();

    struct icmp6_hdr *icmp6_recv = (struct icmp6_hdr *)recv_packet;

    g_ping.ping_state.hopli = -1;
    for (cmsg = CMSG_FIRSTHDR (&msg); cmsg != NULL;
         cmsg = CMSG_NXTHDR (&msg, cmsg))
    {
        if (cmsg->cmsg_level == IPPROTO_IPV6
            && cmsg->cmsg_type == IPV6_HOPLIMIT)
        {
            uint8_t hoplimit;
            memcpy (&hoplimit, CMSG_DATA (cmsg), sizeof (hoplimit));
            g_ping.ping_state.hopli = hoplimit;
            break;
        }
    }

    uint8_t type = icmp6_recv->icmp6_type;
    g_ping.ping_state.sequence
        = ntohs (icmp6_recv->icmp6_dataun.icmp6_un_data16[1]);

    PING_DEBUG ("Received ICMPv6 packet:\n");
    PING_DEBUG ("Type: %d\n", type);
    PING_DEBUG ("Code: %d\n", icmp6_recv->icmp6_code);
    PING_DEBUG ("Checksum: %d\n", ntohs (icmp6_recv->icmp6_cksum));
    PING_DEBUG ("Sequence: %d\n", g_ping.ping_state.sequence);
    PING_DEBUG ("Hop Limit: %d\n", g_ping.ping_state.hopli);

    if (type == ND_ROUTER_ADVERT || type == ND_NEIGHBOR_SOLICIT
        || type == ND_NEIGHBOR_ADVERT)
    {
        PING_DEBUG ("Ignored ICMPv6 type %d from %s.\n", type,
                    g_ping.sock_info.ip_addr);
        return;
    }

    switch (type)
    {
        case ICMP6_ECHO_REPLY:
            if (icmp6_recv->icmp6_dataun.icmp6_un_data16[0] == htons (getpid ())
                && g_ping.ping_state.sequence
                       == ntohs (icmp6_recv->icmp6_dataun.icmp6_un_data16[1]))
            {
                ping_messages_handler (PING);
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
    /* Hostname resolution */

    if (resolve_hostname (hostname) == -1)
    {
        fprintf (stderr, "Failed to resolve hostname\n");
        exit (EXIT_FAILURE);
    }

    g_ping.sock_info.hostname = hostname;

    /* Socket preparation */

    if ((g_ping.sock_info.sock_fd
         = socket (g_ping.options.ipv6 ? AF_INET6 : AF_INET, SOCK_RAW,
                   g_ping.options.ipv6 ? IPPROTO_ICMPV6 : IPPROTO_ICMP))
        == -1)
    {
        perror ("socket");
        exit (EXIT_FAILURE);
    }

    /* Socket options */

    if (g_ping.options.ipv6 == true)
    {
        memset (&g_ping.sock_info.addr_6, 0, sizeof (g_ping.sock_info.addr_6));
        g_ping.sock_info.addr_6.sin6_family = AF_INET6;
        g_ping.sock_info.addr_6.sin6_port = htons (0);

        if (inet_pton (AF_INET6, g_ping.sock_info.ip_addr,
                       &g_ping.sock_info.addr_6.sin6_addr)
            != 1)
        {
            perror ("inet_pton IPv6");
            exit (EXIT_FAILURE);
        }
    }
    else
    {
        memset (&g_ping.sock_info.addr_4, 0, sizeof (g_ping.sock_info.addr_4));
        g_ping.sock_info.addr_4.sin_family = AF_INET;
        g_ping.sock_info.addr_4.sin_port = htons (0);
        if (!(g_ping.sock_info.addr_4.sin_addr.s_addr
              = inet_addr (g_ping.sock_info.ip_addr)))
        {
            fprintf (stderr, "inet_addr failed ipv4 convert\n");
            exit (EXIT_FAILURE);
        }
    }

    int hopli = g_ping.options.ttl ? g_ping.options.ttl : 64;

    /* Configure the IP_TTL option to define the Time To Live (TTL) of IP
     * packets sent by the socket, using the IP protocol level (IPPROTO_IP). */

    if (setsockopt (g_ping.sock_info.sock_fd,
                    g_ping.options.ipv6 ? IPPROTO_IPV6 : IPPROTO_IP,
                    g_ping.options.ipv6 ? IPV6_UNICAST_HOPS : IP_TTL, &hopli,
                    sizeof (hopli))
        < 0)
    {
        perror ("setsockopt failed");
        exit (EXIT_FAILURE);
    }

    if (g_ping.options.ipv6 == true)
    {
        int on = 1;

        if (setsockopt (g_ping.sock_info.sock_fd, IPPROTO_IPV6,
                        IPV6_RECVHOPLIMIT, &on, sizeof (on))
            < 0)
        {
            perror ("setsockopt IPV6_RECVHOPLIMIT");
            close (g_ping.sock_info.sock_fd);
            exit (EXIT_FAILURE);
        }
    }

    /* Start ping coordination */

    ping_messages_handler (START);

    while (1)
    {
        if (g_ping.options.ipv4 == false
            && g_ping.sock_info.ai.ai_family == AF_INET6)
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
            && g_ping.ping_state.nb_res >= g_ping.options.count)
        {
            break;
        }

        sleep (1);
    }

    close (g_ping.sock_info.sock_fd);
}