/* Developted by Se Kwon Lee in UNIST NECSST Lab
 * E-mail: sekwonlee90@gmail.com */

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

unsigned long IN_count = 0;
unsigned long LN_count = 0;

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

/* Alloc volatile node */
void *allocINode(unsigned long num)
{
	IN *new_INs = calloc(num, sizeof(IN));
	IN_count = num;
	return (void *)new_INs;
}

/* Alloc non-volatile leaf node */
LN *allocLNode()
{
	LN *new_LN = calloc(1, sizeof(LN));
	flush_buffer(new_LN, sizeof(LN), false);
	LN_count++;
	return new_LN;
}

tree *initTree()
{
	tree *t = calloc(1, sizeof(tree));
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
	unsigned int pos, id, i;
	IN *current_IN;
	PLN *current_PLN;
	LN *current_LN;

	if (t->is_leaf == 2) {
		id = 0;

		current_IN = (IN *)t->root;

		while (id < t->first_PLN_id) {
			pos = binary_search_IN(key, &current_IN[id]);
			/* MAX_NUM_ENTRY_IN = 2m + 1 */
			id = id * (MAX_NUM_ENTRY_IN)+ 1 + pos;
		}

		current_PLN = (PLN *)t->root;

		pos = binary_search_PLN(key, &current_PLN[id]);

		if (pos < current_PLN[id].nKeys)
			return current_PLN[id].entries[pos].ptr;
		else
			return NULL;
	} else if (t->is_leaf == 1) {
		current_PLN = (PLN *)t->root;

		pos = binary_search_PLN(key, &current_PLN[0]);

		if (pos < current_PLN[0].nKeys)
			return current_PLN[0].entries[pos].ptr;
		else
			return NULL;
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
				node->LN_Element[i].flag == false) {
			valid--;
		}
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

	current_LN = find_leaf(t, key);
	if (current_LN == NULL)
		goto fail;

	pos = search_leaf_node(current_LN, key);
	if (pos < 0)
		goto fail;

	return current_LN->LN_Element[pos].value;
fail:
	return;
}
/*
void insertion_sort(struct LN_entry *base, int num)
{
	unsigned long i, j;
	struct LN_entry temp;

//	MAX_NUM_ENTRY_LN

	for (i = 1; i < num; i++) {
		temp = base[i];
		j = i - 1;

		while (j >= 0 && base[j].key > temp.key) {
			base[j + 1] = base[j];
			j = j - 1;
		}
		base[j + 1] = temp;
	}
}
*/

