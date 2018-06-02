#define _GNU_SOURCE
#include "db.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <libowfat/uint32.h>
#include <libowfat/byte.h>
#include <libowfat/open.h>
#include <libowfat/io.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/file.h>

typedef uint32 journal_entry_type;

typedef enum db_region_boundary_type {
	DB_REGION_START,
	DB_REGION_END
} db_region_boundary_type;

typedef struct db_region_boundary {
	db_region_boundary_type type:1;
	db_ptr position:63;
} db_region_boundary;

static int region_boundary_comp(const void *_a, const void *_b)
{
	const db_region_boundary *a = _a;
	const db_region_boundary *b = _b;
	if (a->position < b->position)
		return -1;
	if (a->position > b->position)
		return +1;

	if (a->type == DB_REGION_START && b->type == DB_REGION_END)
		return -1;

	if (a->type == DB_REGION_END   && b->type == DB_REGION_START)
		return -1;

	return 0;
}

#define JOURNAL_WRITE  0
#define JOURNAL_COMMIT 1

static int db_check_journal(db_obj *db);
static void db_replay_journal(db_obj *db);
static void db_init(db_obj *db);
static void db_grow(db_obj *db);

//#define MAP_SIZE 1024UL*1024UL*1024UL // 1 GB... use this for valgrind
#define MAP_SIZE ((sizeof(long)==8)? \
                  (1024UL*1024UL*1024UL*1024UL):   /* 1 TB... ought to be enough for anyone */  \
                  (256UL*1024UL*1024UL))           /* 256 MB... use this for 32 bit systems */

db_obj* db_open(const char *file)
{
	db_obj *db = malloc(sizeof(db_obj));
	byte_zero(db, sizeof(db));

	size_t journal_path_length = strlen(file) + strlen(".journal") +1;
	char *journal = alloca(journal_path_length);
	byte_zero(journal, journal_path_length);
	strcat(journal, file);
	strcat(journal, ".journal");

	db->fd = open_rw(file);
	if (db->fd < 0) {
		perror("Could not open database");
		goto fail;
	}
	if (lockf(db->fd, F_TLOCK, 0) < 0) {
		perror("Could not lock database");
		goto fail;
	}
	io_closeonexec(db->fd);
	db->journal_fd = open_rw(journal);
	io_closeonexec(db->journal_fd);

	db->shared_map = mmap(0, MAP_SIZE, PROT_WRITE, MAP_SHARED | MAP_NORESERVE, db->fd, 0);
	if (db->shared_map == 0) {
		perror("mmap (shared) is NULL");
		goto fail;
	}
	if (db->shared_map == (char*)-1) {
		perror("mmap (shared) failed");
		goto fail;
	}
	db->priv_map   = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_NORESERVE, db->fd, 0);
	if (db->priv_map == 0) {
		perror("mmap is NULL");
		goto fail;
	}
	if (db->priv_map == (char*)-1) {
		perror("mmap (private) failed");
		goto fail;
	}

	db->header = (db_header*)db->priv_map;
	db->bucket0 = (char*)db->header + sizeof(db_header);

	if (db_check_journal(db) == 1) {
		db_replay_journal(db);
	} else {
		off_t size = lseek(db->fd, 0, SEEK_END);
		if (size == 0) {
			db_init(db);
		}
	}

	return db;

fail:
	free(db);
	return 0;
}

static db_bucket* get_bucket(db_obj *db, int index)
{
	return (db_bucket*)(db_unmarshal(db, db->header->buckets[index]));
}

static uint64 bucket_size(int i)
{
	uint64 tmp = (uint64)MIN_BUCKET_SIZE*((uint64)1 << (uint64)i);
	return tmp;
}

static uint64 net_bucket_size(int i)
{
	return bucket_size(i) - sizeof(uchar);
}

static int get_bucket_for_size(db_obj *db, uint64 size)
{
	for (int i=BUCKET_COUNT-1; i>=0; --i) {
		uint64 s = net_bucket_size(i);
		if (i==0 || net_bucket_size(i-1) < size)
			return i;
	}
}

