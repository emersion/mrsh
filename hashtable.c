#define _POSIX_C_SOURCE 200809L
#include <mrsh/hashtable.h>
#include <stdlib.h>
#include <string.h>

static unsigned int djb2(const char *str) {
	unsigned int hash = 5381;
	char c;
	while ((c = *str++)) {
		hash = ((hash << 5) + hash) + c;
	}
	return hash;
}

void *mrsh_hashtable_get(struct mrsh_hashtable *table, const char *key) {
	unsigned int hash = djb2(key);
	unsigned int bucket = hash % MRSH_HASHTABLE_BUCKETS;
	struct mrsh_hashtable_entry *entry = table->buckets[bucket];

	while (entry != NULL) {
		if (entry->hash == hash && strcmp(entry->key, key) == 0) {
			return entry->value;
		}
		entry = entry->next;
	}

	return NULL;
}

void *mrsh_hashtable_set(struct mrsh_hashtable *table, const char *key,
		void *value) {
	unsigned int hash = djb2(key);
	unsigned int bucket = hash % MRSH_HASHTABLE_BUCKETS;
	struct mrsh_hashtable_entry *entry = table->buckets[bucket];

	struct mrsh_hashtable_entry *previous = NULL;
	while (entry != NULL) {
		if (entry->hash == hash && strcmp(entry->key, key) == 0) {
			break;
		}
		previous = entry;
		entry = entry->next;
	}

	if (entry == NULL) {
		entry = calloc(1, sizeof(struct mrsh_hashtable_entry));
		entry->hash = hash;
		entry->key = strdup(key);
		table->buckets[bucket] = entry;
		if (previous != NULL) {
			previous->next = entry;
		}
	}

	void *old_value = entry->value;
	entry->value = value;
	return old_value;
}

static void hashtable_entry_destroy(struct mrsh_hashtable_entry *entry) {
	if (entry == NULL) {
		return;
	}
	free(entry->key);
	free(entry);
}

void *mrsh_hashtable_del(struct mrsh_hashtable *table, const char *key) {
	unsigned int hash = djb2(key);
	unsigned int bucket = hash % MRSH_HASHTABLE_BUCKETS;
	struct mrsh_hashtable_entry *entry = table->buckets[bucket];

	struct mrsh_hashtable_entry *previous = NULL;
	while (entry != NULL) {
		if (entry->hash == hash && strcmp(entry->key, key) == 0) {
			break;
		}
		previous = entry;
		entry = entry->next;
	}

	if (entry == NULL) {
		return NULL;
	}

	if (previous != NULL) {
		previous->next = entry->next;
	} else {
		table->buckets[bucket] = NULL;
	}
	void *old_value = entry->value;
	hashtable_entry_destroy(entry);
	return old_value;
}

void mrsh_hashtable_finish(struct mrsh_hashtable *table) {
	for (size_t i = 0; i < MRSH_HASHTABLE_BUCKETS; ++i) {
		struct mrsh_hashtable_entry *entry = table->buckets[i];
		while (entry != NULL) {
			struct mrsh_hashtable_entry *next = entry->next;
			hashtable_entry_destroy(entry);
			entry = next;
		}
	}
}

void mrsh_hashtable_for_each(struct mrsh_hashtable *table,
		mrsh_hashtable_iterator_func_t iterator, void *user_data) {
	for (size_t i = 0; i < MRSH_HASHTABLE_BUCKETS; ++i) {
		struct mrsh_hashtable_entry *entry = table->buckets[i];
		while (entry != NULL) {
			struct mrsh_hashtable_entry *next = entry->next;
			iterator(entry->key, entry->value, user_data);
			entry = next;
		}
	}
}
