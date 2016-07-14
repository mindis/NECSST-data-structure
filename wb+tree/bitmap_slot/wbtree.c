#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <x86intrin.h>
#include <malloc.h>
#include <stdint.h>
#include <time.h>
#include "wbtree.h"

#define mfence() asm volatile("mfence":::"memory")
#define sfence() asm volatile("sfence":::"memory")
#define BITOP_WORD(nr)	((nr) / BITS_PER_LONG)

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

node *allocNode()
{
	node *n = malloc(sizeof(node));
	memset(n->slot,0,sizeof(n->slot));
	n->bitmap = 1;
	n->isleaf = 1;
	return n;
}

tree *initTree()
{
	tree *t =malloc(sizeof(tree)); 
	t->root = allocNode(); 
	t->height = 0;
	return t;
}

void *Lookup(tree *t, unsigned long key)
{
	node *curr = t->root;
	curr = find_leaf_node(curr, key);
	int loc = Search(curr, curr->slot, key);

	if (loc > curr->slot[0]) 
		loc = curr->slot[0];

	if (curr->entries[curr->slot[loc]].key != key || loc > curr->slot[0])
		return NULL;

	return curr->entries[curr->slot[loc]].ptr;
}

void Range_Lookup(tree *t, unsigned long start_key, unsigned int num, 
		unsigned long buf[])
{
	int loc, i;
	unsigned long search_count = 0;
	struct timespec t1, t2;
	unsigned long elapsed_time;
	node *curr = t->root;

//	clock_gettime(CLOCK_MONOTONIC, &t1);
	curr = find_leaf_node(curr, start_key);
	loc = Search(curr, curr->slot, start_key);
//	printf("loc = %d\n", loc);
//	clock_gettime(CLOCK_MONOTONIC, &t2);
//	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
//	elapsed_time += (t2.tv_nsec - t1.tv_nsec);
//	printf("Range lookup 1 = %lu\n", elapsed_time);

//	clock_gettime(CLOCK_MONOTONIC, &t1);
	while (search_count < num) {
		for (i = loc; i <= curr->slot[0]; i++) {
			buf[search_count] = *(unsigned long *)curr->entries[curr->slot[i]].ptr;
			search_count++;
			if(search_count == num) {
//				clock_gettime(CLOCK_MONOTONIC, &t2);
//				elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
//				elapsed_time += (t2.tv_nsec - t1.tv_nsec);
//				printf("Range lookup 2 = %lu\n", elapsed_time);
				return ;
			}
		}

		curr = curr->leftmostPtr;
		if (curr == NULL) {
			printf("error\n");
			return ;
		}
		loc = 1;
	}
//	clock_gettime(CLOCK_MONOTONIC, &t2);
//	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
//	elapsed_time += (t2.tv_nsec - t1.tv_nsec);
//	printf("Range lookup 2 = %lu\n", elapsed_time);
}

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

int Append(node *n, unsigned long key, void *value)
{
	unsigned long index;
	index = find_next_zero_bit(&n->bitmap, BITMAP_SIZE, 1) - 1;

	n->entries[index].key = key;
	n->entries[index].ptr = value;
	return index;
}

int Append_in_inner(node *n, unsigned long key, void *value)
{
	unsigned long index;
	index = find_next_zero_bit(&n->bitmap, BITMAP_SIZE, 1) - 1;

	n->entries[index].key = key;
	n->entries[index].ptr = value;
	return index;
}

int Search(node *curr, char *temp, unsigned long key)
{
	int low = 1, mid = 1;
	int high = temp[0];

	while (low <= high){
		mid = (low + high) / 2;
		if (curr->entries[temp[mid]].key > key)
			high = mid - 1;
		else if (curr->entries[temp[mid]].key < key)
			low = mid + 1;
		else
			break;
	}

	if (low > mid) 
		mid = low;

	return mid;
}