static void extract_bucket(db_obj *db, db_bucket *bucket)
{
	assert(bucket->free);

	db_bucket *bucket_prev = db_unmarshal(db, bucket->prev);
	db_bucket *bucket_next = db_unmarshal(db, bucket->next);
	if (bucket_prev)
		bucket_prev->next = db_marshal(db, bucket_next);
	if (bucket_next)
		bucket_next->prev = db_marshal(db, bucket_prev);
	if (get_bucket(db, bucket->order) == bucket) {
		assert(bucket_prev == 0);
		db->header->buckets[bucket->order] = db_marshal(db, bucket_next);
	}

	bucket->prev = 0;
	bucket->next = 0;

	db_invalidate_region(db, bucket, sizeof(db_bucket));
	db_invalidate_region(db, bucket_prev, sizeof(db_bucket));
	db_invalidate_region(db, bucket_next, sizeof(db_bucket));
	db_invalidate_region(db, &(db->header->buckets[bucket->order]), sizeof(db_ptr));
}

static void insert_bucket(db_obj *db, db_bucket *bucket)
{
	assert(bucket->free);

	db_bucket *bucket_next = get_bucket(db, bucket->order);
	db_bucket *bucket_prev = bucket_next?db_unmarshal(db, bucket_next->prev):0;
	assert (bucket_prev == 0);
	bucket->next = db_marshal(db, bucket_next);
	bucket->prev = 0;
	if (bucket_next)
		bucket_next->prev = db_marshal(db, bucket);
	db->header->buckets[bucket->order] = db_marshal(db, bucket);

	db_invalidate_region(db, bucket, sizeof(db_bucket));
	db_invalidate_region(db, bucket_prev, sizeof(db_bucket));
	db_invalidate_region(db, bucket_next, sizeof(db_bucket));
	db_invalidate_region(db, &(db->header->buckets[bucket->order]), sizeof(db_ptr));
}

static void split_bucket(db_obj *db, db_bucket *bucket)
{
	assert(bucket->free);
	assert(bucket->order > 0);

	extract_bucket(db, bucket);

	--bucket->order;

	db_bucket *buddy = (db_bucket*)((char*)(bucket) + bucket_size(bucket->order));
	buddy->free = 1;
	buddy->order = bucket->order;

	insert_bucket(db, bucket);
	insert_bucket(db, buddy);

	db_invalidate_region(db, buddy, sizeof(db_bucket));
	db_invalidate_region(db, bucket, sizeof(db_bucket));
	db_invalidate_region(db, &(db->header->buckets[bucket->order]), sizeof(db_ptr));
}

static uint64 get_position_of_bucket(db_obj *db, db_bucket *bucket)
{
	return ((char*)bucket - db->bucket0)/MIN_BUCKET_SIZE;
}

static db_bucket* get_bucket_at_position(db_obj *db, uint64 pos)
{
	if (sizeof(db_header)+pos*MIN_BUCKET_SIZE >= db->header->size)
		return 0;
	return (db_bucket*)(db->bucket0 + pos*MIN_BUCKET_SIZE);
}


void* db_alloc(db_obj *db, const uint64 size)
{
	int i = get_bucket_for_size(db, size);

	// Grow database?
	while (i>=db->header->bucket_count)
		db_grow(db);

	db_bucket *bucket = get_bucket(db, i);

	if (bucket != 0) {
		assert(bucket->free);
		// Great, we have a bucket of the correct size
	} else {
		// We have to find the next largest bucket and split it
		int j = i+1;
		while ((bucket = get_bucket(db, j)) == 0) {
			++j;
			// Grow database?
			while (j>=db->header->bucket_count) {
				db_grow(db);
				--j;
			}
		}
		assert(bucket->free);
		while (j>i) {
			assert(bucket->free);
			split_bucket(db, bucket);
			--j;
		}
	}

	assert(bucket->free);

	extract_bucket(db, bucket);
	bucket->free = 0;


	#if 0
	memset((char*)bucket + sizeof(uchar), 0x88, net_bucket_size(bucket->order));
	#endif

	return ((char*)bucket + sizeof(uchar));
}

void* db_realloc(db_obj *db, void *ptr, uint64 new_size)
{
	if (!ptr)
		return db_alloc(db, new_size);

	db_bucket *bucket = (db_bucket*)((char*)ptr - sizeof(uchar));
	uint64 bucket_size = net_bucket_size(bucket->order);
	if (bucket_size >= new_size)
		return ptr;

	// Todo: We could optimize here by merging if buddy is free.
	// Right now we just always allocate a new buffer and copy the data.

	void *new_ptr = db_alloc(db, new_size);
	memcpy(new_ptr, ptr, bucket_size);

	db_free(db, ptr);
	db_invalidate_region(db, new_ptr, bucket_size);

	return new_ptr;
}

db_ptr db_marshal(db_obj *db, const void *ptr)
{
	return ptr?((db_ptr)ptr - (db_ptr)db->priv_map):0;
}

void* db_unmarshal(db_obj *db, const db_ptr ptr)
{
	return ptr?(void*)((ptr + (db_ptr)db->priv_map)):0;
}

