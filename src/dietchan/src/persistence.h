#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include <stdlib.h>

#include "db.h"
#include "db_hashmap.h"
#include "ip.h"

extern db_obj *db;
extern struct master *master;
extern db_hashmap post_tbl;
extern db_hashmap ban_tbl;
extern db_hashmap captcha_tbl;

int   db_init(const char *file);
char* db_strdup(const char *s);
void* db_alloc0(size_t size);
void begin_transaction();
void commit();

#define get_ptr(type, obj, prop)        ((type)db_unmarshal(db, (obj)->prop))
#define set_ptr(type, obj, prop, val)   do {(obj)->prop = db_marshal(db, val); \
                                            db_invalidate_region(db, &((obj)->prop), sizeof(obj->prop)); } while (0)
#define get_val(obj, prop)              ((obj)->prop)
#define set_val(obj, prop, val)         do {(obj)->prop = val; \
                                            db_invalidate_region(db, &((obj)->prop), sizeof(obj->prop)); } while (0)

#define get_str(obj, prop)              get_ptr(char*, obj, prop)
#define set_str(obj, prop, val)         do { \
                                            const char *v=(val); \
                                            if (db_marshal(db, v) == ((obj)->prop)) break; \
                                            if ((obj)->prop) db_free(db, db_unmarshal(db, (obj)->prop)); \
                                            (obj)->prop = db_marshal(db, db_strdup(v)); \
                                            db_invalidate_region(db, &((obj)->prop), sizeof(obj->prop)); \
                                        } while (0)

#define get_flag(obj, flags, flag)      ((obj)->flags & (flag))
#define set_flag(obj, flags, flag, val) do { (obj)->flags = (val)?((obj)->flags | (flag)):((obj)->flags & (~(flag))); \
                                             db_invalidate_region(db, &((obj)->flags), sizeof(obj->flags)); } while (0)

#define db_new(type)                   ((type*)(db_alloc0(sizeof(type))))


struct board;
struct thread;
struct post;
struct report;
struct ban;

struct master {
	              uint64 version;
	/* board* */  db_ptr first_board;
	/* board* */  db_ptr last_board;
	              uint64 board_counter;
	              uint64 post_counter;
	              db_ptr post_tbl;
	              uint64 last_upload;
	              db_ptr ban_tbl;
	/* ban* */    db_ptr first_ban;
	/* ban* */    db_ptr last_ban;
	              uint64 ban_counter;
	/* report* */ db_ptr first_report;
	/* report* */ db_ptr last_report;
	              uint64 report_counter;
	/* user* */   db_ptr first_user;
	/* user* */   db_ptr last_user;
	              uint64 user_counter;
	/*session* */ db_ptr first_session;
	              db_ptr captcha_tbl;
	              uint64 captcha_count;
	/* uint64* */ db_ptr captchas;
};

