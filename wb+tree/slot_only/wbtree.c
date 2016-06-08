#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <x86intrin.h>
#include <malloc.h>
#include <stdint.h>
#include <time.h>
#include "wbtree.h"

char *redoLog;
int redoLog_offset=0;
long clflush_cnt;

#define mfence() asm volatile("mfence":::"memory")
#define sfence() asm volatile("sfence":::"memory")

long long mbtime=0;

static inline void clflush(volatile void *__p)
{
	//asm volatile("clflush %0" : "+m" (*(volatile char __force *)__p));
	clflush_cnt++;
	//__builtin_ia32_clflush((void *) __p);
	asm volatile("clflush %0" : "+m" (*(volatile char *)__p));
}

long long clftime=0;

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
//		mfence();
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
	memset(n->slot,0,8);
	n->isleaf = 1;
	return n;
}

tree *initTree()
{
	tree *t =malloc(sizeof(tree)); 
	t->root = allocNode(); 
	t->height = 0;
	clflush_cnt=0;
	flush_buffer(t, sizeof(tree), true);
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

int Append(node *n, unsigned long key, void *value)
{
	int checkBit = (1 << 8) - 1;
	int i, j, missingMin = 0;

	for (j = 1; j <= n->slot[0]; j++)
		checkBit ^= (1 << n->slot[j]);

	while ((checkBit % 2) == 0) {
		checkBit = checkBit >> 1;
		missingMin++;
	}

	n->entries[missingMin].key = key;
	n->entries[missingMin].ptr = value;
	//Flush
	return missingMin; 
}

int Append_in_inner(node *n, unsigned long key, void *value)
{
	int checkBit = (1 << 8) - 1;
	int j, missingMin = 0;

	for (j = 1; j <= n->slot[0]; j++)
		checkBit ^= (1 << n->slot[j]);

	while ((checkBit % 2) == 0) {
		checkBit = checkBit >> 1;
		missingMin++;
	}

	n->entries[missingMin].key = key;
	n->entries[missingMin].ptr = value;
	//Flush
	return missingMin;
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

node *find_leaf_node(node *curr, unsigned long key){
	int loc;

	if (curr->isleaf) 
		return curr;
	loc = Search(curr, curr->slot, key);

	if (loc > curr->slot[0]) 
		find_leaf_node(curr->entries[curr->slot[loc - 1]].ptr, key);
	else if (curr->entries[curr->slot[loc]].key <= key) 
		find_leaf_node(curr->entries[curr->slot[loc]].ptr, key);
	else if (loc == 1) 
		find_leaf_node(curr->leftmostPtr, key);
	else 
		find_leaf_node(curr->entries[curr->slot[loc - 1]].ptr, key);
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
			cp--;
		}

		add_redo_logentry();
		curr->slot[0] -= min_live_entries;

		if (splitNode->entries[splitNode->slot[1]].key > key) {
			add_redo_logentry();	//slot redo logging for insert_in_leaf_noflush
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
	char temp[8];
	int loc, mid, j;

	loc = Append(curr, key, value);

	mid = Search(curr, curr->slot, key);

	for (j = curr->slot[0]; j >= mid; j--)
		temp[j + 1] = curr->slot[j];

	temp[mid] = loc;

	for (j = mid-1; j >= 1; j--)
		temp[j] = curr->slot[j];

	temp[0] = curr->slot[0] + 1;

	*((uint64_t *)curr->slot) = *((uint64_t *)temp);
	return loc;
}

void insert_in_leaf(node *curr, unsigned long key, void *value){
	char temp[8];
	int loc, mid, j;

	loc = Append(curr,key,value);

	flush_buffer(&(curr->entries[loc]), sizeof(entry), true);

	mid = Search(curr, curr->slot, key);

	for (j = curr->slot[0]; j >= mid; j--)
		temp[j+1] = curr->slot[j];

	temp[mid] = loc;

	for (j = mid-1; j >= 1; j--)
		temp[j] = curr->slot[j];

	temp[0] = curr->slot[0] + 1;

	*((uint64_t *)curr->slot) = *((uint64_t *)temp);
	flush_buffer(curr->slot, 8, true);
}

void insert_in_inner(node *curr, unsigned long key, void *value)
{
	int mid, j, loc;
	char temp[8];

	loc = Append_in_inner(curr, key, value);
	flush_buffer(&(curr->entries[loc]), sizeof(entry), true);

	mid = Search(curr, curr->slot, key);

	for (j = curr->slot[0]; j >= mid; j--)
		temp[j+1] = curr->slot[j];

	temp[mid] = loc;

	for (j = mid-1; j >= 1; j--)
		temp[j] = curr->slot[j];

	temp[0] = curr->slot[0] + 1;

	*((uint64_t *)curr->slot) = *((uint64_t *)temp);
	flush_buffer(curr->slot, 8, true);
}

int insert_in_inner_noflush(node *curr, unsigned long key, void *value)
{
	int mid, j, loc;
	char temp[8];

	loc = Append_in_inner(curr, key, value);

	mid = Search(curr, curr->slot, key);

	for( j=curr->slot[0]; j>=mid; j-- )
		temp[j+1] = curr->slot[j];

	temp[mid] = loc;

	for( j=mid-1; j>=1; j-- )
		temp[j] = curr->slot[j];

	temp[0] = curr->slot[0]+1;

	*((uint64_t *)curr->slot) = *((uint64_t *)temp);

	return loc;
}

void insert_in_parent(tree *t, node *curr, unsigned long key, node *splitNode) {
	if (curr == t->root) {
		node *root = allocNode();
		root->isleaf = 0;
		root->leftmostPtr = curr;
		root->entries[0].ptr = splitNode;
		root->entries[0].key = key;
		splitNode->parent = root;

		root->slot[1] = 0;
		root->slot[0] = 1;
		flush_buffer(root, sizeof(node), false);
		flush_buffer(splitNode, sizeof(node), false);

		add_redo_logentry();
		curr->parent = root;
		add_redo_logentry();
		t->root = root;
		return ;
	}

	node *parent = curr->parent;

	if (parent->slot[0] < NODE_SIZE) {
		int mid, j, loc;
		char temp[8];

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
		*((uint64_t *)parent->slot) = *((uint64_t *)temp);
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
			cp--;
		}

		add_redo_logentry();
		parent->slot[0] -= min_live_entries;

		if (splitParent->entries[splitParent->slot[1]].key > key) {
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

void printNode(node *n){
	int i,numEntries=n->slot[0];
	printf("slot array: %d |", n->slot[0]);
	for(i=1; i<=numEntries; i++){
		printf("index: %d |", n->slot[i]);
	}
	printf("\n");
	for(i=0; i<NODE_SIZE; i++){
		printf("Key: %ld, Value: %ld |",n->entries[i].key,n->entries[i].ptr);
	}
	printf("\n");
	if(!n->isleaf){
		n = n->entries[1].ptr;
		printNode(n);
	}
}