static void merge_buckets(db_obj *db, db_bucket *bucket)
{
	while(1) {
		uint64 pos       = get_position_of_bucket(db, bucket);
		uint64 buddy_pos = pos ^ (1 << bucket->order);

		db_bucket *buddy = get_bucket_at_position(db, buddy_pos);
		if (!buddy)
			break;

		assert(bucket->free);

		if (!buddy->free || buddy->order != bucket->order)
			break;

		assert(buddy->order == bucket->order);

		extract_bucket(db, bucket);
		extract_bucket(db, buddy);

		if (buddy < bucket)
			bucket = buddy;

		++bucket->order;

		#if 0
		memset((char*)bucket + sizeof(db_bucket), 0xAA, bucket_size(bucket->order) - sizeof(db_bucket));
		#endif

		insert_bucket(db, bucket);
	}
}

void db_free(db_obj *db, void *ptr)
{
	if (!ptr)
		return;

	db_bucket *bucket = (db_bucket*)((char*)ptr - sizeof(uchar));

	#if 0
	memset(ptr, 0x88, net_bucket_size(bucket->order));
	#endif

	assert (!bucket->free);

	bucket->free = 1;

	insert_bucket(db, bucket);
	merge_buckets(db, bucket);
}

void* db_get_master_ptr(db_obj *db)
{
	return db_unmarshal(db, db->header->master_pointer);
}

void db_set_master_ptr(db_obj *db, void *ptr)
{
	db->header->master_pointer = db_marshal(db, ptr);
	db_invalidate_region(db, &db->header->master_pointer, sizeof(db_ptr));
}

void db_begin_transaction(db_obj *db)
{
	++db->transactions;
}

void db_invalidate(db_obj *db, void *ptr)
{
	db_bucket *bucket = (db_bucket*)((char*)ptr - sizeof(uchar));
	db_invalidate_region(db, ptr, net_bucket_size(bucket->order));
}

void db_invalidate_region(db_obj *db, void *ptr, const uint64 size)
{
	if (!ptr)
		return;
	assert(size > 0);

	db->changed = 1;

	size_t len=array_length(&db->dirty_regions, sizeof(db_region_boundary));
	db_region_boundary *start = array_allocate(&db->dirty_regions, sizeof(db_region_boundary), len);
	db_region_boundary *end = array_allocate(&db->dirty_regions, sizeof(db_region_boundary), len+1);

	start->type = DB_REGION_START;
	start->position = db_marshal(db, ptr);

	end->type = DB_REGION_END;
	end->position = start->position + size;
}

static void db_init(db_obj *db)
{
	uint64 size = sizeof(db_header);
	fallocate(db->fd, 0, 0, size);

	db_begin_transaction(db);

	#if 0
	// For debugging: Initialize whole file with "random" data
	{
		uint64 s = 0x4412457309583741;
		uint64 r = s;
		uint64 *p = (uint64*)db->header;
		for (int64 i=0; i<size/sizeof(uint64); ++i) {
			*p = r;
			r = r*s + s;
			++p;
		}
	}
	db_invalidate_region(db, db->header, size);
	#endif

	byte_zero(db->header, sizeof(db_header));

	db->header->size = size;
	db->header->bucket_count = 0;
	db_invalidate_region(db, db->header, sizeof(db_header));

	db_commit(db);
}

static void db_grow(db_obj *db)
{
	uint64 order = db->header->bucket_count;
	if (order == 0) {
		uint64 s = bucket_size(order-1);
		fallocate(db->fd, 0, 0, sizeof(db_header) + 2*s);
		db_bucket *bucket=(db_bucket*)db->bucket0;
		bucket->order = 0;
		bucket->free = 1;
		insert_bucket(db,bucket);
	} else {
		uint64 s = bucket_size(order-1);
		fallocate(db->fd, 0, 0, sizeof(db_header) + 2*s);

		db_bucket *bucket = (db_bucket*)(db->bucket0 + s);
		bucket->order = order-1;
		bucket->free = 1;
		db_invalidate_region(db, bucket, 1);

		insert_bucket(db, bucket);
		merge_buckets(db, bucket);

	}
	++(db->header->bucket_count);
	db_invalidate_region(db, db->header, sizeof(db_header));
}