#define master_new()                    db_new(struct master)
#define master_version(o)               get_val(o, version)
#define master_set_version(o,v)         set_val(o, version, v)
#define master_first_board(o)           get_ptr(struct board*, o, first_board)
#define master_set_first_board(o,v)     set_ptr(struct board*, o, first_board, v)
#define master_last_board(o)            get_ptr(struct board*, o, last_board)
#define master_set_last_board(o,v)      set_ptr(struct board*, o, last_board, v)
#define master_board_counter(o)         get_val(o, board_counter)
#define master_set_board_counter(o,v)   set_val(o, board_counter, v)
#define master_post_counter(o)          get_val(o, post_counter)
#define master_set_post_counter(o,v)    set_val(o, post_counter, v)
#define master_post_tbl(o)              get_val(o, post_tbl)
#define master_set_post_tbl(o,v)        set_val(o, post_tbl, v)
#define master_last_upload(o)           get_val(o, last_upload)
#define master_set_last_upload(o,v)     set_val(o, last_upload, v)
#define master_ban_tbl(o)               get_val(o, ban_tbl)
#define master_set_ban_tbl(o,v)         set_val(o, ban_tbl, v)
#define master_ban_counter(o)           get_val(o, ban_counter)
#define master_set_ban_counter(o,v)     set_val(o, ban_counter, v)
#define master_first_ban(o)             get_ptr(struct ban*, o, first_ban)
#define master_set_first_ban(o,v)       set_ptr(struct ban*, o, first_ban, v)
#define master_last_ban(o)              get_ptr(struct ban*, o, last_ban)
#define master_set_last_ban(o,v)        set_ptr(struct ban*, o, last_ban, v)
#define master_first_report(o)          get_ptr(struct report*, o, first_report)
#define master_set_first_report(o,v)    set_ptr(struct report*, o, first_report, v)
#define master_last_report(o)           get_ptr(struct report*, o, last_report)
#define master_set_last_report(o,v)     set_ptr(struct report*, o, last_report, v)
#define master_report_counter(o)        get_val(o, report_counter)
#define master_set_report_counter(o,v)  set_val(o, report_counter, v)
#define master_first_user(o)            get_ptr(struct user*, o, first_user)
#define master_set_first_user(o,v)      set_ptr(struct user*, o, first_user, v)
#define master_last_user(o)             get_ptr(struct user*, o, last_user)
#define master_set_last_user(o,v)       set_ptr(struct user*, o, last_user, v)
#define master_user_counter(o)          get_val(o, user_counter)
#define master_set_user_counter(o,v)    set_val(o, user_counter, v)
#define master_first_session(o)         get_ptr(struct session*, o, first_session)
#define master_set_first_session(o,v)   set_ptr(struct session*, o, first_session, v)
#define master_captcha_tbl(o)           get_val(o, captcha_tbl)
#define master_set_captcha_tbl(o,v)     set_val(o, captcha_tbl, v)
#define master_captcha_count(o)         get_val(o, captcha_count)
#define master_set_captcha_count(o,v)   set_val(o, captcha_count, v)
#define master_captchas(o)              get_ptr(uint64*, o, captchas)
#define master_set_captchas(o,v)        set_ptr(uint64*, o, captchas, v)


struct board {
	              uint64 id;
	/* char* */   db_ptr name;
	/* char* */   db_ptr title;
	              uint64 thread_count;
	              uint64 max_thread_count;
	/* thread* */ db_ptr first_thread;
	/* thread* */ db_ptr last_thread;
	/* board* */  db_ptr next_board;
	/* board* */  db_ptr prev_board;
};

#define board_new()                     db_new(struct board)
#define board_id(o)                     get_val(o, id)
#define board_set_id(o,v)               set_val(o, id, v)
#define board_name(o)                   get_str(o, name)
#define board_set_name(o,v)             set_str(o, name, v)
#define board_title(o)                  get_str(o, title)
#define board_set_title(o,v)            set_str(o, title, v)
#define board_thread_count(o)           get_val(o, thread_count)
#define board_set_thread_count(o,v)     set_val(o, thread_count, v)
#define board_max_thread_count(o)       get_val(o, max_thread_count)
#define board_set_max_thread_count(o,v) set_val(o, max_thread_count, v)
#define board_first_thread(o)           get_ptr(struct thread*, o, first_thread)
#define board_set_first_thread(o,v)     set_ptr(struct thread*, o, first_thread, v)
#define board_last_thread(o)            get_ptr(struct thread*, o, last_thread)
#define board_set_last_thread(o,v)      set_ptr(struct thread*, o, last_thread, v)
#define board_next_board(o)             get_ptr(struct board*, o, next_board)
#define board_set_next_board(o,v)       set_ptr(struct board*, o, next_board, v)
#define board_prev_board(o)             get_ptr(struct board*, o, prev_board)
#define board_set_prev_board(o,v)       set_ptr(struct board*, o, prev_board, v)
struct board* find_board_by_name(const char *name);
struct board* find_board_by_id(uint64 id);
void board_free(struct board *o);

void delete_board(struct board *board);

