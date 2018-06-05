#ifndef DB_H
#define DB_H

#include<libowfat/array.h>
#include<libowfat/uint32.h>
#include<libowfat/uint64.h>

typedef int64 db_ptr;

#define BUCKET_COUNT    56

typedef unsigned char uchar;

typedef struct db_bucket {
	uchar order:7;
	uchar free:1;
	db_ptr next:56;
	db_ptr prev:56;
} db_bucket;

#define MIN_BUCKET_SIZE (sizeof(db_bucket))

typedef struct db_header {
	uint64 version;
	uint64 size;
	uint64 bucket_count;
	db_ptr master_pointer;
	db_ptr buckets[BUCKET_COUNT];
	char _padding[7*8+8];
} db_header;


typedef struct db_obj {
	int   fd;
	int   journal_fd;
	int   changed;
	char *shared_map;
	char *priv_map;
	int   transactions;
	array dirty_regions;

	db_header *header;
	char *bucket0;
} db_obj;

db_obj* db_open(const char *file);
void*   db_alloc(db_obj *db, const uint64 size);
void*   db_realloc(db_obj *db, void *ptr, uint64 new_size);
db_ptr  db_marshal(db_obj *db, const void *ptr);
void*   db_unmarshal(db_obj *db, const db_ptr ptr);
void    db_free(db_obj *db, void *ptr);
void*   db_get_master_ptr(db_obj *db);
void    db_set_master_ptr(db_obj *db, void *ptr);
void    db_begin_transaction(db_obj *db);
void    db_invalidate(db_obj *db, void *ptr);
void    db_invalidate_region(db_obj *db, void *ptr, const uint64 size);
void    db_commit(db_obj *db);

#endif // DB_H
