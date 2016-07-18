#include <stdbool.h>
#define NUM_LN_ENTRY 31
#define MIN_LIVE_ENTRIES NUM_LN_ENTRY / 2
#define CACHE_LINE_SIZE 64

#define LE_DATA		0
#define LE_COMMIT	1

#define BITS_PER_LONG	64
#define BITMAP_SIZE		NUM_LN_ENTRY

typedef struct entry entry;
typedef struct Internal_Node IN;
typedef struct Leaf_Node LN;
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

struct entry{
	unsigned long key;
	void *ptr;
};

struct Internal_Node {
	unsigned int nKeys;
	unsigned long keys[NUM_IN_ENTRY];
	void *leafmostptr;
	void *ptr[NUM_IN_ENTRY];
};

struct Leaf_Node {
	unsigned long bitmap;
	struct Leaf_Node *pNext;
	char fingerprints[NUM_LN_ENTRY];
	struct entry entries[NUM_LN_ENTRY];
};

struct tree {
	node *root;
	int height;
};

void flush_buffer(void *buf, unsigned int len, bool fence);
tree *initTree();
void Range_Lookup(tree *t, unsigned long start_key, unsigned int num, 
		unsigned long buf[]);
void *Lookup(tree *t, unsigned long key);
int Append(node *n, unsigned long key, void *value);
int Append_in_inner(node *n, unsigned long key, void *value);
int Search(node *curr, char *temp, unsigned long key);
node *find_leaf_node(node *curr, unsigned long key);
void Insert(tree *t, unsigned long key, void *value);
void *Update(tree *t, unsigned long key, void *value);
int insert_in_leaf_noflush(node *curr, unsigned long key, void *value);
void insert_in_leaf(node *curr, unsigned long key, void *value);
void insert_in_inner(node *curr, unsigned long key, void *value);
void insert_in_parent(tree *t, node *curr, unsigned long key, node *splitNode);
void printNode(node *n);
int Delete(tree *t, unsigned long key);

static inline unsigned long ffz(unsigned long word)
{
	asm("rep; bsf %1,%0"
		: "=r" (word)
		: "r" (~word));
	return word;
}
