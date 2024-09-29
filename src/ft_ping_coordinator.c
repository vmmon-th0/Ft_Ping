#include "ft_ping.h"

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

    printf ("64 bytes from %s (%s): icmp_seq=%d ttl=%hhu time=%.2fms\n",
            g_ping.sock_info.hostname, g_ping.sock_info.ip_addr,
            icmp_hdr->un.echo.sequence, g_ping.options.ttl,
            g_ping.rtt_metrics.std_rtt);

    if (icmp_hdr->type == ICMP_ECHOREPLY)
    {
        // printf ("Ping succeeds from %s\n", inet_ntoa (r_addr.sin_addr));
    }
    else
    {
        // printf ("Received non-echo reply type: %d\n", icmp_hdr->type);
    }
}

/**
 * @brief Supervises the steps of the ping diagnosis.
 * This function is the central point regarding the supervision of the steps to
 * perform a ping diagnostic from the creation of sockets to the sending and
 * reception of ICMP packets.
 * @param hostname destination hostname
 */

void
ft_ping_coordinator (const char *hostname)
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

    uint8_t ttl = 64;

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

    printf ("PING %s (%s) %lu(%lu) bytes of data.\n", g_ping.sock_info.hostname,
            g_ping.sock_info.ip_addr, PAYLOAD_SIZE, IP_PACKET_SIZE);

    while (1)
    {
        send_icmp ();
        recv_icmp ();
        sleep (1);
    }

    close (g_ping.sock_info.sock_fd);
}