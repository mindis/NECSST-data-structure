// o1 yjmoon

#include <geekos/ktypes.h>
#include <geekos/defs.h>
#include <geekos/symbol.h>
#include <geekos/sched-o1.h>
#include <geekos/mem.h>
#include <geekos/string.h>
#include <geekos/int.h>
#include <geekos/screen.h>
#include <geekos/kthread.h>
#include <geekos/malloc.h>

// find first bit offset in given word
inline ulong_t __ffs(ulong_t word)
{
	__asm__ __volatile__(
			"bsfl %1, %0"
			:"=r"(word)
			:"rm"(word));
	return word;
}

// set bit to 1
inline void __set_bit(int nr, volatile ulong_t *addr)
{
	__asm__ __volatile__(
			"btsl %1, %0"
			:"+m"(*(volatile long *)addr)
			:"lr"(nr));
}

// set bit to 0
inline void __clear_bit(int nr, volatile ulong_t *addr)
{
	__asm__ __volatile__(
			"btrl %1, %0"
			:"+m"(*(volatile long *)addr)
			:"lr"(nr));
}

// toggle bit
inline void __change_bit(int nr, volatile ulong_t *addr)
{
	__asm__ __volatile__(
			"btcl %1, %0"
			:"+m"(*(volatile long *)addr)
			:"lr"(nr));
}

// set bit to 1 in array
inline void __set_nbit(int nr, volatile ulong_t *addr)
{
	int arrayNum = nr / BITS_PER_LONG;
	int offset   = nr % BITS_PER_LONG;

	__asm__ __volatile__(
			"btsl %1, %0"
			:"+m"(*((volatile long *)addr + arrayNum))
			:"lr"(offset));
}

// set bit to 0 in array
inline void __clear_nbit(int nr, volatile ulong_t *addr)
{
	int arrayNum = nr / BITS_PER_LONG;
	int offset   = nr % BITS_PER_LONG;

	__asm__ __volatile__(
			"btrl %1, %0"
			:"+m"(*((volatile long *)addr + arrayNum))
			:"lr"(offset));
}

// initilize bitmap
inline void bitmap_zero(ulong_t *dst, int nbits)
{
	if(nbits <= BITS_PER_LONG)
		*dst = 0UL;
	else
	{
		int len = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
		memset(dst, 0, len);
	}
}

// find first bit in given bitmap
inline ulong_t find_first_bit(const ulong_t *b)
{
	if(__builtin_expect(!!(b[0]), 0))
		return __ffs(b[0]);
	if(__builtin_expect(!!(b[1]), 0))
		return __ffs(b[1]) + 32;
	if(__builtin_expect(!!(b[2]), 0))
		return __ffs(b[2]) + 64;
	if(b[3])
		return __ffs(b[3]) + 96;
	return __ffs(b[4]) + 128;
}

