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

unsigned long IN_count = 0;
unsigned long LN_count = 0;
unsigned long clflush_count = 0;
unsigned long mfence_count = 0;

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

static inline int max(int a, int b) {
	return (a > b) ? a : b;
}

void add_log_entry(tree *t, void *addr, unsigned int size, unsigned char type)
{
	log_entry *log;
	int i, remain_size;

	remain_size = size - ((size / LOG_DATA_SIZE) * LOG_DATA_SIZE);

	if ((char *)t->start_log->next_offset == 
			(t->start_log->log_data + LOG_AREA_SIZE))
		t->start_log->next_offset = (log_entry *)t->start_log->log_data;

	if (size <= LOG_DATA_SIZE) {
		log = t->start_log->next_offset;
		log->size = size;
		log->type = type;
		log->addr = addr;
		memcpy(log->data, addr, size);

		if (type == LE_DATA)
			flush_buffer(log, sizeof(log_entry), false);
		else
			flush_buffer(log, sizeof(log_entry), true);

		t->start_log->next_offset = t->start_log->next_offset + 1;
	} else {
		void *next_addr = addr;

		for (i = 0; i < size / LOG_DATA_SIZE; i++) {
			log = t->start_log->next_offset;
			log->size = LOG_DATA_SIZE;
			log->type = type;
			log->addr = next_addr;
			memcpy(log->data, next_addr, LOG_DATA_SIZE);

			flush_buffer(log, sizeof(log_entry), false);

			t->start_log->next_offset = t->start_log->next_offset + 1;
			if ((char *)t->start_log->next_offset == 
					(t->start_log->log_data + LOG_AREA_SIZE))
				t->start_log->next_offset = (log_entry *)t->start_log->log_data;

			next_addr = (char *)next_addr + LOG_DATA_SIZE;
		}

		if (remain_size > 0) {
			log = t->start_log->next_offset;
			log->size = LOG_DATA_SIZE;
			log->type = type;
			log->addr = next_addr;
			memcpy(log->data, next_addr, remain_size);

			flush_buffer(log, sizeof(log_entry), false);
			
			t->start_log->next_offset = t->start_log->next_offset + 1;
		}
	}
}

key_item *make_key_item(unsigned char *key, int key_len)
{
	key_item *new_key = malloc(sizeof(key_item) + key_len);
	new_key->key_len = key_len;
	memcpy(new_key->key, key, key_len);

	flush_buffer(new_key, sizeof(key_item) + key_len, false);

	return new_key;
}

LN *allocLNode()
{
	LN *node = malloc(sizeof(LN));
	node->type = THIS_LN;
	node->bitmap = 0;
	LN_count++;
	return node;
}

IN *allocINode()
{
	IN *node = malloc(sizeof(IN));
	node->type = THIS_IN;
	node->nKeys = 0;
	IN_count++;
	return node;
}

tree *initTree()
{
	tree *t =malloc(sizeof(tree)); 
	t->root = allocLNode();
	((LN *)t->root)->pNext = NULL;
	t->start_log = malloc(sizeof(log_area));
	t->start_log->next_offset = (log_entry *)t->start_log->log_data;
	return t;
}

extern uint32_t jenkins_hash(const void *key, size_t length);

unsigned char hash(unsigned char *key, int key_len) {
	uint32_t jenkins = jenkins_hash(key, key_len);
	unsigned char hash_key = jenkins % 256;
	return hash_key;
}

