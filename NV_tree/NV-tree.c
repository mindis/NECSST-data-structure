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

void *allocINode(unsigned long num)
{
	IN *new_INs = calloc(num, sizeof(IN));
	return (void *)new_INs;
}

LN *allocLNode()
{
	LN *new_LN = calloc(1, sizeof(LN));
	flush_buffer(new_LN, sizeof(LN), false);
	return new_LN;
}

tree *initTree()
{
	tree *t = calloc(1, sizeof(tree));
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

//	if (low > mid)
//		mid = low;

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
	unsigned int pos, id, i;
	IN *current_IN;
	PLN *current_PLN;
	LN *current_LN;

	if (t->is_leaf == 2) {
		id = 0;

		current_IN = (IN *)t->root;
	//	printf("current_IN->nKeys = %lu\n", current_IN->nKeys);

		while (id < t->first_PLN_id) {
			pos = binary_search_IN(key, &current_IN[id]);
			//	id = id * ((2 * (MAX_NUM_ENTRY_PLN - 1)) + 1) + 1 + pos;
			id = id * (MAX_NUM_ENTRY_IN) + 1 + pos;
		}

		current_PLN = (PLN *)t->root;
		printf("id in find leaf = %lu\n", id);
		pos = binary_search_PLN(key, &current_PLN[id]);
		printf("pos in find leaf = %lu\n", pos);
		printf("current_PLN[%d].nKeys = %lu\n", id, current_PLN[id].nKeys);
		printf("current_PLN[%d].last_ptr->nElements = %lu\n", id, current_PLN[id].last_ptr->nElements);
		if (pos < current_PLN[id].nKeys) {
			return current_PLN[id].entries[pos].ptr;
		}
		else {
			for (i = 0; i < current_PLN[id].last_ptr->nElements; i++)
				printf("current_PLN[%d].last_ptr->LN_Element[%d].key = %lu		%lu\n", id, i, current_PLN[id].last_ptr->LN_Element[i].key,
						*(unsigned long *)current_PLN[id].last_ptr->LN_Element[i].value);
			return current_PLN[id].last_ptr;
		}
	} else if (t->is_leaf == 1) {
		current_PLN = (PLN *)t->root;
		pos = binary_search_PLN(key, &current_PLN[0]);
		if (pos < current_PLN[0].nKeys)
			return current_PLN[0].entries[pos].ptr;
		else
			return current_PLN[0].last_ptr;
	} else {
		current_LN = (LN *)t->root;
		return current_LN;
	}
}

int search_leaf_node(LN *node, unsigned long key)
{
	int i, pos, valid = 0;

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
	int pos, i;
	LN *current_LN;
	IN *current_IN = t->root;
//	for (i = 0; i < current_IN->nKeys; i++)
//		printf("current_IN->key[%d] = %lu\n", i, current_IN->key[i]);

	current_LN = find_leaf(t, key);
//	printf("current_LN->nElements = %lu\n", current_LN->nElements);
//	printf("current_LN->parent_id = %lu\n", current_LN->parent_id);
	pos = search_leaf_node(current_LN, key);
	printf("fuck you!!!!!\n");
	printf("pos = %d\n", pos);
	if (pos < 0) {
		goto fail;
	}

	return current_LN->LN_Element[pos].value;
fail:
	return;
}

void Range_Lookup(tree *t, unsigned long start_key, unsigned int num, 
		unsigned long buf[])
{

}

