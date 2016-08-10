#include <stdbool.h>

#define META_NODE_SHIFT 	8
#define MAX_HEIGHT			8
#define CACHE_LINE_SIZE 	64
#define NUM_ENTRY			(0x1UL << META_NODE_SHIFT)
#define NODE_ORIGIN			1
#define ITEM_LAZY			2

typedef struct Tree tree;
typedef struct Node node;
typedef struct Item item;
typedef struct Entry entry;

unsigned long node_count;
unsigned long item_count;

struct Entry {
	unsigned long key;
	void *value;
};

struct Item {
	unsigned char type;
	unsigned long key;
	void *value;
	item *next_ptr;
};

struct Node {
	unsigned char type;
	unsigned long p_index;
	void *entry_ptr[NUM_ENTRY];
	node *parent_ptr;
//	char dummy[16];		// 2
	char dummy[40];		// 3, 4, 5, 6, 7, 8, 9
};

struct Tree {
	unsigned long height;
	node *root;
};

void flush_buffer(void *buf, unsigned long len, bool fence);
tree *initTree();
int Insert(tree **t, unsigned long key, void *value);
void *Update(tree *t, unsigned long key, void *value);
void *Lookup(tree *t, unsigned long key);
void find_next_leaf(node *curr_node, unsigned long index, unsigned long height,
		unsigned long buf[], unsigned long *count, unsigned long num);
void search_entry_in_node(node *level_ptr, unsigned long index, unsigned long height,
		unsigned long buf[], unsigned long *count, unsigned long num);
void Range_Lookup(tree *t, unsigned long start_key, unsigned long num,
		unsigned long buf[]);
void Delete(tree *t, unsigned long key);