enum THREAD_FLAGS {
	THREAD_CLOSED = 1 << 0,
	THREAD_PINNED = 1 << 1,
	THREAD_SAGED  = 1 << 2
};

struct thread {
	/* board* */  db_ptr board;
	/* post* */   db_ptr first_post;
	/* post* */   db_ptr last_post;
	              uint64 post_count;
	              uint64 file_count;
	              uint64 flags;
	/* thread* */ db_ptr next_thread;
	/* thread* */ db_ptr prev_thread;
};

#define thread_new()                    db_new(struct thread)
#define thread_board(o)                 get_ptr(struct board*, o, board)
#define thread_set_board(o,v)           set_ptr(struct board*, o, board, v)
#define thread_first_post(o)            get_ptr(struct post*,  o, first_post)
#define thread_set_first_post(o,v)      set_ptr(struct post*,  o, first_post, v)
#define thread_last_post(o)             get_ptr(struct post*,  o, last_post)
#define thread_set_last_post(o,v)       set_ptr(struct post*,  o, last_post, v)
#define thread_post_count(o)            get_val(o, post_count)
#define thread_set_post_count(o,v)      set_val(o, post_count, v)
#define thread_file_count(o)            get_val(o, file_count)
#define thread_set_file_count(o,v)      set_val(o, file_count, v)
#define thread_flags(o)                 get_val(o, flags)
#define thread_set_flags(o,v)           set_val(o, flags, v)
#define thread_closed(o)                get_flag(o, flags, THREAD_CLOSED)
#define thread_set_closed(o,v)          set_flag(o, flags, THREAD_CLOSED, v)
#define thread_pinned(o)                get_flag(o, flags, THREAD_PINNED)
#define thread_set_pinned(o,v)          set_flag(o, flags, THREAD_PINNED, v)
#define thread_saged(o)                 get_flag(o, flags, THREAD_SAGED)
#define thread_set_saged(o,v)           set_flag(o, flags, THREAD_SAGED, v)
#define thread_next_thread(o)           get_ptr(struct thread*,  o, next_thread)
#define thread_set_next_thread(o,v)     set_ptr(struct thread*,  o, next_thread, v)
#define thread_prev_thread(o)           get_ptr(struct thread*,  o, prev_thread)
#define thread_set_prev_thread(o,v)     set_ptr(struct thread*,  o, prev_thread, v)
void thread_free(struct thread *o);

struct thread* find_thread_by_id(uint64 id);
void bump_thread(struct thread *thread);
void delete_thread(struct thread *thread);

typedef enum upload_state {
	UPLOAD_NORMAL,
	UPLOAD_DELETED
} upload_state;

struct upload {
	/* char* */   db_ptr file;
	/* char* */   db_ptr thumbnail;
	/* char* */   db_ptr original_name;
	/* char* */   db_ptr mime_type;
	uint64               size;
	int                  width;
	int                  height;
	int64                duration; /* milliseconds */
	upload_state         state;

	/* upload* */ db_ptr next_upload;
	/* upload* */ db_ptr prev_upload;
};

#define upload_new()                    db_new(struct upload)
#define upload_file(o)                  get_str(o, file)
#define upload_set_file(o,v)            set_str(o, file, v)
#define upload_thumbnail(o)             get_str(o, thumbnail)
#define upload_set_thumbnail(o,v)       set_str(o, thumbnail, v)
#define upload_original_name(o)         get_str(o, original_name)
#define upload_set_original_name(o,v)   set_str(o, original_name, v)
#define upload_mime_type(o)             get_str(o, mime_type)
#define upload_set_mime_type(o,v)       set_str(o, mime_type, v)
#define upload_size(o)                  get_val(o, size)
#define upload_set_size(o,v)            set_val(o, size, v)
#define upload_width(o)                 get_val(o, width)
#define upload_set_width(o,v)           set_val(o, width, v)
#define upload_height(o)                get_val(o, height)
#define upload_set_height(o,v)          set_val(o, height, v)
#define upload_duration(o)              get_val(o, duration)
#define upload_set_duration(o,v)        set_val(o, duration, v)
#define upload_state(o)                 get_val(o, state)
#define upload_set_state(o,v)           set_val(o, state, v)
#define upload_next_upload(o)           get_ptr(struct upload*, o, next_upload)
#define upload_set_next_upload(o,v)     set_ptr(struct upload*, o, next_upload, v)
#define upload_prev_upload(o)           get_ptr(struct upload*, o, prev_upload)
#define upload_set_prev_upload(o,v)     set_ptr(struct upload*, o, prev_upload, v)
void upload_free(struct upload *o);

