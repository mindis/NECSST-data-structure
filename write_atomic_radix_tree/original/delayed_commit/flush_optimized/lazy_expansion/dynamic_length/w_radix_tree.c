#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86intrin.h>
#include <malloc.h>
#include <time.h>
#include "w_radix_tree.h"

#define mfence() asm volatile("mfence":::"memory")

unsigned long node_count = 0;
unsigned long item_count = 0;

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
	item_count++;
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
	wradix_tree->height = MAX_HEIGHT;
	flush_buffer(wradix_tree, sizeof(tree), true);
	return wradix_tree;
}

int remapping_items(tree *t, node *level_ptr, item *first_item, 
		unsigned long height)
{
	int i, errval = -1;
	unsigned long next_key, bit_shift, meta_bits = META_NODE_SHIFT;
	item *next_item = first_item;
	item *new_item;

	bit_shift = height * META_NODE_SHIFT;

	while (next_item != NULL) {
		next_key = next_item->key;
		next_key = (next_key & ((0x1UL << bit_shift) - 1));
		new_item = allocItem(next_item->key, next_item->value);
//		flush_buffer(new_item, sizeof(item), false);
		errval = recursive_alloc_nodes(t, level_ptr, next_item->key, 
				next_key, next_item->value, new_item, height);
		next_item = next_item->next_ptr;
	}

	if (height != 1) {
		for (i = 0; i < NUM_ENTRY; i++) {
			if (level_ptr->entry_ptr[i] != NULL 
					&& ((item *)level_ptr->entry_ptr[i])->type == ITEM_LAZY) {
				next_item = level_ptr->entry_ptr[i];
				while (next_item != NULL) {
					flush_buffer(next_item, sizeof(item), false);
					next_item = next_item->next_ptr;
				}
			}
		}
	}
	return errval;
}

int recursive_alloc_nodes(tree *t, node *level_ptr, unsigned long key,
		unsigned long next_key, void *value, item *new_item, unsigned long height)
{
	int item_height, errval = -1;
	unsigned long index, node_bits, meta_bits = META_NODE_SHIFT;

	node_bits = (height - 1) * meta_bits;

	index = next_key >> node_bits;

	if (height == 1) {
		level_ptr->entry_ptr[index] = value;
		free(new_item);
		item_count--;
		if (level_ptr->entry_ptr[index] == NULL)
			goto fail;
	} else {
		if (level_ptr->entry_ptr[index] == NULL) {
			level_ptr->entry_ptr[index] = new_item;
			if (level_ptr->entry_ptr[index] == NULL)
				goto fail;
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
			
			item_height = (sizeof(node) / sizeof(item));

			if (level_count == item_height) {
				item *curr_item;
				node *temp_node = allocNode(level_ptr, index);
				next_item->next_ptr = new_item;
				errval = remapping_items(t, temp_node, 
						level_ptr->entry_ptr[index], height - 1);

				curr_item = level_ptr->entry_ptr[index];
				level_ptr->entry_ptr[index] = temp_node;

				next_item = curr_item->next_ptr;
				while (curr_item != NULL) {
					free(curr_item);
					curr_item = next_item;
					if (next_item != NULL)
						next_item = curr_item->next_ptr;
					item_count--;
				}

				flush_buffer(temp_node, sizeof(node), false);

				if(errval < 0)
					goto fail;
			} else {
				next_item->next_ptr = new_item;
		//		flush_buffer(&next_item->next_ptr, 8, false);
				if (next_item->next_ptr == NULL)
					goto fail;
			}
		}
	}
	errval = 0;
fail:
	return errval;
}

