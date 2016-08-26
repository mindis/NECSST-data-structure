#include <stdio.h>
#include <limits.h>

typedef struct bit_array {
	unsigned char a : 4;
	unsigned char b : 4;
	unsigned char c : 4;
	unsigned char d : 4;
	unsigned char e : 4;
	unsigned char f : 4;
	unsigned char g : 4;
	unsigned char h : 4;
	unsigned char i : 4;
	unsigned char j : 4;
	unsigned char k : 4;
	unsigned char l : 4;
	unsigned char m : 4;
	unsigned char n : 4;
	unsigned char o : 4;
	unsigned char p : 4;
} bit_array;

int main(void) {
	unsigned long a = 999999999;
	unsigned char b = 1;

	a = b;

	printf("%lu\n", a);

	return 0;
}
