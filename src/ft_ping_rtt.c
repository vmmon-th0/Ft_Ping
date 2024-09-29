#include "ft_ping.h"

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