void insertion_sort(struct LN_entry *base, int num)
{
	int i, j;
	struct LN_entry temp;

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

int Range_Lookup(tree *t, unsigned long start_key, unsigned int num, 
		unsigned long buf[])
{
	int i, j = 0, count = 0, invalid_count, valid_num;
	LN *current_LN;
	unsigned long *invalid_key =
		malloc(((MAX_NUM_ENTRY_LN)/2 + 1) * sizeof(unsigned long));
	struct LN_entry *valid_Element =
		malloc(MAX_NUM_ENTRY_LN * sizeof(struct LN_entry));

	current_LN = find_leaf(t, start_key);
	if (current_LN == NULL)
		goto fail;

	while (current_LN != NULL && j < num) {
		valid_num = 0;
		invalid_count = 0;
		for (i = 0; i < current_LN->nElements; i++) {
			if (current_LN->LN_Element[i].flag == false) {
				invalid_key[invalid_count] = current_LN->LN_Element[i].key;
				invalid_count++;
			}
		}

		for (i = 0; i < current_LN->nElements; i++) {
			if (current_LN->LN_Element[i].flag == false)
				continue;
	
			if (invalid_count > 0) {
				if (current_LN->LN_Element[i].key == invalid_key[count]) {
					count++;
					invalid_count--;
					continue;
				}
			}
			valid_Element[valid_num] = current_LN->LN_Element[i];
			valid_num++;
		}

		insertion_sort(valid_Element, valid_num);

		for (i = 0; i < current_LN->nElements; i++) {
			buf[j] = *(unsigned long *)valid_Element[i].value;
			j++;
			if (j == num)
				break;
		}
		current_LN = current_LN->sibling;
	}

	return 0;
fail:
	return -1;
}

int create_new_tree(tree *t, unsigned long key, void *value)
{
	int errval = -1;
	LN *current_LN;
	t->root = (LN *)allocLNode();
	if (t->root == NULL)
		return errval;

	current_LN = (LN *)t->root;

	current_LN->parent_id = -1;
	current_LN->sibling = NULL;
	current_LN->LN_Element[0].flag = true;
	current_LN->LN_Element[0].key = key;
	current_LN->LN_Element[0].value = value;
	current_LN->nElements++;
	flush_buffer(current_LN, sizeof(LN), true);

	t->height = 0;
	t->first_leaf = (LN *)t->root;
	t->is_leaf = 0;
	t->first_PLN_id = 0;
	t->last_PLN_id = 0;

	return 0;
}

void quick_sort(struct LN_entry *base, int left, int right)
{
	unsigned long i, j, pivot = base[left].key;
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

	for (i = 0; i < leaf->nElements; i++) {
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

//	quick_sort(valid_Element, 0, j - 1);
	insertion_sort(valid_Element, j);

	memcpy(split_node1->LN_Element, valid_Element, 
			sizeof(struct LN_entry) * (j / 2));
	split_node1->nElements = (j / 2);

	memcpy(split_node2->LN_Element, &valid_Element[j / 2],
			sizeof(struct LN_entry) * (j - (j / 2)));
	split_node2->nElements = (j - (j / 2));

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
	int i, errval;
	IN *prev_IN;
	PLN *current_PLN;

	id = first_PLN_id;
	last_id = first_PLN_id + num_PLN - 1;

	while (id <= last_id) {
		for (pos = 0; pos < MAX_NUM_ENTRY_IN; pos++) {
			current_PLN = (PLN *)root_addr;
			prev_id = (id - pos - 1) / MAX_NUM_ENTRY_IN;

			prev_IN = (IN *)root_addr;
			prev_IN[prev_id].key[prev_IN[prev_id].nKeys] =
				current_PLN[id].entries[current_PLN[id].nKeys - 1].key;
			prev_IN[prev_id].nKeys++;

			for (i = 0; i < current_PLN[id].nKeys; i++) {
				current_PLN[id].entries[i].ptr->parent_id = id;
	//			flush_buffer(&current_PLN[id].entries[i].ptr->parent_id,
	//					sizeof(unsigned long), false);
			}
	//		sfence();

			id++;
			if (id > last_id)
				break;
		}
		IN_count++;	//num_IN
	}

	first_IN_id = (first_PLN_id - 1) / MAX_NUM_ENTRY_IN;
	if (first_IN_id > 0) {
		errval = recursive_insert_IN(root_addr, first_IN_id, IN_count);
		if (errval < 0)
			return errval;
	}

	return 0;
}

int insert_node_to_PLN(PLN *new_PLNs, unsigned long parent_id, unsigned long insert_key,
		unsigned long split_max_key, LN *split_node1, LN *split_node2)
{
	int entry_index, i;
	unsigned long inserted_PLN_id;

	if (split_max_key >
			new_PLNs[parent_id].entries[new_PLNs[parent_id].nKeys - 1].key)
		inserted_PLN_id = parent_id + 1;
	else
		inserted_PLN_id = parent_id;

	for (i = 0; i < new_PLNs[inserted_PLN_id].nKeys; i++) {
		if (split_max_key <= new_PLNs[inserted_PLN_id].entries[i].key) {
			struct PLN_entry temp[new_PLNs[inserted_PLN_id].nKeys - i];
			memcpy(temp, &new_PLNs[inserted_PLN_id].entries[i],
					sizeof(struct PLN_entry) * (new_PLNs[inserted_PLN_id].nKeys - i));
			memcpy(&new_PLNs[inserted_PLN_id].entries[i + 1], temp,
					sizeof(struct PLN_entry) * (new_PLNs[inserted_PLN_id].nKeys - i));
			new_PLNs[inserted_PLN_id].entries[i].key = insert_key;
			new_PLNs[inserted_PLN_id].entries[i].ptr = split_node1;
			new_PLNs[inserted_PLN_id].entries[i + 1].ptr = split_node2;
			new_PLNs[inserted_PLN_id].nKeys++;
			entry_index = i;
			break;
		}
	}

	return entry_index;
}

int reconstruct_PLN(tree *t, unsigned long parent_id, unsigned long insert_key,
		unsigned long split_max_key, LN *split_node1, LN *split_node2)
{
	unsigned long height, max_PLN, total_PLN, total_IN = 1;
	unsigned long new_parent_id;
	unsigned int i, entry_index;
	int errval;
	IN *new_INs;
	PLN *old_PLNs, *new_PLNs;

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

			new_parent_id = total_IN + parent_id - t->first_PLN_id;

			new_PLNs = (PLN *)allocINode(total_IN + total_PLN);
			old_PLNs = (PLN *)t->root;
			/* total IN == new_first_PLN_id */
			memcpy(&new_PLNs[total_IN], &old_PLNs[t->first_PLN_id],
					sizeof(PLN) * (parent_id - t->first_PLN_id + 1));
			memcpy(&new_PLNs[new_parent_id + 2], &old_PLNs[parent_id + 1], 
					sizeof(PLN) * (t->last_PLN_id - parent_id));
			memcpy(&new_PLNs[new_parent_id + 1].entries,
					&new_PLNs[new_parent_id].entries[new_PLNs[new_parent_id].nKeys / 2],
					sizeof(struct PLN_entry) * (new_PLNs[new_parent_id].nKeys -
						(new_PLNs[new_parent_id].nKeys / 2)));

			new_PLNs[new_parent_id + 1].nKeys = (new_PLNs[new_parent_id].nKeys -
				 (new_PLNs[new_parent_id].nKeys / 2));
			new_PLNs[new_parent_id].nKeys = (new_PLNs[new_parent_id].nKeys / 2);

			entry_index = insert_node_to_PLN(new_PLNs, new_parent_id, 
					insert_key, split_max_key, split_node1, split_node2);

			errval = reconstruct_from_PLN(new_PLNs, total_IN, total_PLN);
			if (errval < 0)
				goto fail;

			free(t->root);
			t->height = height;
			t->root = (IN *)new_PLNs;
			t->first_PLN_id = total_IN;
			t->last_PLN_id = total_IN + total_PLN - 1;
			t->is_leaf = 2;
		} else {
			old_PLNs = (PLN *)t->root;
			new_PLNs = (PLN *)allocINode(t->first_PLN_id + total_PLN);

			/* copy from first PLN of old PLNs to parent_id's PLN of new PLNs */
			memcpy(&new_PLNs[t->first_PLN_id], &old_PLNs[t->first_PLN_id],
					sizeof(PLN) * (parent_id - t->first_PLN_id + 1));
			/* copy from (parent_id + 1) ~ last of old PLNs to parent_id + 2
			 * of newPLNsnew */
			memcpy(&new_PLNs[parent_id + 2], &old_PLNs[parent_id + 1],
					sizeof(PLN) * (t->last_PLN_id - parent_id));
			/* copy the half of PLN's entries to (parent_id + 1)'s PLN */
			memcpy(&new_PLNs[parent_id + 1].entries, 
					&new_PLNs[parent_id].entries[new_PLNs[parent_id].nKeys / 2],
					sizeof(struct PLN_entry) * (new_PLNs[parent_id].nKeys -
						(new_PLNs[parent_id].nKeys / 2)));

			new_PLNs[parent_id + 1].nKeys = (new_PLNs[parent_id].nKeys -
					(new_PLNs[parent_id].nKeys / 2));
			new_PLNs[parent_id].nKeys = (new_PLNs[parent_id].nKeys / 2);

			entry_index = insert_node_to_PLN(new_PLNs, parent_id, insert_key,
					split_max_key, split_node1, split_node2);

			errval = reconstruct_from_PLN(new_PLNs, t->first_PLN_id, total_PLN);
			if (errval < 0)
				goto fail;

			free(t->root);
			t->root = (IN *)new_PLNs;
			t->last_PLN_id++;
			t->is_leaf = 2;
		}
	} else {
		total_IN = 1;
		old_PLNs = (PLN *)t->root;
		new_PLNs = (PLN *)allocINode(total_IN + total_PLN);
		new_parent_id = total_IN + parent_id - t->first_PLN_id;

		memcpy(&new_PLNs[total_IN], &old_PLNs[t->first_PLN_id], sizeof(PLN) * 1);
		memcpy(&new_PLNs[2].entries, &new_PLNs[1].entries[new_PLNs[1].nKeys / 2],
				sizeof(struct PLN_entry) * (new_PLNs[1].nKeys - (new_PLNs[1].nKeys / 2)));

		new_PLNs[new_parent_id + 1].nKeys = (new_PLNs[new_parent_id].nKeys -
			 (new_PLNs[new_parent_id].nKeys / 2));
		new_PLNs[new_parent_id].nKeys = (new_PLNs[new_parent_id].nKeys / 2);

		entry_index = insert_node_to_PLN(new_PLNs, new_parent_id, 
				insert_key, split_max_key, split_node1, split_node2);

		errval = reconstruct_from_PLN(new_PLNs, total_IN, total_PLN);
		if (errval < 0)
			goto fail;

		free(t->root);
		t->height = 1;
		t->root = (IN *)new_PLNs;
		t->first_PLN_id = total_IN;
		t->last_PLN_id = total_IN + total_PLN - 1;
		t->is_leaf = 2;
	}
	return entry_index;
fail:
	return errval;
}