void upload_delete_files(struct upload *upload);

enum report_type {
	REPORT_SPAM,
	REPORT_ILLEGAL,
	REPORT_OTHER
};

struct report {
	              uint64 id;
	              uint64 post_id;
	              uint64 thread_id;
	              uint64 board_id;
	              enum report_type type;
	              struct ip reporter_ip;
	              uint64 reporter_uid;
	              uint64 timestamp;
	/* char* */   db_ptr comment;
	/* report* */ db_ptr next_report;
	/* report* */ db_ptr prev_report;
};

#define report_new()                    db_new(struct report)
#define report_id(o)                    get_val(o, id)
#define report_set_id(o,v)              set_val(o, id, v)
#define report_post_id(o)               get_val(o, post_id)
#define report_set_post_id(o,v)         set_val(o, post_id, v)
#define report_thread_id(o)             get_val(o, thread_id)
#define report_set_thread_id(o,v)       set_val(o, thread_id, v)
#define report_board_id(o)              get_val(o, board_id)
#define report_set_board_id(o,v)        set_val(o, board_id, v)
#define report_reporter_ip(o)           get_val(o, reporter_ip)
#define report_set_reporter_ip(o,v)     set_val(o, reporter_ip, v)
#define report_type(o)                  get_val(o, type)
#define report_set_type(o,v)            set_val(o, type, v)
#define report_reporter_ip(o)           get_val(o, reporter_ip)
#define report_set_reporter_ip(o,v)     set_val(o, reporter_ip, v)
#define report_reporter_uid(o)          get_val(o, reporter_uid)
#define report_set_reporter_uid(o,v)    set_val(o, reporter_uid, v)
#define report_timestamp(o)             get_val(o, timestamp)
#define report_set_timestamp(o,v)       set_val(o, timestamp, v)
#define report_comment(o)               get_str(o, comment)
#define report_set_comment(o,v)         set_str(o, comment, v)
#define report_next_report(o)           get_ptr(struct report*, o, next_report)
#define report_set_next_report(o,v)     set_ptr(struct report*, o, next_report, v)
#define report_prev_report(o)           get_ptr(struct report*, o, prev_report)
#define report_set_prev_report(o,v)     set_ptr(struct report*, o, prev_report, v)
void report_free(struct report *o);
void delete_report(struct report *report);
struct report* find_report_by_id(uint64 id);

enum post_flags {
	POST_FLAG_SAGE     = 1<<0,
	POST_FLAG_BANNED   = 1<<1,
	POST_FLAG_REPORTED = 1<<2
};

struct post {
	              uint64 id;
	              struct ip ip;
	              struct ip x_real_ip;
	              uint64 x_forwarded_for_count;
	/* ip* */     db_ptr x_forwarded_for;
	              db_ptr useragent;
	              uint64 user_role;
	/* char* */   db_ptr username;
	/* char* */   db_ptr password;
	/* char* */   db_ptr subject;
	              uint64 timestamp;
	/* char* */   db_ptr text;
	              uint64 flags;
	/* char* */   db_ptr ban_message;

	/* thread* */ db_ptr thread;
	/* upload* */ db_ptr first_upload;
	/* upload* */ db_ptr last_upload;
	/* post* */   db_ptr next_post;
	/* post* */   db_ptr prev_post;
};

