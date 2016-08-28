#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <emmintrin.h>
#include <assert.h>
#include <x86intrin.h>
#include "PART.h"

#define mfence() asm volatile("mfence":::"memory")
#define BITOP_WORD(nr)	((nr) / BITS_PER_LONG)

unsigned long node4_count = 0;
unsigned long node16_count = 0;
unsigned long node128_count = 0;
unsigned long leaf_count = 0;
unsigned long clflush_count = 0;
unsigned long mfence_count = 0;

/**
 * Macros to manipulate pointer tags
 */
#define IS_LEAF(x) (((uintptr_t)x & 1))
#define SET_LEAF(x) ((void*)((uintptr_t)x | 1))
#define LEAF_RAW(x) ((art_leaf*)((void*)((uintptr_t)x & ~1)))

#define LATENCY			200
#define CPU_FREQ_MHZ	2400

static inline void cpu_pause()
{
	__asm__ volatile ("pause" ::: "memory");
}

static inline unsigned long read_tsc(void)
{
	unsigned long var;
	unsigned int hi, lo;

	asm volatile ("rdtsc" : "=a" (lo), "=d" (hi));
	var = ((unsigned long long int) hi << 32) | lo;

	return var;
}

void flush_buffer(void *buf, unsigned long len, bool fence)
{
	unsigned long i, etsc;
	len = len + ((unsigned long)(buf) & (CACHE_LINE_SIZE - 1));
	if (fence) {
		mfence();
		for (i = 0; i < len; i += CACHE_LINE_SIZE) {
			clflush_count++;
//			etsc = read_tsc() + (unsigned long)(LATENCY * CPU_FREQ_MHZ / 1000);
			asm volatile ("clflush %0\n" : "+m" (*(char *)(buf+i)));
//			while (read_tsc() < etsc)
//				cpu_pause();
		}
		mfence();
		mfence_count = mfence_count + 2;
	} else {
		for (i = 0; i < len; i += CACHE_LINE_SIZE) {
			clflush_count++;
//			etsc = read_tsc() + (unsigned long)(LATENCY * CPU_FREQ_MHZ / 1000);
			asm volatile ("clflush %0\n" : "+m" (*(char *)(buf+i)));
//			while (read_tsc() < etsc)
//				cpu_pause();
		}
	}
}

void flush_buffer_nocount(void *buf, unsigned long len, bool fence)
{
	unsigned long i;
	len = len + ((unsigned long)(buf) & (CACHE_LINE_SIZE - 1));
	if (fence) {
		mfence();
		for (i = 0; i < len; i += CACHE_LINE_SIZE)
			asm volatile ("clflush %0\n" : "+m" (*(char *)(buf+i)));
		mfence();
	} else {
		for (i = 0; i < len; i += CACHE_LINE_SIZE)
			asm volatile ("clflush %0\n" : "+m" (*(char *)(buf+i)));
	}
}

/*
 * Find the next set bit in a memory region.
 */
unsigned long find_next_bit(const unsigned long *addr, unsigned long size,
			    unsigned long offset)
{
	const unsigned long *p = addr + BITOP_WORD(offset);
	unsigned long result = offset & ~(BITS_PER_LONG-1);
	unsigned long tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset %= BITS_PER_LONG;
	if (offset) {
		tmp = *(p++);
		tmp &= (~0UL << offset);
		if (size < BITS_PER_LONG)
			goto found_first;
		if (tmp)
			goto found_middle;
		size -= BITS_PER_LONG;
		result += BITS_PER_LONG;
	}
	while (size & ~(BITS_PER_LONG-1)) {
		if ((tmp = *(p++)))
			goto found_middle;
		result += BITS_PER_LONG;
		size -= BITS_PER_LONG;
	}
	if (!size)
		return result;
	tmp = *p;

found_first:
	tmp &= (~0UL >> (BITS_PER_LONG - size));
	if (tmp == 0UL)		/* Are any bits set? */
		return result + size;	/* Nope. */
found_middle:
	return result + __ffs(tmp);
}

/*
 * Find the next zero bit in a memory region
 */
unsigned long find_next_zero_bit(const unsigned long *addr, unsigned long size,
		unsigned long offset)
{
	const unsigned long *p = addr + BITOP_WORD(offset);
	unsigned long result = offset & ~(BITS_PER_LONG - 1);
	unsigned long tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset %= BITS_PER_LONG;
	if (offset) {
		tmp = *(p++);
		tmp |= ~0UL >> (BITS_PER_LONG - offset);
		if (size < BITS_PER_LONG)
			goto found_first;
		if (~tmp)
			goto found_middle;
		size -= BITS_PER_LONG;
		result += BITS_PER_LONG;
	}
	while (size & ~(BITS_PER_LONG - 1)) {
		if (~(tmp = *(p++)))
			goto found_middle;
		result += BITS_PER_LONG;
		size -= BITS_PER_LONG;
	}
	if (!size)
		return result;
	tmp = *p;

found_first:
	tmp |= ~0UL << size;
	if (tmp == ~0UL)
		return result + size;
found_middle:
	return result + ffz(tmp);
}

