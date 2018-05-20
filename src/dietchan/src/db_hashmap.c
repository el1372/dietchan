#include "db_hashmap.h"

#include <assert.h>
#include <libowfat/byte.h>

#define RELOCATED (~0UL)

static uint64 db_hashmap_hash(db_hashmap *map, void *key);
static int    db_hashmap_eq(db_hashmap *map, void *key_a, void *key_b);
static uint64 murmur(uint64 val);
static uint64 shrink_hash(uint64 hash, uint64 modulus);
static void   insert_element_internal(db_hashmap *map, db_ptr *buckets, uint64 index, void *key, void *val);
static void*  get_element_internal(db_hashmap *map, db_ptr *buckets, uint64 index, void *key);
static void   remove_element_internal(db_hashmap *map, db_ptr *buckets, uint64 index, void *key);
static void   rehash_bucket(db_hashmap *map, uint64 hash);
static void   rehash_some(db_hashmap *map);
static void   maybe_grow(db_hashmap *map);

void  db_hashmap_init(db_hashmap *map, db_obj *db, db_hashmap_data *data,
                      db_hash_func hash_func, void *hash_func_extra,
                      db_eq_func eq_func, void *eq_func_extra)
{
	byte_zero(map, sizeof(db_hashmap));

	if (!data) {
		data = db_alloc(db, sizeof(db_hashmap_data));
		byte_zero(data, sizeof(db_hashmap_data));
		db_invalidate_region(db, data, sizeof(db_hashmap_data));

		data->buckets_new_capacity = 64; // must be a power of two
		uint64 *new_buckets = db_alloc(db, sizeof(uint64)*data->buckets_new_capacity);
		data->buckets_new = db_marshal(db, new_buckets);
		byte_zero(new_buckets, sizeof(db_ptr)*data->buckets_new_capacity);
		db_invalidate_region(db, new_buckets, sizeof(db_ptr)*data->buckets_new_capacity);

		db_invalidate_region(db, data, sizeof(db_hashmap_data));
	}

	map->db              = db;
	map->data            = data;
	map->hash_func       = hash_func;
	map->hash_func_extra = hash_func_extra;
	map->eq_func         = eq_func;
	map->eq_func_extra   = eq_func_extra;
}

db_ptr db_hashmap_marshal(db_hashmap *map)
{
	return db_marshal(map->db, map->data);
}

void   db_hashmap_insert(db_hashmap *map, void *key, void *val)
{
	int64 hash = db_hashmap_hash(map, key);
	rehash_some(map);

	int inserted = 0;

	if (map->data->buckets_old_capacity > 0) {
		uint64 old_index = shrink_hash(hash, map->data->buckets_old_capacity);
		if (old_index >= map->data->buckets_old_progress) {
			// Insert into old table
			insert_element_internal(map, db_unmarshal(map->db, map->data->buckets_old), old_index, key, val);
			inserted = 1;
		} /*else {
			// Mark in old table that we have data in the new table
			uint64 old_index = shrink_hash(hash, map->data->buckets_old_capacity);
			db_ptr *buckets_old = db_unmarshal(map->db, map->data->buckets_old);
			buckets_old[old_index] = RELOCATED;

		}*/
	}

	if (!inserted) {
		// Otherwise insert into new table
		uint64 new_index = shrink_hash(hash, map->data->buckets_new_capacity);
		insert_element_internal(map, db_unmarshal(map->db, map->data->buckets_new), new_index, key, val);
	}

	++(map->data->number_of_elements);
	db_invalidate_region(map->db, &map->data->number_of_elements, sizeof(uint64));
	maybe_grow(map);
}

void* db_hashmap_get(db_hashmap  *map, void *key)
{
	uint64 hash = db_hashmap_hash(map, key);

	if (map->data->buckets_old_capacity > 0) {
		uint64 old_index = shrink_hash(hash, map->data->buckets_old_capacity);

		if (old_index >= map->data->buckets_old_progress) {
			// Search in old table
			return get_element_internal(map, db_unmarshal(map->db, map->data->buckets_old), old_index, key);
		}
	}

	// Search in new table
	uint64 new_index = shrink_hash(hash, map->data->buckets_new_capacity);
	return get_element_internal(map, db_unmarshal(map->db, map->data->buckets_new), new_index, key);
}

void   db_hashmap_remove(db_hashmap *map, void *key)
{
	int64 hash = db_hashmap_hash(map, key);
	rehash_some(map);

	if (map->data->buckets_old_capacity > 0) {
		uint64 old_index = shrink_hash(hash, map->data->buckets_old_capacity);
		if (old_index >= map->data->buckets_old_progress) {
			// Remove from old table
			remove_element_internal(map, db_unmarshal(map->db, map->data->buckets_old), old_index, key);
			return;
		}
	}

	// Remove from new table
	uint64 new_index = shrink_hash(hash, map->data->buckets_new_capacity);
	remove_element_internal(map, db_unmarshal(map->db, map->data->buckets_new), new_index, key);
}

