//#define META_NODE_SHIFT	9

#define META_NODE_SHIFT 3

#define CACHE_LINE_SIZE 64

typedef struct Tree tree;
typedef struct Node node;
unsigned long elapsed_node_flush;
unsigned long elapsed_entry_flush;
unsigned long node_count;
unsigned long entry_count;
unsigned long level_descript_time;

struct Node {
	void *entry_ptr[8];
};

struct Tree {
	node *root[16];
};

void flush_buffer(void *buf, unsigned int len);
void flush();
void flush_range();
node *allocNode();
tree *initTree();
int recursive_alloc_nodes(node *level_ptr, unsigned long key, void *value,
		unsigned char height);
int Insert(tree *t, unsigned long key, void *value);
void *Lookup(tree *t, unsigned long key);
