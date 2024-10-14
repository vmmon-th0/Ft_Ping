#include "ft_ping.h"

/**
 * https://datatracker.ietf.org/doc/html/rfc6298
 */

static void
compute_std_rtt ()
{
    double rtt_sec = (double)(g_ping.rtt_metrics->end.tv_sec
                              - g_ping.rtt_metrics->start.tv_sec)
                     * 1000.0;
    double rtt_nsec = (double)(g_ping.rtt_metrics->end.tv_nsec
                               - g_ping.rtt_metrics->start.tv_nsec)
                      / 1000000.0;
    g_ping.rtt_metrics->rtt = rtt_sec + rtt_nsec;
}

/**
 * @brief Updates the estimated Round Trip Time (RTT) using an exponential
 * weighted average. This function calculates a new RTT estimate by combining
 * the current estimated RTT with the latest RTT measurement. It uses a
 * weighting factor (RTT_WEIGHT_FACTOR) to determine the relative importance of
 * the new measurement compared to the existing estimate. The formula used is:
 *
 * Estimated RTT = (1- α) * Estimated RTT + α * Sample RTT
 */

static void
compute_estimated_rtt ()
{
    if (g_ping.rtt_metrics->rtt <= 0)
    {
        return;
    }

    double sample_rtt = g_ping.rtt_metrics->rtt;
    double estimated_rtt = g_ping.ping_stats.estimated_rtt;
    double alpha = RTT_WEIGHT_FACTOR;
    if (g_ping.ping_stats.estimated_rtt <= 0)
    {
        g_ping.ping_stats.estimated_rtt = g_ping.rtt_metrics->rtt;
        g_ping.ping_stats.dev_rtt = g_ping.rtt_metrics->rtt / 2;
        return;
    }
    estimated_rtt = ((1 - alpha) * estimated_rtt) + (alpha * sample_rtt);

    g_ping.ping_stats.estimated_rtt = estimated_rtt;
}

/**
 * @brief Updates the RTT deviation using an exponential weighted average.
 * This function computes the deviation of the RTT estimates by combining
 * the current deviation with the latest RTT measurement and the estimated RTT.
 * It uses a weighting factor (RTT_DEVIATION_FACTOR) to adjust the impact of the
 * in other words this is the average calculation of the absolute deviations
 * between each measured RTT and the average RTT. It reflects the instability or
 * fluctuations of the network. new measurement on the deviation estimate. in
 * other words  The formula used is:
 *
 * DevRTT = (1- β) * DevRTT + β * |Sample RTT - Estimated RTT|
 */

static void
compute_deviation_rtt ()
{
    if (g_ping.rtt_metrics->rtt <= 0 || g_ping.ping_stats.estimated_rtt <= 0)
    {
        return;
    }

    double dev_rtt;
    double sample_rtt = g_ping.rtt_metrics->rtt;
    double estimated_rtt = g_ping.ping_stats.estimated_rtt;
    double beta = RTT_DEVIATION_FACTOR;

    dev_rtt = (1 - beta) * g_ping.ping_stats.dev_rtt
              + beta * fabs (sample_rtt - estimated_rtt);

    g_ping.ping_stats.dev_rtt = dev_rtt;
}

/**
 * @brief Calculates the timeout interval for RTT based on the estimated RTT and
 * deviation. This function computes the timeout interval using the formula:
 *
 * timeout_interval = 4 * dev_rtt + estimated_rtt
 *
 * The calculated timeout interval determines how long to wait for a response
 * before considering a packet lost.
 */

static void
compute_timeout_interval_rtt ()
{
    double timeout_interval;

    if (g_ping.ping_stats.dev_rtt <= 0 || g_ping.ping_stats.estimated_rtt <= 0)
    {
        return;
    }

    timeout_interval
        = 4 * g_ping.ping_stats.dev_rtt + g_ping.ping_stats.estimated_rtt;

    g_ping.ping_stats.timeout_threshold = timeout_interval;
}