uint64 uint64_hash(void *value, void *extra)
{
	return (*((uint64*)value));
}

int uint64_eq(void *a, void *b, void *extra)
{
	return ((*((uint64*)a)) == (*((uint64*)b)));
}

// --- Internal ---

static uint64 db_hashmap_hash(db_hashmap *map, void *key)
{
	if (map->hash_func)
		return map->hash_func(key, map->hash_func_extra);
	else
		return (uint64)key;
}

static int db_hashmap_eq(db_hashmap *map, void *key_a, void *key_b)
{
	if (map->eq_func)
		return map->eq_func(key_a, key_b, map->hash_func_extra);
	else
		return key_a == key_b;
}

uint64 murmur(uint64 val)
{
	const uint64 m=0x5bd1e995;
	val ^= val >> 24;
	val *= m;
	val ^= val >> 13;
	val *= m;
	val ^= val >> 15;
	return val;
}

static uint64 shrink_hash(uint64 hash, uint64 modulus)
{
	// modulus must be a power of two
	assert (!(((int64)modulus - 1) & (int64)modulus));

	// Make bad hash functions more random
	hash = murmur(hash);

	// Mix some higher-order into lower-order bits.
	// Experience has shown that this works better.
	return (hash ^ (hash >>  8) ^ (hash >> 16) ^ (hash >> 24)
	             ^ (hash >> 32) ^ (hash >> 40) ^ (hash >> 48)
	             ^ (hash >> 56)) % modulus;
}

static void insert_element_internal(db_hashmap *map, db_ptr *buckets, uint64 index, void *key, void *val)
{
	assert(buckets);
	uint64 *bucket = db_unmarshal(map->db, buckets[index]);

	assert(bucket != db_unmarshal(map->db, map->data->buckets_new));
	assert(!bucket || bucket != db_unmarshal(map->db, map->data->buckets_old));

	if (!bucket) {
		// Allocate new bucket
		bucket = db_alloc(map->db, sizeof(uint64)*3); // 3 = count + key + value
		bucket[0] = 1;
		bucket[1] = db_marshal(map->db, key);
		bucket[2] = db_marshal(map->db, val);
		db_invalidate_region(map->db, bucket, sizeof(uint64)*3);
	} else {
		#if 1
		for (int i=0; i<bucket[0]; ++i) {
			assert(!db_hashmap_eq(map, key, db_unmarshal(map->db, bucket[2*i+1])));
		}
		#endif
		// Append to existing bucket
		uint64 new_size = bucket[0] + 1;
		bucket = db_realloc(map->db, bucket, sizeof(uint64)*(new_size*2 + 1));
		bucket[0] = new_size;
		bucket[2*new_size-1] = db_marshal(map->db, key);
		bucket[2*new_size]   = db_marshal(map->db, val);
		db_invalidate_region(map->db, &bucket[0], sizeof(uint64));
		db_invalidate_region(map->db, &bucket[2*new_size-1], 2*sizeof(uint64));
	}

	buckets[index] = db_marshal(map->db, bucket);
	db_invalidate_region(map->db, &buckets[index], sizeof(db_ptr));
}

static void* get_element_internal(db_hashmap *map, db_ptr *buckets, uint64 index, void *key)
{
	if (!buckets)
		goto not_found;
	assert (buckets[index] != RELOCATED);
	if (buckets[index] == RELOCATED)
		goto not_found;
	uint64 *bucket = db_unmarshal(map->db, buckets[index]);
	if (!bucket)
		goto not_found;
	uint64 bucket_size = bucket[0];
	for (int64 i=0; i<bucket_size; ++i) {
		void *k = db_unmarshal(map->db, bucket[2*i+1]);
		void *v = db_unmarshal(map->db, bucket[2*i+2]);
		if (db_hashmap_eq(map, key, k))
			return v;
	}
not_found:
	return 0;
}

static void remove_element_internal(db_hashmap *map, db_ptr *buckets, uint64 index, void *key)
{
	assert(buckets);

	uint64 *bucket = db_unmarshal(map->db, buckets[index]);
	if (!bucket)
		return;

	uint64 bucket_size = bucket[0];
	for (int64 i=0; i<bucket_size; ++i) {
		void *k = db_unmarshal(map->db, bucket[2*i+1]);
		if (db_hashmap_eq(map, key, k)) {
			bucket[2*i+1] = bucket[2*bucket_size-1];
			bucket[2*i+2] = bucket[2*bucket_size];
			db_invalidate_region(map->db, &bucket[2*i+1], sizeof(uint64)*2);
			--bucket_size;
			if (bucket_size == 0) {
				db_free(map->db, bucket);
				bucket = 0;
			} else {
				bucket = db_realloc(map->db, bucket, sizeof(uint64)*(2*bucket_size + 1));
				bucket[0] = bucket_size;
				db_invalidate_region(map->db, &bucket[0], sizeof(uint64));
			}
			buckets[index] = db_marshal(map->db, bucket);
			db_invalidate_region(map->db, &buckets[index], sizeof(db_ptr));

			--(map->data->number_of_elements);
			db_invalidate_region(map->db, &map->data->number_of_elements, sizeof(uint64));

			return;
		}
	}

}

