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

static void
handle_sigint (int sig)
{
    ping_messages_handler (END);
    exit (EXIT_SUCCESS);
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

    signal (SIGINT, handle_sigint);

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
    ping_coord (*argv);
    return 0;
}
