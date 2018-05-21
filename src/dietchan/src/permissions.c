#include "permissions.h"

int is_mod_for_board(struct user *user, struct board *board)
{
	if (user && (user_type(user) == USER_MOD || user_type(user) == USER_ADMIN)) {
		uint64 *boards = user_boards(user);
		if (!boards) // Global mod
			return 1;

		uint64 bid = board_id(board);
		uint64 *b = &boards[0];
		while (*b != -1) {
			if (*b == bid)
				return 1;
			++b;
		}
	}
	return 0;
}

int can_see_ban(struct user *user, struct ban *ban)
{
	if (!ban) return 1;
	if (!user) return 0;
	if (user_type(user) == USER_ADMIN)
		return 1;
	if (user_type(user) == USER_MOD) {
		if (!ban_boards(ban) || !user_boards(user))
			return 1;

		for (uint64 *bb = ban_boards(ban); *bb != -1; ++bb) {
			// Ignore boards that no longer exist
			if (!find_board_by_id(*bb))
				continue;
			for (uint64 *ub = user_boards(user); *ub != -1; ++ub) {
				if (*ub == *bb)
					return 1;
			}
		}
	}
	return 0;
}