/**
 * Allocates a node of the given type,
 * initializes to zero and sets the type.
 */
static art_node* alloc_node(uint8_t type) {
	art_node* n;
	int i;
	switch (type) {
		case NODE4:
			n = (art_node *)malloc(sizeof(art_node4));
			for (i = 0; i < 4; i++)
				((art_node4 *)n)->slot[i].key = -1;
			node4_count++;
			break;
		case NODE16:
			n = (art_node *)malloc(sizeof(art_node16));
			memset(((art_node16 *)n)->bitmap, 0, sizeof(((art_node16 *)n)->bitmap));
			node16_count++;
			break;
//		case NODE32:
//			n = (art_node *)calloc(1, sizeof(art_node32));
//			break;
		case NODE128:
			n = (art_node *)calloc(1, sizeof(art_node128));
			node128_count++;
			break;
		default:
			abort();
	}
	n->type = type;
	return n;
}

/**
 * Initializes an ART tree
 * @return 0 on success.
 */
int art_tree_init(art_tree *t) {
	t->root = NULL;
	t->size = 0;
	return 0;
}

// Recursively destroys the tree
/*
static void destroy_node(art_node *n) {
	// Break if null
	if (!n) return;

	// Special case leafs
	if (IS_LEAF(n)) {
		free(LEAF_RAW(n));
		return;
	}

	// Handle each node type
	int i;
	union {
		art_node4 *p1;
		art_node16 *p2;
		art_node48 *p3;
		art_node256 *p4;
	} p;
	switch (n->type) {
		case NODE4:
			p.p1 = (art_node4*)n;
			for (i=0;i<n->num_children;i++) {
				destroy_node(p.p1->children[i]);
			}
			break;

		case NODE16:
			p.p2 = (art_node16*)n;
			for (i=0;i<n->num_children;i++) {
				destroy_node(p.p2->children[i]);
			}
			break;

		case NODE48:
			p.p3 = (art_node48*)n;
			for (i=0;i<n->num_children;i++) {
				destroy_node(p.p3->children[i]);
			}
			break;

		case NODE256:
			p.p4 = (art_node256*)n;
			for (i=0;i<256;i++) {
				if (p.p4->children[i])
					destroy_node(p.p4->children[i]);
			}
			break;

		default:
			abort();
	}

	// Free ourself on the way up
	free(n);
}
*/
/**
 * Destroys an ART tree
 * @return 0 on success.
 */
/*
int art_tree_destroy(art_tree *t) {
	destroy_node(t->root);
	return 0;
}
*/

/**
 * Returns the size of the ART tree.
 */

#ifndef BROKEN_GCC_C99_INLINE
extern inline uint64_t art_size(art_tree *t);
#endif

static art_node** find_child(art_node *n, unsigned char c) {
	int i;
	union {
		art_node4 *p1;
		art_node16 *p2;
		art_node128 *p3;
	} p;
	switch (n->type) {
		case NODE4:
			p.p1 = (art_node4 *)n;
			for (i = 0; ((p.p1->slot[i].key != -1) && i < 4); i++) {
				if (p.p1->slot[i].key == c)
					return &p.p1->children[p.p1->slot[i].i_ptr];
			}
			break;
		case NODE16:
			p.p2 = (art_node16 *)n;
			if(p.p2->bitmap[c / 32][0] & (0x1U << (c % 32)))
				return &p.p2->children[p.p2->keys[c]];
			break;
		case NODE128:
			p.p3 = (art_node128 *)n;
			if (p.p3->children[c])
				return &p.p3->children[c];
			break;
		default:
			abort();
	}
	return NULL;
}

// Simple inlined if
static inline int min(int a, int b) {
	return (a < b) ? a : b;
}

/**
 * Returns the number of prefix characters shared between
 * the key and node.
 */
static int check_prefix(const art_node *n, const unsigned char *key, int key_len, int depth) {
	int max_cmp = min(min(n->partial_len, MAX_PREFIX_LEN), key_len - depth);
	int idx;
	for (idx=0; idx < max_cmp; idx++) {
		if (n->partial[idx] != key[depth + idx])
			return idx;
	}
	return idx;
}

/**
 * Checks if a leaf matches
 * @return 0 on success.
 */
