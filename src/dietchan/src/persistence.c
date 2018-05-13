#include "persistence.h"

#include <time.h>
#include <string.h>
#include <unistd.h>
#include <libowfat/byte.h>
#include <libowfat/str.h>
#include <libowfat/case.h>
#include "util.h"

db_obj *db;
struct master *master;
db_hashmap post_tbl;
db_hashmap ban_tbl;
db_hashmap captcha_tbl;


static void insert_ban_into_hashmap(struct ban *ban);
static void delete_ban_from_hashmap(struct ban *ban);

int db_init(const char *file)
{
	db = db_open(file);
	if (!db)
		return -1;
	master = db_get_master_ptr(db);
	if (!master) {
		begin_transaction();

		master = master_new();
		memset(master, 0, sizeof(master));
		db_invalidate(db, master);
		db_set_master_ptr(db, master);

		db_hashmap_init(&post_tbl, db, 0, uint64_hash, 0, uint64_eq, 0);
		master_set_post_tbl(master, db_hashmap_marshal(&post_tbl));

		db_hashmap_init(&ban_tbl, db, 0, ip_range_hash, 0, ip_range_eq, 0);
		master_set_ban_tbl(master, db_hashmap_marshal(&ban_tbl));

		db_hashmap_init(&captcha_tbl, db, 0, uint64_hash, 0, uint64_eq, 0);
		master_set_captcha_tbl(master, db_hashmap_marshal(&captcha_tbl));

		/* Temporary */
		struct board *board = board_new();
		board_set_name(board, "c");
		board_set_title(board, "Pufferüberlauf");
		board_set_id(board, 1);
		master_set_board_counter(master, 1);

		struct thread *thread = thread_new();
		thread_set_board(thread, board);

		struct post *post = post_new();
		post_set_thread(post, thread);
		post_set_id(post, 1);
		db_hashmap_insert(&post_tbl, &post_id(post), post);
		post_set_subject(post, "");
		post_set_username(post, "Felix");
		post_set_password(post, crypt_password("secret"));
		post_set_text(post, "Hab eine Bilderbrett-Software in C gehackt. Natürlich mit "
		                    "dietlibc, libowfat und selbstgefrickelter Persistierungsschicht.");

		thread_set_first_post(thread, post);
		thread_set_last_post(thread, post);

		board_set_first_thread(board, thread);
		board_set_last_thread(board, thread);

		master_set_first_board(master, board);
		master_set_last_board(master, board);
		master_set_post_counter(master, post_id(post));

		struct user *admin = user_new();
		user_set_id(admin, 1);
		user_set_type(admin, USER_ADMIN);
		user_set_name(admin, "admin");
		user_set_password(admin, crypt_password("admin"));
		user_set_email(admin, "");

		master_set_first_user(master, admin);
		master_set_last_user(master, admin);
		master_set_user_counter(master, 1);

		commit();
	} else {
		db_hashmap_init(&post_tbl, db, db_unmarshal(db, master_post_tbl(master)), uint64_hash, 0, uint64_eq, 0);
		db_hashmap_init(&ban_tbl, db, db_unmarshal(db, master_ban_tbl(master)), ip_range_hash, 0, ip_range_eq, 0);
		db_hashmap_init(&captcha_tbl, db, db_unmarshal(db, master_captcha_tbl(master)), uint64_hash, 0, uint64_eq, 0);
	}
	return 0;
}

char* db_strdup(const char *s)
{
	size_t length = strlen(s)+1;
	char *copy = db_alloc(db, length);
	memcpy(copy, s, length);
	db_invalidate(db, copy);
	return copy;
}

void* db_alloc0(size_t size)
{
	void *rec = db_alloc(db, size);
	if (rec)
		byte_zero(rec, size);
	return rec;
}

void begin_transaction()
{
	db_begin_transaction(db);
}

void commit()
{
	db_commit(db);
}

struct board* find_board_by_name(const char *name)
{
	// Dumb linear search
	struct board *board = master_first_board(master);
	while (board) {
		if (case_equals(name, board_name(board)))
			return board;
		board = board_next_board(board);
	}
	return 0;
}

struct board* find_board_by_id(uint64 id)
{
	// Dumb linear search
	struct board *board = master_first_board(master);
	while (board) {
		if (id == board_id(board))
			return board;
		board = board_next_board(board);
	}
	return 0;
}

void board_free(struct board *o)
{
	db_free(db, db_unmarshal(db, o->name));
	db_free(db, db_unmarshal(db, o->title));
	db_free(db, o);
}

