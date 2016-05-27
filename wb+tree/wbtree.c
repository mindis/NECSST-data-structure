#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <x86intrin.h>
#include <malloc.h>
#include <stdint.h>
#include "wbtree.h"

char *redoLog;
int redoLog_offset=0;
long clflush_cnt;

#define mfence() asm volatile("mfence":::"memory")

long long mbtime=0;

static inline void clflush(volatile void *__p)
{
	//asm volatile("clflush %0" : "+m" (*(volatile char __force *)__p));
	clflush_cnt++;
	//__builtin_ia32_clflush((void *) __p);
	asm volatile("clflush %0" : "+m" (*(volatile char *)__p));
}

long long clftime=0;

int clflush_range(void *start, void *end){
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
int clflush_range_nomb(void *start, void *end){
	start= (void *)((unsigned long)start &~(CACHE_LINE_SIZE-1));
	//printf("start = %x start&63 = %x\n",start,(unsigned long)start&~(CACHE_LINE_SIZE-1));
	void *addr;
	for(addr=start; addr<end; addr+=CACHE_LINE_SIZE){
		clflush(addr);
	}
	/// Flush any possible final partial cacheline:
}

node* allocNode(){
	node *n = (node *)memalign(64,sizeof(node));
	memset(n->slot,0,8);
	n->isleaf = 1;
	return n;
}

tree *initTree(){
	tree *t =malloc(sizeof(tree)); 
	t->root = allocNode(); 
	t->height = 0;
	clflush_cnt=0;
	return t;
}

void *Lookup(tree *t, long key){
	node *curr = t->root;
	curr = find_leaf_node(curr, key);
	int loc = Search(curr, curr->slot, key);
	if( loc > curr->slot[0] ) loc = curr->slot[0];
	if( curr->entries[curr->slot[loc]].key != key || loc > curr->slot[0] ){ 
		return NULL;
	}
	return curr->entries[curr->slot[loc]].ptr;
}

int Append(node *n, long key, void *value){
	int checkBit = (1 << 8) - 1;
	int j, missingMin = 0;

	for (j = 1; j <= n->slot[0]; j++)
		checkBit ^= (1 << n->slot[j]);

	while ((checkBit % 2) == 0){
		checkBit = checkBit >> 1;
		missingMin++;
	}

	n->entries[missingMin].key = key;
	n->entries[missingMin].ptr = value;
	//Flush
	return missingMin; 
}

int Append_in_inner(node *n, long key, void *value){
	int checkBit = (1<<8)-1;
	int j;
	for( j=1; j<=n->slot[0]; j++ )
		checkBit ^= (1<<n->slot[j]);
	int missingMin = 0;
	while( checkBit%2==0 ){
		checkBit = checkBit>>1;
		missingMin++;
	}
	n->entries[missingMin].key = key;
	n->entries[missingMin].ptr = value;
	//Flush
	return missingMin;
}

int Search(node *curr, char *temp, long key){
	int low = 1, mid = 1;
	int high = temp[0];

	while (low <= high){
		mid = (low+high)/2;
		if (curr->entries[temp[mid]].key > key)
			high = mid-1;
		else if (curr->entries[temp[mid]].key < key)
			low = mid+1;
		else
			break;
	}

	if (low > mid) 
		mid = low;

	return mid;
}

node *find_leaf_node(node *curr, long key){
	int loc;

	if (curr->isleaf) 
		return curr;
	loc = Search(curr, curr->slot, key);

	if (loc > curr->slot[0]) 
		find_leaf_node(curr->entries[curr->slot[loc-1]].ptr, key);
	else if (curr->entries[curr->slot[loc]].key == key) 
		find_leaf_node(curr->entries[curr->slot[loc]].ptr, key);
	else if (loc == 1) 
		find_leaf_node(curr->leftmostPtr, key);
	else 
		find_leaf_node(curr->entries[curr->slot[loc-1]].ptr, key);
}


void Insert(tree *t, long key, void *value){
	int numEntries;
	node *curr = t->root;
	/* Find proper leaf */
	curr = find_leaf_node(curr, key);

	/* Check overflow & split */
	numEntries = curr->slot[0];
	if(numEntries == NODE_SIZE){
		node *splitNode = allocNode();
		int j, cp = curr->slot[0];

		//overflown node
		node* log = allocNode(); 
		memcpy(log, curr,sizeof(node));
		clflush_range((void *)(log),(void *)(log)+sizeof(node));

		for( j=min_live_entries; j>0; j-- ){
			int loc = Append(splitNode, curr->entries[curr->slot[cp]].key, curr->entries[curr->slot[cp]].ptr);
			splitNode->slot[j] = loc;
			splitNode->slot[0]++;
			cp--;
		}
		if( splitNode->entries[splitNode->slot[1]].key > key ){
			curr->slot[0] -= min_live_entries;
			//insert_in_leaf_noflush(curr, key, value);
			insert_in_leaf(curr, key, value); //In this function, slot and new entry will be flushed
			clflush_range((void *)(splitNode),(void *)(splitNode)+sizeof(node));
		}
		else{
			curr->slot[0] -= min_live_entries;
			clflush_range((void *)(curr->slot),(void *)(curr->slot)+8);
			insert_in_leaf_noflush(splitNode, key, value);
			clflush_range((void *)(splitNode),(void *)(splitNode)+sizeof(node));
		}

		//flush

		//flush

		insert_in_parent(t, curr, splitNode->entries[splitNode->slot[1]].key, splitNode);

	}
	else{
		insert_in_leaf(curr, key, value);
	}
}

void insert_in_leaf_noflush(node *curr, long key, void *value){
	int loc = Append(curr,key,value);

	char temp[8];

	int mid = Search(curr, curr->slot, key);
	int j;
	for( j=curr->slot[0]; j>=mid; j-- )
		temp[j+1] = curr->slot[j];

	temp[mid] = loc;

	for( j=mid-1; j>=1; j-- )
		temp[j] = curr->slot[j];

	temp[0] = curr->slot[0]+1;

	*((uint64_t *)curr->slot) = *((uint64_t *)temp);
}

void insert_in_leaf(node *curr, long key, void *value){
	char temp[8];
	int loc, mid, j;

	loc = Append(curr,key,value);

	clflush_range((void *)&(curr->entries[loc]),
			(void *)&(curr->entries[loc])+sizeof(entry));

	mid = Search(curr, curr->slot, key);

	for (j = curr->slot[0]; j >= mid; j--)
		temp[j+1] = curr->slot[j];

	temp[mid] = loc;

	for (j = mid-1; j >= 1; j--)
		temp[j] = curr->slot[j];

	temp[0] = curr->slot[0] + 1;

	*((uint64_t *)curr->slot) = *((uint64_t *)temp);
	clflush_range((void *)(curr->slot),(void *)(curr->slot)+8);
}

void insert_in_inner(node *curr, long key, void *value){
	int loc = Append_in_inner(curr, key, value);
	clflush_range((void *)&(curr->entries[loc]),(void *)&(curr->entries[loc])+sizeof(entry));

	char temp[8];

	int mid = Search(curr, curr->slot, key);
	int j;

	for (j = curr->slot[0]; j >= mid; j--)
		temp[j+1] = curr->slot[j];

	temp[mid] = loc;

	for (j = mid-1; j >= 1; j--)
		temp[j] = curr->slot[j];

	temp[0] = curr->slot[0] + 1;

	for (j = 0; j <= temp[0]; j++)
		curr->slot[j] = temp[j];

	clflush_range((void *)(curr->slot),(void *)(curr->slot)+8);
}
void insert_in_inner_noflush(node *curr, long key, void *value){
	int loc = Append_in_inner(curr, key, value);

	char temp[8];

	int mid = Search(curr, curr->slot, key);
	int j;

	for( j=curr->slot[0]; j>=mid; j-- )
		temp[j+1] = curr->slot[j];

	temp[mid] = loc;

	for( j=mid-1; j>=1; j-- )
		temp[j] = curr->slot[j];

	temp[0] = curr->slot[0]+1;

	for( j=0; j<=temp[0]; j++ )
		curr->slot[j] = temp[j];
}
void insert_in_parent(tree *t, node *curr, long key, node *splitNode){
	if( curr == t->root ){
		node *root = allocNode();
		root->isleaf = 0;
		root->leftmostPtr = curr;
		root->entries[0].ptr = splitNode;
		root->entries[0].key = key;
		curr->parent = root;
		splitNode->parent = root;

		root->slot[1] = 0;
		root->slot[0] = 1;
		clflush_range((void *)(root),(void *)(root)+sizeof(node));

		t->root = root;
		clflush_range((void *)&(t->root),(void *)&(t->root)+sizeof(node *));
		return;
	}

	node *parent = curr->parent;
	if( parent->slot[0] < NODE_SIZE ){
		int loc = Append_in_inner(parent, key, splitNode);
		clflush_range((void *)&(curr->entries[loc]),(void *)&(curr->entries[loc])+sizeof(entry));
		splitNode->parent = parent;

		int mid = Search(parent, parent->slot, key);
		char temp[8];
		int j;

		for( j=parent->slot[0]; j>=mid; j-- )
			temp[j+1] = parent->slot[j];

		temp[mid] = loc;

		for( j=mid-1; j>=1; j-- )
			temp[j] = parent->slot[j];

		temp[0] = parent->slot[0]+1;

		for( j=0; j<=temp[0]; j++ )
			parent->slot[j] = temp[j];

		clflush_range((void *)(parent->slot),(void *)(parent->slot)+8);

	}
	else{
		int log_start_offset = redoLog_offset;
		node* log = allocNode();
		memcpy(log, curr,sizeof(node));
		clflush_range((void *)(log),(void *)(log)+sizeof(node));
		//    memcpy(&redoLog[redoLog_offset], splitParent,sizeof(node));
		node *splitParent = allocNode();
		splitParent->isleaf = 0;
		int j = min_live_entries, cp = parent->slot[0];
		for( ; j>0; j-- ){
			int loc = Append_in_inner(splitParent, parent->entries[parent->slot[cp]].key, parent->entries[parent->slot[cp]].ptr);
			node *child = parent->entries[parent->slot[cp]].ptr;
			child->parent = splitParent;
			splitParent->slot[j] = loc;
			splitParent->slot[0]++;
			cp--;
		}
		if( splitParent->entries[splitParent->slot[1]].key > key ){
			parent->slot[0] -= min_live_entries;
			insert_in_inner(parent, key, splitNode);
			splitNode->parent = parent;
			clflush_range((void *)(splitParent),(void *)splitParent+sizeof(node));
		}
		else{
			parent->slot[0] -= min_live_entries;
			clflush_range((void *)(parent->slot),(void *)(parent->slot)+8);
			splitNode->parent = splitParent;
			insert_in_inner_noflush(splitParent, key, splitNode);
			clflush_range((void *)(splitParent),(void *)splitParent+sizeof(node));
		}


		//overflown node
		//   redoLog_offset+=sizeof(node);

		//sibling node

		insert_in_parent(t, parent, splitParent->entries[splitParent->slot[1]].key, splitParent);
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