int create_new_tree(tree *t, unsigned long key, void *value)
{
	int errval = -1;
	LN *current_LN;
	t->root = (LN *)allocLNode();
	if (t->root == NULL)
		return errval;

	current_LN = (LN *)t->root;
	current_LN->LN_Element[0].flag = true;
	current_LN->LN_Element[0].key = key;
	current_LN->LN_Element[0].value = value;
	current_LN->parent_id = -1;
	current_LN->nElements++;

	t->height = 0;
	t->first_leaf = (LN *)t->root;
	t->is_leaf = 0;
	t->first_PLN_id = 0;
	t->last_PLN_id = 0;
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

int leaf_scan_divide(tree *t, LN *leaf, LN *split_node1, LN *split_node2)
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

	for (i = 0; i < leaf->nElements; i++) {
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
			sizeof(struct LN_entry) * (j / 2));
	split_node1->nElements = (j / 2);

	memcpy(split_node2->LN_Element, &valid_Element[j / 2],
			sizeof(struct LN_entry) * (j - (j / 2)));
	split_node2->nElements = (j - (j / 2));
/*
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
*/
	return 0;
}

int recursive_insert_IN(void *root_addr, unsigned long first_IN_id,
		unsigned long num_IN)
{
	int errval = -1;
	unsigned long id, last_id, pos, prev_id, IN_count = 0;
	IN *current_IN, *prev_IN;

	id = first_IN_id;
	last_id = first_IN_id + num_IN - 1;
	printf("recursive_insert_in \n");

	if (id > 0) {
		while (id <= last_id) {
			for (pos = 0; pos < MAX_NUM_ENTRY_IN; pos++) {
				current_IN = (IN *)root_addr;
				prev_id = (id - pos - 1) / MAX_NUM_ENTRY_IN;
				prev_IN = (IN *)root_addr;
				prev_IN[prev_id].key[prev_IN[prev_id].nKeys] =
					current_IN[id].key[current_IN[id].nKeys - 1];
				prev_IN[prev_id].nKeys++;
				id++;
				if (id > last_id)
					break;
			}
			IN_count++;
		}
		first_IN_id = (first_IN_id - 1) / MAX_NUM_ENTRY_IN;
		errval = recursive_insert_IN(root_addr, first_IN_id, IN_count);
		if (errval < 0)
			goto fail;
	}
	errval = 0;
fail:
	return errval;
}

int reconstruct_from_PLN(void *root_addr, unsigned long first_PLN_id, 
		unsigned long num_PLN)
{
	unsigned long prev_id, id, pos, last_id, IN_count = 0, first_IN_id;
	int i;
	IN *prev_IN;
	PLN *current_PLN;

	id = first_PLN_id;
	last_id = first_PLN_id + num_PLN - 1;

	while (id < last_id) {
		for (pos = 0; pos < MAX_NUM_ENTRY_IN; pos++) {
			current_PLN = (PLN *)root_addr;

			prev_id = (id - pos - 1) / MAX_NUM_ENTRY_IN;

			prev_IN = (IN *)root_addr;
			prev_IN[prev_id].key[prev_IN[prev_id].nKeys] = 
				current_PLN[id].entries[current_PLN[id].nKeys - 1].key;
			prev_IN[prev_id].nKeys++;

			for (i = 0; i < current_PLN[id].nKeys; i++)
				current_PLN[id].entries[i].ptr->parent_id = id;
			current_PLN[id].last_ptr->parent_id = id;

			id++;
			if (id > last_id)
				break;
		}
		IN_count++;	//num_IN
	}
	first_IN_id = (first_PLN_id - 1) / MAX_NUM_ENTRY_IN;
	if (first_IN_id > 0)
		recursive_insert_IN(root_addr, first_IN_id, IN_count);
	return 0;
}

