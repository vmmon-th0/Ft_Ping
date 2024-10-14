#include "ft_ping.h"

void
release_resources ()
{
    struct s_rtt *tmp;
    struct s_rtt *next;

    tmp = g_ping.rtt_metrics_beg;

    while (tmp != NULL)
    {
        next = tmp->next;
        free (tmp);
        tmp = next;
    }

    close (g_ping.sock_info.sock_fd);
}