// -1: Error
//  0: Journal empty.
// +1: Journal contains data that needs to be replayed.
static int db_check_journal(db_obj *db)
{
	journal_entry_type type;
	db_ptr _ptr;
	uint64 size;
	ssize_t consumed;

	int commit = 0;

	while (1) {
		consumed = read(db->journal_fd, &type, sizeof(journal_entry_type));
		if (consumed == 0)
			break;
		if (consumed < sizeof(journal_entry_type))
			goto fail;

		if (type == JOURNAL_WRITE) {
			commit = -1;
			if (read(db->journal_fd, &_ptr, sizeof(db_ptr)) < (ssize_t)sizeof(db_ptr))
				goto fail;

			if (read(db->journal_fd, &size, sizeof(uint64)) < (ssize_t)sizeof(uint64))
				goto fail;

			if (lseek(db->journal_fd, size, SEEK_CUR) < 0)
				goto fail;
		} else if (type == JOURNAL_COMMIT) {
			commit = 1;
		}
	}

	return commit;

fail:
	return -1;
}

static void db_replay_journal(db_obj *db)
{
	journal_entry_type type;
	db_ptr _ptr;
	uint64 size;
	ssize_t consumed;

	lseek(db->journal_fd, 0, SEEK_SET);

	while (1) {
		consumed = read(db->journal_fd, &type, sizeof(journal_entry_type));
		if (consumed == 0)
			goto success;
		if (consumed < (ssize_t)sizeof(journal_entry_type))
			goto fail;

		if (type == JOURNAL_WRITE) {
			if (read(db->journal_fd, &_ptr, sizeof(db_ptr)) < (ssize_t)sizeof(db_ptr))
				goto fail;

			if (read(db->journal_fd, &size, sizeof(uint64)) < (ssize_t)sizeof(uint64))
				goto fail;

			char *write_ptr = db->shared_map + _ptr;

			//printf("Replay %p %d\n", _ptr, size);

			consumed = read(db->journal_fd, write_ptr, size);
			if (consumed < size)
				goto fail;
		}
	}

success:
	//printf("success!!!!!\n");
	return;

fail:
	perror("FAIL FAIL FAIL !!!!! Could not replay journal");
}


void db_commit(db_obj *db)
{
	--db->transactions;
	if (db->transactions > 0)
		return;

	if (!db->changed)
		return;

	// Sort dirty regions
	qsort(array_start(&db->dirty_regions),
	      array_length(&db->dirty_regions, sizeof(db_region_boundary)),
	      sizeof(db_region_boundary),
	      region_boundary_comp);

	lseek(db->journal_fd, 0, SEEK_END);

	// Write changes to log, skip duplicate regions
	db_ptr region_start = ~0L;
	int64 nesting=0;
	for (int i=0; i<array_length(&db->dirty_regions, sizeof(db_region_boundary)); ++i) {
		db_region_boundary *region = array_get(&db->dirty_regions, sizeof(db_region_boundary), i);
		if (region->type == DB_REGION_START) {
			if (likely(nesting == 0))
				region_start = region->position;
			++nesting;
		} else {
			--nesting;
			assert(likely(nesting >= 0));
			assert(likely(region_start != ~0L));
			if (likely(nesting == 0)) {
				db_ptr region_end  = region->position;
				db_ptr region_size = region_end - region_start;

				journal_entry_type type = JOURNAL_WRITE;
				printf("Invalidate %x - %x (%d)\n", (int)region_start, (int)region_end, (int)region_size);
				if (unlikely(write(db->journal_fd, &type, sizeof(journal_entry_type)) < (ssize_t)sizeof(journal_entry_type)))
					goto fail;
				if (unlikely(write(db->journal_fd, &region_start, sizeof(db_ptr)) < (ssize_t)sizeof(db_ptr)))
					goto fail;
				if (unlikely(write(db->journal_fd, &region_size, sizeof(uint64)) < (ssize_t)sizeof(uint64)))
					goto fail;
				if (unlikely(write(db->journal_fd, db->priv_map + region_start, region_size) < (ssize_t)region_size))
					goto fail;
			}
		}
	}

	assert(likely(nesting == 0));

	if (fsync(db->journal_fd) == -1) {
		perror("FAIL, COULD NOT FSYNC");
		return;
	}

	journal_entry_type type = JOURNAL_COMMIT;
	write(db->journal_fd, &type, sizeof(journal_entry_type));

	db_replay_journal(db);

	if (msync(db->shared_map, db->header->size, MS_SYNC) == 0) {
		lseek(db->journal_fd, 0, SEEK_SET);
		ftruncate(db->journal_fd, 0);
		fsync(db->journal_fd);

		db->changed = 0;
	}

	array_trunc(&db->dirty_regions);
	return;

fail:
	perror("Error writing journal");
	return;
}