int insert_node_to_PLN(PLN *new_PLNs, unsigned long parent_id, unsigned long insert_key,
		LN *split_node1, LN *split_node2)
{
	int entry_index, i;

	if (new_PLNs[parent_id].entries[new_PLNs[parent_id].nKeys - 1].key < insert_key
			&& insert_key < new_PLNs[parent_id + 1].entries[0].key) {
		new_PLNs[parent_id].entries[new_PLNs[parent_id].nKeys].key = insert_key;
		new_PLNs[parent_id].entries[new_PLNs[parent_id].nKeys].ptr = split_node1;
		new_PLNs[parent_id].last_ptr = split_node2;
		new_PLNs[parent_id].last_ptr->parent_id = parent_id;
//		new_PLNs[parent_id].last_ptr->sibling = split_node2;
//		split_node1->sibling = new_PLNs[parent_id].last_ptr;
		new_PLNs[parent_id].nKeys++;
		memcpy(new_PLNs[parent_id + 1].entries, &new_PLNs[parent_id + 1].entries[1],
				sizeof(struct PLN_entry) * (new_PLNs[parent_id + 1].nKeys - 1));
		new_PLNs[parent_id + 1].nKeys--;
//		new_PLNs[parent_id + 1].entries[0].ptr = split_node2;
		entry_index = new_PLNs[parent_id].nKeys - 1;
	} else if (insert_key < new_PLNs[parent_id].entries[new_PLNs[parent_id].nKeys - 1].key) {
		for (i = 0; i < new_PLNs[parent_id].nKeys; i++) {
			if (insert_key < new_PLNs[parent_id].entries[i].key) {
				memcpy(&new_PLNs[parent_id].entries[i + 1], &new_PLNs[parent_id].entries[i],
						sizeof(struct PLN_entry) * (new_PLNs[parent_id].nKeys - i));
				new_PLNs[parent_id].entries[i].key = insert_key;
				new_PLNs[parent_id].entries[i].ptr = split_node1;
				new_PLNs[parent_id].entries[i + 1].ptr = split_node2;

				new_PLNs[parent_id].nKeys++;

				split_node1->parent_id = parent_id;
				split_node2->parent_id = parent_id;
				entry_index = i;
				break;
			}
		}
	} else if (insert_key > new_PLNs[parent_id + 1].entries[0].key) {
		if (insert_key > new_PLNs[parent_id + 1].entries[new_PLNs[parent_id + 1].nKeys - 1].key) {
			new_PLNs[parent_id + 1].entries[new_PLNs[parent_id + 1].nKeys].key = insert_key;
			new_PLNs[parent_id + 1].entries[new_PLNs[parent_id + 1].nKeys].ptr = split_node1;
			new_PLNs[parent_id + 1].last_ptr = split_node2;

			new_PLNs[parent_id + 1].nKeys++;

			split_node1->parent_id = parent_id + 1;
			split_node2->parent_id = parent_id + 1;
			entry_index = new_PLNs[parent_id + 1].nKeys - 1;
		} else {
			for (i = 0; i < new_PLNs[parent_id + 1].nKeys; i++) {
				if (insert_key < new_PLNs[parent_id + 1].entries[i].key) {
					memcpy(&new_PLNs[parent_id + 1].entries[i + 1], 
							&new_PLNs[parent_id + 1].entries[i],
							sizeof(struct PLN_entry) * (new_PLNs[parent_id + 1].nKeys - i));
					new_PLNs[parent_id + 1].entries[i].key = insert_key;
					new_PLNs[parent_id + 1].entries[i].ptr = split_node1;
					new_PLNs[parent_id + 1].entries[i + 1].ptr = split_node2;

					new_PLNs[parent_id + 1].nKeys++;

					split_node1->parent_id = parent_id + 1;
					split_node2->parent_id = parent_id + 1;
					entry_index = i;
					break;
				}
			}
		}
	}
	return entry_index;
}

