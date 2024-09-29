#include "ft_ping.h"

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

static uint16_t
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

    /* Fold 32-bit sum to 16 bits */
    while (sum >> 16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;
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
    ping_pkt->hdr.checksum
        = compute_checksum (ping_pkt, sizeof (struct ping_packet));
}

static int
verify_checksum (struct ping_packet *ping_pkt)
{
    uint16_t computed_checksum
        = compute_checksum (ping_pkt, sizeof (struct ping_packet));
    return (computed_checksum == ping_pkt->hdr.checksum);
}