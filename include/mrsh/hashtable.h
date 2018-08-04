#ifndef _MRSH_HASHTABLE_H
#define _MRSH_HASHTABLE_H

#define MRSH_HASHTABLE_BUCKETS 256

struct mrsh_hashtable_entry {
	struct mrsh_hashtable_entry *next;
	unsigned int hash;
	char *key;
	void *value;
};

struct mrsh_hashtable {
	struct mrsh_hashtable_entry *buckets[MRSH_HASHTABLE_BUCKETS];
};

typedef void (*mrsh_hashtable_iterator_func_t)(const char *key, void *value,
	void *user_data);

void mrsh_hashtable_finish(struct mrsh_hashtable *table);
void *mrsh_hashtable_get(struct mrsh_hashtable *table, const char *key);
void *mrsh_hashtable_set(struct mrsh_hashtable *table, const char *key,
	void *value);
void *mrsh_hashtable_del(struct mrsh_hashtable *table, const char *key);
void mrsh_hashtable_for_each(struct mrsh_hashtable *table,
	mrsh_hashtable_iterator_func_t iterator, void *user_data);

#endif