int reconstruct_PLN(tree *t, unsigned long parent_id, unsigned long insert_key,
		LN *split_node1, LN *split_node2)
{
	unsigned long height, max_PLN, total_PLN, total_IN = 1;
	unsigned int i, entry_index;
	IN *new_INs;
	PLN *old_PLNs, *new_PLNs;
//	printf("reconstruct PLN\n");

	height = t->height;

	max_PLN = 1;

	while (height) {
		max_PLN = max_PLN * MAX_NUM_ENTRY_IN;
		height--;
	}

	total_PLN = (t->last_PLN_id - t->first_PLN_id) + 2;

	if (t->is_leaf != 1) {
		if (total_PLN > max_PLN) {
			height = t->height;
			height++;
			for (i = 1; i < height; i++)
				total_IN += total_IN * MAX_NUM_ENTRY_IN;

			new_PLNs = (PLN *)allocINode(total_IN + total_PLN);	//total_IN + total_PLN
			old_PLNs = (PLN *)t->root;
			memcpy(&new_PLNs[total_IN], &old_PLNs[t->first_PLN_id],
					sizeof(PLN) * (parent_id - t->first_PLN_id + 1));
			memcpy(&new_PLNs[total_IN + t->first_PLN_id - parent_id + 2],
					&old_PLNs[parent_id + 1], 
					sizeof(PLN) * (t->last_PLN_id - parent_id));
			memcpy(&new_PLNs[total_IN + t->first_PLN_id - parent_id + 1].entries,
					&new_PLNs[total_IN + t->first_PLN_id - parent_id].entries[new_PLNs[total_IN + t->first_PLN_id - parent_id].nKeys / 2],
					sizeof(struct PLN_entry) * (new_PLNs[total_IN + t->first_PLN_id - parent_id].nKeys -
						(new_PLNs[total_IN + t->first_PLN_id - parent_id].nKeys / 2)));
			new_PLNs[total_IN + t->first_PLN_id - parent_id + 1].last_ptr =
				new_PLNs[total_IN + t->first_PLN_id - parent_id].last_ptr;

			new_PLNs[total_IN + t->first_PLN_id - parent_id + 1].nKeys =
				(new_PLNs[total_IN + t->first_PLN_id - parent_id].nKeys -
				 (new_PLNs[total_IN + t->first_PLN_id - parent_id].nKeys / 2));
			new_PLNs[total_IN + t->first_PLN_id - parent_id].nKeys =
				(new_PLNs[total_IN + t->first_PLN_id - parent_id].nKeys / 2);

			entry_index = insert_node_to_PLN(new_PLNs, total_IN + t->first_PLN_id - parent_id, 
					insert_key, split_node1, split_node2);

			reconstruct_from_PLN(new_PLNs, total_IN, total_PLN);
			free(t->root);
			t->height = height;
			t->root = (IN *)new_PLNs;
			t->first_PLN_id = total_IN;
			t->last_PLN_id = total_IN + total_PLN - 1;
			t->is_leaf = 2;
		} else {
			old_PLNs = (PLN *)t->root;
			new_PLNs = (PLN *)allocINode(t->first_PLN_id + total_PLN);

			/* old PLN에서 첫번째 PLN부터 parent_id's PLN까지 복사 */
			memcpy(&new_PLNs[t->first_PLN_id], &old_PLNs[t->first_PLN_id],
					sizeof(PLN) * (parent_id - t->first_PLN_id + 1));
			/* old PLN의 parent_id + 1부터 마지막 PLN까지를 new의 parent_id + 2부터
			 * 해서 복사 */
			memcpy(&new_PLNs[parent_id + 2], &old_PLNs[parent_id + 1],
					sizeof(PLN) * (t->last_PLN_id - parent_id));
			/* parent_id's PLN의 entry값의 반을 parent_id + 1로 복사 */
			memcpy(&new_PLNs[parent_id + 1].entries, 
					&new_PLNs[parent_id].entries[new_PLNs[parent_id].nKeys / 2],
					sizeof(struct PLN_entry) * (new_PLNs[parent_id].nKeys -
						(new_PLNs[parent_id].nKeys / 2)));
			new_PLNs[parent_id + 1].last_ptr = new_PLNs[parent_id].last_ptr;

			new_PLNs[parent_id + 1].nKeys = (new_PLNs[parent_id].nKeys -
					(new_PLNs[parent_id].nKeys / 2));
			new_PLNs[parent_id].nKeys = (new_PLNs[parent_id].nKeys / 2);

			//		new_PLNs[parent_id].last_ptr = new_PLNs[parent_id].entries[new_PLNs[parent_id].nKeys].ptr;

			entry_index = insert_node_to_PLN(new_PLNs, parent_id, insert_key,
					split_node1, split_node2);

			reconstruct_from_PLN(new_PLNs, t->first_PLN_id, total_PLN);
			free(t->root);
			t->root = (IN *)new_PLNs;
			t->last_PLN_id++;
			t->is_leaf = 2;
		}
	} else {
		total_IN = 1;
		old_PLNs = (PLN *)t->root;
		new_PLNs = (PLN *)allocINode(total_IN + total_PLN);

		//parent_id = 0, t->first_PLN_id = 0
		memcpy(&new_PLNs[total_IN], &old_PLNs[t->first_PLN_id], sizeof(PLN) * 1);
//		memcpy(&new_PLNs[total_IN + t->first_PLN_id - parent_id + 1].entries,
//				&new_PLNs[total_IN + t->first_PLN_id - parent_id].entries[new_PLNs[total_IN + t->first_PLN_id - parent_id].nKeys / 2],
//				sizeof(struct PLN_entry) * (new_PLNs[total_IN + t->first_PLN_id - parent_id + 1].nKeys -
//					(new_PLNs[total_IN + t->first_PLN_id - parent_id + 1].nKeys / 2)));
		memcpy(&new_PLNs[2].entries, &new_PLNs[1].entries[new_PLNs[1].nKeys / 2],
				sizeof(struct PLN_entry) * (new_PLNs[1].nKeys - (new_PLNs[1].nKeys / 2)));
		new_PLNs[total_IN + t->first_PLN_id - parent_id + 1].last_ptr =
			new_PLNs[total_IN + t->first_PLN_id - parent_id].last_ptr;

		new_PLNs[total_IN + t->first_PLN_id - parent_id + 1].nKeys =
			(new_PLNs[total_IN + t->first_PLN_id - parent_id].nKeys -
			 (new_PLNs[total_IN + t->first_PLN_id - parent_id].nKeys / 2));
		new_PLNs[total_IN + t->first_PLN_id - parent_id].nKeys =
			(new_PLNs[total_IN + t->first_PLN_id - parent_id].nKeys / 2);

		//		new_PLNs[total_IN + t->first_PLN_id - parent_id].last_ptr =
		//			new_PLNs[total_IN + t->first_PLN_id - parent_id].entries[new_PLNs[total_IN + t->first_PLN_id - parent_id].nKeys].ptr;

		entry_index = insert_node_to_PLN(new_PLNs, total_IN + t->first_PLN_id - parent_id, 
				insert_key, split_node1, split_node2);

		reconstruct_from_PLN(new_PLNs, total_IN, total_PLN);
		free(t->root);
		t->height = 1;
		t->root = (IN *)new_PLNs;
		t->first_PLN_id = total_IN;
		t->last_PLN_id = total_IN + total_PLN - 1;
		t->is_leaf = 2;
	}
	return entry_index;
}