int insert_to_PLN(tree *t, unsigned long parent_id, 
		LN *split_node1, LN *split_node2)
{
	int entry_index;
	/* Newly inserted key to PLN */
	unsigned long insert_key = split_node1->LN_Element[split_node1->nElements - 1].key;
	unsigned long split_max_key = split_node2->LN_Element[split_node2->nElements - 1].key;

	PLN *parent = (PLN *)t->root;

	if (parent[parent_id].nKeys == MAX_NUM_ENTRY_PLN) {
		entry_index = reconstruct_PLN(t, parent_id, insert_key, 
				split_max_key, split_node1, split_node2);
		if (entry_index < 0)
			goto fail;
	} else if (split_max_key <= parent[parent_id].entries[parent[parent_id].nKeys - 1].key) {
		for (entry_index = 0; entry_index < parent[parent_id].nKeys; entry_index++) {
			if (split_max_key <= parent[parent_id].entries[entry_index].key) {

				struct PLN_entry temp[parent[parent_id].nKeys - entry_index];

				memcpy(temp, &parent[parent_id].entries[entry_index],
						sizeof(struct PLN_entry) * (parent[parent_id].nKeys - entry_index));
				memcpy(&parent[parent_id].entries[entry_index + 1], temp,
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
	}

fail:
	return entry_index;	//새로운 key가 삽입된 PLN의 entry index번호
}

void insert_entry_to_leaf(LN *leaf, unsigned long key, void *value, bool flush)
{
	if (flush == true) {
		leaf->LN_Element[leaf->nElements].flag = true;
		leaf->LN_Element[leaf->nElements].key = key;
		leaf->LN_Element[leaf->nElements].value = value;
		flush_buffer(&leaf->LN_Element[leaf->nElements], 
				sizeof(struct LN_entry), true);
		leaf->nElements++;
		flush_buffer(&leaf->nElements, sizeof(unsigned char), true);
	} else {
		leaf->LN_Element[leaf->nElements].flag = true;
		leaf->LN_Element[leaf->nElements].key = key;
		leaf->LN_Element[leaf->nElements].value = value;
		leaf->nElements++;
	}
}

int leaf_split_and_insert(tree *t, LN *leaf, unsigned long key, void *value)
{
	int errval = -1, current_idx;
	LN *split_node1, *split_node2, *prev_leaf, *new_leaf;
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
		if (current_idx < 0)
			goto fail;

		if (current_idx != 0) {
			prev_PLN = (PLN *)t->root;
			prev_leaf = prev_PLN[split_node1->parent_id].entries[current_idx - 1].ptr;
		} else {
			if (split_node1->parent_id > t->first_PLN_id) {
				prev_PLN = (PLN *)t->root;
				prev_leaf = prev_PLN[split_node1->parent_id - 1].entries[prev_PLN[split_node1->parent_id - 1].nKeys - 1].ptr;
			} else {
				t->first_leaf = split_node1;
				goto end;
			}
		}

		new_leaf = find_leaf(t, key);

		insert_entry_to_leaf(new_leaf, key, value, false);

		flush_buffer(split_node1, sizeof(LN), false);
		flush_buffer(split_node2, sizeof(LN), false);

		prev_leaf->sibling = split_node1;
		flush_buffer(&prev_leaf->sibling, sizeof(&prev_leaf->sibling), true);
	} else {
		PLN *new_PLN = (PLN *)allocINode(1);

		split_node1->parent_id = 0;
		split_node2->parent_id = 0;
		split_node2->sibling = NULL;

		new_PLN->entries[new_PLN->nKeys].key = 
			split_node1->LN_Element[split_node1->nElements - 1].key;
		new_PLN->entries[new_PLN->nKeys].ptr = split_node1;
		new_PLN->nKeys++;
		new_PLN->entries[new_PLN->nKeys].key = MAX_KEY;
		new_PLN->entries[new_PLN->nKeys].ptr = split_node2;
		new_PLN->nKeys++;

		t->height = 0;
		t->is_leaf = 1;
		t->first_PLN_id = 0;
		t->last_PLN_id = 0;
		t->root = new_PLN;

		new_leaf = find_leaf(t, key);
		
		insert_entry_to_leaf(new_leaf, key, value, false);

		flush_buffer(split_node1, sizeof(LN), false);
		flush_buffer(split_node2, sizeof(LN), true);
	}

end:
	free(leaf);
	return 0;
fail:
	return current_idx;
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
		printf("key = %lu\n",key);
		goto fail;
	}	

	if (leaf->nElements < MAX_NUM_ENTRY_LN)
		insert_entry_to_leaf(leaf, key, value, true);
	else {
		/* Insert after split */
		errval = leaf_split_and_insert(t, leaf, key, value);
		if (errval < 0)
			goto fail;
	}

	return 0;
fail:
	return errval;
}

void update_entry_to_leaf(LN *leaf, unsigned long old_key, void *old_value,
		unsigned long new_key, void *new_value, bool flush)
{
	if (flush == true) {
		leaf->LN_Element[leaf->nElements].flag = false;
		leaf->LN_Element[leaf->nElements].key = old_key;
		leaf->LN_Element[leaf->nElements].value = old_value;
		leaf->LN_Element[leaf->nElements + 1].flag = true;
		leaf->LN_Element[leaf->nElements + 1].key = new_key;
		leaf->LN_Element[leaf->nElements + 1].value = new_value;
		flush_buffer(&leaf->LN_Element[leaf->nElements], 
				sizeof(struct LN_entry) * 2, true);
		leaf->nElements = leaf->nElements + 2;
		flush_buffer(&leaf->nElements, sizeof(unsigned char), true);
	} else {
		leaf->LN_Element[leaf->nElements].flag = false;
		leaf->LN_Element[leaf->nElements].key = old_key;
		leaf->LN_Element[leaf->nElements].value = old_value;
		leaf->LN_Element[leaf->nElements + 1].flag = true;
		leaf->LN_Element[leaf->nElements + 1].key = new_key;
		leaf->LN_Element[leaf->nElements + 1].value = new_value;
		leaf->nElements = leaf->nElements + 2;
	}
}

int leaf_split_and_update(tree *t, LN *leaf, unsigned long key, void *value,
		unsigned long new_key, void *new_value)
{
	int errval = -1, current_idx;
	LN *split_node1, *split_node2, *prev_leaf, *new_leaf;
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
		if (current_idx < 0)
			goto fail;

		if (current_idx != 0) {
			prev_PLN = (PLN *)t->root;
			prev_leaf = prev_PLN[split_node1->parent_id].entries[current_idx - 1].ptr;
		} else {
			if (split_node1->parent_id > t->first_PLN_id) {
				prev_PLN = (PLN *)t->root;
				prev_leaf = prev_PLN[split_node1->parent_id - 1].entries[prev_PLN[split_node1->parent_id - 1].nKeys - 1].ptr;
			} else {
				t->first_leaf = split_node1;
				goto end;
			}
		}

		new_leaf = find_leaf(t, key);

		update_entry_to_leaf(new_leaf, key, value, new_key, new_value, false);

		flush_buffer(split_node1, sizeof(LN), false);
		flush_buffer(split_node2, sizeof(LN), false);

		prev_leaf->sibling = split_node1;
		flush_buffer(&prev_leaf->sibling, sizeof(&prev_leaf->sibling), true);
	} else {
		PLN *new_PLN = (PLN *)allocINode(1);

		split_node1->parent_id = 0;
		split_node2->parent_id = 0;
		split_node2->sibling = NULL;

		new_PLN->entries[new_PLN->nKeys].key = 
			split_node1->LN_Element[split_node1->nElements - 1].key;
		new_PLN->entries[new_PLN->nKeys].ptr = split_node1;
		new_PLN->nKeys++;
		new_PLN->entries[new_PLN->nKeys].key = MAX_KEY;
		new_PLN->entries[new_PLN->nKeys].ptr = split_node2;
		new_PLN->nKeys++;

		t->height = 0;
		t->is_leaf = 1;
		t->first_PLN_id = 0;
		t->last_PLN_id = 0;
		t->root = new_PLN;

		new_leaf = find_leaf(t, key);
		
		update_entry_to_leaf(new_leaf, key, value, new_key, new_value, false);

		flush_buffer(split_node1, sizeof(LN), false);
		flush_buffer(split_node2, sizeof(LN), true);
	}

end:
	free(leaf);
	return 0;
fail:
	return current_idx;
}

