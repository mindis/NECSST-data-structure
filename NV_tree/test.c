#include <stdio.h>
#include <stdbool.h>

typedef struct Internal_Node IN;
typedef struct Parent_Leaf_Node PLN;
typedef struct Leaf_Node LN;

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
	PLN *parent;
	struct LN_entry LN_Element[169];
	char dummy[16];
};

int main(void)
{
	LN test;
	printf("sizeof(test.sibling) = %d\n", sizeof(test.sibling));
	printf("sizeof(*(test.sibling)) = %d\n", sizeof(*(test.sibling)));
	printf("sizeof(&test.sibling) = %d\n", sizeof(&test.sibling));

	return 0;
}