void delete_board(struct board *board)
{
	// Delete all threads of board
	struct thread *thread = board_first_thread(board);
	while (thread) {
		struct thread *next = thread_next_thread(thread);
		delete_thread(thread);
		thread = next;
	}
	// Delete all reports belonging to this board
	struct report *report = master_first_report(master);
	while (report) {
		struct report *next_report = report_next_report(report);
		if (report_board_id(report) && board_id(board))
			delete_report(report);
		report = next_report;
	}
	// Remove board from linked list & free
	struct board *prev = board_prev_board(board);
	struct board *next = board_next_board(board);
	if (prev) board_set_next_board(prev, next);
	if (next) board_set_prev_board(next, prev);
	if (board == master_first_board(master))
		master_set_first_board(master, next);
	if (board == master_last_board(master))
		master_set_last_board(master, prev);
	board_free(board);
}

void thread_free(struct thread *o)
{
	db_free(db, o);
}

struct thread* find_thread_by_id(uint64 id)
{
	struct post *post = find_post_by_id(id);
	if (!post)
		return 0;

	struct thread *thread = post_thread(post);
	if (thread_first_post(thread) != post)
		return 0;
	return thread;
}

void bump_thread(struct thread *thread)
{
	struct board *board = thread_board(thread);

	// Remove thread from linked list
	struct thread *next = thread_next_thread(thread);
	struct thread *prev = thread_prev_thread(thread);
	if (board_first_thread(board) == thread)
		board_set_first_thread(board, next);
	if (board_last_thread(board) == thread)
		board_set_last_thread(board, prev);
	if (next) thread_set_prev_thread(next, prev);
	if (prev) thread_set_next_thread(prev, next);

	// Find insertion position
	prev = 0;
	next = board_first_thread(board);

	while (next && thread_pinned(next) && !thread_pinned(thread)) {
		prev = next;
		next = thread_next_thread(next);
	}

	// Reinsert
	thread_set_next_thread(thread, next);
	if (next) thread_set_prev_thread(next, thread);
	if (!next) board_set_last_thread(board, thread);

	thread_set_prev_thread(thread, prev);
	if (prev) thread_set_next_thread(prev, thread);
	if (!prev) board_set_first_thread(board, thread);

}

void delete_thread(struct thread *thread)
{
	struct board *board = thread_board(thread);
	struct post *post = thread_first_post(thread);
	while (post) {
		struct post *next = post_next_post(post);

		delete_post(post);

		post=next;
	}

	struct thread *prev = thread_prev_thread(thread);
	struct thread *next = thread_next_thread(thread);
	if (prev) thread_set_next_thread(prev, next);
	if (next) thread_set_prev_thread(next, prev);
	if (board_first_thread(board) == thread)
		board_set_first_thread(board, next);
	if (board_last_thread(board) == thread)
		board_set_last_thread(board, prev);

	uint64 thread_count = board_thread_count(board);
	--thread_count;
	board_set_thread_count(board, thread_count);

	thread_free(thread);
}

void upload_free(struct upload *o)
{
	db_free(db, db_unmarshal(db, o->file));
	db_free(db, db_unmarshal(db, o->original_name));
	db_free(db, db_unmarshal(db, o->mime_type));
	db_free(db, o);
}

void upload_delete_files(struct upload *upload)
{
	char *upload_dir = "/srv/http/bbs/uploads/";
	char *file_path = alloca(strlen(upload_dir) + strlen(upload_file(upload)) + 1);
	strcpy(file_path, upload_dir);
	strcat(file_path, upload_file(upload));

	// Todo: Error handling
	unlink(file_path);

	char *thumb_path = alloca(strlen(upload_dir) + strlen(upload_file(upload)) + 1);
	strcpy(thumb_path, upload_dir);
	strcat(thumb_path, upload_thumbnail(upload));

	// Todo: Error handling
	unlink(thumb_path);
}

void report_free(struct report *o)
{
	db_free(db, db_unmarshal(db, o->comment));
}

void delete_report(struct report *report)
{
	struct report *next = report_next_report(report);
	struct report *prev = report_prev_report(report);

	if (master_first_report(master) == report)
		master_set_first_report(master, next);
	if (master_last_report(master) == report)
		master_set_last_report(master, prev);
	if (next)
		report_set_prev_report(next, prev);
	if (prev)
		report_set_next_report(prev, next);

	report_free(report);
}

