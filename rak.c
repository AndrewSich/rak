#include "rak.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

void rak_print(rak_t *rak)
{
	for (int i = 0; i < rak-> cap; i++) {
		rak_slot *slot = rak-> slots[i];
		if (slot != NULL)
			printf("{\"%s\": %d}\n", slot-> key, slot-> value);
	}
}

static rak_slot *r_slot_new(const char *key, void *value)
{
	rak_slot *slot = malloc(sizeof(rak_slot));
	slot-> link = -1;
	slot-> key = strdup(key);
	slot-> value = value;
	return slot;
}

static void r_slot_free(rak_slot *slot)
{
	free(slot-> key);
	free(slot);
}

static void r_slots_free(rak_slot **slots, size_t slots_cap)
{
	for (int i = 0; i < slots_cap; i++ ) {
		if (slots[i])
			r_slot_free(slots[i]);
	}
	free(slots);
}

static int r_slot_indexer(rak_slot **slots, size_t slots_cap, int/*current index*/ cidx)
{
	int idx = cidx; /*current index*/
	int midx = slots_cap; /*maximal index*/
	
repeat:
	while (idx < midx) {
		if (slots[idx] == NULL)
			return idx;
		idx++;
	}
	if (midx != cidx) {
		midx = cidx;
		idx = 0;
		goto repeat;
	}
#if DEBUG
	printf("failed to indexing empty slot !\n");
#endif
	return -1;
}

static int r_threshold(rak_t *rak)
{
	size_t threshold = (size_t)((rak-> cap / 3) * 2 + 1);
#if DEBUG
	printf("threshold is '%s'\n", (rak-> len > threshold) ? "over" : "safe");
#endif
	if (rak-> len > threshold)
		return 1; /* over */
	return 0; /* safe */
}

rak_t *rak_new(size_t size)
{
	rak_t *rak = malloc(sizeof(struct rak_t));
	rak-> cap = size ? size : RAK_DEFAULT_SIZE;
	rak-> len = 0;
	rak-> maxi = rak-> cap - 1;
	rak-> slots = (rak_slot **) calloc(rak-> cap, sizeof(rak_slot *));
	return rak;
}

void rak_free(rak_t *rak)
{
	r_slots_free(rak-> slots, rak-> cap);
	free(rak);
}

size_t rak_len(rak_t *rak)
{
	return rak-> len;
}

/* using custom FNV hash algorithm */
static uint64_t r_hash(const char *key)
{
	uint64_t hash = RAK_OFFSET;
	for (const char *k = key; *k; k++) {
		hash ^= (uint64_t)(*k) * RAK_MAGIC;
		hash *= RAK_PRIME;
	}
#if DEBUG
	printf("hash of key '%s' is %lu\n", key, hash);
#endif
	return hash;
}

static int r_get_index(size_t maxi, uint64_t hash)
{
	return (int)(hash & (uint64_t) maxi);
}

/* return is value of key, or NULL if key not found */
void *rak_find(rak_t *rak, const char *key)
{
	rak_slot *slot;
	int index = r_get_index(rak-> maxi, r_hash(key));
	while ((size_t) index < rak-> cap) {
		slot = rak-> slots[index];
		if (slot == NULL)
			break;
		if (strcmp(slot-> key, key) == 0)
			/* key found, with assume slot-> link is -1 */
			return slot-> value;
		if (slot-> link == -1)
			break;
		index = slot-> link;
	}
#if DEBUG
	printf("key '%s' not found!\n", key);
#endif
	return NULL;
}

void *_rak_find_or(rak_t *rak, const char *key, void/*return value*/ *retval)
{
	void *value = rak_find(rak, key);
	if (value)
		return value;
	return retval;
}

/* 1 if over threshold, or 0 if safe threshold */
// and -1 if failed
static int rak_resize(rak_t *rak)
{
	/* checking rak threshold */
	if (r_threshold(rak) == 0)
		return 0;
#if DEBUG
	printf("rak is oversize !!! ");
#endif
	size_t new_cap = rak-> cap * 2;
	rak_slot **slots = (rak_slot **) calloc(new_cap, sizeof(rak_slot *));
	if (slots == NULL) {
#if DEBUG
		printf("[rak_resize] failed to reallocate rak slots !!!\n");
#endif
		return -1;
	}
	// move all old slots to new slots
	for (int i = 0; i < rak-> cap; i++) {
		rak_slot *tmp_slot = rak-> slots[i];
		if (tmp_slot != NULL) {
			rak_slot *slot = r_slot_new(tmp_slot-> key, tmp_slot-> value);
			if (slot == NULL) {
#if DEBUG
				printf("[rak_resize] failed to reallocate new slot !\n");
#endif
				r_slots_free(slots, new_cap);
				return -1;
			}
			int idx = r_get_index(new_cap - 1, r_hash(tmp_slot-> key));
			while ((size_t) idx < new_cap) {
				rak_slot/*empty slot*/ *emp_slot = slots[idx];
				if (emp_slot == NULL)
					/* found empty slot, can make problem ? */
					break;
				if (emp_slot-> link == -1) {
					int new_link = r_slot_indexer(slots, new_cap, idx);
					if (new_link == -1) {
						/* failed to indexing empty slot */
						r_slot_free(slot);
						r_slots_free(slots, new_cap);
						return -1;
					}
					emp_slot-> link = new_link;
					idx = new_link;
					break;
				}
				idx = emp_slot-> link;
			}
			slots[idx] = slot;
		}
		/* move to next slot */
	}
#if DEBUG
	printf("success resize rak capacity ");
	printf("from '%d' to '%d'\n", rak-> cap, new_cap);
#endif
	r_slots_free(rak-> slots, rak-> cap);
	rak-> slots = slots;
	rak-> cap = new_cap;
	rak-> maxi = new_cap - 1;
	return 1;
}

/* 0 if success, or -1 if failed */
// and 1 if have same hash
int _rak_add(rak_t *rak, const char *key, void *value)
{
	if (rak_resize(rak) == -1)
		/* failed to resizing rak capacity */
		return -1;
	rak_slot *slot = r_slot_new(key, value);
	if (slot == NULL) {
#if DEBUG
		printf("[_rak_add] failed to reallocate new slot !\n");
#endif
		return -1;
	}
	int index = r_get_index(rak-> maxi, r_hash(key));
	while ((size_t) index < rak-> cap) {
		rak_slot *tmp_slot = rak-> slots[index];
		if (tmp_slot == NULL)
			break;
		if (strcmp(tmp_slot-> key, key) == 0) {
#if DEBUG
			printf("key '%s' is exists !\n", key);
#endif
			r_slot_free(slot);
			return 1;
		}
		if (tmp_slot-> link == -1) {
			int new_link = r_slot_indexer(rak-> slots, rak-> cap, index);
			if (new_link == -1) {
				/* failed to indexing empty slot */
				r_slot_free(slot);
				return -1;
			}
			tmp_slot-> link = new_link;
			index = new_link;
			break;
		}
		/* have same hash, but have difference key name */
		index = tmp_slot-> link;
	}
#if DEBUG
	printf("success add new slot at index '%d'\n", index);
#endif
	rak-> slots[index] = slot;
	rak-> len += 1;
	return 0;
}