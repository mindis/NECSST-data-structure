#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <emmintrin.h>
#include <assert.h>
#include <x86intrin.h>
#include "art_l.h"

#define mfence() asm volatile("mfence":::"memory")

unsigned long node_count = 0;
unsigned long leaf_count = 0;
unsigned long clflush_count = 0;
unsigned long mfence_count = 0;

/**
 * Macros to manipulate pointer tags
 */
#define IS_LEAF(x) (((uintptr_t)x & 1))
#define SET_LEAF(x) ((void*)((uintptr_t)x | 1))
#define LEAF_RAW(x) ((art_leaf*)((void*)((uintptr_t)x & ~1)))

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

static int get_index(unsigned long key, int depth) 
{
	int index;

	index = ((key >> ((MAX_DEPTH - depth) * NODE_BITS)) & LOW_BIT_MASK);
	return index;
}

/**
 * Allocates a node of the given type,
 * initializes to zero and sets the type.
 */
static art_node16* alloc_node() {
	art_node16 * n;
	n = (art_node16 *)calloc(1, sizeof(art_node16));
	node_count++;
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

static art_node16** find_child(art_node16 *n, unsigned char c) {
	art_node16 *p;

	p = n;
	if (p->children[c])
		return &p->children[c];

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
/*
static int check_prefix(const art_node *n, const unsigned long key, int key_len, int depth) {
	int max_cmp = min(min(n->partial_len, MAX_PREFIX_LEN), (key_len * INDEX_BITS) - depth);
	int idx;
	for (idx=0; idx < max_cmp; idx++) {
		if (n->partial[idx] != get_index(key, depth + idx))
			return idx;
	}
	return idx;
}
*/

/**
 * Checks if a leaf matches
 * @return 0 on success.
 */
static int leaf_matches(const art_leaf *n, unsigned long key, int key_len, int depth) {
	(void)depth;
	// Fail if the key lengths are different
//	if (n->key_len != (uint32_t)key_len) return 1;

	// Compare the keys starting at the depth
	//	return memcmp(n->key, key, key_len);
	return !(n->key == key);
}

/**
 * Searches for a value in the ART tree
 * @arg t The tree
 * @arg key The key
 * @arg key_len The length of the key
 * @return NULL if the item was not found, otherwise
 * the value pointer is returned.
 */
void* art_search(const art_tree *t, const unsigned long key, int key_len) {
	art_node16 **child;
	art_node16 *n = t->root;
	int depth = 0;

	while (n) {
		// Might be a leaf
		if (IS_LEAF(n)) {
			n = (art_node16*)LEAF_RAW(n);
			// Check if the expanded path matches
			if (!leaf_matches((art_leaf*)n, key, key_len, depth)) {
				return ((art_leaf*)n)->value;
			}
			return NULL;
		}

		// Recursively search
		child = find_child(n, get_index(key, depth));
		n = (child) ? *child : NULL;
		depth++;
	}
	return NULL;
}

// Find the minimum leaf under a node
static art_leaf* minimum(const art_node16 *n) {
	// Handle base cases
	if (!n) return NULL;
	if (IS_LEAF(n)) return LEAF_RAW(n);

	int idx = 0;

	while (!((art_node16 *)n)->children[idx]) idx++;
	return minimum(((art_node16 *)n)->children[idx]);
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

static art_leaf* make_leaf(const unsigned long key, int key_len, void *value) {
	art_leaf *l = (art_leaf*)malloc(sizeof(art_leaf));
	l->value = value;
	l->key_len = key_len;
	l->key = key;
	leaf_count++;
	return l;
}

static int longest_common_prefix(art_leaf *l1, art_leaf *l2, int depth) {
	int idx, max_cmp = (min(l1->key_len, l2->key_len) * INDEX_BITS) - depth;
	int l1_index, l2_index;

	for (idx=0; idx < max_cmp; idx++) {
		if (get_index(l1->key, depth + idx) != get_index(l2->key, depth + idx))
			return idx;
	}
	return idx;
}

/*
   static void copy_header(art_node *dest, art_node *src) {
   dest->num_children = src->num_children;
   dest->partial_len = src->partial_len;
   memcpy(dest->partial, src->partial, min(MAX_PREFIX_LEN, src->partial_len));
   }
   */

static void add_child(art_node16 *n, art_node16 **ref, unsigned char c, void *child) {
	(void)ref;
	n->children[c] = (art_node16*)child;
}

/**
 * Calculates the index at which the prefixes mismatch
static int prefix_mismatch(const art_node *n, const unsigned long key, int key_len, int depth, art_leaf **l) {
	
	   int max_cmp = min(min(MAX_PREFIX_LEN, n->partial_len), (key_len * INDEX_BITS) - depth);
	   int idx;
	   for (idx=0; idx < max_cmp; idx++) {
	   if (n->partial[idx] != get_index(key, depth + idx))
	   return idx;
	   }

	int max_cmp;
	int idx;
	// If the prefix is short we can avoid finding a leaf
	//if (n->partial_len > MAX_PREFIX_LEN) {
	// Prefix is longer than what we've checked, find a leaf
	*l = minimum(n);
	max_cmp = (min((*l)->key_len, key_len) * INDEX_BITS) - depth;
	for (idx = 0; idx < max_cmp; idx++) {
		if (get_index((*l)->key, idx + depth) != get_index(key, depth + idx))
			return idx;
	}
	//}
	return idx;
}
*/

static void* recursive_insert(art_node16 *n, art_node16 **ref, const unsigned long key,
		int key_len, void *value, int depth, int *old)
{
	// If we are at a NULL node, inject a leaf
	if (!n) {
		*ref = (art_node16 *)SET_LEAF(make_leaf(key, key_len, value));
		flush_buffer(*ref, sizeof(art_leaf), false);
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
		art_node16 *new_node = alloc_node();

		// Create a new leaf
		art_leaf *l2 = make_leaf(key, key_len, value);
		
		// Remapping leaves
		if (get_index(l->key, depth) == get_index(l2->key, depth)) {
			int l_index, l2_index;
			depth++;
			art_node16 *parent_node = new_node;
			art_node16 *next_node = alloc_node();
			
			while(next_node) {
				l_index = get_index(l->key, depth);
				l2_index = get_index(l2->key, depth);

				parent_node->children[get_index(l->key, depth - 1)] = next_node;
				flush_buffer(parent_node, sizeof(art_node16), false);

				if (l_index != l2_index) {
					add_child(next_node, NULL, l_index, SET_LEAF(l));
					add_child(next_node, NULL, l2_index, SET_LEAF(l2));
					break;
				} else {
					parent_node = next_node;
					next_node = alloc_node();
					depth++;
				}
			}

			*ref = new_node;
			flush_buffer(l2, sizeof(art_leaf), false);
			flush_buffer(ref, 8, true);
			return NULL;
		}

		// Add the leafs to the new node4
		*ref = new_node;
		add_child(new_node, ref, get_index(l->key, depth), SET_LEAF(l));
		add_child(new_node, ref, get_index(l2->key, depth), SET_LEAF(l2));

		flush_buffer(new_node, sizeof(art_node16), false);
		flush_buffer(l2, sizeof(art_leaf), false);
		flush_buffer(ref, 8, true);
		return NULL;
	}

	// Find a child to recurse to
	art_node16 **child = find_child(n, get_index(key, depth));
	if (child) {
		return recursive_insert(*child, child, key, key_len, value, depth + 1, old);
	}

	// No child, node goes within us
	art_leaf *l = make_leaf(key, key_len, value);
	add_child(n, ref, get_index(key, depth), SET_LEAF(l));

	flush_buffer(l, sizeof(art_leaf), false);
	flush_buffer(&n->children[get_index(key, depth)], 8, true);
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
void* art_insert(art_tree *t, const unsigned long key, int key_len, void *value) {
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
