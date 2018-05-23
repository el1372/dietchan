#ifndef BANS_H
#define BANS_H

#include <libowfat/uint64.h>
#include "persistence.h"

int ban_matches_ip(struct ban *ban, struct ip *ip);
int ban_matches_board(struct ban *ban, uint64 board_id);

typedef void (*find_bans_callback)(struct ban *ban, struct ip *ip, void *extra);
void find_bans(struct ip *ip, find_bans_callback callback, void *extra);

int64 is_banned(struct ip *ip, struct board *board, enum ban_target target);
int64 is_flood_limited(struct ip *ip, struct board *board, enum ban_target target);
int64 is_captcha_required(struct ip *ip, struct board *board, enum ban_target target);

int64 any_ip_affected(struct ip *ip, struct ip *x_real_ip, array *x_forwarded_for,
                      struct board *board, enum ban_target target,
                      int64 (*predicate)(struct ip *ip, struct board *board, enum ban_target target));

void purge_expired_bans();

#endif // BANS_H
