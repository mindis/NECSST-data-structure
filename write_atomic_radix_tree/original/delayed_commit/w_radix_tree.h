//#define META_NODE_SHIFT	9

#define META_NODE_SHIFT 3

#define CACHE_LINE_SIZE 64

typedef struct Tree tree;
typedef struct Node node;
typedef struct Logentry logentry;

struct Logentry {
	unsigned long addr;
	unsigned long old_value;
	unsigned long new_value;
};

struct Node {
	void *entry_ptr[8];
	node *parent_ptr;
	unsigned long p_index;
};

struct Tree {
	node *root;
	unsigned char height;
};

void flush_buffer(void *buf, unsigned int len);
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
