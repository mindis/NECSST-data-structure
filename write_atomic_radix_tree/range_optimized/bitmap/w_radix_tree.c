#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86intrin.h>
#include <malloc.h>
#include <time.h>
#include "w_radix_tree.h"

#define mfence() asm volatile("mfence":::"memory")
#define sfence() asm volatile("sfence":::"memory")

#define BITOP_WORD(nr)	((nr) / BITS_PER_LONG)

void flush_buffer(void *buf, unsigned int len, bool fence)
{
	unsigned int i;
	len = len + ((unsigned long)(buf) & (CACHE_LINE_SIZE - 1));
	if (fence) {
//		sfence();
		for (i = 0; i < len; i += CACHE_LINE_SIZE)
			asm volatile ("clflush %0\n" : "+m" (*(char *)(buf+i)));
		sfence();
	} else {
		for (i = 0; i < len; i += CACHE_LINE_SIZE)
			asm volatile ("clflush %0\n" : "+m" (*(char *)(buf+i)));
	}
}

void add_logentry()
{
	logentry *log = malloc(sizeof(logentry));
	log->addr = 0;
	log->old_value = 0;
	log->new_value = 0;
	flush_buffer(log, sizeof(logentry), true);
}


node *allocNode(node *parent, unsigned int index)
{
	node *new_node = calloc(1, sizeof(node));
	if (parent != NULL) {
		new_node->parent_ptr = parent;
		new_node->p_index = index;
	}
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

tree *CoW_Tree(node *changed_root, unsigned char height)
{
	tree *changed_tree = malloc(sizeof(tree));
	changed_tree->root = changed_root;
	changed_tree->height = height;
	flush_buffer(changed_tree, sizeof(tree), false);
	return changed_tree;
}

int increase_radix_tree_height(tree **t, unsigned char new_height)
{
	unsigned char height = (*t)->height;
	node *root, *prev_root;
	int errval = 0;
//	struct timespec t1, t2;

	prev_root = (*t)->root;

	while (height < new_height) {
		/* allocate the tree nodes for increasing height */
		root = allocNode(NULL, 0);
		if (root == NULL){
			errval = 1;
			return errval;
		}
		root->entry_ptr[0] = prev_root;
//		entry_count++;
//		clock_gettime(CLOCK_MONOTONIC, &t1);
//		flush_buffer(root->entry_ptr[0], 8);
//		clock_gettime(CLOCK_MONOTONIC, &t2);
//		elapsed_entry_flush += (t2.tv_sec - t1.tv_sec) * 1000000000;
//		elapsed_entry_flush += (t2.tv_nsec - t1.tv_nsec);
		prev_root->parent_ptr = root;
		flush_buffer(prev_root, sizeof(node), false);
		prev_root = root;
		height++;
	}
	flush_buffer(prev_root, sizeof(node), false);
	*t = CoW_Tree(prev_root, height);
//	flush_buffer(*t, 8);
	return errval;
}

int recursive_alloc_nodes(node *temp_node, unsigned long key, void *value,
		unsigned char height)
{
	int errval = -1;
	unsigned int meta_bits = META_NODE_SHIFT, node_bits;
	unsigned long next_key;
	unsigned int index;
	
	node_bits = (height - 1) * meta_bits;

	index = key >> node_bits;

	if (height == 1) {
		temp_node->entry_ptr[index] = value;
		if (temp_node->entry_ptr[index] == NULL)
			goto fail;
		temp_node->bitmap = temp_node->bitmap + (0x1UL << index);
		flush_buffer(temp_node, sizeof(node), false);
	}
	else {
		if (temp_node->entry_ptr[index] == NULL) {
			temp_node->entry_ptr[index] = allocNode(temp_node, index);
			flush_buffer(temp_node, sizeof(node), false);
			if (temp_node->entry_ptr[index] == NULL)
				goto fail;
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


int recursive_search_leaf(node *level_ptr, unsigned long key, void *value, 
		unsigned char height)
{
	int errval = -1;
	unsigned int meta_bits = META_NODE_SHIFT, node_bits;
	unsigned long next_key;
	unsigned int index;
//	struct timespec t1, t2;

	node_bits = (height - 1) * meta_bits;

	index = key >> node_bits;

	if (height == 1) {
		level_ptr->entry_ptr[index] = value;
//		entry_count++;
//		clock_gettime(CLOCK_MONOTONIC, &t1);
		flush_buffer(&level_ptr->entry_ptr[index], 8, true);
		level_ptr->bitmap = level_ptr->bitmap + (0x1UL << index);
		flush_buffer(&level_ptr->bitmap, sizeof(unsigned long), true);
//		clock_gettime(CLOCK_MONOTONIC, &t2);
//		elapsed_entry_flush += (t2.tv_sec - t1.tv_sec) * 1000000000;
//		elapsed_entry_flush += (t2.tv_nsec - t1.tv_nsec);
		if (level_ptr->entry_ptr[index] == NULL)
			goto fail;
	}
	else {
		if (level_ptr->entry_ptr[index] == NULL) {
			/* delayed commit */
			node *tmp_node = allocNode(level_ptr, index);
			next_key = (key & ((0x1UL << node_bits) - 1));
			errval = recursive_alloc_nodes(tmp_node, next_key, (void *)value, 
					height - 1);

			if (errval < 0)
				goto fail;

			sfence();
			level_ptr->entry_ptr[index] = tmp_node;
			flush_buffer(&level_ptr->entry_ptr[index], 8, true);
			return errval;
		}
		next_key = (key & ((0x1UL << node_bits) - 1));
		
		errval = recursive_search_leaf(level_ptr->entry_ptr[index], next_key, 
			(void *)value, height - 1);
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
	unsigned char height;
	unsigned int blk_shift, meta_bits = META_NODE_SHIFT;
	unsigned long total_keys;

	height = (*t)->height;

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

		sfence();
		*t = tmp_t;
		flush_buffer(*t, 8, true);
		free(prev_tree);
		return 0;
	}
	errval = recursive_search_leaf((*t)->root, key, (void *)value, height);
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

node *search_to_next_leaf(node *next_branch, unsigned char height)
{
	int i;
	node *next_leaf;

	if (height != 1) {
		for (i = 0; i < NUM_ENTRY; i++) {
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

node *find_next_leaf(tree *t, node *parent, unsigned int index, 
		unsigned char height)
{
	int i;
	node *next_leaf;

	for (i = (index + 1); i < NUM_ENTRY; i++) {
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

/*
 * Find the next set bit in a memory region.
 */
unsigned long find_next_bit(const unsigned long *addr, unsigned long size,
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
		tmp &= (~0UL << offset);
		if (size < BITS_PER_LONG)
			goto found_first;
		if (tmp)
			goto found_middle;
		size -= BITS_PER_LONG;
		result += BITS_PER_LONG;
	}
	while (size & ~(BITS_PER_LONG - 1)) {
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
	if (tmp == 0UL)
		return result + size;
found_middle:
	return result + __ffs(tmp);
}

void Range_Lookup(tree *t, unsigned long start_key, unsigned long num,
		unsigned long buf[])
{
	node *level_ptr;
	unsigned char height;
	unsigned int bit_shift, i;
	unsigned long idx, search_count = 0;
	unsigned long decision_bit;
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

	while (search_count < num) {
		idx = 0;
		decision_bit = 0;

		while (1) {
			if (level_ptr->bitmap == 0)
				break;

			idx = find_next_bit(&level_ptr->bitmap, BITMAP_SIZE, idx);
			buf[search_count] = *(unsigned long *)level_ptr->entry_ptr[idx];
			search_count++;
			if (search_count == num)
				return;

			decision_bit = decision_bit + (0x1UL << idx);
			if (decision_bit == level_ptr->bitmap)
				break;
			idx++;
		}

		level_ptr = find_next_leaf(t, level_ptr->parent_ptr,
				level_ptr->p_index, 2);
		if (level_ptr == NULL) {
			printf("error\n");
			return;
		}
	}
}
/*
int main(void)
{
	int i;
	char *dummy;
	unsigned long *keys;
	void *ret;
	FILE *fp;
	unsigned long *buf;
	char line[1024];
	FILE *fp2;
	unsigned long nVmSize = 0;
	unsigned long nVmRss = 0;
	unsigned long max;
	unsigned long min;

	if((fp = fopen("/home/sekwon/Public/input_file/input_2billion.txt","r")) == NULL)
	{
		puts("error");
		exit(0);
	}

	keys = malloc(sizeof(unsigned long) * 100000100);
	buf = malloc(sizeof(unsigned long) * 100000100);
	memset(buf, 0, sizeof(unsigned long) * 100000100);
	for (i = 0; i < 100000100; i++) {
//		keys[i] = i;
		fscanf(fp, "%lu", &keys[i]);
	}

	max = keys[0];
	min = keys[0];
	for (i = 1; i < 100000100; i++) {
		if (keys[i] > max)
			max = keys[i];
		if (keys[i] < min)
			min = keys[i];
	}

	tree *t = initTree();
	flush_buffer(t, 8, true);
	dummy = (char *)malloc(15*1024*1024);
	memset(dummy, 0, 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024, true);

	for(i = 0; i < 100000000; i++) {
		if (Insert(&t, keys[i], &keys[i]) < 0) {
			printf("Insert error!\n");
			exit(1);
		}
	}

	sprintf(line, "/proc/%d/status", getpid());
	fp2 = fopen(line, "r");
	if (fp2 == NULL)
		return ;

	while (fgets(line, 1024, fp2) != NULL) {
		if (strstr(line, "VmSize")) {
			char tmp[32];
			char size[32];
			sscanf(line, "%s%s", tmp, size);
			nVmSize = atoi(size);
			printf("nVmSize = %lu KB\n", nVmSize);
		}
		else if (strstr(line, "VmRSS")) {
			char tmp[32];
			char size[32];
			sscanf(line, "%s%s", tmp, size);
			nVmRss = atoi(size);
			printf("nVmRss = %lu KB\n", nVmRss);
			break;
		}
	}
	fclose(fp2);

	printf("sizeof(node) = %d\n", sizeof(node));
//	printf("Node flush Time = %lu ns\n", elapsed_node_flush);
//	printf("Node producing count = %lu\n", node_count);
//	printf("Flush overhead per Node = %lu ns\n", elapsed_node_flush/node_count);
//	printf("Entry flush Time = %lu ns\n", elapsed_entry_flush);
//	printf("Flush overhead per entry = %lu ns\n", elapsed_entry_flush/entry_count);

	memset(dummy, 0, 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024, true);

	for(i = 100000000; i < 100000100; i++)
		Insert(&t, keys[i], &keys[i]);
	
	memset(dummy, 0, 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024, true);

	for(i = 0; i < 100000100; i++) {
		ret = Lookup(t, keys[i]);	
		if (ret == NULL) {
			printf("There is no key[%d] = %lu\n", i, keys[i]);
			exit(1);
		}
		else {
			printf("Search value = %lu\n", *(unsigned long *)ret);
			sleep(1);
		}
	}

	memset(dummy, 0, 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024, true);

	Range_Lookup(t, min, 100000100, buf);

//	for (i = 0; i < 50000100; i++)
//		printf("buf[%d] = %lu\n", i, buf[i]);

	return 0;
}
*/
