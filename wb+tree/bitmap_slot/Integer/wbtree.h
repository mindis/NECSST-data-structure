#include <stdbool.h>
#define NODE_SIZE 			63
#define SLOT_SIZE 			NODE_SIZE + 1
#define MIN_LIVE_ENTRIES 	NODE_SIZE / 2
#define CACHE_LINE_SIZE 	64

#define LOG_DATA_SIZE		48
#define LOG_AREA_SIZE		4194304
#define LE_DATA			0
#define LE_COMMIT		1

#define BITS_PER_LONG	64
#define BITMAP_SIZE		NODE_SIZE + 1

typedef struct entry entry;
typedef struct node node;
typedef struct tree tree;

unsigned long node_count;
unsigned long clflush_count;
unsigned long mfence_count;

typedef struct {
	unsigned int size;
	unsigned char type;
	void *addr;
	char data[LOG_DATA_SIZE];
} log_entry;

typedef struct {
	log_entry *next_offset;
	char log_data[LOG_AREA_SIZE];
} log_area;

struct entry{
	unsigned long key;
	void *ptr;
};

struct node{
	char slot[SLOT_SIZE];
	unsigned long bitmap;
	struct entry entries[NODE_SIZE];
	struct node *leftmostPtr;
	struct node *parent;
	int isleaf;
//	char dummy[32];		//15
//	char dummy[16];		//31
	char dummy[48];		//63
};

struct tree{
	node *root;
	log_area *start_log;
};

void flush_buffer_nocount(void *buf, unsigned long len, bool fence);
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