node *find_leaf_node(node *curr, unsigned long key) 
{
	int loc;

	if (curr->isleaf) 
		return curr;
	loc = Search(curr, curr->slot, key);

	if (loc > curr->slot[0]) 
		return find_leaf_node(curr->entries[curr->slot[loc - 1]].ptr, key);
	else if (curr->entries[curr->slot[loc]].key <= key) 
		return find_leaf_node(curr->entries[curr->slot[loc]].ptr, key);
	else if (loc == 1) 
		return find_leaf_node(curr->leftmostPtr, key);
	else 
		return find_leaf_node(curr->entries[curr->slot[loc - 1]].ptr, key);
}


void Insert(tree *t, unsigned long key, void *value){
	int numEntries;
	node *curr = t->root;
	/* Find proper leaf */
	curr = find_leaf_node(curr, key);

	/* Check overflow & split */
	numEntries = curr->slot[0];
	if(numEntries == NODE_SIZE){
		node *splitNode = allocNode();
		int j, loc, cp = curr->slot[0];
		splitNode->leftmostPtr = curr->leftmostPtr;

		//overflown node
		for (j = min_live_entries; j > 0; j--) {
			loc = Append(splitNode, curr->entries[curr->slot[cp]].key, 
					curr->entries[curr->slot[cp]].ptr);
			splitNode->slot[j] = loc;
			splitNode->slot[0]++;
			splitNode->bitmap = splitNode->bitmap + (0x1UL << (loc + 1));
			curr->bitmap = curr->bitmap & (~(0x1UL << (curr->slot[cp] + 1)));
			cp--;
		}

		add_redo_logentry();
		curr->slot[0] -= min_live_entries;

		if (splitNode->entries[splitNode->slot[1]].key > key) {
			add_redo_logentry();	//slot redo logging for insert_in_leaf_noflush
			add_redo_logentry();
			loc = insert_in_leaf_noflush(curr, key, value);
			flush_buffer(&(curr->entries[loc]), sizeof(entry), false);
		}
		else
			insert_in_leaf_noflush(splitNode, key, value);

		insert_in_parent(t, curr, splitNode->entries[splitNode->slot[1]].key, splitNode);
		add_redo_logentry();
		curr->leftmostPtr = splitNode;
		sfence();
		add_commit_entry();
	}
	else{
		insert_in_leaf(curr, key, value);
	}
}

int insert_in_leaf_noflush(node *curr, unsigned long key, void *value)
{
	char temp[SLOT_SIZE];
	int loc, mid, j;

	curr->bitmap = curr->bitmap - 1;
	loc = Append(curr, key, value);

	mid = Search(curr, curr->slot, key);

	for (j = curr->slot[0]; j >= mid; j--)
		temp[j + 1] = curr->slot[j];

	temp[mid] = loc;

	for (j = mid-1; j >= 1; j--)
		temp[j] = curr->slot[j];

	temp[0] = curr->slot[0] + 1;

	for (j = 0; j < temp[0] + 1; j++)
		curr->slot[j] = temp[j];

	curr->bitmap = curr->bitmap + 1 + (0x1UL << (loc + 1));
	return loc;
}

void insert_in_leaf(node *curr, unsigned long key, void *value){
	char temp[SLOT_SIZE];
	int loc, mid, j;

	curr->bitmap = curr->bitmap - 1;
	flush_buffer(&curr->bitmap, sizeof(unsigned long), true);
	loc = Append(curr, key, value);
	flush_buffer(&(curr->entries[loc]), sizeof(entry), false);

	mid = Search(curr, curr->slot, key);

	for (j = curr->slot[0]; j >= mid; j--)
		temp[j + 1] = curr->slot[j];

	temp[mid] = loc;

	for (j = mid-1; j >= 1; j--)
		temp[j] = curr->slot[j];

	temp[0] = curr->slot[0] + 1;

	for (j = 0; j < temp[0] + 1; j++)
		curr->slot[j] = temp[j];
	flush_buffer(curr->slot, sizeof(curr->slot), true);
	
	curr->bitmap = curr->bitmap + 1 + (0x1UL << (loc + 1));
	flush_buffer(&curr->bitmap, sizeof(unsigned long), true);
}

