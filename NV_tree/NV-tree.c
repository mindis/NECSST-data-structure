#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <x86intrin.h>
#include <malloc.h>
#include <stdint.h>
#include <time.h>
#include "NV-tree.h"

#define mfence() asm volatile("mfence":::"memory")
#define sfence() asm volatile("sfence":::"memory")
#define SWAP(a,b)	{int t; t = a; a = b; b = t;}

static inline void clflush(volatile void *__p)
{
	asm volatile("clflush %0" : "+m" (*(volatile char *)__p));
}

int clflush_range(void *start, void *end)
{
	start= (void *)((unsigned long)start &~(CACHE_LINE_SIZE-1));
	mfence();
	//printf("start = %x start&63 = %x\n",start,(unsigned long)start&~(CACHE_LINE_SIZE-1));
	void *addr;
	for(addr=start; addr<end; addr+=CACHE_LINE_SIZE){
		clflush(addr);
	}
	mfence();
	/// Flush any possible final partial cacheline:
}

int clflush_range_nomb(void *start, void *end)
{
	start= (void *)((unsigned long)start &~(CACHE_LINE_SIZE-1));
	//printf("start = %x start&63 = %x\n",start,(unsigned long)start&~(CACHE_LINE_SIZE-1));
	void *addr;
	for(addr=start; addr<end; addr+=CACHE_LINE_SIZE){
		clflush(addr);
	}
	/// Flush any possible final partial cacheline:
}

