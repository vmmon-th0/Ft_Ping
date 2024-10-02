#include "ft_ping.h"

static int
resolve_hostname (const char *hostname)
{
    struct addrinfo hints, *res, *p;
    int status;

    memset (&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_RAW;

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
                           INET_ADDRSTRLEN)
                == NULL)
            {
                perror ("inet_ntop");
                freeaddrinfo (res);
                return -1;
            }

            // printf ("IPv4 addr for %s: %s\n", hostname,
            //         g_ping.sock_info.ip_addr);
            break;
        }
    }

    freeaddrinfo (res);
    return 0;
}

static void
send_icmp ()
{
    struct ping_packet ping_pkt;

    fill_icmp_packet (&ping_pkt);

    gettimeofday (&g_ping.rtt_metrics.start, NULL);

    if (sendto (g_ping.sock_info.sock_fd, &ping_pkt,
                sizeof (struct ping_packet), 0,
                (struct sockaddr *)&g_ping.sock_info.addr,
                sizeof (g_ping.sock_info.addr))
        == -1)
    {
        perror ("sendto");
        close (g_ping.sock_info.sock_fd);
        exit (EXIT_FAILURE);
    }

    // printf ("Ping sent to %s\n", inet_ntoa (g_ping.sock_info.addr.sin_addr));
}

static void
recv_icmp ()
{
    char buffer[PACKET_SIZE + sizeof (struct iphdr)];
    struct sockaddr_in r_addr;
    socklen_t addr_len = sizeof (r_addr);

    if (recvfrom (g_ping.sock_info.sock_fd, buffer, sizeof (buffer), 0,
                  (struct sockaddr *)&r_addr, &addr_len)
        == -1)
    {
        perror ("recvfrom");
        close (g_ping.sock_info.sock_fd);
        exit (EXIT_FAILURE);
    }

    gettimeofday (&g_ping.rtt_metrics.end, NULL);

    compute_std_rtt ();

    struct iphdr *ip_hdr = (struct iphdr *)buffer;
    struct icmphdr *icmp_hdr = (struct icmphdr *)(buffer + ip_hdr->ihl * 4);

    // printf ("Received ICMP packet:\n");
    // printf ("Type: %d\n", icmp_hdr->type);
    // printf ("Code: %d\n", icmp_hdr->code);
    // printf ("Checksum: %d\n", ntohs (icmp_hdr->checksum));
    // printf ("ID: %d\n", ntohs (icmp_hdr->un.echo.id));
    // printf ("Sequence: %d\n", icmp_hdr->un.echo.sequence);

    /* WARNING : CHECK ACCURACY OF IP ADDR AND TTL, actually std_rtt represent
    actual rtt of the packet, later we have to create states and register them
    to have more metrics on the different types of RTT's. */

    g_ping.sequence = icmp_hdr->un.echo.sequence;

    ping_messages_handler (PING);

    // switch (icmp_hdr->type)
    // {
    //     case ICMP_ECHOREPLY:
    //         printf ("Type: ICMP Echo Reply\n");
    //         break;
    //     case ICMP_DEST_UNREACH:
    //         printf ("Type: ICMP Destination Unreachable\n");
    //         break;
    //     case ICMP_SOURCE_QUENCH:
    //         printf ("Type: ICMP Source Quench\n");
    //         break;
    //     case ICMP_REDIRECT:
    //         printf ("Type: ICMP Redirect\n");
    //         break;
    //     case ICMP_ECHO:
    //         printf ("Type: ICMP Echo Request\n");
    //         break;
    //     case ICMP_TIME_EXCEEDED:
    //         printf ("Type: ICMP Time Exceeded\n");
    //         break;
    //     case ICMP_PARAMETERPROB:
    //         printf ("Type: ICMP Parameter Problem\n");
    //         break;
    //     case ICMP_TIMESTAMP:
    //         printf ("Type: ICMP Timestamp Request\n");
    //         break;
    //     case ICMP_TIMESTAMPREPLY:
    //         printf ("Type: ICMP Timestamp Reply\n");
    //         break;
    //     default:
    //         printf ("Type: Unknown ICMP Type\n");
    // }
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

    /* Socket preparation */

    if ((g_ping.sock_info.sock_fd = socket (AF_INET, SOCK_RAW, IPPROTO_ICMP))
        == -1)
    {
        perror ("socket");
        exit (EXIT_FAILURE);
    }

    /* Socket options */

    memset (&g_ping.sock_info.addr, 0, sizeof (g_ping.sock_info.addr));
    g_ping.sock_info.addr.sin_family = AF_INET;
    g_ping.sock_info.addr.sin_port = htons (0);
    g_ping.sock_info.addr.sin_addr.s_addr
        = inet_addr (g_ping.sock_info.ip_addr);

    uint8_t ttl = g_ping.options.ttl ? g_ping.options.ttl : 64;

    /* Configure the IP_TTL option to define the Time To Live (TTL) of IP
    packets sent by the socket, using the IP protocol level (IPPROTO_IP). */

    if (setsockopt (g_ping.sock_info.sock_fd, IPPROTO_IP, IP_TTL, &ttl,
                    sizeof (ttl))
        < 0)
    {
        perror ("Setting socket options failed");
        exit (EXIT_FAILURE);
    }

    /* Start ping coordination */

    // printf ("PING %s (%s) %lu(%lu) bytes of data.\n",
    // g_ping.sock_info.hostname,
    //         g_ping.sock_info.ip_addr, PAYLOAD_SIZE, IP_PACKET_SIZE);

    ping_messages_handler (START);

    while (1)
    {
        send_icmp ();
        recv_icmp ();
        sleep (1);
    }

    close (g_ping.sock_info.sock_fd);
}