void insert_in_inner(node *curr, unsigned long key, void *value)
{
	char temp[SLOT_SIZE];
	int loc, mid, j;

	curr->bitmap = curr->bitmap - 1;
	flush_buffer(&curr->bitmap, sizeof(unsigned long), true);
	loc = Append_in_inner(curr, key, value);
	flush_buffer(&(curr->entries[loc]), sizeof(entry), false);

	mid = Search(curr, curr->slot, key);

	for (j = curr->slot[0]; j >= mid; j--)
		temp[j + 1] = curr->slot[j];

	temp[mid] = loc;

	for (j = mid-1; j >= 1; j--)
		temp[j] = curr->slot[j];

	temp[0] = curr->slot[0] + 1;

	for (j = 0; j < temp[0] + 1; j++)
		curr->slot[j] = temp[j];
	flush_buffer(curr->slot, sizeof(curr->slot), true);
	
	curr->bitmap = curr->bitmap + 1 + (0x1UL << (loc + 1));
	flush_buffer(&curr->bitmap, sizeof(unsigned long), true);
}

int insert_in_inner_noflush(node *curr, unsigned long key, void *value)
{
	char temp[SLOT_SIZE];
	int loc, mid, j;

	curr->bitmap = curr->bitmap - 1;
	loc = Append_in_inner(curr, key, value);

	mid = Search(curr, curr->slot, key);

	for (j = curr->slot[0]; j >= mid; j--)
		temp[j + 1] = curr->slot[j];

	temp[mid] = loc;

	for (j = mid-1; j >= 1; j--)
		temp[j] = curr->slot[j];

	temp[0] = curr->slot[0] + 1;

	for (j = 0; j < temp[0] + 1; j++)
		curr->slot[j] = temp[j];

	curr->bitmap = curr->bitmap + 1 + (0x1UL << (loc + 1));
	return loc;
}

void insert_in_parent(tree *t, node *curr, unsigned long key, node *splitNode) {
	if (curr == t->root) {
		node *root = allocNode();
		root->isleaf = 0;
		root->leftmostPtr = curr;
		root->bitmap = root->bitmap + (0x1UL << 1);
		root->entries[0].ptr = splitNode;
		root->entries[0].key = key;
		splitNode->parent = root;

		root->slot[1] = 0;
		root->slot[0] = 1;
		flush_buffer(root, sizeof(node), false);
		flush_buffer(splitNode, sizeof(node), false);

		add_redo_logentry();
		curr->parent = root;
		t->root = root;
		return ;
	}

	node *parent = curr->parent;

	if (parent->slot[0] < NODE_SIZE) {
		int mid, j, loc;
		char temp[SLOT_SIZE];

		add_redo_logentry();
		parent->bitmap = parent->bitmap - 1;
		loc = Append_in_inner(parent, key, splitNode);
		flush_buffer(&(parent->entries[loc]), sizeof(entry), false);
		splitNode->parent = parent;
		flush_buffer(splitNode, sizeof(node), false);

		mid = Search(parent, parent->slot, key);

		for (j = parent->slot[0]; j >= mid; j--)
			temp[j+1] = parent->slot[j];

		temp[mid] = loc;

		for (j = mid-1; j >= 1; j--)
			temp[j] = parent->slot[j];

		temp[0] = parent->slot[0]+1;

		add_redo_logentry();
		for (j = 0; j < temp[0] + 1; j++)
			parent->slot[j] = temp[j];
		add_redo_logentry();
		parent->bitmap = parent->bitmap + 1 + (0x1UL << (loc + 1));
	}
	else {
		int j, loc, cp = parent->slot[0];
		node *splitParent = allocNode();
		splitParent->isleaf = 0;

		for (j = min_live_entries ; j > 0; j--) {
			loc = Append_in_inner(splitParent,parent->entries[parent->slot[cp]].key, parent->entries[parent->slot[cp]].ptr);
			node *child = parent->entries[parent->slot[cp]].ptr;
			add_redo_logentry();
			child->parent = splitParent;
			splitParent->slot[j] = loc;
			splitParent->slot[0]++;
			splitParent->bitmap = splitParent->bitmap + (0x1UL << (loc + 1));
			parent->bitmap = parent->bitmap & (~(0x1UL << (parent->slot[cp] + 1)));
			cp--;
		}

		add_redo_logentry();
		parent->slot[0] -= min_live_entries;

		if (splitParent->entries[splitParent->slot[1]].key > key) {
			add_redo_logentry();
			add_redo_logentry();
			loc = insert_in_inner_noflush(parent, key, splitNode);
			flush_buffer(&(parent->entries[loc]), sizeof(entry), false);
			splitNode->parent = parent;
			flush_buffer(splitNode, sizeof(node), false);
		}
		else {
			splitNode->parent = splitParent;
			flush_buffer(splitNode, sizeof(node), false);
			insert_in_inner_noflush(splitParent, key, splitNode);
		}

		insert_in_parent(t, parent, 
				splitParent->entries[splitParent->slot[1]].key, splitParent);
	}
}

