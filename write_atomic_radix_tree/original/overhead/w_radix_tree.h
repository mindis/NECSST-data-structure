//#define META_NODE_SHIFT	9

#define META_NODE_SHIFT 5

#define CACHE_LINE_SIZE 64

typedef struct Tree tree;
typedef struct Node node;
typedef struct Logentry logentry;
unsigned long elapsed_node_flush;
unsigned long elapsed_entry_flush;
unsigned long elapsed_log_flush;
unsigned long elapsed_leaf_search;
unsigned long elapsed_entry_search;
unsigned long node_count;
unsigned long entry_count;
unsigned long log_count;

struct Logentry {
	unsigned long addr;
	unsigned long old_value;
	unsigned long new_value;
};

struct Node {
	void *entry_ptr[32];
	node *parent_ptr;
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
node *allocNode(node *parent, unsigned int index);
//node *allocNode();
tree *initTree();
int increase_radix_tree_height(tree *t, unsigned char new_height);
int recursive_alloc_nodes(node *level_ptr, unsigned long key, void *value,
		unsigned char height);
int Insert(tree *t, unsigned long key, void *value);
void *Lookup(tree *t, unsigned long key);
node *search_to_next_leaf(node *next_branch, unsigned char height);
node *find_next_leaf(tree *t, node *parent, unsigned int index,
		unsigned char height);
void Range_Lookup(tree *t, unsigned long start_key, unsigned long num,
		unsigned long buf[]);
