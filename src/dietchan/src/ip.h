#ifndef IP_H
#define IP_H

#include <stddef.h>
#include <libowfat/uint32.h>
#include <libowfat/uint64.h>

enum ip_version {
	IP_DUMMY=0,
	IP_V4,
	IP_V6
};

struct ip {
	enum ip_version version;
	unsigned char bytes[16];
};

struct ip_range {
	struct ip ip;
	int32 range;
};

int ip_in_range(const struct ip_range *range, const struct ip *ip);
int ip_range_eq(void *a, void *b, void *extra);
uint64 ip_range_hash(void *range, void *extra);
int is_external_ip(struct ip *ip);

size_t scan_ip(const char *src, struct ip *ip);
size_t fmt_ip(char *dest, struct ip *ip);

#endif // IP_H