int Update(tree *t, unsigned long key, void *value)
{
	int errval = -1, pos, i;
	unsigned long old_key, new_key;
	void *old_value, *new_value;
	LN *current_LN;
	IN *current_IN = t->root;

	current_LN = find_leaf(t, key);
	if (current_LN == NULL)
		goto fail;

	if (current_LN->nElements < MAX_NUM_ENTRY_LN - 1)
		update_entry_to_leaf(current_LN, key, NULL, key, value, true);
	else {
		/* Insert after split */
		errval = leaf_split_and_update(t, current_LN, key, NULL, key, value);
		if (errval < 0)
			goto fail;
	}

	return 0;
fail:
	return errval;
}

void delete_entry_to_leaf(LN *leaf, unsigned long key, void *value, bool flush)
{
	if (flush == true) {
		leaf->LN_Element[leaf->nElements].flag = false;
		leaf->LN_Element[leaf->nElements].key = key;
		leaf->LN_Element[leaf->nElements].value = value;
		flush_buffer(&leaf->LN_Element[leaf->nElements], 
				sizeof(struct LN_entry), true);
		leaf->nElements++;
		flush_buffer(&leaf->nElements, sizeof(unsigned char), true);
	} else {
		leaf->LN_Element[leaf->nElements].flag = false;
		leaf->LN_Element[leaf->nElements].key = key;
		leaf->LN_Element[leaf->nElements].value = value;
		leaf->nElements++;
	}
}