#define post_new()                      db_new(struct post)
#define post_id(o)                      get_val(o, id)
#define post_set_id(o,v)                set_val(o, id, v)
#define post_ip(o)                      get_val(o, ip)
#define post_set_ip(o,v)                set_val(o, ip, v)
#define post_x_real_ip(o)               get_val(o, x_real_ip)
#define post_set_x_real_ip(o,v)         set_val(o, x_real_ip, v)
#define post_x_forwarded_for_count(o)   get_val(o, x_forwarded_for_count)
#define post_set_x_forwarded_for_count(o,v) set_val(o, x_forwarded_for_count, v)
#define post_x_forwarded_for(o)         get_ptr(struct ip*, o, x_forwarded_for)
#define post_set_x_forwarded_for(o,v)   set_ptr(struct ip*, o, x_forwarded_for, v)
#define post_useragent(o)               get_str(o, useragent)
#define post_set_useragent(o,v)         set_str(o, useragent, v)
#define post_username(o)                get_str(o, username)
#define post_set_username(o,v)          set_str(o, username, v)
#define post_user_role(o)               get_val(o, user_role)
#define post_set_user_role(o,v)         set_val(o, user_role, v)
#define post_password(o)                get_str(o, password)
#define post_set_password(o,v)          set_str(o, password, v)
#define post_subject(o)                 get_str(o, subject)
#define post_set_subject(o,v)           set_str(o, subject, v)
#define post_timestamp(o)               get_val(o, timestamp)
#define post_set_timestamp(o,v)         set_val(o, timestamp, v)
#define post_text(o)                    get_str(o, text)
#define post_set_text(o,v)              set_str(o, text, v)
#define post_flags(o)                   get_val(o, flags)
#define post_set_flags(o,v)             set_val(o, flags, v)
#define post_sage(o)                    get_flag(o, flags, POST_FLAG_SAGE)
#define post_set_sage(o,v)              set_flag(o, flags, POST_FLAG_SAGE, v)
#define post_banned(o)                  get_flag(o, flags, POST_FLAG_BANNED)
#define post_set_banned(o,v)            set_flag(o, flags, POST_FLAG_BANNED, v)
#define post_reported(o)                get_flag(o, flags, POST_FLAG_REPORTED)
#define post_set_reported(o,v)          set_flag(o, flags, POST_FLAG_REPORTED, v)
#define post_ban_message(o)             get_str(o, ban_message)
#define post_set_ban_message(o,v)       set_str(o, ban_message, v)
#define post_thread(o)                  get_ptr(struct thread*, o, thread)
#define post_set_thread(o,v)            set_ptr(struct thread*, o, thread, v)
#define post_first_upload(o)            get_ptr(struct upload*, o, first_upload)
#define post_set_first_upload(o,v)      set_ptr(struct upload*, o, first_upload, v)
#define post_last_upload(o)             get_ptr(struct upload*, o, last_upload)
#define post_set_last_upload(o,v)       set_ptr(struct upload*, o, last_upload, v)
#define post_next_post(o)               get_ptr(struct post*, o, next_post)
#define post_set_next_post(o,v)         set_ptr(struct post*, o, next_post, v)
#define post_prev_post(o)               get_ptr(struct post*, o, prev_post)
#define post_set_prev_post(o,v)         set_ptr(struct post*, o, prev_post, v)
void post_free(struct post *o);

struct post* find_post_by_id(uint64 id);
void delete_post(struct post *post);

enum user_type {
	USER_REGULAR, // not used
	USER_MOD,
	USER_ADMIN
};

struct user {
	              uint64         id;
	/* char* */   db_ptr         name;
	/* char* */   db_ptr         password;
	/* char* */   db_ptr         email;
	              uchar          type;
	/* int64* */  db_ptr         boards; // Boards for which user is moderator, terminated by -1.
	/* user */    db_ptr         next_user;
	/* user */    db_ptr         prev_user;
};

