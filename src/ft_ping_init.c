#include "ft_ping.h"

void
ping_init_g_info()
{
    g_ping.options.ipv = UNSPEC;
    g_ping.info.read_loop = true;
    g_ping.info.ready_send = true;
    g_ping.stats.timeout_threshold = TIMEOUT;
}

void
ping_socket_init ()
{
    /* SOCK_RAW provides access to internal network protocols and interfaces,
     * which is essential for creating, sending and receiving ICMP packets, only
     * available to users with root-user authority. */

    if ((g_ping.sock_info.sock_fd
         = socket (g_ping.options.ipv == IPV6 ? AF_INET6 : AF_INET, SOCK_RAW,
                   g_ping.options.ipv == IPV6 ? IPPROTO_ICMPV6 : IPPROTO_ICMP))
        == -1)
    {
        perror ("socket");
        exit (EXIT_FAILURE);
    }

    /* setup destination socket address structure for preparing the socket to
     * send ICMPv4/v6 Echo Request messages */

    if (g_ping.options.ipv == IPV6)
    {
        memset (&g_ping.sock_info.addr_6, 0, sizeof (g_ping.sock_info.addr_6));
        g_ping.sock_info.addr_6.sin6_family = AF_INET6;
        g_ping.sock_info.addr_6.sin6_port = htons (0);
        if (inet_pton (AF_INET6, g_ping.sock_info.ip_addr,
                       &g_ping.sock_info.addr_6.sin6_addr)
            != 1)
        {
            perror ("inet_pton");
            release_resources ();
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
            fprintf (stderr, "inet_addr\n");
            release_resources ();
            exit (EXIT_FAILURE);
        }
    }

    /* The purpose of TTL is to prevent packets from circulating indefinitely in
     * case of routing loops. "Time Exceeded" is returned to the user if the
     * value has been decremented to 0 by routers. */

    int hopli = g_ping.options.ttl ? g_ping.options.ttl : 64;

    if (setsockopt (g_ping.sock_info.sock_fd,
                    g_ping.options.ipv == IPV6 ? IPPROTO_IPV6 : IPPROTO_IP,
                    g_ping.options.ipv == IPV6 ? IPV6_UNICAST_HOPS : IP_TTL,
                    &hopli, sizeof (hopli))
        < 0)
    {
        perror ("setsockopt");
        release_resources ();
        exit (EXIT_FAILURE);
    }

    if (g_ping.options.ipv == IPV6)
    {
        int on = 1;
        struct icmp6_filter filter;

        /* Tell which ICMPs we are interested in (very nice). */

        ICMP6_FILTER_SETBLOCKALL (&filter);
        ICMP6_FILTER_SETPASS (ICMP6_ECHO_REPLY, &filter);
        ICMP6_FILTER_SETPASS (ICMP6_DST_UNREACH, &filter);
        ICMP6_FILTER_SETPASS (ICMP6_PACKET_TOO_BIG, &filter);
        ICMP6_FILTER_SETPASS (ICMP6_TIME_EXCEEDED, &filter);
        ICMP6_FILTER_SETPASS (ICMP6_PARAM_PROB, &filter);

        /* configures to receive the Hop Limit value from incoming packets by
         * setting the IPV6_RECVHOPLIMIT socket option. */

        if (setsockopt (g_ping.sock_info.sock_fd, IPPROTO_IPV6,
                        IPV6_RECVHOPLIMIT, &on, sizeof (on))
            < 0)
        {
            perror ("setsockopt");
            release_resources ();
            exit (EXIT_FAILURE);
        }

        /* Applies filtering on ICMPv6 packets, allowing us to remove some
         * message handling complexity on receiving */

        if (setsockopt (g_ping.sock_info.sock_fd, IPPROTO_ICMPV6, ICMP6_FILTER,
                        &filter, sizeof (filter))
            < 0)
        {
            perror ("setsockopt");
            release_resources ();
            exit (EXIT_FAILURE);
        }
    }

    /* Socket options IP_TOS and IPV6_TCLASS are used to set the Type of Service
     * (ToS) and Traffic Class, respectively. These settings determine how
     * routers and network devices handle and prioritize packets as they travel
     * to their destination. */

    int optval = 0x10;

    if (setsockopt (g_ping.sock_info.sock_fd,
                    g_ping.options.ipv == IPV6 ? IPPROTO_IPV6 : IPPROTO_IP,
                    g_ping.options.ipv == IPV6 ? IPV6_TCLASS : IP_TOS, &optval,
                    sizeof (optval))
        < 0)
    {
        perror ("setsockopt");
        release_resources ();
        exit (EXIT_FAILURE);
    }
}