#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86intrin.h>
#include <malloc.h>
#include <time.h>
#include "wart.h"

#define mfence() asm volatile("mfence":::"memory")

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

node *allocNode(node *parent, unsigned long index)
{
	node *new_node = calloc(1, sizeof(node));
	if (parent != NULL) {
		new_node->parent_ptr = parent;
		new_node->p_index = index;
	}
	return new_node;
}

node2 *alloc_Node2(unsigned char group_num)
{
	node2 *new_node = calloc(1, sizeof(node2));
	new_node->group_num = group_num;
	new_node->next_ptr = NULL;
	return new_node;
}

node_header *alloc_header(void *parent, unsigned long index)
{
	node_header *header = malloc(sizeof(node_header));
	header->p_index = index;
	header->parent_ptr = parent;
	header->next_ptr = NULL;
	return header;
}

tree *initTree()
{
	tree *wradix_tree = malloc(sizeof(tree));
	wradix_tree->root = alloc_header(NULL, 0);
	flush_buffer(wradix_tree->root, sizeof(node_header), true);
	wradix_tree->height = 1;
	flush_buffer(wradix_tree, sizeof(tree), true);
	return wradix_tree;
}

tree *CoW_Tree(node_header *changed_root, unsigned long height)
{
	tree *changed_tree = malloc(sizeof(tree));
	changed_tree->root = changed_root;
	changed_tree->height = height;
	flush_buffer(changed_tree, sizeof(tree), false);
	return changed_tree;
}

int increase_radix_tree_height(tree **t, unsigned long new_height)
{
	unsigned long height = (*t)->height;
	node_header *root, *prev_root;
	int errval = 0;

	prev_root = (*t)->root;

	while (height < new_height) {
		/* allocate the tree nodes for increasing height */
		root = alloc_header(NULL, 0);
		root->next_ptr = alloc_Node2(0);
		if (root == NULL){
			errval = 1;
			return errval;
		}
		((node2 *)root->next_ptr)->entry_ptr[0] = prev_root;
		prev_root->parent_ptr = root;
		prev_root = root;
		height++;
	}
	*t = CoW_Tree(prev_root, height);
	return errval;
}

