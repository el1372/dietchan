#ifndef DB_HASHMAP_H
#define DB_HASHMAP_H

#include "db.h"

typedef struct db_hashmap_data {
	db_ptr buckets_old;
	uint64 buckets_old_capacity;
	uint64 buckets_old_progress;
	db_ptr buckets_new;
	uint64 buckets_new_capacity;
	uint64 number_of_elements;
} db_hashmap_data;

typedef uint64 (*db_hash_func)(void *key, void *extra);
typedef int    (*db_eq_func)(void *key_a, void *key_b, void *extra);

typedef struct db_hashmap {
	db_obj          *db;
	db_hashmap_data *data;
	db_hash_func     hash_func;
	void            *hash_func_extra;
	db_eq_func       eq_func;
	void            *eq_func_extra;
} db_hashmap;

void    db_hashmap_init(db_hashmap *map, db_obj *db, db_hashmap_data *data,
                        db_hash_func hash_func, void *hash_func_extra,
                        db_eq_func eq_func, void *eq_func_extra);

db_ptr  db_hashmap_marshal(db_hashmap *map);

void    db_hashmap_insert(db_hashmap *map, void *key, void *val);
void*   db_hashmap_get(db_hashmap  *map, void *key);
void    db_hashmap_remove(db_hashmap *map, void *key);

uint64 uint64_hash(void *value, void *extra);
int uint64_eq(void *a, void *b, void *extra);

#endif // DB_HASHMAP_H
