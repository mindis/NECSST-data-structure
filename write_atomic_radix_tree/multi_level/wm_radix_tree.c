#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86intrin.h>
#include <malloc.h>
#include <time.h>
#include "wm_radix_tree.h"

#define mfence() asm volatile("mfence":::"memory")
#define key_range0 1UL << (META_NODE_SHIFT * 1)
#define key_range1 1UL << (META_NODE_SHIFT * 2)
#define key_range2 1UL << (META_NODE_SHIFT * 3)
#define key_range3 1UL << (META_NODE_SHIFT * 4)
#define key_range4 1UL << (META_NODE_SHIFT * 5)
#define key_range5 1UL << (META_NODE_SHIFT * 6)
#define key_range6 1UL << (META_NODE_SHIFT * 7)
#define key_range7 1UL << (META_NODE_SHIFT * 8)
#define key_range8 1UL << (META_NODE_SHIFT * 9)
#define key_range9 1UL << (META_NODE_SHIFT * 10)
#define key_range10 1UL << (META_NODE_SHIFT * 11)
unsigned long elapsed_node_flush = 0;
unsigned long elapsed_entry_flush = 0;
unsigned long node_count = 0;
unsigned long entry_count = 0;
unsigned long level_descript_time = 0;

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

node *allocNode()
{
//	node *new_node = (node *)memalign(64, sizeof(node));
	node *new_node = malloc(sizeof(node));
	memset(new_node, 0, sizeof(node));
	flush_buffer(new_node, sizeof(node));
	return new_node;
}

tree *initTree()
{
	tree *wm_radix_tree = malloc(sizeof(tree));
	memset(wm_radix_tree, 0, sizeof(tree));
	return wm_radix_tree;
}

int recursive_alloc_nodes(node *level_ptr, unsigned long key, void *value, 
		unsigned char height)
{
	int errval;
	node *temp_node;
	unsigned int meta_bits = META_NODE_SHIFT, node_bits;
	unsigned long next_key;
	unsigned int index;

	temp_node = level_ptr;

	node_bits = (height - 1) * meta_bits;

	index = key >> node_bits;

	if (height == 1) {
		temp_node->entry_ptr[index] = value;
		flush_buffer(temp_node->entry_ptr[index], 8);
		if (temp_node->entry_ptr[index] == NULL) {
			errval = -1;
			goto fail;
		}
	}
	else {
		if (temp_node->entry_ptr[index] == NULL) {
			temp_node->entry_ptr[index] = allocNode();
			flush_buffer(temp_node->entry_ptr[index], 8);
			if (temp_node->entry_ptr[index] == NULL) {
				printf("Node allocation is failed\n");
				errval = -1;
				goto fail;
			}
		}
		next_key = (key & ((0x1UL << node_bits) - 1));
		
		errval = recursive_alloc_nodes(temp_node->entry_ptr[index], next_key, 
				(void *)value, height - 1);

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
	unsigned char height = 1;
	unsigned int meta_bits = META_NODE_SHIFT;
	unsigned long total_keys;
/*
	total_keys = key >> meta_bits;
	while (total_keys > 0) {
		total_keys = total_keys >> meta_bits;
		height++;
	}
*/
	if (0 <= key && key < key_range0)
		height = 1;
	else if (key_range0 <= key && key < key_range1)
		height = 2;
	else if (key_range1 <= key && key < key_range2)
		height = 3;
	else if (key_range2 <= key && key < key_range3)
		height = 4;
	else if (key_range3 <= key && key < key_range4)
		height = 5;
	else if (key_range4 <= key && key < key_range5)
		height = 6;
	else if (key_range5 <= key && key < key_range6)
		height = 7;
	else if (key_range6 <= key && key < key_range7)
		height = 8;
	else if (key_range7 <= key && key < key_range8)
		height = 9;
	else if (key_range8 <= key && key < key_range9)
		height = 10;
	else if (key_range9 <= key && key < key_range10)
		height = 11;

	if (t->root[height - 1] == NULL) {
		t->root[height - 1] = allocNode();
		if (t->root[height - 1] == NULL) {
			printf("Node allocation is failed\n");
			errval = -1;
			goto fail;
		}
	}

	errval = recursive_alloc_nodes(t->root[height - 1], key, 
			(void *)value, height);

	if (errval < 0)
		goto fail;

	return 0;
fail:
	return errval;
}

void *Lookup(tree *t, unsigned long key)
{
	node *level_ptr;
	unsigned char height = 1;
	unsigned int bit_shift, idx, meta_bits = META_NODE_SHIFT;
	unsigned long total_keys;
	void *value;
	struct timespec t1, t2;
/*
	total_keys = key >> meta_bits;
	while (total_keys > 0) {
		total_keys = total_keys >> meta_bits;
		height++;
	}
*/
	clock_gettime(CLOCK_MONOTONIC, &t1);
	if (0 <= key && key < key_range0)
		height = 1;
	else if (key_range0 <= key && key < key_range1)
		height = 2;
	else if (key_range1 <= key && key < key_range2)
		height = 3;
	else if (key_range2 <= key && key < key_range3)
		height = 4;
	else if (key_range3 <= key && key < key_range4)
		height = 5;
	else if (key_range4 <= key && key < key_range5)
		height = 6;
	else if (key_range5 <= key && key < key_range6)
		height = 7;
	else if (key_range6 <= key && key < key_range7)
		height = 8;
	else if (key_range7 <= key && key < key_range8)
		height = 9;
	else if (key_range8 <= key && key < key_range9)
		height = 10;
	else if (key_range9 <= key && key < key_range10)
		height = 11;
	clock_gettime(CLOCK_MONOTONIC, &t2);
	level_descript_time += (t2.tv_sec - t1.tv_sec) * 1000000000;
	level_descript_time += (t2.tv_nsec - t1.tv_nsec);

	level_ptr = t->root[height - 1];
	
	while (height > 1) {
		bit_shift = (height - 1) * meta_bits;
		idx = key >> bit_shift;

		level_ptr = level_ptr->entry_ptr[idx];

		key = key & ((0x1UL << bit_shift) - 1);
		height--;
	}
	bit_shift = (height - 1) * meta_bits;
	idx = key >> bit_shift;
	value = level_ptr->entry_ptr[idx];
	return value;
}