static int leaf_matches(const art_leaf *n, const unsigned char *key, int key_len, int depth) {
	(void)depth;
	// Fail if the key lengths are different
	if (n->key_len != (uint32_t)key_len) return 1;

	// Compare the keys starting at the depth
	return memcmp(n->key, key, key_len);
}

/**
 * Searches for a value in the ART tree
 * @arg t The tree
 * @arg key The key
 * @arg key_len The length of the key
 * @return NULL if the item was not found, otherwise
 * the value pointer is returned.
 */
void* art_search(const art_tree *t, const unsigned char *key, int key_len) {
	art_node **child;
	art_node *n = t->root;
	int prefix_len, depth = 0;

	while (n) {
		// Might be a leaf
		if (IS_LEAF(n)) {
			n = (art_node*)LEAF_RAW(n);
			// Check if the expanded path matches
			if (!leaf_matches((art_leaf*)n, key, key_len, depth)) {
				return ((art_leaf*)n)->value;
			}
			return NULL;
		}

		// Bail if the prefix does not match
		if (n->partial_len) {
			prefix_len = check_prefix(n, key, key_len, depth);
			if (prefix_len != min(MAX_PREFIX_LEN, n->partial_len))
				return NULL;
			depth = depth + n->partial_len;
		}

		// Recursively search
		child = find_child(n, key[depth]);
		n = (child) ? *child : NULL;
		depth++;
	}
	return NULL;
}

// Find the minimum leaf under a node
static art_leaf* minimum(const art_node *n) {
	// Handle base cases
	if (!n) return NULL;
	if (IS_LEAF(n)) return LEAF_RAW(n);

	int i, j, idx;
	switch (n->type) {
		case NODE4:
			return minimum(((art_node4 *)n)->children[((art_node4 *)n)->slot[0].i_ptr]);
		case NODE16:
			for (i = 0; i < 4; i++) {
				for (j = 0; j < 32; j++) { 
					j = find_next_bit((unsigned long *)&((art_node16 *)n)->bitmap[i][0], 32, j);
					if (j < 32)
						return minimum(((art_node16 *)n)->children[((art_node16 *)n)->keys[j + (i * 32)]]);
				}
			}
		case NODE128:
			idx = 0;
			while (!((art_node128 *)n)->children[idx]) idx++;
			return minimum(((art_node128 *)n)->children[idx]);
		default:
			abort();
	}
}

// Find the maximum leaf under a node
/*
static art_leaf* maximum(const art_node *n) {
	// Handle base cases
	if (!n) return NULL;
	if (IS_LEAF(n)) return LEAF_RAW(n);

	int idx;
	switch (n->type) {
		case NODE4:
			return maximum(((art_node4*)n)->children[n->num_children-1]);
		case NODE16:
			return maximum(((art_node16*)n)->children[n->num_children-1]);
		case NODE48:
			idx=255;
			while (!((art_node48*)n)->keys[idx]) idx--;
			idx = ((art_node48*)n)->keys[idx] - 1;
			return maximum(((art_node48*)n)->children[idx]);
		case NODE256:
			idx=255;
			while (!((art_node256*)n)->children[idx]) idx--;
			return maximum(((art_node256*)n)->children[idx]);
		default:
			abort();
	}
}
*/

/**
 * Returns the minimum valued leaf
 */
/*
art_leaf* art_minimum(art_tree *t) {
	return minimum((art_node*)t->root);
}
*/

/**
 * Returns the maximum valued leaf
 */
/*
art_leaf* art_maximum(art_tree *t) {
	return maximum((art_node*)t->root);
}
*/

static art_leaf* make_leaf(const unsigned char *key, int key_len, void *value) {
	art_leaf *l = (art_leaf*)malloc(sizeof(art_leaf) + key_len);
	l->value = value;
	l->key_len = key_len;
	memcpy(l->key, key, key_len);
	leaf_count++;
	return l;
}

static int longest_common_prefix(art_leaf *l1, art_leaf *l2, int depth) {
	int idx, max_cmp = min(l1->key_len, l2->key_len) - depth;

	for (idx=0; idx < max_cmp; idx++) {
		if (l1->key[depth + idx] != l2->key[depth + idx])
			return idx;
	}
	return idx;
}

static void copy_header(art_node *dest, art_node *src) {
	dest->partial_len = src->partial_len;
	memcpy(dest->partial, src->partial, min(MAX_PREFIX_LEN, src->partial_len));
}

static void add_child128_noflush(art_node128 *n, art_node **ref, unsigned char c, void *child) {
	(void)ref;
	n->children[c] = (art_node *)child;
}

