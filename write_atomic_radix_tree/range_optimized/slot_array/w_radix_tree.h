#include <stdbool.h>

#define META_NODE_SHIFT 3
#define CACHE_LINE_SIZE 64
#define NUM_ENTRY	0x1UL << META_NODE_SHIFT

typedef struct Tree tree;
typedef struct Node node;
typedef struct Logentry logentry;

struct Logentry {
	unsigned long addr;
	unsigned long old_value;
	unsigned long new_value;
};

struct Node {
	unsigned int p_index;
	node *parent_ptr;
	char slot[NUM_ENTRY + 1]
	void *entry_ptr[NUM_ENTRY];
};

struct Tree {
	unsigned char height;
	node *root;
};

void flush_buffer(void *buf, unsigned int len, bool fence);
void flush();
void flush_range();
void add_logentry();
node *allocNode(node *parent, unsigned int index);
tree *initTree();
tree *CoW_Tree(node *changed_root, unsigned char height);
int increase_radix_tree_height(tree **t, unsigned char new_height);
int recursive_alloc_nodes(node *temp_node, unsigned long key, void *value,
		unsigned char height);
int recursive_search_leaf(node *level_ptr, unsigned long key, void *value,
		unsigned char height);
int Insert(tree **t, unsigned long key, void *value);
void *Lookup(tree *t, unsigned long key);
node *search_to_next_leaf(node *next_branch, unsigned char height);
node *find_next_leaf(tree *t, node *parent, unsigned int index,
		unsigned char height);
void Range_Lookup(tree *t, unsigned long start_key, unsigned long num,
		unsigned long buf[]);
