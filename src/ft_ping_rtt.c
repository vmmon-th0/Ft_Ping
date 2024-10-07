#include "ft_ping.h"

static void
compute_std_rtt ()
{
    double rtt_sec = (double)(g_ping.rtt_metrics->end.tv_sec
                              - g_ping.rtt_metrics->start.tv_sec)
                     * 1000.0;
    double rtt_nsec = (double)(g_ping.rtt_metrics->end.tv_nsec
                               - g_ping.rtt_metrics->start.tv_nsec)
                      / 1000000.0;
    g_ping.rtt_metrics->std_rtt = rtt_sec + rtt_nsec;
}

/**
 * @brief Updates the estimated Round Trip Time (RTT) using an exponential
 * weighted average. This function calculates a new RTT estimate by combining
 * the current estimated RTT with the latest RTT measurement. It uses a
 * weighting factor (RTT_WEIGHT_FACTOR) to determine the relative importance of
 * the new measurement compared to the existing estimate. The formula used is:
 *
 * RTT_estimate = (1 - RTT_WEIGHT_FACTOR) * RTT_estimate + RTT_WEIGHT_FACTOR *
 * last_rtt
 *
 * @note The update is performed only if the latest RTT measurement is greater
 * than zero to avoid corrupted stats by packet loss or other issues.
 */

static void
compute_estimated_rtt ()
{
    double rtt_estimate;

    if (g_ping.rtt_metrics->std_rtt <= 0)
    {
        return;
    }

    rtt_estimate = ((1 - RTT_WEIGHT_FACTOR) * g_ping.ping_state.estimated_rtt)
                   + (RTT_WEIGHT_FACTOR * g_ping.rtt_metrics->std_rtt);

    g_ping.ping_state.estimated_rtt = rtt_estimate;
}

// Deviation in RTT

static void
compute_deviation_rtt ()
{
}

// Time-out Interval

static void
compute_timeout_interval_rtt()
{
}

void
release_rtt_resources ()
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

void
start_rtt_metrics ()
{
    /* clock_gettime(CLOCK_MONOTONIC, ...) is preferable to gettimeofday(...)
     * because it assures us stability, precision for Intervals and security
     * against System Manipulation */
    init_rtt_node ();
    clock_gettime (CLOCK_MONOTONIC, &g_ping.rtt_metrics->start);
}

void
end_rtt_metrics ()
{
    clock_gettime (CLOCK_MONOTONIC, &g_ping.rtt_metrics->end);
    compute_std_rtt ();
}