static void add_child128(art_node128 *n, art_node **ref, unsigned char c, void *child) {
	(void)ref;
	n->children[c] = (art_node *)child;
	flush_buffer(&n->children[c], 8, true);
}

static void add_child16_noflush(art_node16 *n, art_node **ref, unsigned char c, void *child) {
	int idx;
	unsigned int p_bitmap = 0;
	p_bitmap = n->bitmap[0][1] + n->bitmap[1][1] + n->bitmap[2][1] + n->bitmap[3][1];

	idx = find_next_zero_bit((unsigned long *)&p_bitmap, 16, 0);
	if (idx == 16) {
		printf("find next zero bit error in child 16\n");
		abort();
	}

	n->keys[c] = idx;
	n->children[idx] = child;

	n->bitmap[c / 32][0] = n->bitmap[c / 32][0] + (0x1U << (c % 32));
	n->bitmap[c / 32][1] = n->bitmap[c / 32][1] + (0x1U << idx);
}

static void add_child16(art_node16 *n, art_node **ref, unsigned char c, void *child) {
	unsigned int p_bitmap = 0;
	p_bitmap = n->bitmap[0][1] + n->bitmap[1][1] + n->bitmap[2][1] + n->bitmap[3][1];

	if (p_bitmap != ((0x1U << 16) - 1)) {
		int idx;

		idx = find_next_zero_bit((unsigned long *)&p_bitmap, 16, 0);
		if (idx == 16) {
			printf("find next zero bit error in child 16\n");
			abort();
		}

		n->keys[c] = idx;
		n->children[idx] = child;
		
		n->bitmap[c / 32][0] = n->bitmap[c / 32][0] + (0x1U << (c % 32));
		n->bitmap[c / 32][1] = n->bitmap[c / 32][1] + (0x1U << idx);

		flush_buffer(&n->keys[c], sizeof(unsigned char), false);
		flush_buffer(&n->children[idx], 8, false);
		flush_buffer(n->bitmap[c / 32], sizeof(n->bitmap[c / 32]), true);
	} else {
		int i, j, num = 0;
		art_node128 *new_node = (art_node128 *)alloc_node(NODE128);

		for (i = 0; i < 4; i++) {
			for (j = 0; j < 32; j++) { 
				j = find_next_bit((unsigned long *)&n->bitmap[i][0], 32, j);
				if (j < 32) {
					new_node->children[j + (i * 32)] = n->children[n->keys[j + (i * 32)]];
					num++;
					if (num == 16)
						break;
				}
			}
			if (num == 16)
				break;
		}
		copy_header((art_node *)new_node, (art_node *)n);
		*ref = (art_node *)new_node;
		add_child128_noflush(new_node, ref, c, child);
		flush_buffer(new_node, sizeof(art_node128), false);
		flush_buffer(ref, 8, true);

		node16_count--;
		free(n);
	}
}

static void add_child4_noflush(art_node4 *n, art_node **ref, unsigned char c, void *child) {
	slot_array temp_slot[4];
	int i, idx, mid = -1;
	unsigned long p_idx = 0;

	for (idx = 0; ((n->slot[idx].key != -1) && (idx < 4)); idx++) {
		p_idx = p_idx + (1 << n->slot[idx].i_ptr);
		if (mid == -1 && c < n->slot[idx].key)
			mid = idx;
	}

	if (mid == -1)
		mid = idx;

	p_idx = find_next_zero_bit(&p_idx, 4, 0);
	if (p_idx == 4) {
		printf("find next zero bit error in child4\n");
		abort();
	}

	n->children[p_idx] = child;

	for (i = idx - 1; i >= mid; i--) {
		temp_slot[i + 1].key = n->slot[i].key;
		temp_slot[i + 1].i_ptr = n->slot[i].i_ptr;
	}

	if (idx < 3) {
		for (i = idx + 1; i < 4; i++)
			temp_slot[i].key = -1;
	}

	temp_slot[mid].key = c;
	temp_slot[mid].i_ptr = p_idx;

	for (i = mid - 1; i >=0; i--) {
		temp_slot[i].key = n->slot[i].key;
		temp_slot[i].i_ptr = n->slot[i].i_ptr;
	}

	*((uint64_t *)n->slot) = *((uint64_t *)temp_slot);
}