int recursive_search_leaf(tree *t, node *level_ptr, unsigned long key,
		unsigned long next_key, void *value, item *new_item, unsigned long height)
{
	int item_height, errval = -1;
	unsigned long index, node_bits, meta_bits = META_NODE_SHIFT;

	node_bits = (height - 1) * meta_bits;

	index = next_key >> node_bits;

	if (height == 1) {
		level_ptr->entry_ptr[index] = value;
		flush_buffer(&level_ptr->entry_ptr[index], 8, true);
		free(new_item);
		item_count--;
		if (level_ptr->entry_ptr[index] == NULL)
			goto fail;
	} else {
		if (level_ptr->entry_ptr[index] == NULL) {
			level_ptr->entry_ptr[index] = new_item;
			flush_buffer(new_item, sizeof(item), false);
			flush_buffer(&level_ptr->entry_ptr[index], 8, true);
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
			
			item_height = (sizeof(node) / sizeof(item));	//item 높이 바꿀때 item_height 값 변경
				
			if (level_count == item_height) {
				item *curr_item;
				node *temp_node = allocNode(level_ptr, index);
				next_item->next_ptr = new_item;
				errval = remapping_items(t, temp_node, 
						level_ptr->entry_ptr[index], height - 1);

				curr_item = level_ptr->entry_ptr[index];
				level_ptr->entry_ptr[index] = temp_node;

				next_item = curr_item->next_ptr;
				while (curr_item != NULL) {
					free(curr_item);
					curr_item = next_item;
					if (next_item != NULL)
						next_item = curr_item->next_ptr;
					item_count--;
				}

				flush_buffer(temp_node, sizeof(node), false);
				flush_buffer(&level_ptr->entry_ptr[index], 8, true);

				if(errval < 0)
					goto fail;
			} else {
				next_item->next_ptr = new_item;
				flush_buffer(new_item, sizeof(item), false);
				flush_buffer(&next_item->next_ptr, 8, true);
			}
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
	item *curr_item;
	unsigned long height;
	unsigned long bit_shift, idx;
	unsigned long next_key = key;

	height = t->height;
	level_ptr = t->root;

	while (height > 1) {
		bit_shift = (height - 1) * META_NODE_SHIFT;
		idx = next_key >> bit_shift;

		if (((node *)level_ptr->entry_ptr[idx])->type == NODE_ORIGIN) {
			level_ptr = level_ptr->entry_ptr[idx];
			if (level_ptr == NULL)
				return level_ptr;

			next_key = next_key & ((0x1UL << bit_shift) - 1);
			height--;
		} else {
			curr_item = level_ptr->entry_ptr[idx];
			while (curr_item != NULL) {
				if (curr_item->key == key) {
					curr_item->value = value;
					flush_buffer(&curr_item->value, 8, true);
					return curr_item->value;
				}
				curr_item = curr_item->next_ptr;
			}
		}
	}
	bit_shift = (height - 1) * META_NODE_SHIFT;
	idx = next_key >> bit_shift;
	level_ptr->entry_ptr[idx] = value;
	flush_buffer(&level_ptr->entry_ptr[idx], 8, true);
	return level_ptr->entry_ptr[idx];
}

void *Lookup(tree *t, unsigned long key)
{
	node *level_ptr;
	item *curr_item;
	unsigned long height;
	unsigned long bit_shift, idx;
	unsigned long next_key = key;
	void *value;

	height = t->height;
	level_ptr = t->root;

	while (height > 1) {
		bit_shift = (height - 1) * META_NODE_SHIFT;
		idx = next_key >> bit_shift;

		if (((node *)level_ptr->entry_ptr[idx])->type == NODE_ORIGIN) {
			level_ptr = level_ptr->entry_ptr[idx];
			if (level_ptr == NULL)
				return level_ptr;

			next_key = next_key & ((0x1UL << bit_shift) - 1);
			height--;
		} else {
			curr_item = level_ptr->entry_ptr[idx];
			while (curr_item != NULL) {
				if (curr_item->key == key) {
					value = curr_item->value;
					return value;
				}
				curr_item = curr_item->next_ptr;
			}
		}
	}
	bit_shift = (height - 1) * META_NODE_SHIFT;
	idx = next_key >> bit_shift;
	value = level_ptr->entry_ptr[idx];
	return value;
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

void find_next_leaf(node *curr_node, unsigned long index, unsigned long height,
		unsigned long buf[], unsigned long *count, unsigned long num)
{
	unsigned long i, j;

	if (height > MAX_HEIGHT)
		return ;

	if (index == NUM_ENTRY)
		find_next_leaf(curr_node->parent_ptr, curr_node->p_index, height + 1,
				buf, count, num);
	else {
		for (i = index; i < NUM_ENTRY; i++) {
			if (curr_node->entry_ptr[i] != NULL) {
				if (((item *)curr_node->entry_ptr[i])->type == ITEM_LAZY)
					search_entry_in_node(curr_node, i, height, buf, count, num);
				else
					search_entry_in_node(curr_node->entry_ptr[i], 0, height - 1, 
							buf, count, num);
			}
		}
	}

	return ;
}

void search_entry_in_node(node *level_ptr, unsigned long index, unsigned long height,
		unsigned long buf[], unsigned long *count, unsigned long num)
{
	int i, item_num;
	unsigned long next_index;

	if (*count == num || height > MAX_HEIGHT || level_ptr == NULL)
		return ;

	if (index == NUM_ENTRY) {
		return search_entry_in_node(level_ptr->parent_ptr, level_ptr->p_index + 1,
					height + 1, buf, count, num);
	} else if (level_ptr->entry_ptr[index] == NULL) {
		next_index = index + 1;
		return search_entry_in_node(level_ptr, next_index, height, buf, count, num);
	} else {
		if (height == 1) {
			buf[*count] = *(unsigned long *)level_ptr->entry_ptr[index];
			(*count)++;
			if (*count == num)
				return ;

			next_index = index + 1;
			for (i = next_index; i < NUM_ENTRY; i++) {
				if (level_ptr->entry_ptr[i] != NULL) {
					buf[*count] = *(unsigned long *)level_ptr->entry_ptr[i];
					(*count)++;
					if (*count == num)
						return ;
				}
			}
			return search_entry_in_node(level_ptr->parent_ptr, level_ptr->p_index + 1,
						height + 1, buf, count, num);
		} else {
			if (((item *)level_ptr->entry_ptr[index])->type == ITEM_LAZY) {
				entry *item_entry = malloc(MAX_HEIGHT * sizeof(entry));
				item *next_item = level_ptr->entry_ptr[index];
				for (i = 0; next_item != NULL; i++) {
					item_entry[i].key = next_item->key;
					item_entry[i].value = next_item->value;
					next_item = next_item->next_ptr;
				}

				item_num = i;
				insertion_sort(item_entry, item_num);

				for (i = 0; i < item_num; i++) {
					buf[*count] = *(unsigned long *)item_entry[i].value;
					(*count)++;
					if (*count == num)
						return ;
				}
				free(item_entry);

				next_index = index + 1;
				if (next_index == NUM_ENTRY)
					return search_entry_in_node(level_ptr->parent_ptr, level_ptr->p_index + 1,
								height + 1, buf, count, num);
				else
					return search_entry_in_node(level_ptr, next_index, height, buf, count, num);
			} else 
				return search_entry_in_node(level_ptr->entry_ptr[index], 0, height - 1, buf, count, num);
		}
	}
}

void Range_Lookup(tree *t, unsigned long start_key, unsigned long num,
		unsigned long buf[])
{
	node *level_ptr;
	unsigned long height;
	unsigned long bit_shift, idx;
	unsigned long next_key = start_key;
	unsigned long count = 0;

	height = t->height;
	level_ptr = t->root;

	while (height > 1) {
		bit_shift = (height - 1) * META_NODE_SHIFT;
		idx = next_key >> bit_shift;

		if (((node *)level_ptr->entry_ptr[idx])->type == NODE_ORIGIN) {
			level_ptr = level_ptr->entry_ptr[idx];
			if (level_ptr == NULL)
				return ;

			next_key = next_key & ((0x1UL << bit_shift) - 1);
			height--;
		} else {
			search_entry_in_node(level_ptr, idx, height, buf, &count, num);
			return ;
		}
	}
	bit_shift = (height - 1) * META_NODE_SHIFT;
	idx = next_key >> bit_shift;
	search_entry_in_node(level_ptr, idx, height, buf, &count, num);
	return ;
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
