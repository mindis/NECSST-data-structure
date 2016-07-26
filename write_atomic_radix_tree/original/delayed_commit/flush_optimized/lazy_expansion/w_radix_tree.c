#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86intrin.h>
#include <malloc.h>
#include <time.h>
#include "w_radix_tree.h"

#define mfence() asm volatile("mfence":::"memory")

unsigned long node_count = 0;

void flush_buffer(void *buf, unsigned long len, bool fence)
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

item *allocItem(unsigned long key, void *value)
{
	item *new_item = malloc(sizeof(item));
	new_item->type = ITEM_LAZY;
	new_item->key = key;
	new_item->value = value;
	new_item->next_ptr = NULL;
	return new_item;
}

node *allocNode(node *parent, unsigned long index)
{
	node *new_node = calloc(1, sizeof(node));
	new_node->type = NODE_ORIGIN;
	if (parent != NULL) {
		new_node->parent_ptr = parent;
		new_node->p_index = index;
	}
	node_count++;
	return new_node;
}

tree *initTree()
{
	tree *wradix_tree = malloc(sizeof(tree));
	wradix_tree->root = allocNode(NULL, 0);
	wradix_tree->height = 1;
	flush_buffer(wradix_tree, sizeof(tree), true);
	return wradix_tree;
}

int remapping_items(tree *t, node *level_ptr, item *first_item, 
		unsigned long height)
{
	int errval = -1;
	unsigned long next_key, bit_shift, meta_bits = META_NODE_SHIFT;
	item *curr_item = first_item;
	item *new_item;

	bit_shift = height * META_NODE_SHIFT;
	
	while (curr_item != NULL) {
		next_key = curr_item->key;
		next_key = (next_key & ((0x1UL << bit_shift) - 1));
		new_item = allocItem(curr_item->key, curr_item->value);
		errval = recursive_search_leaf(t, level_ptr, new_item->key, 
				next_key, new_item->value, new_item, height);
		curr_item = curr_item->next_ptr;
	}

	return errval;
}

int recursive_search_leaf(tree *t, node *level_ptr, unsigned long key,
		unsigned long next_key, void *value, item *new_item, unsigned long height)
{
	int errval = -1;
	unsigned long index, node_bits, meta_bits = META_NODE_SHIFT;

	node_bits = (height - 1) * meta_bits;

	index = next_key >> node_bits;

	if (height == 1) {
		level_ptr->entry_ptr[index] = value;
		flush_buffer(&level_ptr->entry_ptr[index], 8, true);
		free(new_item);
		if (level_ptr->entry_ptr[index] == NULL)
			goto fail;
	} else {
		if (level_ptr->entry_ptr[index] == NULL) {
			level_ptr->entry_ptr[index] = new_item;
		} else {
			if (((item *)level_ptr->entry_ptr[index])->type == NODE_ORIGIN) {
				next_key = (next_key & ((0x1UL << node_bits) - 1));
				errval = recursive_search_leaf(t, level_ptr->entry_ptr[index],
						key, next_key, value, new_item, height - 1);
				if (errval < 0)
					goto fail;
				return errval;
			}

			int level_count = 1;
			item *next_item = level_ptr->entry_ptr[index];

			while (next_item->next_ptr != NULL) {
				next_item = next_item->next_ptr;
				level_count++;
			}
			
			if (level_count == height) {
				node *temp_node = allocNode(level_ptr, index);
				next_item->next_ptr = new_item;
				errval = remapping_items(t, temp_node, 
						level_ptr->entry_ptr[index], height - 1);
				level_ptr->entry_ptr[index] = temp_node;
				
				if(errval < 0)
					goto fail;
			} else
				next_item->next_ptr = new_item;
		}
	}
	errval = 0;
fail:
	return errval;
}

int Insert(tree **t, unsigned long key, void *value) 
{
	int errval;
	unsigned long max_keys, height, blk_shift, total_keys, level_key;
	unsigned long meta_bits = META_NODE_SHIFT;
	item *new_item;

	height = (*t)->height;

	blk_shift = height * meta_bits;

	if (blk_shift < 64) {
		max_keys = 0x1UL << blk_shift;

		if (key > max_keys - 1) {
			/* Radix tree height increases as a result of this allocation */
			total_keys = key >> blk_shift;
			while (total_keys > 0) {
				total_keys = total_keys >> meta_bits;
				height++;
			}
		}
	}

	if (height == 0)
		return 0;

	(*t)->height = height;

	new_item = allocItem(key, value);
	errval = recursive_search_leaf((*t), (*t)->root, key, key,
			(void *)value, new_item, height);
	if (errval < 0)
		goto fail;

	return 0;
fail:
	return errval;
}