int leaf_split_and_delete(tree *t, LN *leaf, unsigned long key, void *value)
{
	int errval = -1, current_idx;
	LN *split_node1, *split_node2, *prev_leaf, *new_leaf;
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
		if (current_idx < 0)
			goto fail;

		if (current_idx != 0) {
			prev_PLN = (PLN *)t->root;
			prev_leaf = prev_PLN[split_node1->parent_id].entries[current_idx - 1].ptr;
		} else {
			if (split_node1->parent_id > t->first_PLN_id) {
				prev_PLN = (PLN *)t->root;
				prev_leaf = prev_PLN[split_node1->parent_id - 1].entries[prev_PLN[split_node1->parent_id - 1].nKeys - 1].ptr;
			} else {
				t->first_leaf = split_node1;
				goto end;
			}
		}

		new_leaf = find_leaf(t, key);

		delete_entry_to_leaf(new_leaf, key, value, false);

		flush_buffer(split_node1, sizeof(LN), false);
		flush_buffer(split_node2, sizeof(LN), false);

		prev_leaf->sibling = split_node1;
		flush_buffer(&prev_leaf->sibling, sizeof(&prev_leaf->sibling), true);
	} else {
		PLN *new_PLN = (PLN *)allocINode(1);

		split_node1->parent_id = 0;
		split_node2->parent_id = 0;
		split_node2->sibling = NULL;

		new_PLN->entries[new_PLN->nKeys].key = 
			split_node1->LN_Element[split_node1->nElements - 1].key;
		new_PLN->entries[new_PLN->nKeys].ptr = split_node1;
		new_PLN->nKeys++;
		new_PLN->entries[new_PLN->nKeys].key = MAX_KEY;
		new_PLN->entries[new_PLN->nKeys].ptr = split_node2;
		new_PLN->nKeys++;

		t->height = 0;
		t->is_leaf = 1;
		t->first_PLN_id = 0;
		t->last_PLN_id = 0;
		t->root = new_PLN;

		new_leaf = find_leaf(t, key);
		
		delete_entry_to_leaf(new_leaf, key, value, false);

		flush_buffer(split_node1, sizeof(LN), false);
		flush_buffer(split_node2, sizeof(LN), true);
	}

end:
	free(leaf);
	return 0;
fail:
	return current_idx;
}

int Delete(tree *t, unsigned long key)
{
	int errval = -1;
	LN *leaf;

	leaf = find_leaf(t, key);
	if (leaf == NULL)
		goto fail;

	if (leaf->nElements < MAX_NUM_ENTRY_LN)
		delete_entry_to_leaf(leaf, key, NULL, true);
	else {
		/* Delete after split */
		errval = leaf_split_and_delete(t, leaf, key, NULL);
		if (errval < 0)
			goto fail;
	}

	return 0;
fail:
	return errval;
}