#define user_new()                      db_new(struct user)
#define user_id(o)                      get_val(o, id)
#define user_set_id(o,v)                set_val(o, id, v)
#define user_name(o)                    get_str(o, name)
#define user_set_name(o,v)              set_str(o, name, v)
#define user_password(o)                get_str(o, password)
#define user_set_password(o,v)          set_str(o, password, v)
#define user_email(o)                   get_str(o, email)
#define user_set_email(o,v)             set_str(o, email, v)
#define user_type(o)                    get_val(o, type)
#define user_set_type(o,v)              set_val(o, type, v)
#define user_boards(o)                  get_ptr(int64*, o, boards)
#define user_set_boards(o,v)            set_ptr(int64*, o, boards, v)
#define user_next_user(o)               get_ptr(struct user*, o, next_user)
#define user_set_next_user(o,v)         set_ptr(struct user*, o, next_user, v)
#define user_prev_user(o)               get_ptr(struct user*, o, prev_user)
#define user_set_prev_user(o,v)         set_ptr(struct user*, o, prev_user, v)
void user_free(struct user *o);

struct user* find_user_by_name(const char *name);
struct user* find_user_by_id(const uint64 id);
void delete_user(struct user *user);

struct session {
	/* char* */   db_ptr         sid;
	              uint64         user;
	              uint64         login_time;
	              uint64         last_seen;
	              int64          timeout;
	              struct ip      login_ip;
	              struct ip      last_ip;
	/*session* */ db_ptr         next_session;
	/*session* */ db_ptr         prev_session;
};

#define session_new()                   db_new(struct session)
#define session_sid(o)                  get_str(o, sid)
#define session_set_sid(o,v)            set_str(o, sid, v)
#define session_user(o)                 get_val(o, user)
#define session_set_user(o,v)           set_val(o, user, v)
#define session_login_time(o)           get_val(o, login_time)
#define session_set_login_time(o,v)     set_val(o, login_time, v)
#define session_last_seen(o)            get_val(o, last_seen)
#define session_set_last_seen(o,v)      set_val(o, last_seen, v)
#define session_timeout(o)              get_val(o, timeout)
#define session_set_timeout(o,v)        set_val(o, timeout, v)
#define session_login_ip(o)             get_val(o, login_ip)
#define session_set_login_ip(o,v)       set_val(o, login_ip, v)
#define session_last_ip(o)              get_val(o, last_ip)
#define session_set_last_ip(o,v)        set_val(o, last_ip, v)
#define session_next_session(o)         get_ptr(struct session*, o, next_session)
#define session_set_next_session(o,v)   set_ptr(struct session*, o, next_session, v)
#define session_prev_session(o)         get_ptr(struct session*, o, prev_session)
#define session_set_prev_session(o,v)   set_ptr(struct session*, o, prev_session, v)
void session_free(struct session *o);

struct session *find_session_by_sid(const char *sid);


enum ban_type {
	BAN_FLOOD,
	BAN_CAPTCHA_ONCE,
	BAN_CAPTCHA_PERMANENT,
	BAN_BLACKLIST,
};

enum ban_target {
	BAN_TARGET_POST,
	BAN_TARGET_REPORT
};

enum ban_flags {
	BAN_FLAG_ENABLED = 1<<0,
	BAN_FLAG_HIDDEN  = 1<<1,
};

struct ban {
                  struct ip_range range;
	              uchar         flags;
	              uchar         type;
	              uchar         target;
	              uint64        id;
	              uint64        timestamp;
	              int64         duration;
	              uint64        post;
	/* int64* */  db_ptr        boards; // terminated by -1
	/* char* */   db_ptr        reason;
	              uint64        mod;
	/* char* */   db_ptr        mod_name;
	/* ban* */    db_ptr        next_ban;
	/* ban* */    db_ptr        prev_ban;
	/* ban* */    db_ptr        next_in_bucket;
	/* ban* */    db_ptr        prev_in_bucket;
};

