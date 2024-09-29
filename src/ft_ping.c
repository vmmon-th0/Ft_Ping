#include "ft_ping.h"

static char short_options[] = "vhs:c:w:t:";

static struct option long_options[]
    = { { "verbose", no_argument, NULL, 'v' },
        { "help", no_argument, NULL, 'h' },
        { "packetsize", required_argument, NULL, 's' },
        { "count", required_argument, NULL, 'c' },
        { "deadline", required_argument, NULL, 'w' },
        { "ttl", required_argument, NULL, 't' },
        { NULL, 0, NULL, 0 } };

struct s_ping g_ping;

void
show_usage (void)
{
    printf ("\
Usage: ping [OPTION]... [ADDRESS]...\n\
Options :\n\
-h, --help         display this help and exit\n\
-v, --verbose      verbose output\n");
}

int
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

/**
 * @brief Compute a checksum for data integrity.
 *
 * This function computes a checksum for the given data to ensure its integrity.
 * It performs the following operations:
 *
 * 1. Adjacent octets to be checksummed are paired to form 16-bit integers,
 *    and the 1's complement sum of these 16-bit integers is calculated.
 * 2. To generate a checksum, the checksum field itself is cleared, the
 *    16-bit 1's complement sum is computed over the octets concerned, and
 *    the 1's complement of this sum is placed in the checksum field.
 * 3. To check a checksum, the 1's complement sum is computed over the same
 *    set of octets, including the checksum field. If the result is all 1 bits
 *    (i.e., -0 in 1's complement arithmetic), the check succeeds.
 *
 * @param ping_pkt Pointer to the `ping_packet` structure whose data is to be
 * checksummed.
 * @param len Size of the `ping_packet` structure.
 *
 * @return The computed checksum as a 16-bit integer.
 */

uint16_t
compute_checksum (struct ping_packet *ping_pkt, size_t len)
{
    const uint16_t *data = (const void *)ping_pkt;
    uint32_t sum = 0;

    /* Add 16-bit words */
    for (size_t i = 0; i < len / 2; ++i)
    {
        sum += data[i];
    }

    /* Carry bit (if the length is odd, add the last byte) */
    if (len % 2)
    {
        sum += ((const uint8_t *)data)[len - 1] << 8;
    }

    /* Fold 32-bit sum to 16 bits */
    while (sum >> 16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;
}

int
verify_checksum (struct ping_packet *ping_pkt)
{
    uint16_t computed_checksum
        = compute_checksum (ping_pkt, sizeof (struct ping_packet));
    return (computed_checksum == ping_pkt->hdr.checksum);
}

void
fill_icmp_packet (struct ping_packet *ping_pkt)
{
    static int sequence = 0;

    memset (ping_pkt, 0, sizeof (struct ping_packet));
    ping_pkt->hdr.type = ICMP_ECHO;
    ping_pkt->hdr.code = 0;
    ping_pkt->hdr.un.echo.id = getpid ();
    ping_pkt->hdr.un.echo.sequence = sequence++;
    memset (ping_pkt->data, 0xA5, sizeof (ping_pkt->data));
    ping_pkt->hdr.checksum = 0;
    ping_pkt->hdr.checksum
        = compute_checksum (ping_pkt, sizeof (struct ping_packet));
}

void
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

void
compute_std_rtt ()
{
    double rtt
        = (g_ping.rtt_metrics.end.tv_sec - g_ping.rtt_metrics.start.tv_sec)
          * 1000.0;
    rtt += (g_ping.rtt_metrics.end.tv_usec - g_ping.rtt_metrics.start.tv_usec)
           / 1000.0;
    g_ping.rtt_metrics.std_rtt = rtt;
}

void
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
ft_ping_coord (const char *hostname)
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

/**
 * @brief Checks if the program is run with root rights.
 * Creating raw sockets (SOCK_RAW) for protocols like ICMP (IPPROTO_ICMP)
 * requires elevated privileges
 * @return 1 if the program is executed with root rights, 0 otherwise.
 */

int
is_running_as_root ()
{
    return geteuid () == 0;
}

int
main (int argc, char *argv[])
{
    int opt;
    int long_index = 0;

    if (!is_running_as_root ())
    {
        fprintf (stderr, "program need to be run as root");
        exit (EXIT_FAILURE);
    }

    while ((opt = getopt_long (argc, argv, short_options, long_options,
                               &long_index))
           != EOF)
    {
        switch (opt)
        {
            case 'v':
                printf ("Verbose mode enabled.\n");
                g_ping.options.verbose = true;
                break;

            case 'h':
                show_usage ();
                g_ping.options.help = true;
                exit (EXIT_SUCCESS);

            case 'c':
            {
                char *endptr;
                errno = 0;
                long value = strtol (optarg, &endptr, 10);

                if (errno == ERANGE || value < 0 || value > UINT32_MAX
                    || *endptr != '\0')
                {
                    fprintf (stderr, "Invalid count value: %s\n", optarg);
                    exit (EXIT_FAILURE);
                }

                g_ping.options.count = (uint32_t)value;
                printf ("Count: %u\n", g_ping.options.count);
            }
            break;

            case 'w':
            {
                char *endptr;
                errno = 0;
                long value = strtol (optarg, &endptr, 10);

                if (errno == ERANGE || value < 0 || value > UINT32_MAX
                    || *endptr != '\0')
                {
                    fprintf (stderr, "Invalid deadline value: %s\n", optarg);
                    exit (EXIT_FAILURE);
                }

                g_ping.options.deadline = (uint32_t)value;
                printf ("Deadline: %u\n", g_ping.options.deadline);
            }
            break;

            case 't':
            {
                char *endptr;
                errno = 0;
                long value = strtol (optarg, &endptr, 10);

                if (errno == ERANGE || value < 0 || value > UCHAR_MAX
                    || *endptr != '\0')
                {
                    fprintf (stderr, "Invalid TTL value: %s\n", optarg);
                    exit (EXIT_FAILURE);
                }

                g_ping.options.ttl = (uint8_t)value;
                printf ("TTL: %u\n", g_ping.options.ttl);
            }
            break;

            default:
                fprintf (stderr, "%c: not implemented\n", opt);
                exit (EXIT_FAILURE);
        }
    }

    if (optind != argc - 1)
    {
        show_usage ();
        exit (EXIT_FAILURE);
    }

    argv += optind;
    ft_ping_coord (*argv);
    return 0;
}
