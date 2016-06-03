#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86intrin.h>
#include <malloc.h>
#include <time.h>
#include "w_radix_tree.h"

#define mfence() asm volatile("mfence":::"memory")
unsigned long elapsed_node_flush = 0;
unsigned long elapsed_entry_flush = 0;
unsigned long elapsed_log_flush = 0;
unsigned long elapsed_leaf_search = 0;
unsigned long elapsed_entry_search = 0;
unsigned long node_count = 0;
unsigned long entry_count = 0;
unsigned long log_count = 0;

static inline void clflush(volatile void *__p)
{
	asm volatile("clflush %0" : "+m" (*(volatile char *)__p));
}

void flush_buffer(void *buf, unsigned int len)
{
	unsigned int i;
	len = len + ((unsigned long)(buf) & (CACHE_LINE_SIZE - 1));
	mfence();
	for (i = 0; i < len; i += CACHE_LINE_SIZE)
		asm volatile ("clflush %0\n" : "+m" (*(char *)(buf+i)));
	mfence();
}

void flush(void *start)
{
	start = (void *)((unsigned long)start &~(CACHE_LINE_SIZE - 1));
	mfence();
	clflush(start);
	mfence();
}

void flush_range(void *start, void *end)
{
	void *addr;

	start = (void *)((unsigned long)start &~(CACHE_LINE_SIZE - 1));
	mfence();
	for(addr = start; addr < end; addr += CACHE_LINE_SIZE)
		clflush(addr);
	mfence();
}

