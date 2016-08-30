#include <stdbool.h>
#include <limits.h>

#define CACHE_LINE_SIZE		64
#define MAX_NUM_ENTRY_IN	509
#define MAX_NUM_ENTRY_PLN	255
#define MAX_NUM_ENTRY_LN	169
#define MAX_KEY 			"~"

typedef struct entry entry;
typedef struct Internal_Node IN;
typedef struct Parent_Leaf_Node PLN;
typedef struct Leaf_Node LN;
typedef struct tree tree;

unsigned long IN_count;
unsigned long LN_count;
unsigned long clflush_count;
unsigned long mfence_count;

typedef struct {
	int key_len;
	unsigned char key[];
} key_item;

struct PLN_entry {
	key_item *key;
	LN *ptr;
};

struct LN_entry {
	bool flag;
	key_item *key;
	void *value;
};

struct Internal_Node {
	unsigned int nKeys;
	key_item *key[MAX_NUM_ENTRY_IN];
	char dummy[16];
};

struct Parent_Leaf_Node {
	unsigned int nKeys;
	struct PLN_entry entries[MAX_NUM_ENTRY_PLN];
	char dummy[8];
};

struct Leaf_Node {
	unsigned char nElements;
	LN *sibling;
	unsigned long parent_id;
	struct LN_entry LN_Element[MAX_NUM_ENTRY_LN];
	char dummy[16];
};

struct tree{
	unsigned char height;
	unsigned char is_leaf;	// 0.LN 1.PLN 2.IN
	unsigned long first_PLN_id;
	unsigned long last_PLN_id;
	void *root;
	LN *first_leaf;
};

tree *initTree();
void flush_buffer_nocount(void *buf, unsigned long len, bool fence);
int Insert(tree *t, unsigned char *key, int key_len, void *value);
int Update(tree *t, unsigned long key, void *value);
int Range_Lookup(tree *t, unsigned long start_key, unsigned int num, 
		unsigned long buf[]);
void *Lookup(tree *t, unsigned char *key, int key_len);
int Delete(tree *t, unsigned long key);