static void add_child4(art_node4 *n, art_node **ref, unsigned char c, void *child) {
	if (n->slot[3].key == -1) {
		slot_array temp_slot[4];
		int i, idx, mid = -1;
		unsigned long p_idx = 0;

		for (idx = 0; ((n->slot[idx].key != -1) && (idx < 4)); idx++) {
			p_idx = p_idx + (1 << n->slot[idx].i_ptr);
			if (mid == -1 && c < n->slot[idx].key)
				mid = idx;
		}

		if (mid == -1)
			mid = idx;

		p_idx = find_next_zero_bit(&p_idx, 4, 0);
		if (p_idx == 4) {
			printf("find next zero bit error in child4\n");
			abort();
		}

		n->children[p_idx] = child;

		for (i = idx - 1; i >= mid; i--) {
			temp_slot[i + 1].key = n->slot[i].key;
			temp_slot[i + 1].i_ptr = n->slot[i].i_ptr;
		}

		if (idx < 3) {
			for (i = idx + 1; i < 4; i++)
				temp_slot[i].key = -1;
		}

		temp_slot[mid].key = c;
		temp_slot[mid].i_ptr = p_idx;

		for (i = mid - 1; i >=0; i--) {
			temp_slot[i].key = n->slot[i].key;
			temp_slot[i].i_ptr = n->slot[i].i_ptr;
		}

		*((uint64_t *)n->slot) = *((uint64_t *)temp_slot);
		
		flush_buffer(&n->children[p_idx], 8, false);
		flush_buffer(n->slot, 8, true);
	} else {
		int idx;
		art_node16 *new_node = (art_node16 *)alloc_node(NODE16);

		for (idx = 0; idx < 4; idx++) {
			new_node->bitmap[n->slot[idx].key / 32][0] = 
				new_node->bitmap[n->slot[idx].key / 32][0] + 
				(0x1U << (n->slot[idx].key % 32));
			new_node->bitmap[n->slot[idx].key / 32][1] = 
				new_node->bitmap[n->slot[idx].key / 32][1] + 
				(0x1U << (n->slot[idx].i_ptr));
			new_node->keys[n->slot[idx].key] = n->slot[idx].i_ptr;
			new_node->children[n->slot[idx].i_ptr] = n->children[n->slot[idx].i_ptr];
		}
		copy_header((art_node *)new_node, (art_node *)n);
		add_child16_noflush(new_node, ref, c, child);

		*ref = (art_node *)new_node;
		flush_buffer(new_node, sizeof(art_node16), false);
		flush_buffer(ref, 8, true);

		node4_count--;
		free(n);
	}
}

static void add_child(art_node *n, art_node **ref, unsigned char c, void *child) {
	switch (n->type) {
		case NODE4:
			return add_child4((art_node4 *)n, ref, c, child);
		case NODE16:
			return add_child16((art_node16 *)n, ref, c, child);
//		case NODE32:
//			return add_child32((art_node32 *)n, ref, c, child);
		case NODE128:
			return add_child128((art_node128 *)n, ref, c, child);
		default:
			abort();
	}
}

/**
 * Calculates the index at which the prefixes mismatch
 */
static int prefix_mismatch(const art_node *n, const unsigned char *key, int key_len, int depth, art_leaf **l) {
	int max_cmp = min(min(MAX_PREFIX_LEN, n->partial_len), key_len - depth);
	int idx;
	for (idx=0; idx < max_cmp; idx++) {
		if (n->partial[idx] != key[depth + idx])
			return idx;
	}

	// If the prefix is short we can avoid finding a leaf
	if (n->partial_len > MAX_PREFIX_LEN) {
		// Prefix is longer than what we've checked, find a leaf
		*l = minimum(n);
		max_cmp = min((*l)->key_len, key_len) - depth;
		for (; idx < max_cmp; idx++) {
			if ((*l)->key[idx + depth] != key[depth + idx])
				return idx;
		}
	}
	return idx;
}