void flush_buffer(void *buf, unsigned int len, bool fence)
{
	unsigned int i;
	len = len + ((unsigned long)(buf) & (CACHE_LINE_SIZE - 1));
	if (fence) {
		for (i = 0; i < len; i += CACHE_LINE_SIZE)
			asm volatile ("clflush %0\n" : "+m" (*(char *)(buf+i)));
		sfence();
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

void *allocINode()
{
}

LN *allocLNode()
{
}

tree *initTree()
{
	tree *t =calloc(1, sizeof(tree));
	flush_buffer(t, sizeof(tree), true);
	return t;
}

int binary_search_IN(unsigned long key, IN *node)
{
	int low = 0, mid = 0, high = (node->nKeys - 1);

	while (low <= high) {
		mid = (low + high) / 2;
		if (node->key[mid] > key)
			high = mid - 1;
		else if (node->key[mid] < key)
			low = mid + 1;
		else
			break;
	}

	if (low > mid)
		mid = low;

	return mid;
}

int binary_search_PLN(unsigned long key, PLN *node)
{
	int low = 0, mid = 0, high = (node->nKeys - 1);

	while (low <= high) {
		mid = (low + high) / 2;
		if (node->entries[mid].key > key)
			high = mid - 1;
		else if (node->entries[mid].key < key)
			low = mid + 1;
		else
			break;
	}

	if (low > mid)
		mid = low;
	
	return mid;
}

LN *find_leaf(tree *t, unsigned long key)
{
	unsigned int pos, id;
	IN *current_IN;
	PLN *current_PLN;

	id = 0;
	
	while (id < t->first_PLN_id) {
		current_IN = t->root + (id * sizeof(IN));
		pos = binary_search_IN(key, current_IN);
		id = id * ((2 * (MAX_NUM_ENTRY_PLN - 1)) + 1) + 1 + pos;
	//	id = id * (MAX_NUM_ENTRY_IN + 1) + 1 + pos;
	}

	current_PLN = (PLN *)(t->root + (id * sizeof(PLN)));
	pos = binary_search_PLN(key, current_PLN);
	if (pos < current_PLN->nKeys)
		return current_PLN->entries[pos].ptr;
	else
		return current_PLN->last_ptr;
}

int search_leaf_node(LN *node, unsigned long key)
{
	unsigned int i, pos, valid = 0;

	for (i = 0; i < node->nElements; i++) {
		if (node->LN_Element[i].key == key && 
				node->LN_Element[i].flag == true) {
			pos = i;
			valid++;
		}
		
		if (node->LN_Element[i].key == key &&
				node->LN_Element[i].flag == false)
			valid--;
	}

	if (valid > 0)
		return pos;
	else
		return -1;
}

void *Lookup(tree *t, unsigned long key)
{
	unsigned int pos;
	LN *current_LN;

	current_LN = find_leaf(t, key);
	pos = search_leaf_node(current_LN, key);
	if (pos < 0)
		goto fail;

	return current_LN->LN_Element[pos].value;
fail:
	return ;
}

void Range_Lookup(tree *t, unsigned long start_key, unsigned int num, 
		unsigned long buf[])
{
	
}

int create_new_tree(tree **t, unsigned long key, void *value)
{
	int errval = -1;
	IN *new_root = allocINode();	
	if (new_root == NULL)
		return errval;

	PLN *p_leaf = (PLN *)new_root;

	new_root[0].nKeys = 1;
	new_root[0].key[0] = key;

	p_leaf[1].nKeys = 1;
	p_leaf[1].entries[0].key = key;
	p_leaf[1].entries[0].ptr = allocLNode();
	p_leaf[1].last_ptr = allocLNode();
	if (p_leaf[1].entries[0].ptr == NULL || p_leaf[1].last_ptr == NULL)
		return errval;
	p_leaf[1].entries[0].ptr->sibling = p_leaf[1].last_ptr;
	p_leaf[1].last_ptr->sibling = NULL;
	p_leaf[1].entries[0].ptr->parent_id = 1;
	p_leaf[1].last_ptr->parent_id = 1;
	flush_buffer(p_leaf[1].entries[0].ptr->sibling, 16, true);

	p_leaf[1].entries[0].ptr->LN_Element[0].flag = true;
	p_leaf[1].entries[0].ptr->LN_Element[0].key = key;
	p_leaf[1].entries[0].ptr->LN_Element[0].value = value;
	flush_buffer(&p_leaf[1].entries[0].ptr->LN_Element[0], 
			sizeof(struct LN_entry), true);
	p_leaf[1].entries[0].ptr->nElements++;
	flush_buffer(&p_leaf[1].entries[0].ptr->nElements, 1, true);

	(*t)->height = 1;
	(*t)->first_PLN_id = 1;
	(*t)->last_PLN_id = 1;
	(*t)->first_leaf = p_leaf[1].entries[0].ptr;
	(*t)->root = new_root;
	flush_buffer(*t, sizeof(tree), true);
	return 0;
}

void quick_sort(struct LN_entry *base, int left, int right)
{
	int i, j, pivot = base[left].key;
	struct LN_entry temp;

	if (left < right)
	{
		i = left;
		j = right + 1;
		while (i <= j) {
			do
				i++;
			while (base[i].key < pivot);

			do
				j--;
			while (base[j].key > pivot);

			if (i < j) {
				temp = base[i];
				base[i] = base[j];
				base[j] = temp;
			}
			else
				break;
		}
		temp = base[j];
		base[j] = base[left];
		base[left] = temp;

		quick_sort(base, left, j - 1);
		quick_sort(base, j + 1, right);
	}
}

int leaf_scan_divide(tree *t, LN *leaf, LN *split_node1, LN *split_node2, 
		unsigned long key, void *value)
{
	int i, j = 0, count = 0, invalid_count = 0, errval = -1;
	unsigned long *invalid_key = 
		malloc(((MAX_NUM_ENTRY_LN)/2 + 1) * sizeof(unsigned long));
	struct LN_entry *valid_Element =
		malloc(MAX_NUM_ENTRY_LN * sizeof(struct LN_entry));

	for (i = 0; i < MAX_NUM_ENTRY_LN; i++) {
		if (leaf->LN_Element[i].flag == false) {
			invalid_key[invalid_count] = leaf->LN_Element[i].key;
			invalid_count++;
		}
	}

	for (i = 0; i < MAX_NUM_ENTRY_LN; i++) {
		if (leaf->LN_Element[i].flag == false)
			continue;

		if (invalid_count > 0) {
			if (leaf->LN_Element[i].key == invalid_key[count]) {
				count++;
				invalid_count--;
				continue;
			}
		}
		valid_Element[j] = leaf->LN_Element[i];
		j++;
	}

	quick_sort(valid_Element, 0, j - 1);
	
	memcpy(split_node1->LN_Element, valid_Element, 
			sizeof(unsigned long) * (j / 2));
	split_node1->nElements = (j / 2);

	memcpy(split_node2->LN_Element, &valid_Element[j / 2],
			sizeof(unsigned long) * (j - (j / 2)));
	split_node2->nElements = (j - (j / 2));

	if (split_node1->LN_Element[split_node1->nElements - 1].key < key) {
		split_node2->LN_Element[split_node2->nElements].flag = true;
		split_node2->LN_Element[split_node2->nElements].key = key;
		split_node2->LN_Element[split_node2->nElements].value = value;
		split_node2->nElements++;
	}
	else {
		split_node1->LN_Element[split_node1->nElements].flag = true;
		split_node1->LN_Element[split_node1->nElements].key = key;
		split_node1->LN_Element[split_node1->nElements].value = value;
		split_node1->nElements++;
	}

	return 0;
}

int reconstruct_from_PLN()
{
}

int reconstruct_PLN(tree *t, unsigned long parent_id, unsigned long insert_key)
{
	unsigned long height, max_PLN, total_PLN, total_IN = 1;
	unsigned int i;
	IN *new_INs;
	PLN *old_PLNs, *new_PLNs;

	height = t->height;

	max_PLN = 1;
	while (height) {
		max_PLN = max_PLN * MAX_NUM_ENTRY_IN;
		height--;
	}

	total_PLN = (t->last_PLN_id - t->first_PLN_id) + 2;

	if (total_PLN > max_PLN) {
		height = t->height;
		height++;
		for (i = 1; i < height; i++)
			total_IN += total_IN * MAX_NUM_ENTRY_IN;

		new_PLNs = allocINode();	//total_IN + total_PLN
		old_PLNs = (PLN *)t->root;
		memcpy(&new_PLNs[total_IN], &old_PLNs[t->first_PLN_id],
				sizeof(PLN) * (parent_id - t->first_PLN_id + 1));
		memcpy(&new_PLNs[total_IN + t->first_PLN_id - parent_id + 2],
				&old_PLNs[t->first_PLN_id], 
				sizeof(PLN) * (t->last_PLN_id - parent_id));
		memcpy(&new_PLNs[total_IN + t->first_PLN_id - parent_id + 1].entries,
				&new_PLNs[total_IN + t->first_PLN_id - parent_id].entries[new_PLNs[total_IN + t->first_PLN_id - parent_id].nKeys / 2],
				sizeof(struct PLN_entry) * (new_PLNs[total_IN + t->first_PLN_id - parent_id + 1].nKeys -
					(new_PLNs[total_IN + t->first_PLN_id - parent_id + 1].nKeys / 2)));
		new_PLNs[total_IN + t->first_PLN_id - parent_id + 1].nKeys =
			(new_PLNs[total_IN + t->first_PLN_id - parent_id].nKeys -
			 (new_PLNs[total_IN + t->first_PLN_id - parent_id].nKeys / 2));
		new_PLNs[total_IN + t->first_PLN_id - parent_id].nKeys =
			new_PLNs[total_IN + t->first_PLN_id - parent_id].nKeys / 2;
		reconstruct_from_PLN();
		t->height = height;
		t->root = (IN *)new_PLNs;
		t->last_PLN_id++;
	} else {
		new_PLNs = allocINode();
		old_PLNs = (PLN *)t->root;
		memcpy(&new_PLNs[t->first_PLN_id], &old_PLNs[t->first_PLN_id],
				sizeof(PLN) * (parent_id - t->first_PLN_id + 1));
		memcpy(&new_PLNs[parent_id + 2], &old_PLNs[parent_id + 1],
				sizeof(PLN) * (t->last_PLN_id - parent_id));
		memcpy(&new_PLNs[parent_id + 1].entries, 
				&new_PLNs[parent_id].entries[new_PLNs[parent_id].nKeys / 2],
				sizeof(struct PLN_entry) * (new_PLNs[parent_id].nKeys -
					(new_PLNs[parent_id].nKeys / 2)));
		new_PLNs[parent_id + 1].nKeys = (new_PLNs[parent_id].nKeys -
				(new_PLNs[parent_id].nKeys / 2));
		new_PLNs[parent_id].nKeys = new_PLNs[parent_id].nKeys / 2;
		reconstruct_from_PLN();
		t->root = (IN *)new_PLNs;
		t->last_PLN_id++;
	}
}

int insert_to_PLN(tree *t, unsigned long parent_id, 
		LN *split_node1, LN *split_node2)
{
	int i;
	unsigned long insert_key = split_node1->LN_Element[split_node1->nElements - 1].key;
	PLN *parent = (PLN *)(t->root + parent_id * sizeof(PLN));

	if (parent->nKeys == MAX_NUM_ENTRY_PLN) {
		reconstruct_PLN(t, parent_id, insert_key);
	} else {
		for (i = 0; i < parent->nKeys; i++) {
			if (insert_key < parent->entries[i].key) {
				memcpy(&parent->entries[i + 1], &parent->entries[i],
						sizeof(struct PLN_entry) * (parent->nKeys - i));
				parent->entries[i].key = insert_key;
				parent->entries[i].ptr = split_node1;
				parent->entries[i + 1].ptr = split_node2;
			}
		}
	}
	return i;
}

int leaf_split_and_insert(tree *t, LN *leaf, unsigned long key, void *value)
{
	int errval = -1, current_idx;
	LN *split_node1, *split_node2, *prev_leaf;
	PLN *prev_PLN;

	split_node1 = allocLNode();
	split_node2 = allocLNode();
	split_node1->sibling = split_node2;
	split_node2->sibling = leaf->sibling;
	if (split_node1 == NULL || split_node2 == NULL)
		return errval;

	leaf_scan_divide(t, leaf, split_node1, split_node2, key, value);
	current_idx = insert_to_PLN(t, leaf->parent_id, split_node1, split_node2);

	if (current_idx != 0) {
		prev_PLN = (PLN *)(t->root + (leaf->parent_id) * sizeof(PLN));
		prev_leaf = prev_PLN->entries[current_idx - 1].ptr;
	} else {	//need to fix
		if (leaf->parent_id > t->first_PLN_id) {
			prev_PLN = (PLN *)(t->root + ((leaf->parent_id - 1) * sizeof(PLN)));
			prev_leaf = prev_PLN->entries[prev_PLN->nKeys - 1].ptr;
		} else
			return 0;
	}

	prev_leaf->sibling = split_node1;
	flush_buffer(prev_leaf->sibling, 8, true);

	if (leaf == t->first_leaf)
		t->first_leaf = split_node1;

	return 0;
}

int Insert(tree *t, unsigned long key, void *value)
{
	int errval = -1;
	LN *leaf;

	if (t->root == NULL) {
		errval = create_new_tree(&t, key, value);
		if (errval < 0)
			goto fail;
		return errval;
	}

	leaf = find_leaf(t, key);
	if (leaf == NULL)
		goto fail;

	if (leaf->nElements < MAX_NUM_ENTRY_LN) {
		leaf->LN_Element[leaf->nElements + 1].flag = true;
		leaf->LN_Element[leaf->nElements + 1].key = key;
		leaf->LN_Element[leaf->nElements + 1].value = value;
		flush_buffer(&leaf->LN_Element[leaf->nElements + 1], 
				sizeof(struct LN_entry), true);
		leaf->nElements++;
		flush_buffer(&leaf->nElements, 1, true);
	}
	else {
		errval = leaf_split_and_insert(t, leaf, key, value);
		if (errval < 0)
			goto fail;
	}

	return 0;
fail:
	return errval;
}