int insert_to_PLN(tree *t, unsigned long parent_id, 
		LN *split_node1, LN *split_node2)
{
	int entry_index;
	/* PLN에 새로 삽입될 key */
	unsigned long insert_key = split_node1->LN_Element[split_node1->nElements - 1].key;

	PLN *parent = (PLN *)t->root;

	if (parent[parent_id].nKeys == MAX_NUM_ENTRY_PLN) {
		entry_index = reconstruct_PLN(t, parent_id, insert_key, split_node1, split_node2);
	} else if (insert_key < parent[parent_id].entries[parent[parent_id].nKeys - 1].key) {
		for (entry_index = 0; entry_index < parent[parent_id].nKeys; entry_index++) {
			if (insert_key < parent[parent_id].entries[entry_index].key) {
				memcpy(&parent[parent_id].entries[entry_index + 1], &parent[parent_id].entries[entry_index],
						sizeof(struct PLN_entry) * (parent[parent_id].nKeys - entry_index));
				parent[parent_id].entries[entry_index].key = insert_key;
				parent[parent_id].entries[entry_index].ptr = split_node1;
				parent[parent_id].entries[entry_index + 1].ptr = split_node2;

				split_node1->parent_id = parent_id;
				split_node2->parent_id = parent_id;
				parent[parent_id].nKeys++;
				break;
			}
		}
	} else {
		parent[parent_id].entries[parent[parent_id].nKeys].key = insert_key;
		parent[parent_id].entries[parent[parent_id].nKeys].ptr = split_node1;
		parent[parent_id].last_ptr = split_node2;
		split_node1->parent_id = parent_id;
		split_node2->parent_id = parent_id;
		entry_index = parent[parent_id].nKeys;
		parent[parent_id].nKeys++;
	}

	return entry_index;	//새로운 key가 삽입된 PLN의 entry index번호
}