void insertion_sort(entry *base, int num)
{
	int i, j;
	entry temp;

	for (i = 1; i < num; i++) {
		for (j = i; j > 0; j--) {
			if (memcmp((base[j - 1].key)->key, (base[j].key)->key, max((base[j - 1].key)->key_len, (base[j].key)->key_len)) > 0) {
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

void *Lookup(tree *t, unsigned char *key, int key_len)
{
	unsigned long loc = 0;
	void *value = NULL;
	LN *curr = t->root;
	curr = find_leaf_node(curr, key, key_len);

	while (loc < NUM_LN_ENTRY) {
		loc = find_next_bit(&curr->bitmap, BITMAP_SIZE, loc);
		if (loc == BITMAP_SIZE)
			break;
		
		if (curr->fingerprints[loc] == hash(key, key_len) &&
				memcmp((curr->entries[loc].key)->key, key, 
					max((curr->entries[loc].key)->key_len, key_len)) == 0) {
			value = curr->entries[loc].ptr;
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
	unsigned long i, entry_num, loc, search_count = 0;
	LN *curr = t->root;
	entry *sorted_entry = malloc(NUM_LN_ENTRY * sizeof(entry));
	curr = find_leaf_node(curr, start_key);

	while (curr != NULL) {
		loc = 0;
		entry_num = 0;

		while (loc < NUM_LN_ENTRY) {
			loc = find_next_bit(&curr->bitmap, BITMAP_SIZE, loc);
			if (loc == BITMAP_SIZE)
				break;

			sorted_entry[entry_num] = curr->entries[loc];
			loc++;
			entry_num++;
		}
		insertion_sort(sorted_entry, entry_num);

		for (i = 0; i < entry_num; i++) {
			buf[search_count] = *(unsigned long *)sorted_entry[i].ptr;
			search_count++;
			if (search_count == num)
				return ;
		}
		curr = curr->pNext;
	}
}
*/
int Search(IN *curr, unsigned char *key, int key_len)
{
	int low = 0, mid = 0;
	int high = curr->nKeys - 1;
	int len, decision;

	while (low <= high){
		mid = (low + high) / 2;
		len = max((curr->keys[mid])->key_len, key_len);
		decision = memcmp((curr->keys[mid])->key, key, len);
		if (decision > 0)
			high = mid - 1;
		else if (decision < 0)
			low = mid + 1;
		else
			break;
	}

	if (low > mid) 
		mid = low;

	return mid;
}

void *find_leaf_node(void *curr, unsigned char *key, int key_len) 
{
	unsigned long loc;

	if (((LN *)curr)->type == THIS_LN) 
		return curr;
	loc = Search(curr, key, key_len);

	if (loc > ((IN *)curr)->nKeys - 1) 
		return find_leaf_node(((IN *)curr)->ptr[loc - 1], key, key_len);
	else if (memcmp((((IN *)curr)->keys[loc])->key, key, max((((IN *)curr)->keys[loc])->key_len, key_len)) <= 0)
		return find_leaf_node(((IN *)curr)->ptr[loc], key, key_len);
	else if (loc == 0) 
		return find_leaf_node(((IN *)curr)->leftmostPtr, key, key_len);
	else 
		return find_leaf_node(((IN *)curr)->ptr[loc - 1], key, key_len);
}


void Insert(tree *t, unsigned char *key, int key_len, void *value)
{
	LN *curr = t->root;
	/* Find proper leaf */
	curr = find_leaf_node(curr, key, key_len);

	/* Make new key item */
	key_item *new_item = make_key_item(key, key_len);

	/* Check overflow & split */
	if(curr->bitmap == IS_FULL) {
		int j;
		LN *split_LNode = allocLNode();
		entry *valid_entry = malloc(NUM_LN_ENTRY * sizeof(entry));

		add_log_entry(t, curr, sizeof(LN), LE_DATA);

		split_LNode->pNext = curr->pNext;

		for (j = 0; j < NUM_LN_ENTRY; j++)
			valid_entry[j] = curr->entries[j];

		insertion_sort(valid_entry, NUM_LN_ENTRY);

		curr->bitmap = 0;
		for (j = 0; j < MIN_LN_ENTRIES; j++)
			insert_in_leaf_noflush(curr, valid_entry[j].key,
					valid_entry[j].ptr);

		for (j = MIN_LN_ENTRIES; j < NUM_LN_ENTRY; j++)
			insert_in_leaf_noflush(split_LNode, valid_entry[j].key,
					valid_entry[j].ptr);

		free(valid_entry);

		if (memcmp((split_LNode->entries[0].key)->key, new_item->key, max((split_LNode->entries[0].key)->key_len, key_len)) > 0) {
			insert_in_leaf_noflush(curr, new_item, value);
		} else
			insert_in_leaf_noflush(split_LNode, new_item, value);

		insert_in_parent(t, curr, split_LNode->entries[0].key, split_LNode);

		curr->pNext = split_LNode;

		flush_buffer(curr, sizeof(LN), false);
		flush_buffer(split_LNode, sizeof(LN), false);

		add_log_entry(t, NULL, 0, LE_COMMIT);
	}
	else{
		insert_in_leaf(curr, new_item, value);
	}
}

int insert_in_leaf_noflush(LN *curr, key_item *new_item, void *value)
{
	int errval = -1;
	unsigned long index;
	index = find_next_zero_bit(&curr->bitmap, BITMAP_SIZE, 0);
	if (index == BITMAP_SIZE)
		return errval;

	curr->entries[index].key = new_item;
	curr->entries[index].ptr = value;
	curr->fingerprints[index] = hash(new_item->key, new_item->key_len);
	curr->bitmap = curr->bitmap + (0x1UL << index);
	return index;
}

void insert_in_leaf(LN *curr, key_item *new_item, void *value)
{
	unsigned long index;
	index = find_next_zero_bit(&curr->bitmap, BITMAP_SIZE, 0);
	if (index == BITMAP_SIZE)
		return ;

	curr->entries[index].key = new_item;
	curr->entries[index].ptr = value;
	curr->fingerprints[index] = hash(new_item->key, new_item->key_len);		//change with jenkins
	curr->bitmap = curr->bitmap + (0x1UL << index);

	flush_buffer(&curr->entries[index], sizeof(entry), false);
	flush_buffer(&curr->fingerprints[index], sizeof(unsigned char), false);
	flush_buffer(&curr->bitmap, sizeof(unsigned long), true);
}

void insert_in_inner(IN *curr, key_item *inserted_item, void *child)
{
	int loc, mid, j;

	mid = Search(curr, inserted_item->key, inserted_item->key_len);

	for (j = (curr->nKeys - 1); j >= mid; j--) {
		curr->keys[j + 1] = curr->keys[j];
		curr->ptr[j + 1] = curr->ptr[j];
	}

	curr->keys[mid] = inserted_item;
	curr->ptr[mid] = child;

	curr->nKeys++;
}

void insert_in_parent(tree *t, void *curr, key_item *inserted_item, void *splitNode) {
	if (curr == t->root) {
		IN *root = allocINode();
		root->leftmostPtr = curr;
		root->keys[0] = inserted_item;
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
		insert_in_inner(parent, inserted_item, splitNode);
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

		if (memcmp((split_INode->keys[0])->key, inserted_item->key,
					max((split_INode->keys[0])->key_len, inserted_item->key_len)) > 0) {
			insert_in_inner(parent, inserted_item, splitNode);
			((IN *)splitNode)->parent = parent;
		}
		else {
			((IN *)splitNode)->parent = split_INode;
			insert_in_inner(split_INode, inserted_item, splitNode);
		}

		insert_in_parent(t, parent, split_INode->keys[0], split_INode);
	}
}

/*
void *Update(tree *t, unsigned long key, void *value)
{
	unsigned long loc = 0;
	LN *curr = t->root;
	curr = find_leaf_node(curr, key);

	while (loc < NUM_LN_ENTRY) {
		loc = find_next_bit(&curr->bitmap, BITMAP_SIZE, loc);
		if (loc == BITMAP_SIZE)
			break;
		
		if (curr->fingerprints[loc] == hash(key) &&
				curr->entries[loc].key == key) {
			curr->entries[loc].ptr = value;
			flush_buffer(&curr->entries[loc].ptr, 8, true);
			return curr->entries[loc].ptr;
		}
		loc++;
	}

	return NULL;
}
*/

/*
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
