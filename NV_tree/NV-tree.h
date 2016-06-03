#include <stdbool.h>
#define NODE_SIZE 7
#define min_live_entries 3
#define CACHE_LINE_SIZE 64

#define LE_DATA		0
#define LE_COMMIT	1

typedef struct entry entry;
typedef struct node node;
typedef struct tree tree;
typedef struct redo_log_entry redo_log_entry;
typedef struct commit_entry commit_entry;

struct redo_log_entry {
	unsigned long addr;
	unsigned long new_value;
	unsigned char type;
};

struct commit_entry {
	unsigned char type;
};

struct PLN_entry {
	unsigned long key;
	Leaf_Node *ptr;
};

struct LN_entry {
	bool flag;
	unsigned long key;
	void *value;
}

struct Internal_Node {
	unsigned int nKeys;
	unsigned long key[5];
}

struct Parent_Leaf_Node {
	unsigned int nKeys;
	struct PLN_entry entries[3];
}

struct Leaf_Node {
	unsigned int nElements;
	struct LN_entry entries[10];
}

struct tree{
	Internal_Node *root;
	Leaf_Node *first_leaf;
	int height;
};

tree *initTree();
void Range_Lookup(tree *t, unsigned long start_key, unsigned int num, 
		unsigned long buf[]);
void *Lookup(tree *t, unsigned long key);
int Append(node *n, unsigned long key, void *value);
int Append_in_inner(node *n, unsigned long key, void *value);
int Search(node *curr, char *temp, unsigned long key);
node *find_leaf_node(node *curr, unsigned long key);
void Insert(tree *t, unsigned long key, void *value);
int insert_in_leaf_noflush(node *curr, unsigned long key, void *value);
void insert_in_leaf(node *curr, unsigned long key, void *value);
void insert_in_inner(node *curr, unsigned long key, void *value);
void insert_in_parent(tree *t, node *curr, unsigned long key, node *splitNode);
void printNode(node *n);