void add_logentry()
{
	/* need to be fixed */
	struct timespec t1, t2;
	logentry *log = malloc(sizeof(logentry));
	log->addr = 0;
	log->old_value = 0;
	log->new_value = 0;
	log_count++;
	clock_gettime(CLOCK_MONOTONIC, &t1);
	flush_buffer(log, sizeof(logentry));
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_log_flush += (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_log_flush += (t2.tv_nsec - t1.tv_nsec);
}

node *search_to_leftmost_leaf(node *level_ptr, unsigned char height)
{
	int i;
	node *left_most;
	
	if (height != 1) {
		for (i = 0; i < (0x1UL << META_NODE_SHIFT); i--) {
			if (level_ptr->entry_ptr[i] != NULL) {
				left_most = search_to_leftmost_leaf(level_ptr->entry_ptr[i], height - 1);
				return left_most;
			}
		}
	}
	else {
		left_most = level_ptr;
		return left_most;
	}
}
/*
node *search_to_next_leaf(node *next_branch, unsigned char height)
{
	int i;
	node *next_leaf;

	if (height != 1) {
		for (i = 0; i < (1 << META_NODE_SHIFT); i++) {
			if (next_branch->entry_ptr[i] != NULL) {
				next_leaf = search_to_next_leaf(next_branch->entry_ptr[i], height - 1);
				return next_leaf;
			}
		}
	}
	else {
		next_leaf = next_branch;
		return next_leaf;
	}
}
*/
node *search_to_prev_leaf(node *prev_branch, unsigned char height)
{
	int i;
	node *prev_leaf;

	if (height != 1) {
		for (i = ((0x1UL << META_NODE_SHIFT) - 1); i >= 0; i--) {
			if (prev_branch->entry_ptr[i] != NULL) {
				prev_leaf = search_to_prev_leaf(prev_branch->entry_ptr[i], height - 1);
				return prev_leaf;
			}
		}
	}
	else {
		prev_leaf = prev_branch;
		return prev_leaf;
	}
}
/*
node *find_next_leaf(tree *t, node *parent, unsigned int index, unsigned char height)
{
	int i;
	node *next_leaf;

	for (i = (index + 1); i < (1 << META_NODE_SHIFT); i++) {
		if (parent->entry_ptr[i] != NULL) {
			next_leaf = search_to_next_leaf(parent->entry_ptr[i], height - 1);
			return next_leaf;
		}
	}

	if (t->height > height) {
		next_leaf = find_next_leaf(t, parent->parent_ptr, parent->p_index, height + 1);
		return next_leaf;
	}
	else
		return NULL;
}
*/
node *find_prev_leaf(node *parent, unsigned int index, unsigned char height)
{
	int i;
	node *prev_leaf;

	for (i = (index - 1); i >= 0; i--) {
		if (parent->entry_ptr[i] != NULL) {
			prev_leaf = search_to_prev_leaf(parent->entry_ptr[i], height - 1);
			return prev_leaf;
		}
	}

	prev_leaf = find_prev_leaf(parent->parent_ptr, parent->p_index, height + 1);
	return prev_leaf;
}

node *allocINode(node *parent, unsigned int index)
{
	struct timespec t1, t2;
	node *new_IN = malloc(sizeof(node));
	memset(new_IN, 0, sizeof(node));
	new_IN->parent_ptr = parent;
	new_IN->p_index = index;
	node_count++;
	clock_gettime(CLOCK_MONOTONIC, &t1);
	flush_buffer(new_IN, sizeof(node));
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_node_flush += (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_node_flush += (t2.tv_nsec - t1.tv_nsec);
	add_logentry();
	return new_IN;
}

node *allocLNode(node *parent, unsigned int index)
{
	struct timespec t1, t2;
	node *new_LN = malloc(sizeof(node));
	node *prev_leaf;
	node *next_leaf;
	memset(new_LN, 0, sizeof(node));
	new_LN->parent_ptr = parent;
	new_LN->p_index = index;
	clock_gettime(CLOCK_MONOTONIC, &t1);
	prev_leaf = find_prev_leaf(new_LN->parent_ptr, new_LN->p_index, 2);
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_leaf_search += (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_leaf_search += (t2.tv_nsec - t1.tv_nsec);
	new_LN->sibling_ptr = prev_leaf->sibling_ptr;
	node_count++;
	clock_gettime(CLOCK_MONOTONIC, &t1);
	flush_buffer(new_LN, sizeof(node));
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_node_flush += (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_node_flush += (t2.tv_nsec - t1.tv_nsec);
	add_logentry();
	prev_leaf->sibling_ptr = new_LN;	//logging is needed
	add_logentry();
	return new_LN;
}

tree *initTree()
{
	tree *wradix_tree = malloc(sizeof(tree));
//	wradix_tree->root = allocNode(NULL);
	wradix_tree->root = allocINode(wradix_tree, 0);
	wradix_tree->height = 1;
	return wradix_tree;
}

int increase_radix_tree_height(tree *t, unsigned char new_height)
{
	unsigned char height = t->height;
	node *root, *prev_root;
	int errval = 0;
	struct timespec t1, t2;

	prev_root = t->root;

	while (height < new_height) {
		/* allocate the meta block */
//		root = allocNode(NULL);
		root = allocINode(t, 0);
		if (root == NULL) {
			errval = 1;
			return errval;
		}
		root->entry_ptr[0] = prev_root;
		entry_count++;
		clock_gettime(CLOCK_MONOTONIC, &t1);
		flush_buffer(root->entry_ptr[0], 8);
		clock_gettime(CLOCK_MONOTONIC, &t2);
		elapsed_entry_flush += (t2.tv_sec - t1.tv_sec) * 1000000000;
		elapsed_entry_flush += (t2.tv_nsec - t1.tv_nsec);
		prev_root->parent_ptr = root;
		add_logentry();
		prev_root->p_index = 0;		//logging is needed
		add_logentry();
//		entry_count++;
//		clock_gettime(CLOCK_MONOTONIC, &t1);
//		clock_gettime(CLOCK_MONOTONIC, &t2);
//		elapsed_entry_flush += (t2.tv_sec - t1.tv_sec) * 1000000000;
//		elapsed_entry_flush += (t2.tv_nsec - t1.tv_nsec);
//		prev_root->parent_ptr = root;
//		flush_buffer(prev_root, 8);
		prev_root = root;
		height++;
	}

	t->root = prev_root;
	t->height = height;
//	flush();
	return errval;
}

int recursive_alloc_nodes(node *level_ptr, unsigned long key, void *value, 
		unsigned char height)
{
	int errval = -1;
	unsigned int meta_bits = META_NODE_SHIFT, node_bits;
	node *temp_node;
	unsigned long next_key;
	unsigned int index;
	struct timespec t1, t2;

	temp_node = level_ptr;

	node_bits = (height - 1) * meta_bits;

	index = key >> node_bits;

	if (height == 1) {
		temp_node->entry_ptr[index] = value;
		entry_count++;
		clock_gettime(CLOCK_MONOTONIC, &t1);
		flush_buffer(temp_node->entry_ptr[index], 8);
		clock_gettime(CLOCK_MONOTONIC, &t2);
		elapsed_entry_flush += (t2.tv_sec - t1.tv_sec) * 1000000000;
		elapsed_entry_flush += (t2.tv_nsec - t1.tv_nsec);
		if (temp_node->entry_ptr[index] == NULL)
			goto fail;
	}
	else {
		if (temp_node->entry_ptr[index] == NULL) {
			if (height == 2) {		//logging is needed
				temp_node->entry_ptr[index] = allocLNode(temp_node, index);
				add_logentry();
			}
			else {
				temp_node->entry_ptr[index] = allocINode(temp_node, index);
				add_logentry();
			}

		//	entry_count++;
		//	clock_gettime(CLOCK_MONOTONIC, &t1);
		//	flush_buffer(temp_node->entry_ptr[index], 8);
		//	clock_gettime(CLOCK_MONOTONIC, &t2);
		//	elapsed_entry_flush += (t2.tv_sec - t1.tv_sec) * 1000000000;
		//	elapsed_entry_flush += (t2.tv_nsec - t1.tv_nsec);

			if (temp_node->entry_ptr[index] == NULL)
				goto fail;
		}
		next_key = (key & ((0x1UL << node_bits) - 1));
		
		errval = recursive_alloc_nodes(temp_node->entry_ptr[index], next_key, (void *)value, height - 1);
		if (errval < 0)
			goto fail;
	}
	errval = 0;
fail:
	return errval;
}

int Insert(tree *t, unsigned long key, void *value) 
{
	int errval;
	unsigned long max_keys;
	unsigned char height;
	unsigned int blk_shift, meta_bits = META_NODE_SHIFT;
	unsigned long total_keys;

	height = t->height;

	blk_shift = height * meta_bits;

	max_keys = 0x1UL << blk_shift;

	if (key > max_keys - 1) {
		/* Radix tree height increases as a result of this allocation */
		total_keys = key >> blk_shift;
		while (total_keys > 0) {
			total_keys = total_keys >> meta_bits;
			height++;
		}
	}

	if (height == 0)
		return 0;

	if(height > t->height) {
		errval = increase_radix_tree_height(t, height);
		if(errval) {
			printf ("increase radix tree height error!\n");
			goto fail;
		}
	}
	errval = recursive_alloc_nodes(t->root, key, (void *)value, height);
	if (errval < 0)
		goto fail;

	return 0;
fail:
	return errval;
}

void *Lookup(tree *t, unsigned long key)
{
	node *level_ptr;
	unsigned char height;
	unsigned int bit_shift, idx;
	void *value;

	height = t->height;
	level_ptr = t->root;
	
	while (height > 1) {
		bit_shift = (height - 1) * META_NODE_SHIFT;
		idx = key >> bit_shift;

		level_ptr = level_ptr->entry_ptr[idx];

		key = key & ((0x1UL << bit_shift) - 1);
		height--;
	}
	bit_shift = (height - 1) * META_NODE_SHIFT;
	idx = key >> bit_shift;
	value = level_ptr->entry_ptr[idx];
	return value;
}

void Range_Lookup(tree *t, unsigned long start_key, unsigned long end_key,
		unsigned long buf[])
{
	node *level_ptr;
	unsigned char height;
	unsigned int bit_shift, idx, i;
	unsigned long search_count = 0;
	void *value;

	height = t->height;
	level_ptr = t->root;

	while (height > 1) {
		bit_shift = (height - 1) * META_NODE_SHIFT;
		idx = start_key >> bit_shift;

		level_ptr = level_ptr->entry_ptr[idx];

		start_key = start_key & ((0x1UL << bit_shift) - 1);
		height--;
	}
	bit_shift = (height - 1) * META_NODE_SHIFT;
	idx = start_key >> bit_shift;

	while (search_count < (end_key - start_key + 1)) {
		for (i = idx; i < (0x1UL << META_NODE_SHIFT); i++) {
			buf[search_count] = *(unsigned long *)level_ptr->entry_ptr[i];
			search_count++;
			if (search_count == (end_key - start_key + 1))
				return ;
		}
		level_ptr = level_ptr->sibling_ptr;
		idx = 0;
	}
}

void Range_Lookup2(tree *t, unsigned long start_key, unsigned long num,
		unsigned long buf[])
{
	node *level_ptr;
	unsigned char height;
	unsigned int bit_shift, idx, i;
	unsigned long search_count = 0;
	void *value;
	struct timespec t1, t2;

	height = t->height;
	level_ptr = t->root;

	while (height > 1) {
		bit_shift = (height - 1) * META_NODE_SHIFT;
		idx = start_key >> bit_shift;

		level_ptr = level_ptr->entry_ptr[idx];

		start_key = start_key & ((0x1UL << bit_shift) - 1);
		height--;
	}
	bit_shift = (height - 1) * META_NODE_SHIFT;
	idx = start_key >> bit_shift;

	while (search_count < num) {
		clock_gettime(CLOCK_MONOTONIC, &t1);
		for (i = idx; i < (0x1UL << META_NODE_SHIFT); i++) {
			if (level_ptr->entry_ptr[i] != NULL) {
				buf[search_count] = *(unsigned long *)level_ptr->entry_ptr[i];
				search_count++;
//				printf("search_count = %lu\n", search_count);
				if (search_count == num) {
					clock_gettime(CLOCK_MONOTONIC, &t2);
					elapsed_entry_search += (t2.tv_sec - t1.tv_sec) * 1000000000;
					elapsed_entry_search += (t2.tv_nsec - t1.tv_nsec);
					return ;
				}
			}
		}
		clock_gettime(CLOCK_MONOTONIC, &t2);
		elapsed_entry_search += (t2.tv_sec - t1.tv_sec) * 1000000000;
		elapsed_entry_search += (t2.tv_nsec - t1.tv_nsec);

		level_ptr = level_ptr->sibling_ptr;
		if (level_ptr == NULL) {
			printf("error\n");
			return ;
		}
		idx = 0;
	}
}

void Scan(tree *t, unsigned long num)
{
	node *next_ptr;
	unsigned char height;
	unsigned long search_count = 0;
	int i, idx = 0;

	height = t->height;
	next_ptr = t->root;
	next_ptr = search_to_leftmost_leaf(next_ptr, height);

	while (search_count < num) {
		for (i = idx; i < (0x1UL << META_NODE_SHIFT); i++) {
			if (next_ptr->entry_ptr[i] != NULL) {
				next_ptr->entry_ptr[i];
				search_count++;
				if (search_count == num)
					return ;
			}
		}
		next_ptr = next_ptr->sibling_ptr;
		idx = 0;
	}
}
