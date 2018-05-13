#include "ip.h"

#include <libowfat/byte.h>
#include <libowfat/ip4.h>
#include <libowfat/ip6.h>

int ip_in_range(const struct ip_range *range, const struct ip *ip)
{
	if (ip->version != range->ip.version)
		return 0;

	unsigned char mask;

	for (int i=0; i < range->range/8 + 1; ++i) {
		if (i*8 + 8 < range->range)
			mask = 0xFF;
		else
			mask = 0xFF << (8 - (range->range-i*8));

		if ((ip->bytes[i] & mask) != (range->ip.bytes[i] & mask))
			return 0;
	}

	return 1;
}

void normalize_ip_range(struct ip_range *range)
{
	int n = 0;
	if (range->ip.version == IP_V4)
		n = 4;
	else if (range->ip.version == IP_V6)
		n = 16;

	unsigned char mask[16];
	int i=0;
	for (; i<range->range/8; )
		mask[i++] = 0xFF;
	if (i<n)
		mask[i++] = 0xFF << (8 - (range->range-i*8));
	for (; i<n; )
		mask[i++] = 0x00;

	for (i=0; i<n; ++i)
		range->ip.bytes[i] &= mask[i];
}

const struct ip_range LOCAL_RANGES[] = {
	{{IP_V4, {127}},         8},
	{{IP_V4, {10}},          8},
	{{IP_V4, {172,16}},     12},
	{{IP_V4, {192,168}},    16},
	{{IP_V6, {0}},          96},
	{{IP_V6, {0xfe, 0xc0}}, 10},
	{{IP_V6, {0xfc}},        8},
	{0}
};

int is_external_ip(struct ip *ip)
{
	if (ip->version != IP_V4 &&
	    ip->version != IP_V6)
	    return 0;
	int i = 0;
	while (1) {
		const struct ip_range *range = &LOCAL_RANGES[i];
		if (!range->ip.version) break;
		if (ip_in_range(range, ip))
			return 0;
		++i;
	}
	return 1;
}

uint64 ip_range_hash(void *key, void *extra)
{
	struct ip_range *range = (struct ip_range*)key;
	struct ip_range nrange = *range;
	normalize_ip_range(&nrange);
	uint64 hash = nrange.ip.version;
	hash = hash*31 + nrange.range;
	switch (nrange.ip.version) {
		case IP_V4:
			for (int i=0; i<4; ++i)
				hash = hash*31 + nrange.ip.bytes[i];
			break;
		case IP_V6:
			for (int i=0; i<16; ++i)
				hash = hash*31 + nrange.ip.bytes[i];
			break;
	}
	return hash;
}

int ip_range_eq(void *a, void *b, void *extra)
{
	struct ip_range *_a = (struct ip_range*)a;
	struct ip_range *_b = (struct ip_range*)b;
	struct ip_range na = *_a;
	normalize_ip_range(&na);
	struct ip_range nb = *_b;
	normalize_ip_range(&nb);
	return (na.range == nb.range &&
	        na.ip.version == nb.ip.version &&
	        byte_equal(na.ip.bytes, (na.ip.version==IP_V6)?16:4, nb.ip.bytes));
}

size_t scan_ip(const char *src, struct ip *ip)
{
	size_t consumed = 0;
	if (consumed = scan_ip4(src, &ip->bytes[0])) {
		ip->version = IP_V4;
	} else if (consumed = scan_ip6(src, &ip->bytes[0])) {
		ip->version = IP_V6;
	}
	return consumed;
}

size_t fmt_ip(char *dest, struct ip *ip)
{
	switch (ip->version) {
	case IP_V4: return fmt_ip4(dest, &ip->bytes[0]);
	case IP_V6: return fmt_ip6(dest, &ip->bytes[0]);
	default:    return 0;
	}
}