int recursive_alloc_nodes(node_header *header, unsigned long key, void *value,
		unsigned long height)
{
	int errval = -1;
	unsigned long next_key, index, node_bits, meta_bits = META_NODE_SHIFT;
	unsigned char group_num, group_index;

	node_bits = (height - 1) * meta_bits;

	index = key >> node_bits;
	group_num = index >> 4;
	group_index = index % 16;

	if (height == 1) {
		if (header->next_ptr != NULL) {
			int count = 0;
			node2 *prev_group;
			node2 *next_group = header->next_ptr;
			while (next_group != NULL) {
				if (next_group->group_num == group_num) {
						next_group->entry_ptr[group_index] = value;
						break;
				} else if (next_group->group_num  > group_num) {
					node2 *new_group = alloc_Node2(group_num);
					new_group->entry_ptr[group_index] = value;
					if (count == 0) { 
						new_group->next_ptr = next_group;
						header->next_ptr = new_group;
					} else {
						new_group->next_ptr = prev_group->next_ptr;
						prev_group->next_ptr = new_group;
					}
					break;
				}
				prev_group = next_group;
				next_group = next_group->next_ptr;
				count++;
				if (next_group == NULL) {
					node2 *new_group = alloc_Node2(group_num);
					new_group->entry_ptr[group_index] = value;
					prev_group->next_ptr = new_group;
				}
			}
		} else {
			node2 *new_group = alloc_Node2(group_num);
			new_group->entry_ptr[group_index] = value;
			header->next_ptr = new_group;
		}
	} else {
		if (header->next_ptr != NULL) {
			int count = 0;
			node2 *prev_group;
			node2 *next_group = header->next_ptr;
			while (next_group != NULL) {
				if (next_group->group_num == group_num) {
					if (next_group->entry_ptr[group_index] == NULL) {
						next_group->entry_ptr[group_index] = 
							alloc_header(header, index);
						next_key = (key & ((0x1UL << node_bits) - 1));
						errval = recursive_alloc_nodes(next_group->entry_ptr[group_index],
								next_key, value, height - 1);
						return errval;
					} else {
						next_key = (key & ((0x1UL << node_bits) - 1));
						errval = recursive_alloc_nodes(next_group->entry_ptr[group_index],
								next_key, value, height - 1);
						return errval;
					}
				} else if (next_group->group_num  > group_num) {
					node2 *new_group = alloc_Node2(group_num);
					new_group->entry_ptr[group_index] = 
						alloc_header(header, index);
					if (count == 0) {
						new_group->next_ptr = next_group;
						header->next_ptr = new_group;
					} else {
						new_group->next_ptr = next_group;
						prev_group->next_ptr = new_group;
					}
					next_key = (key & ((0x1UL << node_bits) - 1));
					errval = recursive_alloc_nodes(new_group->entry_ptr[group_index],
							next_key, value, height - 1);
					return errval;
				}
				prev_group = next_group;
				next_group = next_group->next_ptr;
				count++;
			}
			node2 *new_group = alloc_Node2(group_num);
			new_group->entry_ptr[group_index] = alloc_header(header, index);
			prev_group->next_ptr = new_group;
			next_key = (key & ((0x1UL << node_bits) - 1));
			errval = recursive_alloc_nodes(new_group->entry_ptr[group_index],
					next_key, value, height - 1);
			return errval;
		} else {
			node2 *new_group = alloc_Node2(group_num);
			new_group->entry_ptr[group_index] = alloc_header(header, index);
			header->next_ptr = new_group;
			next_key = (key & ((0x1UL << node_bits) - 1));
			errval = recursive_alloc_nodes(new_group->entry_ptr[group_index],
					next_key, (void *)value, height - 1);
		}
		if (errval < 0)
			goto fail;
	}
	errval = 0;
fail:
	return errval;
}

int Insert(tree **t, unsigned long key, void *value) 
{
	int errval;
	unsigned long max_keys;
	unsigned long height;
	unsigned long blk_shift, meta_bits = META_NODE_SHIFT;
	unsigned long total_keys;

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

	if(height > (*t)->height) {
		/* delayed commit */
		tree *tmp_t = *t;
		tree *prev_tree = *t;
		errval = increase_radix_tree_height(&tmp_t, height);
		if(errval) {
			printf ("Increase radix tree height error!\n");
			goto fail;
		}

		errval = recursive_alloc_nodes(tmp_t->root, key, (void *)value, height);
		if (errval < 0)
			goto fail;

		*t = tmp_t;
		flush_buffer(*t, 8, true);
		free(prev_tree);
		return 0;
	}
	errval = recursive_alloc_nodes((*t)->root, key, (void *)value, height);
	if (errval < 0)
		goto fail;

	return 0;
fail:
	return errval;
}

void *Lookup(tree *t, unsigned long key)
{
	node2 *next_group;
	node_header *level_header;
	unsigned long bit_shift, idx, height;
	unsigned char group_num, group_index;
	void *value;

	height = t->height;
	level_header = t->root;

	while (height > 1) {
		bit_shift = (height - 1) * META_NODE_SHIFT;
		idx = key >> bit_shift;
		group_num = idx >> 4;
		group_index = idx % 16;

		next_group = level_header->next_ptr;
		while (next_group != NULL) {
			if (next_group->group_num == group_num) {
				level_header = next_group->entry_ptr[group_index];
				break;
			}
			next_group = next_group->next_ptr;
		}

		key = key & ((0x1UL << bit_shift) - 1);
		height--;
	}
	bit_shift = (height - 1) * META_NODE_SHIFT;
	idx = key >> bit_shift;
	group_num = idx >> 4;
	group_index = idx % 16;

	next_group = level_header->next_ptr;
	while (next_group != NULL) {
		if (next_group->group_num == group_num) {
			value = next_group->entry_ptr[group_index];
			break;
		}
		next_group = next_group->next_ptr;
	}
	return value;
}
