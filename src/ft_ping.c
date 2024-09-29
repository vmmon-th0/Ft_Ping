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

static void
show_usage (void)
{
    printf ("\
Usage: ping [OPTION]... [ADDRESS]...\n\
Options :\n\
-h, --help         display this help and exit\n\
-v, --verbose      verbose output\n");
}

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

/**
 * @brief Checks if the program is run with root rights.
 * Creating raw sockets (SOCK_RAW) for protocols like ICMP (IPPROTO_ICMP)
 * requires elevated privileges
 * @return 1 if the program is executed with root rights, 0 otherwise.
 */

static int
is_running_as_root ()
{
    return geteuid () == 0;
}

static void
show_usage_and_exit (int exit_code)
{
    show_usage ();
    exit (exit_code);
}

int
main (int argc, char *argv[])
{
    int opt;
    int long_index = 0;

    if (!is_running_as_root ())
    {
        fprintf (stderr, "Program needs to be run as root\n");
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
                show_usage_and_exit (EXIT_SUCCESS);

            case 'c':
            {
                char *endptr;
                errno = 0;
                long value = strtol (optarg, &endptr, 10);

                if (errno == ERANGE || value < 0 || value > UINT32_MAX
                    || *endptr != '\0')
                {
                    fprintf (stderr, "Invalid count value: %s\n", optarg);
                    show_usage_and_exit (EXIT_FAILURE);
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
                    show_usage_and_exit (EXIT_FAILURE);
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
                    show_usage_and_exit (EXIT_FAILURE);
                }

                g_ping.options.ttl = (uint8_t)value;
                printf ("TTL: %u\n", g_ping.options.ttl);
            }
            break;

            default:
                fprintf (stderr, "Unknown option: -%c\n", optopt);
                show_usage_and_exit (EXIT_FAILURE);
        }
    }

    if (optind != argc - 1)
    {
        show_usage_and_exit (EXIT_FAILURE);
    }

    argv += optind;
    ft_ping_coordinator (*argv);
    return 0;
}
