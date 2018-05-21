#ifndef PERMISSIONS_H
#define PERMISSIONS_H

#include "persistence.h"

int is_mod_for_board(struct user *user, struct board *board);
int can_see_ban(struct user *user, struct ban *ban);

#endif // PERMISSIONS_H