struct report* find_report_by_id(uint64 id)
{
	struct report *report = master_first_report(master);
	while (report) {
		if (report_id(report) == id)
			return report;
		report = report_next_report(report);
	}
	return 0;
}

void post_free(struct post *o)
{
	db_free(db, db_unmarshal(db, o->username));
	db_free(db, db_unmarshal(db, o->password));
	db_free(db, db_unmarshal(db, o->subject));
	db_free(db, db_unmarshal(db, o->text));
	db_free(db, db_unmarshal(db, o->ban_message));
	db_free(db, db_unmarshal(db, o->x_forwarded_for));

	db_free(db, o);
}

struct post* find_post_by_id(uint64 id)
{
	return db_hashmap_get(&post_tbl, &id);
}

void delete_post(struct post *post)
{
	struct thread *thread = post_thread(post);

	db_hashmap_remove(&post_tbl, &post_id(post));

	struct upload *upload = post_first_upload(post);
	while (upload) {
		struct upload *next_upload = upload_next_upload(upload);
		upload_delete_files(upload);
		upload_free(upload);
		upload = next_upload;
	}

	struct post *prev = post_prev_post(post);
	struct post *next = post_next_post(post);
	if (prev) post_set_next_post(prev, next);
	if (next) post_set_prev_post(next, prev);

	if (thread_last_post(thread) == post)
		thread_set_last_post(thread, prev);

	uint64 post_count = thread_post_count(thread);
	--post_count;
	thread_set_post_count(thread, post_count);

	post_free(post);
}



void user_free(struct user *o)
{
	db_free(db, db_unmarshal(db, o->name));
	db_free(db, db_unmarshal(db, o->password));
	db_free(db, db_unmarshal(db, o->email));
	db_free(db, db_unmarshal(db, o->boards));
	db_free(db, o);
}

struct user* find_user_by_name(const char *name)
{
	// Dumb linear search
	struct user *u = master_first_user(master);
	while (u) {
		const char *n = user_name(u);
		if (case_equals(n, name))
			return u;
		u = user_next_user(u);
	}
	return 0;
}

struct user* find_user_by_id(const uint64 id)
{
	// Dumb linear search
	struct user *u = master_first_user(master);
	while (u) {
		if (user_id(u) == id)
			return u;
		u = user_next_user(u);
	}
	return 0;
}

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

void delete_user(struct user *user)
{
	struct user *next = user_next_user(user);
	struct user *prev = user_prev_user(user);
	if (prev) user_set_next_user(prev, next);
	if (next) user_set_prev_user(next, prev);
	if (user == master_first_user(master))
		master_set_first_user(master, next);
	if (user == master_last_user(master))
		master_set_last_user(master, prev);

	user_free(user);
}


void session_free(struct session *o)
{
	db_free(db, db_unmarshal(db, o->sid));
	db_free(db, o);
}

struct session *find_session_by_sid(const char *sid)
{
	// Dumb linear search
	struct session *session = master_first_session(master);
	while (session) {
		const char *s = session_sid(session);
		if (str_equal(s, sid))
			return session;
		session = session_next_session(session);
	}
	return 0;
}

void session_destroy(struct session *session)
{
	struct session *next_session = session_next_session(session);
	struct session *prev_session = session_prev_session(session);
	if (master_first_session(master) == session)
		master_set_first_session(master, next_session);
	if (next_session)
		session_set_prev_session(next_session, prev_session);
	if (prev_session)
		session_set_next_session(prev_session, next_session);

	session_free(session);
}

struct session* session_update(struct session *session)
{
	if (!session) return 0;

	uint64 t = time(0);
	uint64 last_seen = session_last_seen(session);
	int64 timeout = session_timeout(session);
	if (timeout > 0 && t > last_seen + timeout) {
		// Expired
		begin_transaction();

		session_destroy(session);

		commit();

		return 0;
	} else {
		begin_transaction();

		session_set_last_seen(session, t);

		#if 0
		// FIXME: Update IP
		struct ip ip;
		ip.version = IP_V4; // FIXME: Support IP_V6
		memcpy(&ip.bytes[0], http->ip, 4);
		session_set_last_ip(session, ip);
		#endif

		commit();

		return session;
	}
}