void *Update(tree *t, unsigned long key, void *value)
{
	node *curr = t->root;
	curr = find_leaf_node(curr, key);
	int loc = Search(curr, curr->slot, key);

	if (loc > curr->slot[0]) 
		loc = curr->slot[0];

	if (curr->entries[curr->slot[loc]].key != key || loc > curr->slot[0])
		return NULL;

	curr->entries[curr->slot[loc]].ptr = value;
	flush_buffer(&curr->entries[curr->slot[loc]].ptr, 8, true);

	return curr->entries[curr->slot[loc]].ptr;
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

	if ((fp = fopen("/home/sekwon/Public/input_file/input_2billion.txt","r")) == NULL)
	{
		puts("error");
		exit(0);
	}

	keys = malloc(sizeof(unsigned long) * 100000100);
	buf = malloc(sizeof(unsigned long) * 100000100);
	memset(buf, 0, sizeof(unsigned long) * 100000100);
	for(i = 0; i < 100000100; i++) {
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
	dummy = (char *)malloc(15*1024*1024);
	memset(dummy, 0, 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024, true);

	for(i = 0; i < 100000000; i++)
		Insert(t, keys[i], &keys[i]);

	sprintf(line, "/proc/%d/status", getpid());
	fp2 = fopen(line, "r");
	if (fp2 == NULL)
		return ;

	while (fgets(line, 1024, fp2) != NULL) {
		if (strstr(line, "VmSize")) {
			char tmp[32];
			char size[32];
			sscanf(line,"%s%s", tmp, size);
			nVmSize = atoi(size);
			printf("nVmSize = %lu KB\n", nVmSize);
		}
		else if (strstr(line, "VmRSS")) {
			char tmp[32];
			char size[32];
			sscanf(line, "%s%s", tmp, size);
			nVmRss = atoi(size);
			printf("nVmRSS = %lu KB\n", nVmRss);
			break;
		}
	}
	fclose(fp2);

	printf("sizeof(node) = %d\n", sizeof(node));

	memset(dummy, 0, 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024, true);

	for(i = 100000000; i < 100000100; i++)
		Insert(t, keys[i], &keys[i]);

	memset(dummy, 0, 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024, true);

	for(i = 0; i < 100000100; i++) {
		ret = (void *)Lookup(t, keys[i]);		
		if (ret == NULL) {
			printf("There is no key[%d] = %lu\n", i, keys[i]);
			exit(1);
		}
		else {
			printf("Search value = %lu\n", *(unsigned long*)ret);
		}
	}

	memset(dummy, 0, 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024, true);

	Range_Lookup(t, min, 100000100, buf);

//	for (i = 0; i < 100000100; i++)
//		printf("buf[%d] = %lu\n", i, buf[i]);

	return 0;
}*/
