#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <x86intrin.h>
#include <malloc.h>
#include <stdint.h>
#include <time.h>
#include "FPTree.h"

#define mfence() asm volatile("mfence":::"memory")
#define BITOP_WORD(nr)	((nr) / BITS_PER_LONG)

void flush_buffer(void *buf, unsigned int len, bool fence)
{
	unsigned int i;
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

void add_redo_logentry()
{
	redo_log_entry *log = malloc(sizeof(redo_log_entry));
	log->addr = 0;
	log->new_value = 0;
	log->type = LE_DATA;
	flush_buffer(log, sizeof(redo_log_entry), false);
}

void add_commit_entry()
{
	commit_entry *commit_log = malloc(sizeof(commit_entry));
	commit_log->type = LE_COMMIT;
	flush_buffer(commit_log, sizeof(commit_entry), true);
}

LN *allocLNode()
{
	LN *node = malloc(sizeof(LN));
	node->type = THIS_LN;
	node->bitmap = 0;
	return node;
}

IN *allocINode()
{
	IN *node = malloc(sizeof(IN));
	node->type = THIS_IN;
	node->nKeys = 0;
	return node;
}

tree *initTree()
{
	tree *t =malloc(sizeof(tree)); 
	t->root = allocLNode(); 
	t->height = 0;
	return t;
}

unsigned char hash(unsigned long key) {
	unsigned char hash_key = key % 256;
	return hash_key;
}

void insertion_sort(entry *base, int num)
{
	int i, j;
	entry temp;

	for (i = 1; i < num; i++) {
		for (j = i; j > 0; j--) {
			if (base[j - 1].key > base[j].key) {
				temp = base[j - 1];
				base[j - 1] = base[j];
				base[j] = temp;
			} else
				break;
		}
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
 * Find the next zero bit in a memory region.
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
	if (tmp == ~0UL)	/* Are any bits zero? */
		return result + size;	/* Nope */
found_middle:
	return result + ffz(tmp);
}

void *Lookup(tree *t, unsigned long key)
{
	unsigned long loc = 0;
	void *value = NULL;
	LN *curr = t->root;
	curr = find_leaf_node(curr, key);

	while (loc < NUM_LN_ENTRY) {
		loc = find_next_bit(&curr->bitmap, BITMAP_SIZE, loc);
		
		if (curr->fingerprints[loc] == hash(key) &&
				curr->entries[loc].key == key) {
			value = curr->entries[loc].ptr;
//			printf("value = %lu\n", *(unsigned long *)value);
			break;
		}
		loc++;
	}

	return value;
}
/*
void Range_Lookup(tree *t, unsigned long start_key, unsigned int num, 
		unsigned long buf[])
{
	int loc, i;
	unsigned long search_count = 0;
	struct timespec t1, t2;
	unsigned long elapsed_time;
	node *curr = t->root;

	curr = find_leaf_node(curr, start_key);
	loc = Search(curr, curr->slot, start_key);
	while (search_count < num) {
		for (i = loc; i <= curr->slot[0]; i++) {
			buf[search_count] = *(unsigned long *)curr->entries[curr->slot[i]].ptr;
			search_count++;
			if(search_count == num) {
				return ;
			}
		}

		curr = curr->leftmostPtr;
		if (curr == NULL) {
			printf("error\n");
			return ;
		}
		loc = 1;
	}
}
*/

int Search(IN *curr, unsigned long key)
{
	int low = 0, mid = 0;
	int high = curr->nKeys - 1;

	while (low <= high){
		mid = (low + high) / 2;
		if (curr->keys[mid] > key)
			high = mid - 1;
		else if (curr->keys[mid] < key)
			low = mid + 1;
		else
			break;
	}

	if (low > mid) 
		mid = low;

	return mid;
}

void *find_leaf_node(void *curr, unsigned long key) 
{
	unsigned long loc;

	if (((LN *)curr)->type == THIS_LN) 
		return curr;
	loc = Search(curr, key);

	if (loc > ((IN *)curr)->nKeys - 1) 
		return find_leaf_node(((IN *)curr)->ptr[loc - 1], key);
	else if (((IN *)curr)->keys[loc] <= key) 
		return find_leaf_node(((IN *)curr)->ptr[loc], key);
	else if (loc == 0) 
		return find_leaf_node(((IN *)curr)->leftmostPtr, key);
	else 
		return find_leaf_node(((IN *)curr)->ptr[loc - 1], key);
}


void Insert(tree *t, unsigned long key, void *value)
{
	LN *curr = t->root;
	/* Find proper leaf */
	curr = find_leaf_node(curr, key);

	/* Check overflow & split */
	if(curr->bitmap == IS_FULL) {
		int j, num = 0;
		unsigned long loc = 0;
		LN *split_LNode = allocLNode();
		entry *valid_entry = malloc(NUM_LN_ENTRY * sizeof(entry));

		split_LNode->pNext = curr->pNext;

		while (loc < BITMAP_SIZE) {
			loc = find_next_bit(&curr->bitmap, BITMAP_SIZE, loc);
			valid_entry[num] = curr->entries[loc];
			num++;
			loc++;
		}

		insertion_sort(valid_entry, num);

		curr->bitmap = 0;
		for (j = 0; j < MIN_LN_ENTRIES; j++)
			insert_in_leaf_noflush(curr, valid_entry[j].key,
					valid_entry[j].ptr);

		for (j = MIN_LN_ENTRIES; j < num; j++)
			insert_in_leaf_noflush(split_LNode, valid_entry[j].key,
					valid_entry[j].ptr);

		free(valid_entry);

		if (split_LNode->entries[0].key > key) {
			add_redo_logentry();	//slot redo logging for insert_in_leaf_noflush
			add_redo_logentry();
			loc = insert_in_leaf_noflush(curr, key, value);
		//	flush_buffer(&(curr->entries[loc]), sizeof(entry), false);
		} else
			insert_in_leaf_noflush(split_LNode, key, value);

		insert_in_parent(t, curr, split_LNode->entries[0].key, split_LNode);
		add_redo_logentry();
		curr->pNext = split_LNode;
		add_commit_entry();
	}
	else{
		insert_in_leaf(curr, key, value);
	}
}

int insert_in_leaf_noflush(LN *curr, unsigned long key, void *value)
{
	unsigned long index;
	index = find_next_zero_bit(&curr->bitmap, BITMAP_SIZE, 0);

	curr->entries[index].key = key;
	curr->entries[index].ptr = value;
	curr->fingerprints[index] = hash(key);
	curr->bitmap = curr->bitmap + (0x1UL << index);
	return index;
}

void insert_in_leaf(LN *curr, unsigned long key, void *value)
{
	unsigned long index;
	index = find_next_zero_bit(&curr->bitmap, BITMAP_SIZE, 0);

	curr->entries[index].key = key;
	curr->entries[index].ptr = value;
	curr->fingerprints[index] = hash(key);
	curr->bitmap = curr->bitmap + (0x1UL << index);
}

void insert_in_inner(IN *curr, unsigned long key, void *child)
{
	int loc, mid, j;

//	mid = Search(curr, key);

	for (j = 0; j < curr->nKeys; j++) {
		if (curr->keys[j] > key) {
			mid = j;
			break;
		}
	}

	mid = j;

	for (j = (curr->nKeys - 1); j >= mid; j--) {
		curr->keys[j + 1] = curr->keys[j];
		curr->ptr[j + 1] = curr->ptr[j];
	}

	curr->keys[mid] = key;
	curr->ptr[mid] = child;

	curr->nKeys++;
}

void insert_in_parent(tree *t, void *curr, unsigned long key, void *splitNode) {
	if (curr == t->root) {
		IN *root = allocINode();
		root->leftmostPtr = curr;
		root->keys[0] = key;
		root->ptr[0] = splitNode;
		root->nKeys++;

		((IN *)splitNode)->parent = root;
		((IN *)curr)->parent = root;
		t->root = root;
		return ;
	}

	IN *parent;

	if (((IN *)curr)->type == THIS_IN)
		parent = ((IN *)curr)->parent;
	else
		parent = ((LN *)curr)->parent;

	if (parent->nKeys < NUM_IN_ENTRY) {
		insert_in_inner(parent, key, splitNode);
		((IN *)splitNode)->parent = parent;
	} else {
		int i, j, loc, parent_nKeys;
		IN *split_INode = allocINode();
		parent_nKeys = parent->nKeys;

		for (j = MIN_IN_ENTRIES, i = 0; j < parent_nKeys; j++, i++) {
			split_INode->keys[i] = parent->keys[j];
			split_INode->ptr[i] = parent->ptr[j];
			((IN *)split_INode->ptr[i])->parent = split_INode;
			split_INode->nKeys++;
			parent->nKeys--;
		}

		if (split_INode->keys[0] > key) {
			add_redo_logentry();
			add_redo_logentry();
			insert_in_inner(parent, key, splitNode);
			((IN *)splitNode)->parent = parent;
		}
		else {
			((IN *)splitNode)->parent = split_INode;
			insert_in_inner(split_INode, key, splitNode);
		}

		insert_in_parent(t, parent, split_INode->keys[0], split_INode);
	}
}

/*
void *Update(tree *t, unsigned long key, void *value)
{
	node *curr = t->root;
	curr = find_leaf_node(curr, key);
	int loc = Search(curr, curr->slot, key);

	if (loc > curr->slot[0]) 
		loc = curr->slot[0];

	if (curr->entries[curr->slot[loc]].key != key || loc > curr->slot[0])
		return NULL;

	curr->entries[curr->slot[loc]].ptr = value;
	flush_buffer(&curr->entries[curr->slot[loc]].ptr, 8, true);

	return curr->entries[curr->slot[loc]].ptr;
}

int delete_in_leaf(node *curr, unsigned long key)
{
	int mid, j;

	curr->bitmap = curr->bitmap - 1;
	flush_buffer(&curr->bitmap, sizeof(unsigned long), true);

	mid = Search(curr, curr->slot, key);

	for (j = curr->slot[0]; j > mid; j--)
		curr->slot[j - 1] = curr->slot[j];

	curr->slot[0] = curr->slot[0] - 1;

	flush_buffer(curr->slot, sizeof(curr->slot), true);
	
	curr->bitmap = curr->bitmap + 1 - (0x1UL << (mid + 1));
	flush_buffer(&curr->bitmap, sizeof(unsigned long), true);
	return 0;
}

int Delete(tree *t, unsigned long key)
{
	int numEntries, errval = 0;
	node *curr = t->root;
	
	curr = find_leaf_node(curr, key);

	numEntries = curr->slot[0];
	if (numEntries <= 1)
		errval = -1;
	else
		errval = delete_in_leaf(curr, key);

	return errval;
}
*/
