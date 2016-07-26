#include <stdio.h>
#include <malloc.h>

struct Item {
	unsigned char type;
	unsigned long key;
	void *value;
	struct Item *next_ptr;
};

struct Node {
	unsigned char type;
	unsigned long p_index;
	void *entry_ptr[256];
	struct Node *parent_ptr;
	char dummy[48];
};

int main(void)
{
	int value = 20;
	void *entry;
	struct Item *item = malloc(sizeof(struct Item));
	struct Node *node = calloc(1, sizeof(struct Node));

	item->type = 1;
	item->key = 10;
	item->value = &value;
	item->next_ptr = NULL;

	entry = item;

	printf("item->type = %d\n", ((struct Node *)entry)->type);

	node->type = 2;

	entry = node;

	printf("node->type = %d\n", ((struct Node *)entry)->type);
	return 0;
}