static void* recursive_insert(art_node *n, art_node **ref, const unsigned char *key,
		int key_len, void *value, int depth, int *old)
{
	// If we are at a NULL node, inject a leaf
	if (!n) {
		*ref = (art_node*)SET_LEAF(make_leaf(key, key_len, value));
		flush_buffer(*ref, sizeof(art_leaf) + key_len, false);
		flush_buffer(ref, 8, true);
		return NULL;
	}

	// If we are at a leaf, we need to replace it with a node
	if (IS_LEAF(n)) {
		art_leaf *l = LEAF_RAW(n);

		// Check if we are updating an existing value
		if (!leaf_matches(l, key, key_len, depth)) {
			*old = 1;
			void *old_val = l->value;
			l->value = value;
			flush_buffer(&l->value, 8, true);
			return old_val;
		}

		// New value, we must split the leaf into a node4
		art_node4 *new_node = (art_node4 *)alloc_node(NODE4);

		// Create a new leaf
		art_leaf *l2 = make_leaf(key, key_len, value);

		// Determine longest prefix
		int i, longest_prefix = longest_common_prefix(l, l2, depth);
		new_node->n.partial_len = longest_prefix;
		memcpy(new_node->n.partial, key+depth, min(MAX_PREFIX_LEN, longest_prefix));
		// Add the leafs to the new node4
		*ref = (art_node*)new_node;
		add_child4_noflush(new_node, ref, l->key[depth + longest_prefix], SET_LEAF(l));
		add_child4_noflush(new_node, ref, l2->key[depth + longest_prefix], SET_LEAF(l2));

		flush_buffer(new_node, sizeof(art_node4), false);
		flush_buffer(l2, sizeof(art_leaf) + key_len, false);
		flush_buffer(ref, 8, true);
		return NULL;
	}

	// Check if given node has a prefix
	if (n->partial_len) {
		// Determine if the prefixes differ, since we need to split
		art_leaf *l = NULL;
		int prefix_diff = prefix_mismatch(n, key, key_len, depth, &l);
		if ((uint32_t)prefix_diff >= n->partial_len) {
			depth += n->partial_len;
			goto RECURSE_SEARCH;
		}

		// Create a new node
		art_node4 *new_node = (art_node4*)alloc_node(NODE4);
		art_node *copy_node;
		switch (n->type) {
			case NODE4:
				copy_node = (art_node *)malloc(sizeof(art_node4));
				memcpy(copy_node, n, sizeof(art_node4));
				free(n);
				break;
			case NODE16:
				copy_node = (art_node *)malloc(sizeof(art_node16));
				memcpy(copy_node, n, sizeof(art_node16));
				free(n);
				break;
			case NODE128:
				copy_node = (art_node *)malloc(sizeof(art_node128));
				memcpy(copy_node, n, sizeof(art_node128));
				free(n);
				break;
			default:
				abort();
		}

		*ref = (art_node*)new_node;
		new_node->n.partial_len = prefix_diff;
		memcpy(new_node->n.partial, copy_node->partial, min(MAX_PREFIX_LEN, prefix_diff));

		// Adjust the prefix of the old node
		if (copy_node->partial_len <= MAX_PREFIX_LEN) {
			add_child4_noflush(new_node, ref, copy_node->partial[prefix_diff], copy_node);
			copy_node->partial_len -= (prefix_diff + 1);
			memmove(copy_node->partial, copy_node->partial + prefix_diff + 1,
					min(MAX_PREFIX_LEN, copy_node->partial_len));
		} else {
			int i;
			copy_node->partial_len -= (prefix_diff + 1);
			if (l == NULL)
				l = minimum(copy_node);
			add_child4_noflush(new_node, ref, l->key[depth + prefix_diff], copy_node);
			memcpy(copy_node->partial, l->key+depth+prefix_diff+1,
					min(MAX_PREFIX_LEN, copy_node->partial_len));
		}

		// Insert the new leaf
		l = make_leaf(key, key_len, value);
		add_child4_noflush(new_node, ref, key[depth + prefix_diff], SET_LEAF(l));

		flush_buffer(new_node, sizeof(art_node4), false);
		flush_buffer(l, sizeof(art_leaf) + key_len, false);
		if(copy_node->type == NODE4)
			flush_buffer(copy_node, sizeof(art_node4), false);
		else if (copy_node->type == NODE16)
			flush_buffer(copy_node, sizeof(art_node16), false);
		else
			flush_buffer(copy_node, sizeof(art_node128), false);
		flush_buffer(ref, 8, true);

		return NULL;
	}

RECURSE_SEARCH:;

	// Find a child to recurse to
	art_node **child = find_child(n, key[depth]);
	if (child) {
		return recursive_insert(*child, child, key, key_len, value, depth + 1, old);
	}

	// No child, node goes within us
	art_leaf *l = make_leaf(key, key_len, value);
	flush_buffer(l, sizeof(art_leaf) + key_len, false);

	add_child(n, ref, key[depth], SET_LEAF(l));

	return NULL;
}

/**
 * Inserts a new value into the ART tree
 * @arg t The tree
 * @arg key The key
 * @arg key_len The length of the key
 * @arg value Opaque value.
 * @return NULL if the item was newly inserted, otherwise
 * the old value pointer is returned.
 */
void* art_insert(art_tree *t, const unsigned char *key, int key_len, void *value) {
	int old_val = 0;
	void *old = recursive_insert(t->root, &t->root, key, key_len, value, 0, &old_val);
	if (!old_val) t->size++;
	return old;
}