static void rehash_bucket(db_hashmap *map, uint64 old_index)
{
	// Remove bucket from old hashmap and move to new hashmap

	db_ptr *old_buckets = db_unmarshal(map->db, map->data->buckets_old);
	if (!old_buckets)
		return;

	// If we already rehashed the bucket, nothing to do here
	if (old_buckets[old_index] == RELOCATED)
		return;

	// The old bucket can map to two possible new buckets.
	// These are not initialized yet. Initialize them.
	uint64 new_index0 = old_index;
	uint64 new_index1 = old_index + map->data->buckets_new_capacity / 2;
	db_ptr *new_buckets = db_unmarshal(map->db, map->data->buckets_new);
	new_buckets[new_index0] = 0;
	new_buckets[new_index1] = 0;

	// Mark old bucket as relocated.
	uint64 *old_bucket = db_unmarshal(map->db, old_buckets[old_index]);
	old_buckets[old_index] = RELOCATED;

	db_invalidate_region(map->db, &new_buckets[new_index0], sizeof(db_ptr));
	db_invalidate_region(map->db, &new_buckets[new_index1], sizeof(db_ptr));
	db_invalidate_region(map->db, &old_buckets[old_index],  sizeof(db_ptr));

	// If old_bucket is null, it means it's empty, nothing to do.
	if (!old_bucket)
		return;

	// First element is the number of elements in the bucket.
	// Further elements are pairs of (key, value).
	uint64 old_bucket_size = old_bucket[0];

	// Insert all elements of this bucket into new hash table
	for (int64 i=0; i<old_bucket_size; ++i) {
		void *key = db_unmarshal(map->db, old_bucket[2*i+1]);
		void *val = db_unmarshal(map->db, old_bucket[2*i+2]);
		uint64 hash = db_hashmap_hash(map, key);
		uint64 index = shrink_hash(hash, map->data->buckets_new_capacity);
		insert_element_internal(map, new_buckets, index, key, val);
	}

	// Free old bucket
	db_free(map->db, old_bucket);
}

static void rehash_some(db_hashmap *map)
{
	if (!map->data->buckets_old)
		return;
	// Rehash 8 buckets in every step.
	// The number could be tweaked, but it must be strictly greater than 1, I think.
	// Better have some safety margin just to be sure. Everything 4 and above should be fine.
	for (uint64 i=0; i<8 && map->data->buckets_old_progress<map->data->buckets_old_capacity; ++i) {
		rehash_bucket(map, map->data->buckets_old_progress);
		++map->data->buckets_old_progress;
	}

	if (map->data->buckets_old_progress >= map->data->buckets_old_capacity) {
		// Success, we have rehashed everything!
		// Now we can free the old hashtable.
		db_ptr *old_buckets = db_unmarshal(map->db, map->data->buckets_old);
		db_free(map->db, old_buckets);
		map->data->buckets_old = db_marshal(map->db, 0);
		map->data->buckets_old_capacity = 0;
		map->data->buckets_old_progress = 0;
		db_invalidate_region(map->db, &map->data->buckets_old, sizeof(db_ptr));
		db_invalidate_region(map->db, &map->data->buckets_old_capacity, sizeof(uint64));
	}

	db_invalidate_region(map->db, &map->data->buckets_old_progress, sizeof(uint64));
}

static void maybe_grow(db_hashmap *map)
{
	static const float load_factor = 4.0/2.0;
	// Check if we have reached the grow threshold
	if (map->data->buckets_new_capacity > 0 &&
	    map->data->number_of_elements <= load_factor*map->data->buckets_new_capacity)
		return;

	// We should have migrated all the old data by this point, otherwise something went very wrong.
	assert(db_unmarshal(map->db, map->data->buckets_old) == 0);

	map->data->buckets_old          = map->data->buckets_new;
	map->data->buckets_old_capacity = map->data->buckets_new_capacity;
	map->data->buckets_old_progress = 0;

	assert (map->data->buckets_old_capacity > 0);
	map->data->buckets_new_capacity = map->data->buckets_old_capacity*2; // MUST BE 2

	uint64 *new_buckets = db_alloc(map->db, sizeof(uint64)*map->data->buckets_new_capacity);
	map->data->buckets_new = db_marshal(map->db, new_buckets);
	#if 0
	// Might be useful for debugging.
	// This gives an O(n) the worst case complexity and render all our efforts useless:
	byte_zero(new_buckets, sizeof(uint64)*map->data->buckets_new_capacity);
	db_invalidate_region(map->db, new_buckets, sizeof(uint64)*map->data->buckets_new_capacity);
	#endif

	db_invalidate_region(map->db, map->data, sizeof(db_hashmap_data));

	rehash_some(map);
}