int leaf_split_and_insert(tree *t, LN *leaf)
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

	leaf_scan_divide(t, leaf, split_node1, split_node2);

	if (t->is_leaf != 0) {
		current_idx = insert_to_PLN(t, leaf->parent_id, split_node1, split_node2);
/*
 * leaf->parent_id 쓰는것 바꿔야함
		if (current_idx != 0) {
			prev_PLN = (PLN *)t->root;
			prev_leaf = prev_PLN[leaf->parent_id].entries[current_idx - 1].ptr;
		} else {
			if (leaf->parent_id > t->first_PLN_id) {
				prev_PLN = (PLN *)t->root;
				prev_leaf = prev_PLN[leaf->parent_id - 1].entries[prev_PLN[leaf->parent_id - 1].nKeys - 1].ptr;
			} else {
				t->first_leaf = split_node1;
				goto end;
			}
		}

		prev_leaf->sibling = split_node1;
		flush_buffer(prev_leaf->sibling, 8, true);

		if (leaf == t->first_leaf)
			t->first_leaf = split_node1;
*/			
	} else {
		PLN *new_PLN = (PLN *)allocINode(1);
		split_node1->parent_id = 0;
		split_node2->parent_id = 0;
		new_PLN->entries[new_PLN->nKeys].key = split_node1->LN_Element[split_node1->nElements - 1].key;
		new_PLN->entries[new_PLN->nKeys].ptr = split_node1;
		new_PLN->last_ptr = split_node2;
		new_PLN->nKeys++;
		t->height = 0;
		t->is_leaf = 1;
		t->first_PLN_id = 0;
		t->last_PLN_id = 0;
		t->root = new_PLN;
		t->first_leaf = split_node1;
	}

end:
	free(leaf);
	return 0;
}

int Insert(tree *t, unsigned long key, void *value)
{
	int errval = -1;
	LN *leaf;

	if (t->root == NULL) {
		errval = create_new_tree(t, key, value);
		if (errval < 0)
			goto fail;
		return errval;
	}

	leaf = find_leaf(t, key);
	if (leaf == NULL) {
		printf("key = %d\n",key);
		goto fail;
	}


	if (leaf->nElements < MAX_NUM_ENTRY_LN) {
		leaf->LN_Element[leaf->nElements].flag = true;
		leaf->LN_Element[leaf->nElements].key = key;
		leaf->LN_Element[leaf->nElements].value = value;
		flush_buffer(&leaf->LN_Element[leaf->nElements], 
				sizeof(struct LN_entry), true);
		leaf->nElements++;
		flush_buffer(&leaf->nElements, 1, true);
	}
	else {
		errval = leaf_split_and_insert(t, leaf);
		leaf = find_leaf(t, key);
		leaf->LN_Element[leaf->nElements].flag = true;
		leaf->LN_Element[leaf->nElements].key = key;
		leaf->LN_Element[leaf->nElements].value = value;
		leaf->nElements++;
		if (errval < 0)
			goto fail;
	}

	return 0;
fail:
	return errval;
}
