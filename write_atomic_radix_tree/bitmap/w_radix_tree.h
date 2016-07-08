#include <stdbool.h>

#define META_NODE_SHIFT 6
#define CACHE_LINE_SIZE 64
#define NUM_ENTRY	0x1UL << META_NODE_SHIFT

#define BITS_PER_LONG	64
#define BITMAP_SIZE		NUM_ENTRY

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
	unsigned long bitmap;
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
//node *allocNode();
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

static inline unsigned long __ffs(unsigned long word)
{
	asm("rep; bsf %1,%0"
		: "=r" (word)
		: "rm" (word));
	return word;
}
