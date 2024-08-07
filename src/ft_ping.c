#include "ft_ping.h"

#define PACKET_SIZE 64
#define TIMEOUT 1

struct ping_packet
{
    struct icmphdr hdr;
    char data[PACKET_SIZE - sizeof (struct icmphdr)];
};

static char short_options[] = "vh";

static struct option long_options[] = { { "verbose", no_argument, NULL, 'v' },
                                        { "help", no_argument, NULL, 'h' },
                                        { NULL, no_argument, NULL, 0 } };

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
resolve_hostname (const char *hostname, char *ip_addr)
{
    struct addrinfo hints, *res, *p;
    int status;

    memset (&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

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

            if (inet_ntop (p->ai_family, addr, ip_addr, INET_ADDRSTRLEN)
                == NULL)
            {
                perror ("inet_ntop");
                freeaddrinfo (res);
                return -1;
            }

            printf ("IPv4 addr for %s: %s\n", hostname, ip_addr);
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

    /*  Fold 32-bit sum to 16 bits */
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
    ping_pkt->hdr.checksum = compute_checksum (ping_pkt, sizeof (ping_pkt));
}

void
send_icmp (int sock_fd, struct sockaddr_in *addr)
{
    struct ping_packet ping_pkt;

    fill_icmp_packet (&ping_pkt);

    if (sendto (sock_fd, &ping_pkt, sizeof (ping_pkt), 0,
                (struct sockaddr *)addr, sizeof (*addr))
        <= 0)
    {
        perror ("sendto");
        close (sock_fd);
        exit (EXIT_FAILURE);
    }

    printf ("Ping sent to %s\n", inet_ntoa (addr->sin_addr));
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
    char ip_addr[INET_ADDRSTRLEN];

    if (resolve_hostname (hostname, ip_addr) == -1)
    {
        fprintf (stderr, "Failed to resolve hostname\n");
        exit (EXIT_FAILURE);
    }

    /* Socket preparation */

    int sock_fd;
    struct sockaddr_in addr;

    if ((sock_fd = socket (AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1)
    {
        perror ("socket");
        exit (EXIT_FAILURE);
    }

    /* Socket options */

    struct timeval timeout;
    timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;

    memset (&addr, 0, sizeof (addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons (0);
    addr.sin_addr.s_addr = inet_addr (ip_addr);

    uint8_t ttl = 64;

    /* Configure the IP_TTL option to define the Time To Live (TTL) of IP
    packets sent by the socket, using the IP protocol level (IPPROTO_IP). */

    if (setsockopt (sock_fd, IPPROTO_IP, IP_TTL, &ttl, sizeof (ttl)) < 0)
    {
        perror ("Setting socket options failed");
        exit (EXIT_FAILURE);
    }

    send_icmp (sock_fd, &addr);
    close (sock_fd);
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
                printf ("Verbose with argument : %s\n", optarg);
                break;
            case 'h':
                show_usage ();
                exit (EXIT_SUCCESS);
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