void purge_expired_sessions()
{
	struct session *session = master_first_session(master);
	uint64 t = time(0);
	begin_transaction();
	while (session) {
		uint64 last_seen = session_last_seen(session);
		int64 timeout = session_timeout(session);
		struct session *next = session_next_session(session);

		if (timeout > 0 && t >last_seen+timeout)
			session_destroy(session);

		session = next;
	}
	commit();
}
#if 0
struct ban* ban_add()
{
	uint64 ban_counter = master_ban_counter(master) + 1;
	master_set_ban_counter(master, ban_counter);

	uint64 ban_count = master_ban_count(master) + 1;
	master_set_ban_count(master, ban_count);

	struct ban *bans = master_bans(master);
	bans = db_realloc(db, bans, sizeof(struct ban)*ban_count);

	master_set_bans(master, bans);

	struct ban *ban = &bans[ban_count-1];
	ban_set_idx(ban, ban_count-1);
	ban_set_id(ban, ban_counter);
	ban_set_enabled(ban, 1);

	return ban;
}
#endif

void ban_free(struct ban *ban)
{
	db_free(db, ban_reason(ban));
	db_free(db, ban_boards(ban));
	db_free(db, ban_mod_name(ban));
	db_free(db, ban);
}


static void insert_ban_into_hashmap(struct ban *ban)
{
	struct ban *item = db_hashmap_get(&ban_tbl, &ban_range(ban));
	if (item) {
		db_hashmap_remove(&ban_tbl, &ban_range(item));
		ban_set_prev_in_bucket(item, ban);
		ban_set_next_in_bucket(ban, item);
	}
	db_hashmap_insert(&ban_tbl, &ban_range(ban), ban);

}

static void delete_ban_from_hashmap(struct ban *ban)
{
	// Remove from hashmap
	struct ban *prev = ban_prev_in_bucket(ban);
	struct ban *next = ban_next_in_bucket(ban);
	if (prev)
		ban_set_next_in_bucket(prev, next);
	if (next)
		ban_set_prev_in_bucket(next, prev);

	if (db_hashmap_get(&ban_tbl, &ban_range(ban)) == ban) {
		db_hashmap_remove(&ban_tbl, &ban_range(ban));
		if (next)
			db_hashmap_insert(&ban_tbl, &ban_range(next), next);
	}
}

void insert_ban(struct ban *ban)
{
	// Insert into linked list
	struct ban *prev = master_last_ban(master);
	ban_set_prev_ban(ban, prev);
	if (prev)
		ban_set_next_ban(prev, ban);
	if (!master_first_ban(master))
		master_set_first_ban(master, ban);
	master_set_last_ban(master, ban);

	// Insert into hashmap
	insert_ban_into_hashmap(ban);
}

void update_ban(struct ban *ban)
{
	delete_ban_from_hashmap(ban);
	insert_ban_into_hashmap(ban);
}

void delete_ban(struct ban *ban)
{
	delete_ban_from_hashmap(ban);

	// Remove from linked list
	struct ban *prev = ban_prev_ban(ban);
	struct ban *next = ban_next_ban(ban);
	if (prev)
		ban_set_next_ban(prev, next);
	if (next)
		ban_set_prev_ban(next, prev);
	if (ban == master_first_ban(master))
		master_set_first_ban(master, next);
	if (ban == master_last_ban(master))
		master_set_last_ban(master, prev);

	ban_free(ban);
}

int ban_matches_ip(struct ban *ban, struct ip *ip)
{
	return ip_in_range(&ban_range(ban), ip);
}

int ban_matches_board(struct ban *ban, uint64 board_id)
{
	int64 *boards = ban_boards(ban);

	// Global ban
	if (!boards)
		return 1;

	for (int i=0; boards[i] >= 0; ++i) {
		if (boards[i] == (int64)board_id)
			return 1;
	}
	return 0;
}

void purge_expired_bans()
{
	uint64 now = time(NULL);
	struct ban *ban = master_first_ban(master);
	while (ban) {
		struct ban *next = ban_next_ban(ban);
		if (ban_duration(ban) > 0 && now > ban_timestamp(ban) + ban_duration(ban)) {
			delete_ban(ban);
		}
		ban = next;
	}
}

struct ban* find_ban_by_id(uint64 bid)
{
	for (struct ban *ban = master_first_ban(master); ban; ban=ban_next_ban(ban)) {
		if (ban_id(ban) == bid)
			return ban;
	}
	return 0;
}

void captcha_free(struct captcha *captcha)
{
	db_free(db, captcha_solution(captcha));
	db_free(db, captcha);
}

struct captcha* find_captcha_by_id(uint64 id)
{
	return db_hashmap_get(&captcha_tbl, &id);
}
