#ifndef RAK_H
#define RAK_H

#include <stdlib.h>

#define DEBUG IS_DEBUG

#define RAK_DEFAULT_SIZE 8
#define RAK_DEFAULT RAK_DEFAULT_SIZE

#define RAK_OFFSET 0x4
#define RAK_MAGIC 0xD
#define RAK_PRIME 0x11

typedef struct {
	int 		link;
	char 		*key;
	void 		*value;
} rak_slot;	

typedef struct rak_t {
	size_t		cap; /* capacity */
	size_t 		len; /* length */
	size_t 		maxi; /* maximal index */
	rak_slot 	**slots;
} rak_t;

void rak_print(rak_t *rak);

rak_t *rak_new(size_t size);
void rak_free(rak_t *rak);
size_t rak_len(rak_t *rak);

void *rak_find(rak_t *rak, const char *key);
void *_rak_find_or(rak_t *rak, const char *key, void *retval);
#define rak_find_or(r, k, v) _rak_find_or(r, k, (void *)v)

int _rak_add(rak_t *rak, const char *key, void *value);
#define rak_add(r, k, v) _rak_add(r, k, (void *)v)

#endif /* #ifndef RAK_H */