/*
static void remove_child256(art_node256 *n, art_node **ref, unsigned char c) {
	n->children[c] = NULL;
	n->n.num_children--;

	// Resize to a node48 on underflow, not immediately to prevent
	// trashing if we sit on the 48/49 boundary
	if (n->n.num_children == 37) {
		art_node48 *new_node = (art_node48*)alloc_node(NODE48);
		*ref = (art_node*)new_node;
		copy_header((art_node*)new_node, (art_node*)n);

		int i, pos = 0;
		for (i=0;i<256;i++) {
			if (n->children[i]) {
				new_node->children[pos] = n->children[i];
				new_node->keys[i] = pos + 1;
				pos++;
			}
		}
		free(n);
	}
}

static void remove_child48(art_node48 *n, art_node **ref, unsigned char c) {
	int pos = n->keys[c];
	n->keys[c] = 0;
	n->children[pos-1] = NULL;
	n->n.num_children--;

	if (n->n.num_children == 12) {
		art_node16 *new_node = (art_node16*)alloc_node(NODE16);
		*ref = (art_node*)new_node;
		copy_header((art_node*)new_node, (art_node*)n);

		int i, child = 0;
		for (i=0;i<256;i++) {
			pos = n->keys[i];
			if (pos) {
				new_node->keys[child] = i;
				new_node->children[child] = n->children[pos - 1];
				child++;
			}
		}
		free(n);
	}
}

static void remove_child16(art_node16 *n, art_node **ref, art_node **l) {
	int pos = l - n->children;
	memmove(n->keys+pos, n->keys+pos+1, n->n.num_children - 1 - pos);
	memmove(n->children+pos, n->children+pos+1, (n->n.num_children - 1 - pos)*sizeof(void*));
	n->n.num_children--;

	if (n->n.num_children == 3) {
		art_node4 *new_node = (art_node4*)alloc_node(NODE4);
		*ref = (art_node*)new_node;
		copy_header((art_node*)new_node, (art_node*)n);
		memcpy(new_node->keys, n->keys, 4);
		memcpy(new_node->children, n->children, 4*sizeof(void*));
		free(n);
	}
}

static void remove_child4(art_node4 *n, art_node **ref, art_node **l) {
	int pos = l - n->children;
	memmove(n->keys+pos, n->keys+pos+1, n->n.num_children - 1 - pos);
	memmove(n->children+pos, n->children+pos+1, (n->n.num_children - 1 - pos)*sizeof(void*));
	n->n.num_children--;

	// Remove nodes with only a single child
	if (n->n.num_children == 1) {
		art_node *child = n->children[0];
		if (!IS_LEAF(child)) {
			// Concatenate the prefixes
			int prefix = n->n.partial_len;
			if (prefix < MAX_PREFIX_LEN) {
				n->n.partial[prefix] = n->keys[0];
				prefix++;
			}
			if (prefix < MAX_PREFIX_LEN) {
				int sub_prefix = min(child->partial_len, MAX_PREFIX_LEN - prefix);
				memcpy(n->n.partial+prefix, child->partial, sub_prefix);
				prefix += sub_prefix;
			}

			// Store the prefix in the child
			memcpy(child->partial, n->n.partial, min(prefix, MAX_PREFIX_LEN));
			child->partial_len += n->n.partial_len + 1;
		}
		*ref = child;
		free(n);
	}
}

static void remove_child(art_node *n, art_node **ref, unsigned char c, art_node **l) {
	switch (n->type) {
		case NODE4:
			return remove_child4((art_node4*)n, ref, l);
		case NODE16:
			return remove_child16((art_node16*)n, ref, l);
		case NODE48:
			return remove_child48((art_node48*)n, ref, c);
		case NODE256:
			return remove_child256((art_node256*)n, ref, c);
		default:
			abort();
	}
}


static art_leaf* recursive_delete(art_node *n, art_node **ref, const unsigned char *key, int key_len, int depth) {
	// Search terminated
	if (!n) return NULL;

	// Handle hitting a leaf node
	if (IS_LEAF(n)) {
		art_leaf *l = LEAF_RAW(n);
		if (!leaf_matches(l, key, key_len, depth)) {
			*ref = NULL;
			return l;
		}
		return NULL;
	}

	// Bail if the prefix does not match
	if (n->partial_len) {
		int prefix_len = check_prefix(n, key, key_len, depth);
		if (prefix_len != min(MAX_PREFIX_LEN, n->partial_len)) {
			return NULL;
		}
		depth = depth + n->partial_len;
	}

	// Find child node
	art_node **child = find_child(n, key[depth]);
	if (!child) return NULL;

	// If the child is leaf, delete from this node
	if (IS_LEAF(*child)) {
		art_leaf *l = LEAF_RAW(*child);
		if (!leaf_matches(l, key, key_len, depth)) {
			remove_child(n, ref, key[depth], child);
			return l;
		}
		return NULL;

		// Recurse
	} else {
		return recursive_delete(*child, child, key, key_len, depth+1);
	}
}
*/

