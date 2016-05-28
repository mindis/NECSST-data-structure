//#define NODE_SIZE 6
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

struct entry{
	long key;
	void *ptr;
};

struct node{
	char slot[NODE_SIZE+1];
	struct entry entries[NODE_SIZE];
	struct node *leftmostPtr;
	//long key[7];
	//void *ptr[8];
	struct node* parent;
	int isleaf;
	char dummy[52];
};

struct tree{
	node *root;
	int height;
};

tree *initTree();
void Range_Lookup(tree *t, long start_key, unsigned int num, 
		unsigned long buf[]);
void *Lookup(tree *t, long key);
int Append(node *n, long key, void *value);
int Append_in_inner(node *n, long key, void *value);
int Search(node *curr, char *temp, long key);
node *find_leaf_node(node *curr, long key);
void Insert(tree *t, long key, void *value);
int insert_in_leaf_noflush(node *curr, long key, void *value);
void insert_in_leaf(node *curr, long key, void *value);
void insert_in_inner(node *curr, long key, void *value);
void insert_in_parent(tree *t, node *curr, long key, node *splitNode);
void printNode(node *n);
