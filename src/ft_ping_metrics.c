#include "ft_ping.h"

/**
 * https://datatracker.ietf.org/doc/html/rfc6298
 */

static double
compute_elapsed_ms(struct timespec start, struct timespec end)
{
    double elapsed_sec = (double)(end.tv_sec
                                  - start.tv_sec)
                         * 1000.0;
    double elapsed_nsec;

    if (end.tv_nsec >= start.tv_nsec)
    {
        elapsed_nsec = (double)(end.tv_nsec - start.tv_nsec) / 1e6;
    }
    else
    {
        elapsed_sec -= 1000.0;
        elapsed_nsec = (double)(end.tv_nsec + 1000000000 - start.tv_nsec) / 1e6;
    }

    return elapsed_sec + elapsed_nsec;
}

static void
compute_std_rtt ()
{
    g_ping.rtt_metrics->rtt = compute_elapsed_ms(g_ping.rtt_metrics->start, g_ping.rtt_metrics->end);
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
    double estimated_rtt;
    double alpha = RTT_WEIGHT_FACTOR;

    // 0 but esimtaed rtt is more precise than that. maintenant il se peut quon
    // rentre dedans a nouveau par probleme de precision.
    if (g_ping.stats.estimated_rtt <= 0)
    {
        g_ping.stats.estimated_rtt = g_ping.rtt_metrics->rtt;
        g_ping.stats.dev_rtt = g_ping.rtt_metrics->rtt / 2;
        return;
    }

    estimated_rtt = ((1 - alpha) * g_ping.stats.estimated_rtt)
                    + (alpha * sample_rtt);
    g_ping.stats.estimated_rtt = estimated_rtt;
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
    if (g_ping.rtt_metrics->rtt <= 0 || g_ping.stats.estimated_rtt <= 0)
    {
        return;
    }

    double dev_rtt;
    double sample_rtt = g_ping.rtt_metrics->rtt;
    double estimated_rtt = g_ping.stats.estimated_rtt;
    double beta = RTT_DEVIATION_FACTOR;

    dev_rtt = (1 - beta) * g_ping.stats.dev_rtt
              + beta * fabs (sample_rtt - estimated_rtt);

    g_ping.stats.dev_rtt = dev_rtt;
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

    if (g_ping.stats.dev_rtt <= 0 || g_ping.stats.estimated_rtt <= 0)
    {
        return;
    }

    timeout_interval
        = 4 * g_ping.stats.dev_rtt + g_ping.stats.estimated_rtt;

    g_ping.stats.timeout_threshold = timeout_interval;
}

/**
 * @brief Computes statistics for round-trip times (RTT) in a ping session.
 *
 * This function calculates the minimum, maximum, and average RTT values
 * based on the collected RTT metrics. It also computes the total duration
 * of the ping session in milliseconds and the packet loss percentage.
 *
 * The following steps are performed:
 * - Initialize min, max, and avg RTT values.
 * - Iterate through the RTT metrics linked list to find min, max, and compute
 *   the sum for average RTT.
 * - If RTT metrics are present, calculate the average. Otherwise, set RTT
 *   values to 0.
 * - Calculate the total session time in milliseconds by combining seconds
 *   and nanoseconds.
 * - Compute the packet loss percentage based on the number of sent and
 *   received packets.
 */

void
compute_rtt_stats ()
{
    double sum = 0.0;
    int i = 0;

    g_ping.stats.min = DBL_MAX;
    g_ping.stats.max = DBL_MIN;
    g_ping.stats.avg = 0.0;

    struct s_rtt *tmp = g_ping.rtt_metrics_beg;
    while (tmp != NULL)
    {
        if (tmp->rtt < g_ping.stats.min)
        {
            g_ping.stats.min = tmp->rtt;
        }
        if (tmp->rtt > g_ping.stats.max)
        {
            g_ping.stats.max = tmp->rtt;
        }
        sum += tmp->rtt;
        i++;
        tmp = tmp->next;
    }
    if (i > 0)
    {
        g_ping.stats.avg = sum / i;
    }
    else
    {
        g_ping.stats.min = 0.0;
        g_ping.stats.max = 0.0;
        g_ping.stats.avg = 0.0;
    }
    
    double elapsed_ms = compute_elapsed_ms(g_ping.rtt_metrics_beg->start, g_ping.rtt_metrics->end);

    g_ping.stats.ping_session = elapsed_ms;

    double packet_loss;

    packet_loss = ((double)(g_ping.stats.nb_snd - g_ping.stats.nb_res)
                   / g_ping.stats.nb_snd)
                  * 100;
    g_ping.stats.pkt_loss = packet_loss;
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

    if (rtt == NULL)
    {
        perror ("malloc");
        release_resources ();
        exit (EXIT_FAILURE);
    }

    rtt_lst_add (rtt);
}

_Bool
rtt_timeout ()
{
    struct timespec now;
    clock_gettime (CLOCK_MONOTONIC, &now);
    double elapsed_ms = compute_elapsed_ms(g_ping.rtt_metrics->start, now);
    return elapsed_ms >= g_ping.stats.timeout_threshold;
}

void
start_rtt_metrics ()
{
    /* This allows us to avoid adding a link unnecessarily if the results of the
     * last trip are not measurable. */
    if (g_ping.rtt_metrics == NULL
        || (g_ping.rtt_metrics->end.tv_sec != 0
               && g_ping.rtt_metrics->end.tv_nsec != 0))
    {
        init_rtt_node ();
    }
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

    if (rtt_timeout () == true)
    {
        fprintf (stderr, "ping reached timeout: %f\n",
                 g_ping.stats.timeout_threshold);
        g_ping.info.read_loop = false;
    }
}