/**
 * Deletes a value from the ART tree
 * @arg t The tree
 * @arg key The key
 * @arg key_len The length of the key
 * @return NULL if the item was not found, otherwise
 * the value pointer is returned.
 */
/*
void* art_delete(art_tree *t, const unsigned char *key, int key_len) {
	art_leaf *l = recursive_delete(t->root, &t->root, key, key_len, 0);
	if (l) {
		t->size--;
		void *old = l->value;
		free(l);
		return old;
	}
	return NULL;
}
*/

// Recursively iterates over the tree
/*
static int recursive_iter(art_node *n, art_callback cb, void *data) {
	// Handle base cases
	if (!n) return 0;
	if (IS_LEAF(n)) {
		art_leaf *l = LEAF_RAW(n);
		return cb(data, (const unsigned char*)l->key, l->key_len, l->value);
	}

	int i, idx, res;
	switch (n->type) {
		case NODE4:
			for (i=0; i < n->num_children; i++) {
				res = recursive_iter(((art_node4*)n)->children[i], cb, data);
				if (res) return res;
			}
			break;

		case NODE16:
			for (i=0; i < n->num_children; i++) {
				res = recursive_iter(((art_node16*)n)->children[i], cb, data);
				if (res) return res;
			}
			break;

		case NODE48:
			for (i=0; i < 256; i++) {
				idx = ((art_node48*)n)->keys[i];
				if (!idx) continue;

				res = recursive_iter(((art_node48*)n)->children[idx-1], cb, data);
				if (res) return res;
			}
			break;

		case NODE256:
			for (i=0; i < 256; i++) {
				if (!((art_node256*)n)->children[i]) continue;
				res = recursive_iter(((art_node256*)n)->children[i], cb, data);
				if (res) return res;
			}
			break;

		default:
			abort();
	}
	return 0;
}
*/

/**
 * Iterates through the entries pairs in the map,
 * invoking a callback for each. The call back gets a
 * key, value for each and returns an integer stop value.
 * If the callback returns non-zero, then the iteration stops.
 * @arg t The tree to iterate over
 * @arg cb The callback function to invoke
 * @arg data Opaque handle passed to the callback
 * @return 0 on success, or the return of the callback.
 */
/*
int art_iter(art_tree *t, art_callback cb, void *data) {
	return recursive_iter(t->root, cb, data);
}
*/

/**
 * Checks if a leaf prefix matches
 * @return 0 on success.
 */
/*
static int leaf_prefix_matches(const art_leaf *n, const unsigned char *prefix, int prefix_len) {
	// Fail if the key length is too short
	if (n->key_len < (uint32_t)prefix_len) return 1;

	// Compare the keys
	return memcmp(n->key, prefix, prefix_len);
}
*/

/**
 * Iterates through the entries pairs in the map,
 * invoking a callback for each that matches a given prefix.
 * The call back gets a key, value for each and returns an integer stop value.
 * If the callback returns non-zero, then the iteration stops.
 * @arg t The tree to iterate over
 * @arg prefix The prefix of keys to read
 * @arg prefix_len The length of the prefix
 * @arg cb The callback function to invoke
 * @arg data Opaque handle passed to the callback
 * @return 0 on success, or the return of the callback.
 */
/*
int art_iter_prefix(art_tree *t, const unsigned char *key, int key_len, art_callback cb, void *data) {
	art_node **child;
	art_node *n = t->root;
	int prefix_len, depth = 0;
	while (n) {
		// Might be a leaf
		if (IS_LEAF(n)) {
			n = (art_node*)LEAF_RAW(n);
			// Check if the expanded path matches
			if (!leaf_prefix_matches((art_leaf*)n, key, key_len)) {
				art_leaf *l = (art_leaf*)n;
				return cb(data, (const unsigned char*)l->key, l->key_len, l->value);
			}
			return 0;
		}

		// If the depth matches the prefix, we need to handle this node
		if (depth == key_len) {
			art_leaf *l = minimum(n);
			if (!leaf_prefix_matches(l, key, key_len))
				return recursive_iter(n, cb, data);
			return 0;
		}

		// Bail if the prefix does not match
		if (n->partial_len) {
			prefix_len = prefix_mismatch(n, key, key_len, depth);

			// If there is no match, search is terminated
			if (!prefix_len)
				return 0;

			// If we've matched the prefix, iterate on this node
			else if (depth + prefix_len == key_len) {
				return recursive_iter(n, cb, data);
			}

			// if there is a full match, go deeper
			depth = depth + n->partial_len;
		}

		// Recursively search
		child = find_child(n, key[depth]);
		n = (child) ? *child : NULL;
		depth++;
	}
	return 0;
}
*/
