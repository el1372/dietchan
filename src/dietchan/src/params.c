#include "params.h"

#include <libowfat/scan.h>

#include "print.h"
#include "persistence.h"
#include "util.h"

size_t parse_boards(http_context *http, char *s, array *boards, int *ok)
{
	int i = 0;
	size_t boards_count = 0;
	*ok = 1;
	while (1) {
		i += scan_whiteskip(&s[i]);
		int d = scan_nonwhiteskip(&s[i]);
		if (!d) break;

		char tmp = s[i+d];
		s[i+d] = '\0';
		struct board *board = find_board_by_name(&s[i]);
		if (!board) {
			if (http)
				PRINT(S("<p class='error'>Brett existiert nicht: /"), E(&s[i]), S("/</p>"));
			*ok = 0;
			i += d;
			continue;
		}
		s[i+d] = tmp;
		i += d;
		int dup = 0;
		for (int j=0; j<boards_count; ++j) {
			struct board **member = array_get(boards, sizeof(struct board*), j);
			if (*member == board) {
				dup = 1;
				break;
			}
		}
		if (!dup) {
			struct board **member = array_allocate(boards, sizeof(struct board*), boards_count);
			*member = board;
			++boards_count;
		}
	}

	return boards_count;
}

void parse_x_forwarded_for(array *ips, const char *header)
{
	size_t offset = 0;
	struct ip ip;
	while (1) {
		offset += scan_whiteskip(&header[offset]);
		size_t delta=0;
		delta = scan_ip(&header[offset], &ip);
		if (!delta)
			break;
		offset += delta;
		offset += scan_whiteskip(&header[offset]);
		if (header[offset] == ',')
			++offset;

		size_t count = array_length(ips, sizeof(struct ip));
		struct ip *member = array_allocate(ips, sizeof(struct ip), count);
		*member = ip;
	}
}
