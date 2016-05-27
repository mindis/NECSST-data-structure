//#define META_NODE_SHIFT	9

#define META_NODE_SHIFT 3

#define CACHE_LINE_SIZE 64

typedef struct Tree tree;
typedef struct Node node;
typedef struct Logentry logentry;
unsigned long elapsed_node_flush;
unsigned long elapsed_entry_flush;
unsigned long node_count;
unsigned long entry_count;

struct Logentry {
	unsigned long addr;
	unsigned long old_value;
	unsigned long new_value;
};

struct Node {
	void *entry_ptr[8];
	node *parent_ptr;
	node *sibling_ptr;
	unsigned int p_index;
};

struct Tree {
	node *root;
	unsigned char height;
};

void flush_buffer(void *buf, unsigned int len);
void flush();
void flush_range();
void add_logentry();
node *search_to_leftmost_leaf(node *level_ptr, unsigned char height);
//node *search_to_next_leaf(node *next_branch, unsigned char height);
node *search_to_prev_leaf(node *prev_branch, unsigned char height);
//node *find_next_leaf(tree *t, node *parent, unsigned int index, unsigned char height);
node *find_prev_leaf(node *parent, unsigned int index, unsigned char height);
node *allocINode(node *parent, unsigned int index);
node *allocLNode(node *parent, unsigned int index);
tree *initTree();
tree *CoW_Tree(node *changed_root, unsigned char height);
int increase_radix_tree_height(tree **t, unsigned char new_height);
int recursive_alloc_nodes(node *level_ptr, unsigned long key, void *value,
		unsigned char height);
int Insert(tree **t, unsigned long key, void *value);
void *Lookup(tree *t, unsigned long key);
void Range_Lookup(tree *t, unsigned long start_key, unsigned long end_key,
		unsigned long buf[]);
void Range_Lookup2(tree *t, unsigned long start_key, unsigned long num,
		unsigned long buf[]);
void Scan(tree *t, unsigned long num);