#define ban_new()                       db_new(struct ban)
#define ban_range(o)                    get_val(o, range)
#define ban_set_range(o,v)              set_val(o, range, v)
#define ban_flags(o)                    get_val(o, flags)
#define ban_set_flags(o,v)              set_val(o, flags, v)
#define ban_enabled(o)                  get_flag(o, flags, BAN_FLAG_ENABLED)
#define ban_set_enabled(o,v)            set_flag(o, flags, BAN_FLAG_ENABLED, v)
#define ban_hidden(o)                   get_flag(o, flags, BAN_FLAG_HIDDEN)
#define ban_set_hidden(o,v)             set_flag(o, flags, BAN_FLAG_HIDDEN, v)
#define ban_type(o)                     get_val(o, type)
#define ban_set_type(o,v)               set_val(o, type, v)
#define ban_target(o)                   get_val(o, target)
#define ban_set_target(o,v)             set_val(o, target, v)
#define ban_id(o)                       get_val(o, id)
#define ban_set_id(o,v)                 set_val(o, id, v)
#define ban_timestamp(o)                get_val(o, timestamp)
#define ban_set_timestamp(o,v)          set_val(o, timestamp, v)
#define ban_duration(o)                 get_val(o, duration)
#define ban_set_duration(o,v)           set_val(o, duration, v)
#define ban_post(o)                     get_val(o, post)
#define ban_set_post(o,v)               set_val(o, post, v)
#define ban_boards(o)                   get_ptr(int64*, o, boards)
#define ban_set_boards(o,v)             set_ptr(int64*, o, boards, v)
#define ban_reason(o)                   get_str(o, reason)
#define ban_set_reason(o,v)             set_str(o, reason, v)
#define ban_mod(o)                      get_val(o, mod)
#define ban_set_mod(o,v)                set_val(o, mod, v)
#define ban_mod_name(o)                 get_str(o, mod_name)
#define ban_set_mod_name(o,v)           set_str(o, mod_name, v)
#define ban_next_ban(o)                 get_ptr(struct ban*, o, next_ban)
#define ban_set_next_ban(o,v)           set_ptr(struct ban*, o, next_ban, v)
#define ban_prev_ban(o)                 get_ptr(struct ban*, o, prev_ban)
#define ban_set_prev_ban(o,v)           set_ptr(struct ban*, o, prev_ban, v)
#define ban_next_in_bucket(o)           get_ptr(struct ban*, o, next_in_bucket)
#define ban_set_next_in_bucket(o,v)     set_ptr(struct ban*, o, next_in_bucket, v)
#define ban_prev_in_bucket(o)           get_ptr(struct ban*, o, prev_in_bucket)
#define ban_set_prev_in_bucket(o,v)     set_ptr(struct ban*, o, prev_in_bucket, v)
void ban_free(struct ban *ban);

void insert_ban(struct ban *ban);
void update_ban(struct ban *ban);
void delete_ban(struct ban *ban);
struct ban* find_ban_by_id(uint64 bid);


struct captcha {
	              uint64        id;
	              uint64        idx;
	              uint64        token;
	              uint64        timestamp;
	/* char* */   db_ptr        solution;
};
#define captcha_new()                   db_new(struct captcha)
#define captcha_id(o)                   get_val(o, id)
#define captcha_set_id(o,v)             set_val(o, id, v)
#define captcha_idx(o)                  get_val(o, idx)
#define captcha_set_idx(o,v)            set_val(o, idx, v)
#define captcha_token(o)                get_val(o, token)
#define captcha_set_token(o,v)          set_val(o, token, v)
#define captcha_timestamp(o)            get_val(o, timestamp)
#define captcha_set_timestamp(o,v)      set_val(o, timestamp, v)
#define captcha_solution(o)             get_str(o, solution)
#define captcha_set_solution(o,v)       set_str(o, solution, v)
void captcha_free(struct captcha *captcha);

struct captcha* find_captcha_by_id(uint64 id);

#endif // PERSISTENCE_H
