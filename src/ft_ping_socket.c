#include "ft_ping.h"

void
ping_socket_init ()
{
    /* Socket preparation */

    if ((g_ping.sock_info.sock_fd
         = socket (g_ping.options.ipv == IPV6 ? AF_INET6 : AF_INET, SOCK_RAW,
                   g_ping.options.ipv == IPV6 ? IPPROTO_ICMPV6 : IPPROTO_ICMP))
        == -1)
    {
        perror ("socket");
        exit (EXIT_FAILURE);
    }

    /* Socket options */

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
            exit (EXIT_FAILURE);
        }
    }

    int hopli = g_ping.options.ttl ? g_ping.options.ttl : 64;

    /* Configure the IP_TTL option to define the Time To Live (TTL) of IP
     * packets sent by the socket, using the IP protocol level (IPPROTO_IP). */

    if (setsockopt (g_ping.sock_info.sock_fd,
                    g_ping.options.ipv == IPV6 ? IPPROTO_IPV6 : IPPROTO_IP,
                    g_ping.options.ipv == IPV6 ? IPV6_UNICAST_HOPS : IP_TTL,
                    &hopli, sizeof (hopli))
        < 0)
    {
        perror ("setsockopt");
        exit (EXIT_FAILURE);
    }

    if (g_ping.options.ipv == IPV6)
    {
        int on = 1;

        if (setsockopt (g_ping.sock_info.sock_fd, IPPROTO_IPV6,
                        IPV6_RECVHOPLIMIT, &on, sizeof (on))
            < 0)
        {
            perror ("setsockopt");
            release_resources ();
            exit (EXIT_FAILURE);
        }
    }

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