void
compute_rtt_stats ()
{
    double sum = 0.0;
    int i = 0;

    g_ping.ping_stats.min = DBL_MAX;
    g_ping.ping_stats.max = DBL_MIN;
    g_ping.ping_stats.avg = 0.0;

    struct s_rtt *tmp = g_ping.rtt_metrics_beg;
    while (tmp != NULL)
    {
        if (tmp->rtt < g_ping.ping_stats.min)
        {
            g_ping.ping_stats.min = tmp->rtt;
        }
        if (tmp->rtt > g_ping.ping_stats.max)
        {
            g_ping.ping_stats.max = tmp->rtt;
        }
        sum += tmp->rtt;
        i++;
        tmp = tmp->next;
    }
    if (i > 0)
    {
        g_ping.ping_stats.avg = sum / i;
    }
    else
    {
        g_ping.ping_stats.min = 0.0;
        g_ping.ping_stats.max = 0.0;
        g_ping.ping_stats.avg = 0.0;
    }

    double elapsed_sec = (double)(g_ping.rtt_metrics->end.tv_sec
                                  - g_ping.rtt_metrics_beg->start.tv_sec)
                         * 1000.0;
    double elapsed_nsec = (double)(g_ping.rtt_metrics->end.tv_nsec
                                   - g_ping.rtt_metrics_beg->start.tv_nsec)
                          / 1000000.0;
    double elapsed_ms = elapsed_sec + elapsed_nsec;

    g_ping.ping_stats.ping_session = elapsed_ms;

    double packet_loss;

    packet_loss = ((double)(g_ping.ping_stats.nb_snd - g_ping.ping_stats.nb_res)
                   / g_ping.ping_stats.nb_snd)
                  * 100;
    g_ping.ping_stats.pkt_loss = packet_loss;
}

static void
rtt_lst_add (struct s_rtt *rtt)
{
    if (g_ping.rtt_metrics == NULL)
    {
        g_ping.rtt_metrics = rtt;
        g_ping.rtt_metrics_beg = g_ping.rtt_metrics;
    }
    else
    {
        g_ping.rtt_metrics->next = rtt;
        g_ping.rtt_metrics = rtt;
    }
}

static void
init_rtt_node ()
{
    struct s_rtt *rtt;

    rtt = malloc (sizeof (struct s_rtt));

    memset (rtt, 0, sizeof (struct s_rtt));

    // Test to see if fprintf + strerror is better than perror

    if (rtt == NULL)
    {
        fprintf (stderr, "Error: %s \n", strerror (errno));
        exit (EXIT_FAILURE);
    }

    rtt_lst_add (rtt);
}

_Bool
rtt_timeout ()
{
    struct timespec now;
    clock_gettime (CLOCK_MONOTONIC, &now);

    double elapsed_sec
        = (double)(now.tv_sec - g_ping.rtt_metrics->start.tv_sec) * 1000.0;
    double elapsed_nsec
        = (double)(now.tv_nsec - g_ping.rtt_metrics->start.tv_nsec) / 1000000.0;
    double elapsed_ms = elapsed_sec + elapsed_nsec;

    // PING_DEBUG ("ping deviation: %f\n", g_ping.ping_stats.dev_rtt);
    // PING_DEBUG ("ping estimated_rtt: %f\n", g_ping.ping_stats.estimated_rtt);
    // PING_DEBUG ("ping last rtt: %f\n", g_ping.rtt_metrics->rtt);
    // PING_DEBUG ("ping elapsed time now - start: %f\n", elapsed_ms);
    // PING_DEBUG ("ping reached timeout threshold: %f\n",
    //             g_ping.ping_stats.timeout_threshold);

    return elapsed_ms > g_ping.ping_stats.timeout_threshold;
}

void
start_rtt_metrics ()
{
    init_rtt_node ();
    /* clock_gettime(CLOCK_MONOTONIC, ...) is preferable to gettimeofday(...)
     * because it assures us stability, precision for Intervals and security
     * against System Manipulation */
    clock_gettime (CLOCK_MONOTONIC, &g_ping.rtt_metrics->start);
}

void
end_rtt_metrics ()
{
    clock_gettime (CLOCK_MONOTONIC, &g_ping.rtt_metrics->end);
    compute_std_rtt ();
    compute_estimated_rtt ();
    compute_deviation_rtt ();
    compute_timeout_interval_rtt ();
}