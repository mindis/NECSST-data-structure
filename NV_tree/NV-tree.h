#include <stdbool.h>
#define CACHE_LINE_SIZE 	64
#define MAX_NUM_ENTRY_IN	509
#define MAX_NUM_ENTRY_PLN	255
#define MAX_NUM_ENTRY_LN	169	

#define LE_DATA		0
#define LE_COMMIT	1

typedef struct entry entry;
typedef struct Internal_Node IN;
typedef struct Parent_Leaf_Node PLN;
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

struct PLN_entry {
	unsigned long key;
	LN *ptr;
};

struct LN_entry {
	bool flag;
	unsigned long key;
	void *value;
};

struct Internal_Node {
	unsigned int nKeys;
	unsigned long key[509];
	char dummy[16];
};

struct Parent_Leaf_Node {
	unsigned int nKeys;
	LN *last_ptr;
	struct PLN_entry entries[255];
};

struct Leaf_Node {
	unsigned char nElements;
	LN *sibling;
	unsigned long parent_id;
	struct LN_entry LN_Element[169];
	char dummy[16];
};

struct tree{
	unsigned char height;
	unsigned long first_PLN_id;
	unsigned long last_PLN_id;
	IN *root;
	LN *first_leaf;
};

tree *initTree();
void Range_Lookup(tree *t, unsigned long start_key, unsigned int num, 
		unsigned long buf[]);
void *Lookup(tree *t, unsigned long key);
int Insert(tree *t, unsigned long key, void *value);