void *Update(tree *t, unsigned long key, void *value)
{
	node *level_ptr;
	unsigned long height;
	unsigned long bit_shift, idx;

	height = t->height;
	level_ptr = t->root;

	while (height > 1) {
		bit_shift = (height - 1) * META_NODE_SHIFT;
		idx = key >> bit_shift;

		level_ptr = level_ptr->entry_ptr[idx];
		if (level_ptr == NULL)
			return level_ptr;

		key = key & ((0x1UL << bit_shift) - 1);
		height--;
	}
	bit_shift = (height - 1) * META_NODE_SHIFT;
	idx = key >> bit_shift;
	level_ptr->entry_ptr[idx] = value;
	flush_buffer(&level_ptr->entry_ptr[idx], 8, true);
	return value;
}

void *Lookup(tree *t, unsigned long key)
{
	node *level_ptr;
	unsigned long height;
	unsigned long bit_shift, idx;
	void *value;

	height = t->height;
	level_ptr = t->root;

	while (height > 1) {
		bit_shift = (height - 1) * META_NODE_SHIFT;
		idx = key >> bit_shift;

		level_ptr = level_ptr->entry_ptr[idx];
		if (level_ptr == NULL)
			return level_ptr;

		key = key & ((0x1UL << bit_shift) - 1);
		height--;
	}
	bit_shift = (height - 1) * META_NODE_SHIFT;
	idx = key >> bit_shift;
	value = level_ptr->entry_ptr[idx];
	return value;
}

node *search_to_next_leaf(node *next_branch, unsigned long height)
{
	int i;
	node *next_leaf;

	if (height != 1) {
		for (i = 0; i < (0x1UL << META_NODE_SHIFT); i++) {
			if (next_branch->entry_ptr[i] != NULL) {
				next_leaf = search_to_next_leaf(next_branch->entry_ptr[i], 
						height - 1);
				return next_leaf;
			}
		}
	}
	else {
		next_leaf = next_branch;
		return next_leaf;
	}
}

node *find_next_leaf(tree *t, node *parent, unsigned long index, 
		unsigned long height)
{
	int i;
	node *next_leaf;

	for (i = (index + 1); i < (0x1UL << META_NODE_SHIFT); i++) {
		if (parent->entry_ptr[i] != NULL) {
			next_leaf = search_to_next_leaf(parent->entry_ptr[i], height - 1);
			return next_leaf;
		}
	}

	if (t->height > height) {
		next_leaf = find_next_leaf(t, parent->parent_ptr, parent->p_index, 
				height + 1);
		return next_leaf;
	}
	else
		return NULL;
}

void Range_Lookup(tree *t, unsigned long start_key, unsigned long num,
		unsigned long buf[])
{
	node *level_ptr;
	unsigned long height;
	unsigned long bit_shift, idx, i;
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

	while (search_count < num) {
		for (i = idx; i < (0x1UL << META_NODE_SHIFT); i++) {
			if (level_ptr->entry_ptr[i] != NULL) {
				buf[search_count] = *(unsigned long *)level_ptr->entry_ptr[i];
				search_count++;
				if (search_count == num)
					return ;
			}
		}
		level_ptr = find_next_leaf(t, level_ptr->parent_ptr,
				level_ptr->p_index, 2);
		if (level_ptr == NULL) {
			printf("error\n");
			return ;
		}
		idx = 0;
	}
}

int recursive_free_nodes(tree *t, node *parent, unsigned long index,
		unsigned long height)
{
	int i, errval = 0;

	if (height < t->height) {
		for (i = 0; i < NUM_ENTRY; i++) {
			/* parent node의 entry에 valid entry가 있을 경우 */
			if (i != index && parent->entry_ptr[i] != NULL) {
				parent->entry_ptr[index] = NULL;
				flush_buffer(&parent->entry_ptr[index], 8, true);
				return errval;
			}
		}
		errval = recursive_free_nodes(t, parent->parent_ptr, 
				parent->p_index, height + 1);
	} else {
		parent->entry_ptr[index] = NULL;
		flush_buffer(&parent->entry_ptr[index], 8, true);
	}

	return errval;
}

void Delete(tree *t, unsigned long key)
{
	node *level_ptr;
	unsigned long height;
	unsigned long bit_shift, idx;

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

	level_ptr->entry_ptr[idx] = NULL;
	flush_buffer(&level_ptr->entry_ptr[idx], 8, true);

	//	recursive_free_nodes(t, level_ptr, idx, 1);
}
