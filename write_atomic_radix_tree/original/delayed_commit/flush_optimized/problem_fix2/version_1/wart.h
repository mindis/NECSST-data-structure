#include <stdbool.h>

#define META_NODE_SHIFT 8
#define CACHE_LINE_SIZE 64
#define NUM_ENTRY	(0x1UL << META_NODE_SHIFT)

typedef struct Tree 		tree;
typedef struct Node 		node;
typedef struct Node_Header 	node_header;
typedef struct Node2		node2;

struct Node_Header {
	unsigned long p_index;
	node_header *parent_ptr;
	void *next_ptr;
};

struct Node2 {
	unsigned char group_num;
	void *entry_ptr[NUM_ENTRY / 16];
	node2 *next_ptr;
};

struct Node {
	node *parent_ptr;
	unsigned long p_index;
	void *entry_ptr[NUM_ENTRY];
//	char dummy[16];		// 2
	char dummy[48];		// 3, 4, 5, 6, 7, 8, 9
};

struct Tree {
	unsigned long height;
	node_header *root;
};

void flush_buffer(void *buf, unsigned long len, bool fence);
tree *initTree();
int Insert(tree **t, unsigned long key, void *value);
void *Lookup(tree *t, unsigned long key);
