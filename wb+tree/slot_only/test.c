#include <stdio.h>
#include <malloc.h>

typedef struct {
//	void *addr;
	unsigned long size;
	unsigned char type;
	unsigned int asdf;
//	void *addr;
	char data[48];
} log_entry;

typedef struct {
	log_entry *next_offset;
	char log_data[4194304];
} log_area;

int main(void)
{
	int remain_size = (100 / 48) * 48;
	log_entry *log = malloc(sizeof(log_entry));

	printf("size = %d\n", log->data - (char *)log);
/*
	log_area *root = malloc(sizeof(log_area));
	root->next_offset = (log_entry *)root->log_data;
	printf("root->next_offset = %p\n", root->next_offset);
	printf("root->log_data = %p\n", root->log_data);
	root->next_offset = (char *)root->next_offset + 1;
	printf("root->next_offset = %p\n", root->next_offset);
	printf("root->log_data[1] = %p\n", &root->log_data[1]);
	*/
	return 0;
}
