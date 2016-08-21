#include <stdio.h>
#include <malloc.h>

typedef unsigned char uchar;
typedef unsigned int uint;
#define ABS(x) 				((x) < 0 ? -(x) : (x))
#define SIGN(x) 			((x) < 0 ? -1 : +1)
#define SHIFT_IN_BYTE 		0x00 // bit values to be shifted in the array
#define SHIFT_L(ptr,shift) 	(*(uchar*)(ptr) << (shift) | *((uchar*)(ptr) + 1) >> (8 - (shift)))
#define SHIFT_R(ptr,shift) 	(*(uchar*)(ptr) >> (shift) | *((uchar*)(ptr)-1) << (8-(shift)))
#define SHIFT(ptr,shift) 	((shift) >= 0 ? SHIFT_L(ptr,shift) : SHIFT_R(ptr,-(shift)))

// Bit array shift function
// Author: Nikolai Borissov
void bit_array_shift(void *dest_arr, void *src_arr, uint arr_size, int shift)
{ 
	// dest_arr - pointer to bit array to get a shifted result
	// src_arr - pointer to source bit array
	// arr_size - size of the above arrays in bytes
	// shift - bit shift value; shift>0 - left shift; shift<0 - right shift

	if(!dest_arr||!src_arr) 
		return; // do nothing for NULL pointers

	int step = 2 * (shift >= 0) - 1;
	int shift_bytes = shift / 8;
	int shift_bits = shift % 8;
	char *dest_beg = shift >= 0 ? (char*)dest_arr : (char*)dest_arr + arr_size - 1;
	char *src_beg = shift >= 0 ? (char*)src_arr : (char*)src_arr + arr_size - 1;
	char *src_end = (shift >= 0 ? (char*)src_arr + arr_size - 1 : (char*)src_arr) - shift_bytes;

	if((uint)ABS(shift_bytes) < arr_size) {
		static char last_byte[3] = {SHIFT_IN_BYTE, 0, SHIFT_IN_BYTE};
		while(src_beg!=src_end) { 
			*dest_beg = SHIFT(src_beg + shift_bytes, shift_bits);
			dest_beg += step;
			src_beg += step;
		};
		last_byte[1] = *(src_beg + shift_bytes);
		*dest_beg = SHIFT(last_byte + 1, shift_bits);
		dest_beg += step;
	}
	else
		shift_bytes = SIGN(shift_bytes) * arr_size;

	while(shift_bytes)
		dest_beg[shift_bytes -= step] = SHIFT_IN_BYTE;

	return;
};

int main(void)
{
	unsigned char *a = "abcde";
	unsigned long *b = malloc(sizeof(unsigned char) * 5);
	b = (unsigned long *)a;

	printf("%lx\n", a);
	printf("%lx\n",b[0]);
/*
	bit_array_shift(b, a, 5, -10);

	printf("%s\n", a);
	printf("%lx\n", b[0]);
	printf("%lx\n", b[1]);	
	printf("%lx\n", b[2]);
	printf("%lx\n", b[3]);
	printf("%lx\n", b[4]);

	printf("%d\n", b[0]);
	printf("%d\n", b[1]);	
	printf("%d\n", b[2]);
	printf("%d\n", b[3]);
	printf("%d\n", b[4]);
*/
	return 0;
}
