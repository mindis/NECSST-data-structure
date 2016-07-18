#include <stdbool.h>

#define META_NODE_SHIFT 9
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
	void *entry_ptr[NUM_ENTRY];
	node *parent_ptr;
//	char dummy[16];		// 2
	char dummy[48];		// 3, 4, 5, 6, 7, 8, 9
};

struct Tree {
	unsigned char height;
	node *root;
};

void flush_buffer(void *buf, unsigned int len, bool fence);
tree *initTree();
int Insert(tree **t, unsigned long key, void *value);
void *Update(tree *t, unsigned long key, void *value);
void *Lookup(tree *t, unsigned long key);
void Range_Lookup(tree *t, unsigned long start_key, unsigned long num,
		unsigned long buf[]);
void Delete(tree *t, unsigned long key);
