/******************************************************************************/
/*   Cache-Sensitive B+-Tree code.                                            */
/*    Written by Jun Rao (junr@cs.columbia.edu)                               */
/*    Sep. 25, 1999                                                           */
/*                                                                            */
/*   This code is copyrighted by the Trustees of Columbia University in the   */
/*   City of New York.                                                        */
/*                                                                            */
/*   Permission to use and modify this code for noncommercial purposes is     */
/*   granted provided these headers are included unchanged.                   */
/******************************************************************************/

#include <iostream.h>
#include <fstream.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifndef PRODUCT
#define ASSERT(x) assert(x);
#else
#define ASSERT(x) ((void)0)
#endif /*PRODUCT*/
//#define RANGE 100
#define RANGE 10000000
#define MAX_KEY ((1<<31)-1)
#define NumKey(x) ((x)->d_entry[0].d_key)
#define IsLeaf(x) !(((BPLNODE64*)x)->d_flag)

//statistical information
struct Stat {
	int    d_NINode;         //number of internal nodes
	int    d_NLNode;         //number of leaf nodes
	int    d_NTotalSlots;    //number of total slots in all the nodes
	int    d_NUsedSlots;     //number of slots being used
	int    d_NSplits;        //number of splits
	int    d_level;          //number of levels of the tree
	int    d_leafSpace;      
	int    d_internalSpace;  // space used by internal nodes
	double d_segRatio;       //the relative size between segments, only used for GCSB+-Trees.
	Stat() {
		d_NINode=0;
		d_NLNode=0;
		d_NTotalSlots=0;
		d_NUsedSlots=0;
		d_NSplits=0;
		d_level=0;
		d_leafSpace=0;
		d_internalSpace=0;
		d_segRatio=0.0;
	}
};

/* one B+-Tree internal entry */
struct IPair {
	int d_key;
	void* d_child;
};

/* a B+-Tree internal node of 64 bytes.
   corresponds to a cache line size of 64 bytes.
   We can store a maximum of 7 keys and 8 child pointers in each node. */
struct BPINODE64 {
	IPair d_entry[8];
	/* d_entry[0].d_key is used to store the number of used keys in this node*/
};

/* one B+-Tree leaf entry */
struct LPair {
	int d_key;
	int d_tid;       /* tuple ID */
};

/* a B+-Tree internal node of 64 bytes.
   corresponds to a cache line size of 64 bytes.
   We can store a maximum of 7 keys and 8 child pointers in each node. */
struct BPLNODE64 {
	int        d_num;       /* number of keys in the node */
	void*      d_flag;      /* this pointer is always set to null and is used to distinguish
							   between and internal node and a leaf node */
	BPLNODE64* d_prev;      /* backward and forward pointers */
	BPLNODE64* d_next;
	LPair d_entry[6];       /* <key, TID> pairs */
};

/* a CSB+-Tree internal node of 64 bytes.
   corresponds to a cache line size of 64 bytes.
   We put all the child nodes of any given node continuously in a node group and
   store explicitly only the pointer to the first node in the node group.
   We can store a maximum of 14 keys in each node.
   Each node has a maximum of 15 implicit child nodes. 
   */
struct CSBINODE64 {
	int    d_num;
	void*  d_firstChild;       //pointer to the first child in a node group
	int    d_keyList[14];
	public:
	int operator == (const CSBINODE64& node) {
		if (d_num!=node.d_num)
			return 0;
		for (int i=0; i<d_num; i++)
			if (d_keyList[i]!=node.d_keyList[i])
				return 0;
		return 1;
	}
};

/* a segmented CSB+-Tree internal node of 64 bytes with 2 segments.
   corresponds to a cache line size of 64 bytes.
   We put all the child nodes of any given node continuously in two node groups and
   store explicitly only the pointer to the first node in each node group.
   We can store a maximum of 13 keys in each node.
   Each node has a maximum of 14 implicit child nodes.
   */
struct GCSBINODE64_2 {
	short int   d_num;              //total number of keys
	short int   d_1stNum;           //number of keys in the first segment.
	void*       d_firstChild;       //pointer to the first child in the first segment
	int         d_keyList[13];
	void*       d_2ndChild;         //pointer to the first child in the second segment
};

/* a segmented CSB+-Tree internal node of 64 bytes with 3 segments.
   corresponds to a cache line size of 64 bytes.
   We put all the child nodes of any given node continuously in three node groups and
   store explicitly only the pointer to the first node in each node group.
   We can store a maximum of 12 keys in each node.
   Each node has a maximum of 13 implicit child nodes.
   */
struct GCSBINODE64_3 {
	short int   d_num;              //total number of keys
	char        d_1stNum;           //number of keys in the first segment.
	char        d_2ndNum;           //number of keys in the first and second segment.
	void*       d_firstChild;       //pointer to the first child in the first segment
	int         d_keyList[12];
	void*       d_2ndChild;         //pointer to the first child in the second segment
	void*       d_3rdChild;         //pointer to the first child in the third segment
};

#define BPISearch64(root, key) \
	if (key<=root->d_entry[4].d_key) { \
		if (key<=root->d_entry[2].d_key) { \
			if (key<=root->d_entry[1].d_key) \
			h=0; \
			else \
			h=1; \
		} \
		else { \
			if (key<=root->d_entry[3].d_key) \
			h=2; \
			else \
			h=3;\
		} \
	} \
else { \
	if (key<=root->d_entry[6].d_key) { \
		if (key<=root->d_entry[5].d_key) \
		h=4; \
		else \
		h=5; \
	} \
	else { \
		if (key<=root->d_entry[7].d_key) \
		h=6; \
		else \
		h=7; \
	} \
} 

#define BPLSearch64(root, key) \
	if (key<=((BPLNODE64*)root)->d_entry[2].d_key) { \
		if (key<=((BPLNODE64*)root)->d_entry[0].d_key) \
		l=0; \
		else { \
			if (key<=((BPLNODE64*)root)->d_entry[1].d_key) \
			l=1; \
			else \
			l=2; \
		} \
	} \
else { \
	if (key<=((BPLNODE64*)root)->d_entry[4].d_key) { \
		if (key<=((BPLNODE64*)root)->d_entry[3].d_key) \
		l=3; \
		else \
		l=4; \
	} \
	else { \
		if (key<=((BPLNODE64*)root)->d_entry[5].d_key) \
		l=5; \
		else \
		l=6; \
	} \
}

#define CSBISearch64(root, key) \
	if (key<=root->d_keyList[6]) { \
		if (key<=root->d_keyList[2]) { \
			if (key<=root->d_keyList[0])\
			l=0;\
			else {\
				if (key<=root->d_keyList[1])\
				l=1;\
				else\
				l=2;\
			}\
		}\
		else {\
			if (key<=root->d_keyList[4]) {\
				if (key<=root->d_keyList[3])\
				l=3;\
				else\
				l=4;\
			}\
			else {\
				if (key<=root->d_keyList[5])\
				l=5;\
				else\
				l=6;\
			}\
		}\
	}\
else {\
	if (key<=root->d_keyList[10]) {\
		if (key<=root->d_keyList[8]) {\
			if (key<=root->d_keyList[7])\
			l=7;\
			else\
			l=8;\
		}\
		else {\
			if (key<=root->d_keyList[9])\
			l=9;\
			else\
			l=10;\
		}\
	}\
	else {\
		if (key<=root->d_keyList[12]) {\
			if (key<=root->d_keyList[11])\
			l=11;\
			else\
			l=12;\
		}\
		else {\
			if (key<=root->d_keyList[13])\
			l=13;\
			else\
			l=14;\
		}\
	}\
}

#define GCSBISearch64_2(root, key) \
	if (key<=root->d_keyList[5]) { \
		if (key<=root->d_keyList[1]) { \
			if (key<=root->d_keyList[0])\
			l=0;\
			else \
			l=1;\
		}\
		else {\
			if (key<=root->d_keyList[3]) {\
				if (key<=root->d_keyList[2])\
				l=2;\
				else\
				l=3;\
			}\
			else {\
				if (key<=root->d_keyList[4])\
				l=4;\
				else\
				l=5;\
			}\
		}\
	}\
else {\
	if (key<=root->d_keyList[9]) {\
		if (key<=root->d_keyList[7]) {\
			if (key<=root->d_keyList[6])\
			l=6;\
			else\
			l=7;\
		}\
		else {\
			if (key<=root->d_keyList[8])\
			l=8;\
			else\
			l=9;\
		}\
	}\
	else {\
		if (key<=root->d_keyList[11]) {\
			if (key<=root->d_keyList[10])\
			l=10;\
			else\
			l=11;\
		}\
		else {\
			if (key<=root->d_keyList[12])\
			l=12;\
			else\
			l=13;\
		}\
	}\
}

#define GCSBISearch64_3(root, key) \
	if (key<=root->d_keyList[4]) { \
		if (key<=root->d_keyList[1]) { \
			if (key<=root->d_keyList[0])\
			l=0;\
			else \
			l=1;\
		}\
		else {\
			if (key<=root->d_keyList[2])\
			l=2;\
			else if (key<=root->d_keyList[3])\
			l=3;\
			else\
			l=4;\
		}\
	}\
else {\
	if (key<=root->d_keyList[8]) {\
		if (key<=root->d_keyList[6]) {\
			if (key<=root->d_keyList[5])\
			l=5;\
			else\
			l=6;\
		}\
		else {\
			if (key<=root->d_keyList[7])\
			l=7;\
			else\
			l=8;\
		}\
	}\
	else {\
		if (key<=root->d_keyList[10]) {\
			if (key<=root->d_keyList[9])\
			l=9;\
			else\
			l=10;\
		}\
		else {\
			if (key<=root->d_keyList[11])\
			l=11;\
			else\
			l=12;\
		}\
	}\
}

/*
#define GCSBISearch64_2(root, key) \
if (key<=root->d_keyList[5]) { \
if (key<=root->d_keyList[1]) { \
if (key<=root->d_keyList[0])\
root=(GCSBINODE64_2*) root->d_firstChild+0;\
else \
root=(GCSBINODE64_2*) root->d_firstChild+1;\
}\
else {\
if (key<=root->d_keyList[3]) {\
if (key<=root->d_keyList[2])\
root=(GCSBINODE64_2*) root->d_firstChild+2;\
else\
root=(GCSBINODE64_2*) root->d_firstChild+3;\
}\
else {\
if (key<=root->d_keyList[4])\
root=(GCSBINODE64_2*) root->d_firstChild+4;\
else\
root=(GCSBINODE64_2*) root->d_firstChild+5;\
}\
}\
}\
else {\
if (key<=root->d_keyList[9]) {\
if (key<=root->d_keyList[7]) {\
if (key<=root->d_keyList[6])\
root=(GCSBINODE64_2*) root->d_firstChild+6;\
else\
root=(GCSBINODE64_2*) root->d_2ndChild+0;\
}\
else {\
if (key<=root->d_keyList[8])\
root=(GCSBINODE64_2*) root->d_2ndChild+1;\
else\
root=(GCSBINODE64_2*) root->d_2ndChild+2;\
}\
}\
else {\
if (key<=root->d_keyList[11]) {\
if (key<=root->d_keyList[10])\
root=(GCSBINODE64_2*) root->d_2ndChild+3;\
else\
root=(GCSBINODE64_2*) root->d_2ndChild+4;\
}\
else {\
if (key<=root->d_keyList[12])\
root=(GCSBINODE64_2*) root->d_2ndChild+5;\
else\
root=(GCSBINODE64_2*) root->d_2ndChild+6;\
}\
}\
}
*/

void bpBulkLoad64(int n, LPair* a, int iupper, int lupper);
//void bpBulkLoad64(int n, LPair* a, int upper);
BPINODE64* bpRightInsert64(BPINODE64* node, int key, void* child, int* new_key, void** new_child);
void bpInsert64(BPINODE64* root, LPair new_entry, int* new_key, void** new_child);
int bpDelete64(BPINODE64* root, LPair del_entry);
int bpSearch64(BPINODE64* root, int key);

void gcsbAdjust_2(CSBINODE64* root);
void gcsbAdjust_3(CSBINODE64* root);

#define GCCBPISEARCH64_VAR(root, key) \
l_bpISearch64_1:\
if (key<=root->d_entry[1].d_key)\
h=0;\
else h=1;\
goto exit;\
\
l_bpISearch64_2:\
if (key<=root->d_entry[1].d_key)\
h=0;\
else if (key<=root->d_entry[2].d_key)\
h=1;\
else h=2;\
goto exit;\
\
l_bpISearch64_3:\
if (key<=root->d_entry[2].d_key) {\
	if (key<=root->d_entry[1].d_key)\
	h=0;\
	else\
	h=1;\
}\
else {\
	if (key<=root->d_entry[3].d_key)\
	h=2;\
	else\
	h=3;\
}\
goto exit;\
\
l_bpISearch64_4:\
if (key<=root->d_entry[2].d_key) {\
	if (key<=root->d_entry[1].d_key)\
	h=0;\
	else\
	h=1;\
}\
else {\
	if (key<=root->d_entry[3].d_key)\
	h=2;\
	else if (key<=root->d_entry[4].d_key)\
	h=3;\
	else\
	h=4;\
}\
goto exit;\
\
l_bpISearch64_5:\
if (key<=root->d_entry[2].d_key) {\
	if (key<=root->d_entry[1].d_key)\
	h=0;\
	else\
	h=1;\
}\
else {\
	if (key<=root->d_entry[4].d_key) {\
		if (key<=root->d_entry[3].d_key)\
		h=2;\
		else\
		h=3;\
	}\
	else {\
		if (key<=root->d_entry[5].d_key)\
		h=4;\
		else\
		h=5;\
	}\
}\
goto exit;\
\
l_bpISearch64_6:\
if (key<=root->d_entry[3].d_key) {\
	if (key<=root->d_entry[1].d_key)\
	h=0;\
	else {\
		if (key<=root->d_entry[2].d_key)\
		h=1;\
		else\
		h=2;\
	}\
}\
else {\
	if (key<=root->d_entry[5].d_key) {\
		if (key<=root->d_entry[4].d_key)\
		h=3;\
		else\
		h=4;\
	}\
	else {\
		if (key<=root->d_entry[6].d_key)\
		h=5;\
		else\
		h=6;\
	}\
}\
goto exit;\
\
l_bpISearch64_7:\
if (key<=root->d_entry[4].d_key) {\
	if (key<=root->d_entry[2].d_key) {\
		if (key<=root->d_entry[1].d_key)\
		h=0;\
		else\
		h=1;\
	}\
	else {\
		if (key<=root->d_entry[3].d_key)\
		h=2;\
		else\
		h=3;\
	}\
}\
else {\
	if (key<=root->d_entry[6].d_key) {\
		if (key<=root->d_entry[5].d_key)\
		h=4;\
		else\
		h=5;\
	}\
	else {\
		if (key<=root->d_entry[7].d_key)\
		h=6;\
		else\
		h=7;\
	}\
}\
exit:\

int bpISearch64_1(BPINODE64* root, int key) {
	if (key<=root->d_entry[1].d_key)
		return 0;
	else return 1;
}

int bpISearch64_2(BPINODE64* root, int key) {
	if (key<=root->d_entry[1].d_key)
		return 0;
	else if (key<=root->d_entry[2].d_key)
		return 1;
	else return 2;
}

int bpISearch64_3(BPINODE64* root, int key) {
	if (key<=root->d_entry[2].d_key) {
		if (key<=root->d_entry[1].d_key)
			return 0;
		else
			return 1;
	}
	else {
		if (key<=root->d_entry[3].d_key)
			return 2;
		else
			return 3;
	}
}

int bpISearch64_4(BPINODE64* root, int key) {
	if (key<=root->d_entry[2].d_key) {
		if (key<=root->d_entry[1].d_key)
			return 0;
		else
			return 1;
	}
	else {
		if (key<=root->d_entry[3].d_key)
			return 2;
		else if (key<=root->d_entry[4].d_key)
			return 3;
		else
			return 4;
	}
}

int bpISearch64_5(BPINODE64* root, int key) {
	if (key<=root->d_entry[2].d_key) {
		if (key<=root->d_entry[1].d_key)
			return 0;
		else
			return 1;
	}
	else {
		if (key<=root->d_entry[4].d_key) {
			if (key<=root->d_entry[3].d_key)
				return 2;
			else
				return 3;
		}
		else {
			if (key<=root->d_entry[5].d_key)
				return 4;
			else
				return 5;
		}
	}
}

int bpISearch64_6(BPINODE64* root, int key) {
	if (key<=root->d_entry[3].d_key) {
		if (key<=root->d_entry[1].d_key)
			return 0;
		else {
			if (key<=root->d_entry[2].d_key)
				return 1;
			else
				return 2;
		}
	}
	else {
		if (key<=root->d_entry[5].d_key) {
			if (key<=root->d_entry[4].d_key)
				return 3;
			else
				return 4;
		}
		else {
			if (key<=root->d_entry[6].d_key)
				return 5;
			else
				return 6;
		}
	}
}

int bpISearch64_7(BPINODE64* root, int key) {
	if (key<=root->d_entry[4].d_key) {
		if (key<=root->d_entry[2].d_key) {
			if (key<=root->d_entry[1].d_key)
				return 0;
			else
				return 1;
		}
		else {
			if (key<=root->d_entry[3].d_key)
				return 2;
			else
				return 3;
		}
	}
	else {
		if (key<=root->d_entry[6].d_key) {
			if (key<=root->d_entry[5].d_key)
				return 4;
			else
				return 5;
		}
		else {
			if (key<=root->d_entry[7].d_key)
				return 6;
			else
				return 7;
		}
	}
}

#define GCCBPLSEARCH64_VAR(root, key) \
l_bpLSearch64_1: \
if (key<=root->d_entry[0].d_key)\
l=0;\
else l=1;\
goto exit1;\
\
l_bpLSearch64_2:\
if (key<=root->d_entry[0].d_key)\
l=0;\
else {\
	if (key<=root->d_entry[1].d_key)\
	l=1;\
	else\
	l=2;\
}\
goto exit1;\
\
l_bpLSearch64_3:\
if (key<=root->d_entry[1].d_key) {\
	if (key<=root->d_entry[0].d_key)\
	l=0;\
	else\
	l=1;\
}\
else {\
	if (key<=root->d_entry[2].d_key)\
	l=2;\
	else\
	l=3;\
}\
goto exit1;\
\
l_bpLSearch64_4:\
if (key<=root->d_entry[1].d_key) {\
	if (key<=root->d_entry[0].d_key)\
	l=0;\
	else\
	l=1;\
}\
else {\
	if (key<=root->d_entry[2].d_key)\
	l=2;\
	else {\
		if (key<=root->d_entry[3].d_key)\
		l=3;\
		else\
		l=4;\
	}\
}\
goto exit1;\
\
l_bpLSearch64_5:\
if (key<=root->d_entry[1].d_key) {\
	if (key<=root->d_entry[0].d_key)\
	l=0;\
	else\
	l=1;\
}\
else {\
	if (key<=root->d_entry[3].d_key) {\
		if (key<=root->d_entry[2].d_key)\
		l=2;\
		else\
		l=3;\
	}\
	else {\
		if (key<=root->d_entry[4].d_key)\
		l=4;\
		else\
		l=5;\
	}\
}\
goto exit1;\
\
l_bpLSearch64_6:\
if (key<=root->d_entry[2].d_key) {\
	if (key<=root->d_entry[0].d_key)\
	l=0;\
	else {\
		if (key<=root->d_entry[1].d_key)\
		l=1;\
		else\
		l=2;\
	}\
}\
else {\
	if (key<=root->d_entry[4].d_key) {\
		if (key<=root->d_entry[3].d_key)\
		l=3;\
		else\
		l=4;\
	}\
	else {\
		if (key<=root->d_entry[5].d_key)\
		l=5;\
		else\
		l=6;\
	}\
}\
exit1:

int bpLSearch64_1(BPLNODE64* root, int key) {
	if (key<=root->d_entry[0].d_key)
		return 0;
	else return 1;
}

int bpLSearch64_2(BPLNODE64* root, int key) {
	if (key<=root->d_entry[0].d_key)
		return 0;
	else {
		if (key<=root->d_entry[1].d_key)
			return 1;
		else
			return 2;
	}
}

int bpLSearch64_3(BPLNODE64* root, int key) {
	if (key<=root->d_entry[1].d_key) {
		if (key<=root->d_entry[0].d_key)
			return 0;
		else
			return 1;
	}
	else {
		if (key<=root->d_entry[2].d_key)
			return 2;
		else
			return 3;
	}
}

int bpLSearch64_4(BPLNODE64* root, int key) {
	if (key<=root->d_entry[1].d_key) {
		if (key<=root->d_entry[0].d_key)
			return 0;
		else
			return 1;
	}
	else {
		if (key<=root->d_entry[2].d_key)
			return 2;
		else {
			if (key<=root->d_entry[3].d_key)
				return 3;
			else
				return 4;
		}
	}
}

int bpLSearch64_5(BPLNODE64* root, int key) {
	if (key<=root->d_entry[1].d_key) {
		if (key<=root->d_entry[0].d_key)
			return 0;
		else
			return 1;
	}
	else {
		if (key<=root->d_entry[3].d_key) {
			if (key<=root->d_entry[2].d_key)
				return 2;
			else
				return 3;
		}
		else {
			if (key<=root->d_entry[4].d_key)
				return 4;
			else
				return 5;
		}
	}
}

int bpLSearch64_6(BPLNODE64* root, int key) {
	if (key<=root->d_entry[2].d_key) {
		if (key<=root->d_entry[0].d_key)
			return 0;
		else {
			if (key<=root->d_entry[1].d_key)
				return 1;
			else
				return 2;
		}
	}
	else {
		if (key<=root->d_entry[4].d_key) {
			if (key<=root->d_entry[3].d_key)
				return 3;
			else
				return 4;
		}
		else {
			if (key<=root->d_entry[5].d_key)
				return 5;
			else
				return 6;
		}
	}
}

#define GCCCSBISEARCH64_VAR(root, key) \
l_csbISearch64_1:\
if (key<=root->d_keyList[0])\
l=0;\
else l=1;\
goto exit2;\
\
l_csbISearch64_2:\
if (key<=root->d_keyList[0])\
l=0;\
else if (key<=root->d_keyList[1])\
l=1;\
else l=2;\
goto exit2;\
\
l_csbISearch64_3:\
if (key<=root->d_keyList[1]) {\
	if (key<=root->d_keyList[0])\
	l=0;\
	else\
	l=1;\
}\
else {\
	if (key<=root->d_keyList[2])\
	l=2;\
	else\
	l=3;\
}\
goto exit2;\
\
l_csbISearch64_4:\
if (key<=root->d_keyList[1]) {\
	if (key<=root->d_keyList[0])\
	l=0;\
	else\
	l=1;\
}\
else {\
	if (key<=root->d_keyList[2])\
	l=2;\
	else if (key<=root->d_keyList[3])\
	l=3;\
	else\
	l=4;\
}\
goto exit2;\
\
l_csbISearch64_5:\
if (key<=root->d_keyList[1]) {\
	if (key<=root->d_keyList[0])\
	l=0;\
	else\
	l=1;\
}\
else {\
	if (key<=root->d_keyList[3]) {\
		if (key<=root->d_keyList[2])\
		l=2;\
		else\
		l=3;\
	}\
	else {\
		if (key<=root->d_keyList[4])\
		l=4;\
		else\
		l=5;\
	}\
}\
goto exit2;\
\
l_csbISearch64_6:\
if (key<=root->d_keyList[2]) {\
	if (key<=root->d_keyList[0])\
	l=0;\
	else {\
		if (key<=root->d_keyList[1])\
		l=1;\
		else\
		l=2;\
	}\
}\
else {\
	if (key<=root->d_keyList[4]) {\
		if (key<=root->d_keyList[3])\
		l=3;\
		else\
		l=4;\
	}\
	else {\
		if (key<=root->d_keyList[5])\
		l=5;\
		else\
		l=6;\
	}\
}\
goto exit2;\
\
l_csbISearch64_7:\
if (key<=root->d_keyList[3]) {\
	if (key<=root->d_keyList[1]) {\
		if (key<=root->d_keyList[0])\
		l=0;\
		else\
		l=1;\
	}\
	else {\
		if (key<=root->d_keyList[2])\
		l=2;\
		else\
		l=3;\
	}\
}\
else {\
	if (key<=root->d_keyList[5]) {\
		if (key<=root->d_keyList[4])\
		l=4;\
		else\
		l=5;\
	}\
	else {\
		if (key<=root->d_keyList[6])\
		l=6;\
		else\
		l=7;\
	}\
}\
goto exit2;\
\
l_csbISearch64_8:\
if (key<=root->d_keyList[3]) {\
	if (key<=root->d_keyList[1]) {\
		if (key<=root->d_keyList[0])\
		l=0;\
		else\
		l=1;\
	}\
	else {\
		if (key<=root->d_keyList[2])\
		l=2;\
		else\
		l=3;\
	}\
}\
else {\
	if (key<=root->d_keyList[5]) {\
		if (key<=root->d_keyList[4])\
		l=4;\
		else\
		l=5;\
	}\
	else {\
		if (key<=root->d_keyList[6])\
		l=6;\
		else {\
			if (key<=root->d_keyList[7])\
			l=7;\
			else\
			l=8;\
		}\
	}\
}\
goto exit2;\
\
l_csbISearch64_9:\
if (key<=root->d_keyList[3]) {\
	if (key<=root->d_keyList[1]) {\
		if (key<=root->d_keyList[0])\
		l=0;\
		else\
		l=1;\
	}\
	else {\
		if (key<=root->d_keyList[2])\
		l=2;\
		else\
		l=3;\
	}\
}\
else {\
	if (key<=root->d_keyList[5]) {\
		if (key<=root->d_keyList[4])\
		l=4;\
		else\
		l=5;\
	}\
	else {\
		if (key<=root->d_keyList[7]) {\
			if (key<=root->d_keyList[6])\
			l=6;\
			else\
			l=7;\
		}\
		else {\
			if (key<=root->d_keyList[8])\
			l=8;\
			else\
			l=9;\
		}\
	}\
}\
goto exit2;\
\
l_csbISearch64_10:\
if (key<=root->d_keyList[3]) {\
	if (key<=root->d_keyList[1]) {\
		if (key<=root->d_keyList[0])\
		l=0;\
		else\
		l=1;\
	}\
	else {\
		if (key<=root->d_keyList[2])\
		l=2;\
		else\
		l=3;\
	}\
}\
else {\
	if (key<=root->d_keyList[6]) {\
		if (key<=root->d_keyList[4])\
		l=4;\
		else {\
			if (key<=root->d_keyList[5])\
			l=5;\
			else\
			l=6;\
		}\
	}\
	else {\
		if (key<=root->d_keyList[8]) {\
			if (key<=root->d_keyList[7])\
			l=7;\
			else\
			l=8;\
		}\
		else {\
			if (key<=root->d_keyList[9])\
			l=9;\
			else\
			l=10;\
		}\
	}\
}\
goto exit2;\
\
l_csbISearch64_11:\
if (key<=root->d_keyList[3]) {\
	if (key<=root->d_keyList[1]) {\
		if (key<=root->d_keyList[0])\
		l=0;\
		else\
		l=1;\
	}\
	else {\
		if (key<=root->d_keyList[2])\
		l=2;\
		else\
		l=3;\
	}\
}\
else {\
	if (key<=root->d_keyList[7]) {\
		if (key<=root->d_keyList[5]) {\
			if (key<=root->d_keyList[4])\
			l=4;\
			else\
			l=5;\
		}\
		else {\
			if (key<=root->d_keyList[6])\
			l=6;\
			else\
			l=7;\
		}\
	}\
	else {\
		if (key<=root->d_keyList[9]) {\
			if (key<=root->d_keyList[8])\
			l=8;\
			else\
			l=9;\
		}\
		else {\
			if (key<=root->d_keyList[10])\
			l=10;\
			else\
			l=11;\
		}\
	}\
}\
goto exit2;\
\
l_csbISearch64_12:\
if (key<=root->d_keyList[4]) {\
	if (key<=root->d_keyList[1]) {\
		if (key<=root->d_keyList[0])\
		l=0;\
		else\
		l=1;\
	}\
	else {\
		if (key<=root->d_keyList[2])\
		l=2;\
		else {\
			if (key<=root->d_keyList[3])\
			l=3;\
			else\
			l=4;\
		}\
	}\
}\
else {\
	if (key<=root->d_keyList[8]) {\
		if (key<=root->d_keyList[6]) {\
			if (key<=root->d_keyList[5])\
			l=5;\
			else\
			l=6;\
		}\
		else {\
			if (key<=root->d_keyList[7])\
			l=7;\
			else\
			l=8;\
		}\
	}\
	else {\
		if (key<=root->d_keyList[10]) {\
			if (key<=root->d_keyList[9])\
			l=9;\
			else\
			l=10;\
		}\
		else {\
			if (key<=root->d_keyList[11])\
			l=11;\
			else\
			l=12;\
		}\
	}\
}\
goto exit2;\
\
l_csbISearch64_13:\
if (key<=root->d_keyList[5]) {\
	if (key<=root->d_keyList[1]) {\
		if (key<=root->d_keyList[0])\
		l=0;\
		else\
		l=1;\
	}\
	else {\
		if (key<=root->d_keyList[3])  {\
			if (key<=root->d_keyList[2])\
			l=2;\
			else\
			l=3;\
		}\
		else {\
			if (key<=root->d_keyList[4])\
			l=4;\
			else\
			l=5;\
		}\
	}\
}\
else {\
	if (key<=root->d_keyList[9]) {\
		if (key<=root->d_keyList[7]) {\
			if (key<=root->d_keyList[6])\
			l=6;\
			else\
			l=7;\
		}\
		else {\
			if (key<=root->d_keyList[8])\
			l=8;\
			else\
			l=9;\
		}\
	}\
	else {\
		if (key<=root->d_keyList[11]) {\
			if (key<=root->d_keyList[10])\
			l=10;\
			else\
			l=11;\
		}\
		else {\
			if (key<=root->d_keyList[12])\
			l=12;\
			else\
			l=13;\
		}\
	}\
}\
goto exit2;\
\
l_csbISearch64_14:\
if (key<=root->d_keyList[6]) {\
	if (key<=root->d_keyList[2]) {\
		if (key<=root->d_keyList[0])\
		l=0;\
		else {\
			if (key<=root->d_keyList[1])\
			l=1;\
			else\
			l=2;\
		}\
	}\
	else {\
		if (key<=root->d_keyList[4])  {\
			if (key<=root->d_keyList[3])\
			l=3;\
			else\
			l=4;\
		}\
		else {\
			if (key<=root->d_keyList[5])\
			l=5;\
			else\
			l=6;\
		}\
	}\
}\
else {\
	if (key<=root->d_keyList[10]) {\
		if (key<=root->d_keyList[8]) {\
			if (key<=root->d_keyList[7])\
			l=7;\
			else\
			l=8;\
		}\
		else {\
			if (key<=root->d_keyList[9])\
			l=9;\
			else\
			l=10;\
		}\
	}\
	else {\
		if (key<=root->d_keyList[12]) {\
			if (key<=root->d_keyList[11])\
			l=11;\
			else\
			l=12;\
		}\
		else {\
			if (key<=root->d_keyList[13])\
			l=13;\
			else\
			l=14;\
		}\
	}\
}\
exit2:

int csbISearch64_1(CSBINODE64* root, int key) {
	if (key<=root->d_keyList[0])
		return 0;
	else return 1;
}

int csbISearch64_2(CSBINODE64* root, int key) {
	if (key<=root->d_keyList[0])
		return 0;
	else if (key<=root->d_keyList[1])
		return 1;
	else return 2;
}

int csbISearch64_3(CSBINODE64* root, int key) {
	if (key<=root->d_keyList[1]) {
		if (key<=root->d_keyList[0])
			return 0;
		else
			return 1;
	}
	else {
		if (key<=root->d_keyList[2])
			return 2;
		else
			return 3;
	}
}

int csbISearch64_4(CSBINODE64* root, int key) {
	if (key<=root->d_keyList[1]) {
		if (key<=root->d_keyList[0])
			return 0;
		else
			return 1;
	}
	else {
		if (key<=root->d_keyList[2])
			return 2;
		else if (key<=root->d_keyList[3])
			return 3;
		else
			return 4;
	}
}

int csbISearch64_5(CSBINODE64* root, int key) {
	if (key<=root->d_keyList[1]) {
		if (key<=root->d_keyList[0])
			return 0;
		else
			return 1;
	}
	else {
		if (key<=root->d_keyList[3]) {
			if (key<=root->d_keyList[2])
				return 2;
			else
				return 3;
		}
		else {
			if (key<=root->d_keyList[4])
				return 4;
			else
				return 5;
		}
	}
}

int csbISearch64_6(CSBINODE64* root, int key) {
	if (key<=root->d_keyList[2]) {
		if (key<=root->d_keyList[0])
			return 0;
		else {
			if (key<=root->d_keyList[1])
				return 1;
			else
				return 2;
		}
	}
	else {
		if (key<=root->d_keyList[4]) {
			if (key<=root->d_keyList[3])
				return 3;
			else
				return 4;
		}
		else {
			if (key<=root->d_keyList[5])
				return 5;
			else
				return 6;
		}
	}
}

int csbISearch64_7(CSBINODE64* root, int key) {
	if (key<=root->d_keyList[3]) {
		if (key<=root->d_keyList[1]) {
			if (key<=root->d_keyList[0])
				return 0;
			else
				return 1;
		}
		else {
			if (key<=root->d_keyList[2])
				return 2;
			else
				return 3;
		}
	}
	else {
		if (key<=root->d_keyList[5]) {
			if (key<=root->d_keyList[4])
				return 4;
			else
				return 5;
		}
		else {
			if (key<=root->d_keyList[6])
				return 6;
			else
				return 7;
		}
	}
}

int csbISearch64_8(CSBINODE64* root, int key) {
	if (key<=root->d_keyList[3]) {
		if (key<=root->d_keyList[1]) {
			if (key<=root->d_keyList[0])
				return 0;
			else
				return 1;
		}
		else {
			if (key<=root->d_keyList[2])
				return 2;
			else
				return 3;
		}
	}
	else {
		if (key<=root->d_keyList[5]) {
			if (key<=root->d_keyList[4])
				return 4;
			else
				return 5;
		}
		else {
			if (key<=root->d_keyList[6])
				return 6;
			else {
				if (key<=root->d_keyList[7])
					return 7;
				else
					return 8;
			}
		}
	}
}

int csbISearch64_9(CSBINODE64* root, int key) {
	if (key<=root->d_keyList[3]) {
		if (key<=root->d_keyList[1]) {
			if (key<=root->d_keyList[0])
				return 0;
			else
				return 1;
		}
		else {
			if (key<=root->d_keyList[2])
				return 2;
			else
				return 3;
		}
	}
	else {
		if (key<=root->d_keyList[5]) {
			if (key<=root->d_keyList[4])
				return 4;
			else
				return 5;
		}
		else {
			if (key<=root->d_keyList[7]) {
				if (key<=root->d_keyList[6])
					return 6;
				else
					return 7;
			}
			else {
				if (key<=root->d_keyList[8])
					return 8;
				else
					return 9;
			}
		}
	}
}

int csbISearch64_10(CSBINODE64* root, int key) {
	if (key<=root->d_keyList[3]) {
		if (key<=root->d_keyList[1]) {
			if (key<=root->d_keyList[0])
				return 0;
			else
				return 1;
		}
		else {
			if (key<=root->d_keyList[2])
				return 2;
			else
				return 3;
		}
	}
	else {
		if (key<=root->d_keyList[6]) {
			if (key<=root->d_keyList[4])
				return 4;
			else {
				if (key<=root->d_keyList[5])
					return 5;
				else
					return 6;
			}
		}
		else {
			if (key<=root->d_keyList[8]) {
				if (key<=root->d_keyList[7])
					return 7;
				else
					return 8;
			}
			else {
				if (key<=root->d_keyList[9])
					return 9;
				else
					return 10;
			}
		}
	}
}

int csbISearch64_11(CSBINODE64* root, int key) {
	if (key<=root->d_keyList[3]) {
		if (key<=root->d_keyList[1]) {
			if (key<=root->d_keyList[0])
				return 0;
			else
				return 1;
		}
		else {
			if (key<=root->d_keyList[2])
				return 2;
			else
				return 3;
		}
	}
	else {
		if (key<=root->d_keyList[7]) {
			if (key<=root->d_keyList[5]) {
				if (key<=root->d_keyList[4])
					return 4;
				else
					return 5;
			}
			else {
				if (key<=root->d_keyList[6])
					return 6;
				else
					return 7;
			}
		}
		else {
			if (key<=root->d_keyList[9]) {
				if (key<=root->d_keyList[8])
					return 8;
				else
					return 9;
			}
			else {
				if (key<=root->d_keyList[10])
					return 10;
				else
					return 11;
			}
		}
	}
}

int csbISearch64_12(CSBINODE64* root, int key) {
	if (key<=root->d_keyList[4]) {
		if (key<=root->d_keyList[1]) {
			if (key<=root->d_keyList[0])
				return 0;
			else
				return 1;
		}
		else {
			if (key<=root->d_keyList[2])
				return 2;
			else {
				if (key<=root->d_keyList[3])
					return 3;
				else
					return 4;
			}
		}
	}
	else {
		if (key<=root->d_keyList[8]) {
			if (key<=root->d_keyList[6]) {
				if (key<=root->d_keyList[5])
					return 5;
				else
					return 6;
			}
			else {
				if (key<=root->d_keyList[7])
					return 7;
				else
					return 8;
			}
		}
		else {
			if (key<=root->d_keyList[10]) {
				if (key<=root->d_keyList[9])
					return 9;
				else
					return 10;
			}
			else {
				if (key<=root->d_keyList[11])
					return 11;
				else
					return 12;
			}
		}
	}
}

int csbISearch64_13(CSBINODE64* root, int key) {
	if (key<=root->d_keyList[5]) {
		if (key<=root->d_keyList[1]) {
			if (key<=root->d_keyList[0])
				return 0;
			else
				return 1;
		}
		else {
			if (key<=root->d_keyList[3])  {
				if (key<=root->d_keyList[2])
					return 2;
				else
					return 3;
			}
			else {
				if (key<=root->d_keyList[4])
					return 4;
				else
					return 5;
			}
		}
	}
	else {
		if (key<=root->d_keyList[9]) {
			if (key<=root->d_keyList[7]) {
				if (key<=root->d_keyList[6])
					return 6;
				else
					return 7;
			}
			else {
				if (key<=root->d_keyList[8])
					return 8;
				else
					return 9;
			}
		}
		else {
			if (key<=root->d_keyList[11]) {
				if (key<=root->d_keyList[10])
					return 10;
				else
					return 11;
			}
			else {
				if (key<=root->d_keyList[12])
					return 12;
				else
					return 13;
			}
		}
	}
}

int csbISearch64_14(CSBINODE64* root, int key) {
	if (key<=root->d_keyList[6]) {
		if (key<=root->d_keyList[2]) {
			if (key<=root->d_keyList[0])
				return 0;
			else {
				if (key<=root->d_keyList[1])
					return 1;
				else
					return 2;
			}
		}
		else {
			if (key<=root->d_keyList[4])  {
				if (key<=root->d_keyList[3])
					return 3;
				else
					return 4;
			}
			else {
				if (key<=root->d_keyList[5])
					return 5;
				else
					return 6;
			}
		}
	}
	else {
		if (key<=root->d_keyList[10]) {
			if (key<=root->d_keyList[8]) {
				if (key<=root->d_keyList[7])
					return 7;
				else
					return 8;
			}
			else {
				if (key<=root->d_keyList[9])
					return 9;
				else
					return 10;
			}
		}
		else {
			if (key<=root->d_keyList[12]) {
				if (key<=root->d_keyList[11])
					return 11;
				else
					return 12;
			}
			else {
				if (key<=root->d_keyList[13])
					return 13;
				else
					return 14;
			}
		}
	}
}

#define LEAF64(p, key) \
	if (key<=(p+3)->d_key) { \
		if (key<=(p+1)->d_key) { \
			if (key<=(p+0)->d_key) \
			l=0; \
			else \
			l=1; \
		} \
		else {\
			if (key<=(p+2)->d_key) \
			l=2; \
			else \
			l=3; \
		} \
	} \
else {\
	if (key<=(p+5)->d_key) { \
		if (key<=(p+4)->d_key) \
		l=4; \
		else \
		l=5; \
	} \
	else {\
		if (key<=(p+6)->d_key) \
		l=6; \
		else if (key<=(p+7)->d_key) \
		l=7; \
		else \
		l=8; \
	} \
}

//Global variables
void*          g_free=0;
int            g_expand=0;
int            g_split=0;
Stat           g_stat_rec;
int            g_space_used=0;
BPINODE64*     g_bp_root64;
CSBINODE64*    g_csb_root64;
GCSBINODE64_2* g_gcsb_root64_2;
GCSBINODE64_3* g_gcsb_root64_3;
char*          g_pool_start;
char*          g_pool_curr;
char*          g_pool_end;
int (*g_bpIList[])(BPINODE64*, int)= { 0, 
	bpISearch64_1,
	bpISearch64_2,
	bpISearch64_3,
	bpISearch64_4,
	bpISearch64_5,
	bpISearch64_6,
	bpISearch64_7 };

int (*g_bpLList[])(BPLNODE64*, int)= { 0, 
	bpLSearch64_1,
	bpLSearch64_2,
	bpLSearch64_3,
	bpLSearch64_4,
	bpLSearch64_5,
	bpLSearch64_6 };

int (*g_csbIList[])(CSBINODE64*, int)= { 0, 
	csbISearch64_1,
	csbISearch64_2,
	csbISearch64_3,
	csbISearch64_4,
	csbISearch64_5,
	csbISearch64_6,
	csbISearch64_7,
	csbISearch64_8,
	csbISearch64_9,
	csbISearch64_10,
	csbISearch64_11,
	csbISearch64_12,
	csbISearch64_13,
	csbISearch64_14};

inline int pairComp(const void* t1, const void* t2) {
	return ((LPair*)t1)->d_key-((LPair*)t2)->d_key;
}

inline void* mynew(int size) {
	void* p=g_pool_curr;

	g_pool_curr=g_pool_curr+size;
	ASSERT(g_pool_curr<g_pool_end);
#ifndef PRODUCT
	g_space_used+=size;
	assert(!(((int)p)&0x3f));    // aligned bu 64 bytes
#endif
	return p;
}

inline void mydelete(void* p, int size) {
	if (size) {
		ASSERT(size>=64);
		*(int*)p=size;
		*((int*)p+4)=(int)g_free;
		g_free=p;
#ifndef PRODUCT
		g_space_used-=size;
#endif
	}
}

int validPointer(void* p) {
	if (p>=g_pool_start && p<g_pool_end)
		return 1;
	else
		return 0;
}

void init_memory(int size) {
	char* a1;

	//align by 16 integers (64 bytes)
	a1=new char[size+64];
	a1=(char*)((int)a1&0xffffffc0);
	a1+=64;

	assert(!(((int)a1)&0x3f));
	g_pool_curr=a1;
	g_pool_start=a1;
	g_pool_end=a1+size;
}

//make sure the pool is aligned by 64 bytes
void syncMemory() {
	g_pool_curr=(char*)((int)g_pool_curr&0xffffffc0);
	g_pool_curr+=64;

	assert(!(((int)g_pool_curr)&0x3f));
	assert(g_pool_curr<g_pool_end);
}

//make sure the pool is aligned by 16K bytes
void sync16kMemory() {
	g_pool_curr=(char*)g_pool_curr+2047;
	g_pool_curr=(char*)((int)g_pool_curr&0xfffffb00);

	assert(g_pool_curr<g_pool_end);
}

// bulk load a B+-Tree 
// n: size of the sorted array
// a: sorted leaf array
// iUpper: maximum number of keys for each internal node duing bulkload.
// lUpper: maximum number of keys for each leaf node duing bulkload.
// Note: iUpper has to be less than 7.
void bpBulkLoad64(int n, LPair* a, int iUpper, int lUpper) {
	BPLNODE64 *lcurr, *start, *lprev;
	BPINODE64 *iLow, *iHigh, *iHighStart, *iLowStart; 
	int temp_key;
	void* temp_child;
	int i, j, nLeaf, nHigh, nLow, remainder;

	// first step, populate all the leaf nodes 
	nLeaf=(n+lUpper-1)/lUpper;
	lcurr=(BPLNODE64*) mynew(sizeof(BPLNODE64)*nLeaf);
	lcurr->d_flag=0;
	lcurr->d_num=0;
	lcurr->d_prev=0;
	start=lcurr;

	for (i=0; i<n; i++) {
		if (lcurr->d_num >= lUpper) { // at the beginning of a new node
#ifdef FIX_HARDCODE
			// fill the empty slots with MAX_KEY
			for (j=lcurr->d_num; j<6; j++)
				lcurr->d_entry[j].d_key=MAX_KEY;
#endif
			lprev=lcurr;
			lcurr++;
			lcurr->d_flag=0;
			lcurr->d_num=0;
			lcurr->d_prev=lprev;
			ASSERT(lprev);
			lprev->d_next=lcurr;
		}
		lcurr->d_entry[lcurr->d_num]=a[i];
		lcurr->d_num++;
	}
	lcurr->d_next=0;
#ifdef FIX_HARDCODE
	// fill the empty slots with MAX_KEY
	for (j=lcurr->d_num; j<6; j++)
		lcurr->d_entry[j].d_key=MAX_KEY;
#endif

	// second step, build the internal nodes, level by level.
	// we can put IUpper keys and IUpper+1 children (implicit) per node
	nHigh=(nLeaf+iUpper)/(iUpper+1);
	remainder=nLeaf%(iUpper+1);
	iHigh=(BPINODE64*) mynew(sizeof(BPINODE64)*nHigh);
	iHighStart=iHigh;
	lcurr=start;
	for (i=0; i<((remainder==0)?nHigh:(nHigh-1)); i++) {
		NumKey(iHigh)=iUpper;
		for (j=0; j<iUpper+1; j++) {
			iHigh->d_entry[j].d_child=lcurr;
			iHigh->d_entry[j+1].d_key=lcurr->d_entry[lcurr->d_num-1].d_key;
			lcurr++;
		}
#ifdef FIX_HARDCODE
		for (j=iUpper+2; j<7+1; j++)
			iHigh->d_entry[j].d_key=MAX_KEY;
#endif    
		iHigh++;
	}
	if (remainder==1) {
		//this is a special case, we have to borrow a key from the left node if there is one
		//leaf node remaining.
		iHigh->d_entry[1].d_key=(iHigh-1)->d_entry[iUpper+1].d_key;
		NumKey(iHigh-1)--;
		NumKey(iHigh)=1;
		iHigh->d_entry[0].d_child=lcurr-1;
		iHigh->d_entry[1].d_child=lcurr;
		lcurr++;
#ifdef FIX_HARDCODE
		for (j=2; j<7+1; j++)
			iHigh->d_entry[j].d_key=MAX_KEY;
		iHigh++;
#endif    
	}
	else if (remainder>1) {
		for (i=0; i<remainder; i++) {
			iHigh->d_entry[i].d_child=lcurr;
			iHigh->d_entry[i+1].d_key=lcurr->d_entry[lcurr->d_num-1].d_key;
			lcurr++;
		}
		NumKey(iHigh)=remainder-1;
#ifdef FIX_HARDCODE
		for (j=remainder+1; j<7+1; j++)
			iHigh->d_entry[j].d_key=MAX_KEY;
		iHigh++;
#endif    
	}
#ifdef FIX_HARDCODE
	(iHigh-1)->d_entry[NumKey(iHigh-1)+1].d_key=MAX_KEY;
#endif
	ASSERT((lcurr-nLeaf) == start);

	while (nHigh>1) {
		nLow=nHigh;
		iLow=iHighStart;
		iLowStart=iLow;
		nHigh=(nLow+iUpper)/(iUpper+1);
		remainder=nLow%(iUpper+1);
		iHigh=(BPINODE64*) mynew(sizeof(BPINODE64)*nHigh);
		iHighStart=iHigh;
		for (i=0; i<((remainder==0)?nHigh:(nHigh-1)); i++) {
			NumKey(iHigh)=iUpper;
			for (j=0; j<iUpper+1; j++) {
				ASSERT(NumKey(iLow)<7);
				iHigh->d_entry[j].d_child=iLow;
				iHigh->d_entry[j+1].d_key=iLow->d_entry[NumKey(iLow)+1].d_key;
				iLow++;
			}
#ifdef FIX_HARDCODE
			for (j=iUpper+2; j<7+1; j++)
				iHigh->d_entry[j].d_key=MAX_KEY;
#endif    
			iHigh++;
		}
		if (remainder==1) { //this is a special case, we have to borrow a key from the left node
			iHigh->d_entry[1].d_key=(iHigh-1)->d_entry[iUpper+1].d_key;
			NumKey(iHigh-1)--;
			NumKey(iHigh)=1;
			iHigh->d_entry[0].d_child=iLow-1;
			iHigh->d_entry[1].d_child=iLow;
			iLow++;
#ifdef FIX_HARDCODE
			for (j=2; j<7+1; j++)
				iHigh->d_entry[j].d_key=MAX_KEY;
			iHigh++;
#endif    
		}
		else if (remainder>1) {
			for (i=0; i<remainder; i++) {
				ASSERT(NumKey(iLow)<7);
				iHigh->d_entry[i].d_child=iLow;
				iHigh->d_entry[i+1].d_key=iLow->d_entry[NumKey(iLow)+1].d_key;
				iLow++;
			}
			NumKey(iHigh)=remainder-1;
#ifdef FIX_HARDCODE
			for (j=remainder+1; j<7+1; j++)
				iHigh->d_entry[j].d_key=MAX_KEY;
			iHigh++;
#endif    
		}
#ifdef FIX_HARDCODE
		(iHigh-1)->d_entry[NumKey(iHigh-1)+1].d_key=MAX_KEY;
#endif
		ASSERT((iLow-nLow) == iLowStart);
	}

	g_bp_root64=iHighStart;
}

/* comment out
   alternative implementation from the textbook [Ramakrishnan] 1997.
// bulk load a B+-Tree 
// n: size of the sorted array
//   a: sorted integer array
//   proot: address of the B+-Tree root point
void bpBulkLoad64(int n, LPair* a, int upper) {
BPLNODE64 *lcurr, *start, *lprev;
BPINODE64 *icurr; 
int temp_key;
void* temp_child;
int i;

// first step, populate all the leaf nodes 

lcurr=(BPLNODE64*) mynew(sizeof(BPLNODE64));
lcurr->d_flag=0;
lcurr->d_num=0;
lcurr->d_prev=0;
start=lcurr;

for (i=0; i<n; i++) {
if (lcurr->d_num >= upper) { // at the beginning of a new node 
lprev=lcurr;
lcurr=(BPLNODE64*) mynew(sizeof(BPLNODE64));
lcurr->d_flag=0;
lcurr->d_num=0;
lcurr->d_prev=lprev;
ASSERT(lprev);
lprev->d_next=lcurr;
}
lcurr->d_entry[lcurr->d_num]=a[i];
lcurr->d_num++;
}
lcurr->d_next=0;

// second step, build the internal nodes 
g_bp_root64=(BPINODE64*) mynew(sizeof(BPINODE64));
g_bp_root64->d_entry[0].d_child=start;
NumKey(g_bp_root64)=0;
icurr=g_bp_root64;
lprev=start;
lcurr=start->d_next;

while (lcurr) {
if (NumKey(icurr)<7) {
// copy the largest key in the previous node 
NumKey(icurr)++;
icurr->d_entry[NumKey(icurr)].d_key=lprev->d_entry[lprev->d_num-1].d_key;
icurr->d_entry[NumKey(icurr)].d_child=lcurr;
}
else // we have to split an internal node 
icurr=bpRightInsert64(g_bp_root64, lprev->d_entry[lprev->d_num-1].d_key, lcurr, &temp_key, &temp_child);
lprev=lcurr;
lcurr=lcurr->d_next;
}
}

// rightInsert:
// insert a new entry in the right most branch of a B+-Tree. A split must occur.
// node: current node being processed.
// key, child: the <key, child> entry to be inserted at the rightmost internal node.
// new_key, new_child: the <new_key, new_child> entry to be inserted at a high level internal node
//
// How to split
// ---------------------------------
// | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |   + key + child
// ---------------------------------
//
// ---------------------   -------------------
// | 0 | 1 | 2 | 3 | 4 |   | 5 | 6 | 7 | key | + entry[5].d_key + pointer to right node
// ---------------------   -------------------
BPINODE64* bpRightInsert64(BPINODE64* node, int key, void* child, int* new_key, void** new_child) {
	BPINODE64 *old_node, *new_node;

	if (IsLeaf((BPINODE64*) node->d_entry[NumKey(node)].d_child)) {
		// This is the internal node that will split.
		old_node=node;
		new_node=(BPINODE64*) mynew(sizeof(BPINODE64));
		NumKey(old_node)=(7+1)>>1;
		new_node->d_entry[0]=old_node->d_entry[5];
		new_node->d_entry[1]=old_node->d_entry[6];
		new_node->d_entry[2]=old_node->d_entry[7];
		new_node->d_entry[3].d_key=key;
		new_node->d_entry[3].d_child=child;
		NumKey(new_node)=7-((7+1)>>1);

		if (node==g_bp_root64) { // now, we need to create a new root 
			g_bp_root64=(BPINODE64*) mynew(sizeof(BPINODE64));
			g_bp_root64->d_entry[0].d_child=old_node;
			g_bp_root64->d_entry[1].d_key=old_node->d_entry[((7+1)>>1)+1].d_key;
			g_bp_root64->d_entry[1].d_child=new_node;
			NumKey(g_bp_root64)=1;
		}
		else { // propagate the insertion to a higher level 
			*new_key=old_node->d_entry[((7+1)>>1)+1].d_key;
			*new_child=new_node;
		}
	}
	else {
		// keep going through the rightmost branch 
		new_node=bpRightInsert64((BPINODE64*)node->d_entry[NumKey(node)].d_child, key, child, new_key, new_child);
		if (*new_child) {
			if (NumKey(node)<7) {
				// insert the key right here, no further split
				NumKey(node)++;
				node->d_entry[NumKey(node)].d_key=*new_key;
				node->d_entry[NumKey(node)].d_child=*new_child;
				*new_child=0;
			}
			else {
				// we have to split again
				old_node=node;
				new_node=(BPINODE64*) mynew(sizeof(BPINODE64));
				NumKey(old_node)=(7+1)>>1;
				new_node->d_entry[0]=old_node->d_entry[5];
				new_node->d_entry[1]=old_node->d_entry[6];
				new_node->d_entry[2]=old_node->d_entry[7];
				new_node->d_entry[3].d_key=*new_key;
				new_node->d_entry[3].d_child=*new_child;
				NumKey(new_node)=7-((7+1)>>1);

				if (node==g_bp_root64) { // now, we need to create a new root 
					g_bp_root64=(BPINODE64*) mynew(sizeof(BPINODE64));
					g_bp_root64->d_entry[0].d_child=old_node;
					g_bp_root64->d_entry[1].d_key=old_node->d_entry[((7+1)>>1)+1].d_key;
					g_bp_root64->d_entry[1].d_child=new_node;
					NumKey(g_bp_root64)=1;
				}
				else { // propagate the insertion to a higher level 
					*new_key=old_node->d_entry[((7+1)>>1)+1].d_key;
					*new_child=new_node;
				}
			}
		}
	}
	return new_node;
}
*/

// bpInsert64:
// insert a new entry into a B+-Tree. A split may occur.
// root: current node being processed.
// new_entry: the <key, tid> entry to be inserted.
// new_key, new_child: the <new_key, new_child> entry to be inserted at a high level internal node
//
// How to split
// ---------------------------------
// | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |   + key + child
// ---------------------------------
//
// ---------------------   -------------------
// | 0 | 1 | 2 | 3 | 4 |   | 5 | 6 | 7 | key | + entry[5].d_key + pointer to right node
// ---------------------   -------------------
void bpInsert64(BPINODE64* root, LPair new_entry, int* new_key, void** new_child) {
	int l,h,m,i,j;
#ifdef GCC
	static void* sTable[]={&&l_bpISearch64_1,
		&&l_bpISearch64_1,
		&&l_bpISearch64_2,
		&&l_bpISearch64_3,
		&&l_bpISearch64_4,
		&&l_bpISearch64_5,
		&&l_bpISearch64_6,
		&&l_bpISearch64_7,
		&&l_bpLSearch64_1,  // leaves
		&&l_bpLSearch64_2,
		&&l_bpLSearch64_3,
		&&l_bpLSearch64_4,
		&&l_bpLSearch64_5,
		&&l_bpLSearch64_6};

#endif

	ASSERT(root);
	if (IsLeaf(root)) {    // This is a leaf node
#ifdef FIX_HARDCODE
		BPLSearch64(root, new_entry.d_key);
#elif VAR_HARDCODE
#ifdef GCC
		goto *sTable[((BPLNODE64*)root)->d_num+7];
		GCCBPLSEARCH64_VAR(((BPLNODE64*)root), new_entry.d_key);
#else    
		l=g_bpLList[((BPLNODE64*)root)->d_num]((BPLNODE64*)root, new_entry.d_key);
#endif
#else
		l=0;
		h=((BPLNODE64*)root)->d_num-1;
		while (l<=h) {
			m=(l+h)>>1;
			if (new_entry.d_key <= ((BPLNODE64*)root)->d_entry[m].d_key)
				h=m-1;
			else
				l=m+1;
		}
#endif
		// by now, d_entry[l-1].d_key < entry.key <= d_entry[l].d_key,
		// l can range from 0 to ((BPLNODE64*)root)->d_num
		ASSERT(l>=0 && l<=((BPLNODE64*)root)->d_num);
		if (((BPLNODE64*)root)->d_num < 6) {   //we still have enough space in this leaf node.
			// insert entry at the lth position, move everything from l to the right.
			for (i=((BPLNODE64*)root)->d_num; i>l; i--)
				((BPLNODE64*)root)->d_entry[i]=((BPLNODE64*)root)->d_entry[i-1];
			((BPLNODE64*)root)->d_entry[l]=new_entry;
			((BPLNODE64*)root)->d_num++;
			*new_child=0;
		}
		else { // we have to split this leaf node
			// first 4 keys in old node, remaining 3 keys in the new node.
			BPLNODE64 *new_lnode;
			new_lnode=(BPLNODE64*) mynew(sizeof(BPLNODE64));

#ifndef PRODUCT
			g_split++;
#endif
			if (l > (6>>1)) { //entry should be put in the new node
				for (i=(6>>1)-1, j=6-1; i>=0; i--) {
					if (i == l-(6>>1)-1) {
						new_lnode->d_entry[i]=new_entry;
					}
					else {
						new_lnode->d_entry[i]=((BPLNODE64*)root)->d_entry[j];
						j--;
					}
				}
				new_lnode->d_num=(6>>1);
				((BPLNODE64*)root)->d_num=(6>>1)+1;
			}
			else { //entry should be put in the original node
				for (i=(6>>1)-1; i>=0; i--) 
					new_lnode->d_entry[i]=((BPLNODE64*)root)->d_entry[i+(6>>1)];
				new_lnode->d_num=(6>>1);
				for (i=(6>>1); i>l; i--) 
					((BPLNODE64*)root)->d_entry[i]=((BPLNODE64*)root)->d_entry[i-1];
				((BPLNODE64*)root)->d_entry[l]=new_entry;
				((BPLNODE64*)root)->d_num=(6>>1)+1;
			}
			new_lnode->d_prev=(BPLNODE64*)root;
			new_lnode->d_next=((BPLNODE64*)root)->d_next;
			((BPLNODE64*)root)->d_next=new_lnode;
			if (new_lnode->d_next)
				new_lnode->d_next->d_prev=new_lnode;
			*new_key=((BPLNODE64*)root)->d_entry[(6>>1)].d_key;
			*new_child=new_lnode;
#ifdef FIX_HARDCODE
			for (i=(6>>1)+1; i<6; i++)
				((BPLNODE64*)root)->d_entry[i].d_key=MAX_KEY;
			for (i=(6>>1); i<6; i++)
				new_lnode->d_entry[i].d_key=MAX_KEY;
#endif
		}
	}
	else {  //this is an internal node
#ifdef FIX_HARDCODE
		BPISearch64(root, new_entry.d_key);
#elif VAR_HARDCODE
#ifdef GCC
		goto *sTable[NumKey(root)];
		GCCBPISEARCH64_VAR(root, new_entry.d_key);    
#else
		h=g_bpIList[NumKey(root)](root, new_entry.d_key);
#endif
#else
		l=1;
		h=NumKey(root);
		while (l<=h) {
			m=(l+h)>>1;
			if (new_entry.d_key <= root->d_entry[m].d_key)
				h=m-1;
			else
				l=m+1;
		}
#endif
		ASSERT(h>=0 && h<=NumKey(root));
		bpInsert64((BPINODE64*) root->d_entry[h].d_child, new_entry, new_key, new_child);
		if (*new_child) {
			if (NumKey(root)<7) { // insert the key right here, no further split
				NumKey(root)++;
				for (i=NumKey(root); i>h+1; i--)
					root->d_entry[i]=root->d_entry[i-1];
				root->d_entry[h+1].d_key=*new_key;
				root->d_entry[h+1].d_child=*new_child;
				*new_child=0;
			}
			else {	// we have to split again
				// first 5 keys go into root, remaining 3 keys go into new node.
				// the largest key in root is promoted later.
				BPINODE64 *new_node;
#ifndef PRODUCT
				g_split++;
#endif
				new_node=(BPINODE64*) mynew(sizeof(BPINODE64));
				if (h >= ((7+1)>>1)) {
					for (i=((7+1)>>1)-1, j=7; i>=0; i--) {
						if (i == h-((7+1)>>1)) {
							new_node->d_entry[i].d_key=*new_key;
							new_node->d_entry[i].d_child=*new_child;
						}
						else {
							new_node->d_entry[i]=root->d_entry[j];
							j--;
						}
					}
				}
				else {
					for (i=((7+1)>>1)-1; i>=0; i--) 
						new_node->d_entry[i]=root->d_entry[i+((7+1)>>1)];
					for (i=((7+1)>>1); i>h+1; i--)
						root->d_entry[i]=root->d_entry[i-1];
					root->d_entry[h+1].d_key=*new_key;
					root->d_entry[h+1].d_child=*new_child;
				}
				*new_key=new_node->d_entry[0].d_key;
				*new_child=new_node;
				NumKey(new_node)=7-((7+1)>>1);
				NumKey(root)=((7+1)>>1);
#ifdef FIX_HARDCODE
				//for (i=((7+1)>>1)+2; i<(7+1);i++)
				//  root->d_entry[i].d_key=MAX_KEY;
				for (i=((7+1)>>1); i<(7+1); i++)
					new_node->d_entry[i].d_key=MAX_KEY;
#endif
				if (root==g_bp_root64) { // now, we need to create a new root 
					g_bp_root64=(BPINODE64*) mynew(sizeof(BPINODE64));
					g_bp_root64->d_entry[0].d_child=root;
					g_bp_root64->d_entry[1].d_key=*new_key;
					g_bp_root64->d_entry[1].d_child=*new_child;
					NumKey(g_bp_root64)=1;
#ifdef FIX_HARDCODE
					for (i=2; i<(7+1);i++)
						g_bp_root64->d_entry[i].d_key=MAX_KEY;
#endif
				}
			}
		}
	}
}

// bpDelete64:
// Since a table typically grows rather than shrinks, we implement the lazy version of delete.
// Instead of maintaining the minimum occupancy, we simply locate the key on the leaves and delete it.
// return 1 if the entry is deleted, otherwise return 0.
int bpDelete64(BPINODE64* root, LPair del_entry) {
	int l,h,m, i;
#ifdef GCC
	static void* sTable[]={&&l_bpISearch64_1,
		&&l_bpISearch64_1,
		&&l_bpISearch64_2,
		&&l_bpISearch64_3,
		&&l_bpISearch64_4,
		&&l_bpISearch64_5,
		&&l_bpISearch64_6,
		&&l_bpISearch64_7,
		&&l_bpLSearch64_1,  // leaves
		&&l_bpLSearch64_2,
		&&l_bpLSearch64_3,
		&&l_bpLSearch64_4,
		&&l_bpLSearch64_5,
		&&l_bpLSearch64_6};

#endif

	while (!IsLeaf(root)) {
#ifdef FIX_HARDCODE
		BPISearch64(root, del_entry.d_key);
#elif VAR_HARDCODE
#ifdef GCC
		goto *sTable[NumKey(root)];
		GCCBPISEARCH64_VAR(root, del_entry.d_key);

#else
		h=g_bpIList[NumKey(root)](root, del_entry.d_key);
#endif
#else    
		l=1;
		h=NumKey(root);
		while (l<=h) {
			m=(l+h)>>1;
			if (del_entry.d_key <= root->d_entry[m].d_key)
				h=m-1;
			else
				l=m+1;
		}
#endif
		root=(BPINODE64*) root->d_entry[h].d_child;
	}

	//now search the leaf
#ifdef FIX_HARDCODE
	BPLSearch64(root, del_entry.d_key);
#elif VAR_HARDCODE
#ifdef GCC
	goto *sTable[((BPLNODE64*)root)->d_num+7];
	GCCBPLSEARCH64_VAR(((BPLNODE64*)root), del_entry.d_key);
#else    
	l=g_bpLList[((BPLNODE64*)root)->d_num]((BPLNODE64*)root, del_entry.d_key);
#endif
#else
	l=0;
	h=((BPLNODE64*)root)->d_num-1;
	while (l<=h) {
		m=(l+h)>>1;
		if (del_entry.d_key <= ((BPLNODE64*)root)->d_entry[m].d_key)
			h=m-1;
		else
			l=m+1;
	}
#endif

	// by now, d_entry[l-1].d_key < key <= d_entry[l].d_key,
	// l can range from 0 to ((BPLNODE64*)root)->d_num
	do {
		while (l<((BPLNODE64*)root)->d_num) {
			if (del_entry.d_key==((BPLNODE64*)root)->d_entry[l].d_key) {
				if (del_entry.d_tid == ((BPLNODE64*)root)->d_entry[l].d_tid) { //delete this entry
					for (i=l; i<((BPLNODE64*)root)->d_num-1; i++)
						((BPLNODE64*)root)->d_entry[i]=((BPLNODE64*)root)->d_entry[i+1];
					((BPLNODE64*)root)->d_num--;
#ifdef FIX_HARDCODE
					((BPLNODE64*)root)->d_entry[((BPLNODE64*)root)->d_num].d_key=MAX_KEY;
#endif
					return 1;
				}
				l++;
			}
			else
				return 0;
		}
		root=(BPINODE64*) ((BPLNODE64*)root)->d_next;
		l=0;
	}while (root);

	return 0;
}

// search for a key in a B+-Tree
// when there are duplicates, the leftmost key of the given value is found.
int bpSearch64(BPINODE64* root, int key) {
	int l,m,h;
#ifdef GCC
	static void* sTable[]={&&l_bpISearch64_1,
		&&l_bpISearch64_1,
		&&l_bpISearch64_2,
		&&l_bpISearch64_3,
		&&l_bpISearch64_4,
		&&l_bpISearch64_5,
		&&l_bpISearch64_6,
		&&l_bpISearch64_7,
		&&l_bpLSearch64_1,  // leaves
		&&l_bpLSearch64_2,
		&&l_bpLSearch64_3,
		&&l_bpLSearch64_4,
		&&l_bpLSearch64_5,
		&&l_bpLSearch64_6};

#endif

	ASSERT(root);
	while (!IsLeaf(root)) {
#ifdef FIX_HARDCODE
		BPISearch64(root, key);
#elif VAR_HARDCODE
#ifdef GCC
		goto *sTable[NumKey(root)];
		GCCBPISEARCH64_VAR(root, key);

#else
		h=g_bpIList[NumKey(root)](root, key);
#endif
#else    
		l=1;
		h=NumKey(root);
		while (l<=h) {
			m=(l+h)>>1;
			if (key <= root->d_entry[m].d_key)
				h=m-1;
			else
				l=m+1;
		}
#endif
		ASSERT(h>=0 && h<=NumKey(root));
		root=(BPINODE64*) root->d_entry[h].d_child;
	}

	//now search the leaf
#ifdef FIX_HARDCODE
	BPLSearch64(root, key);
#elif VAR_HARDCODE
#ifdef GCC
	goto *sTable[((BPLNODE64*)root)->d_num+7];
	GCCBPLSEARCH64_VAR(((BPLNODE64*)root), key);
#else    
	l=g_bpLList[((BPLNODE64*)root)->d_num]((BPLNODE64*)root, key);
#endif
#else
	l=0;
	h=((BPLNODE64*)root)->d_num-1;
	while (l<=h) {
		m=(l+h)>>1;
		if (key <= ((BPLNODE64*)root)->d_entry[m].d_key)
			h=m-1;
		else
			l=m+1;
	}
#endif

	// by now, d_entry[l-1].d_key < key <= d_entry[l].d_key,
	// l can range from 0 to ((BPLNODE64*)root)->d_num
	ASSERT(l>=0 && l<=((BPLNODE64*)root)->d_num);
	if (l<((BPLNODE64*)root)->d_num && key==((BPLNODE64*)root)->d_entry[l].d_key)
		return ((BPLNODE64*)root)->d_entry[l].d_tid;
	else
		return 0;
}

/*-------------------------------------------------------------------*/
/* Cache Sensitive B+-Trees                                          */
/*-------------------------------------------------------------------*/

#ifndef FULL_ALLOC
// bulk load a CSB+-Tree 
// n: size of the sorted array
// a: sorted leaf array
// iUpper: maximum number of keys for each internal node duing bulkload.
// lUpper: maximum number of keys for each leaf node duing bulkload.
// Note: iUpper has to be less than 14.
void csbBulkLoad64(int n, LPair* a, int iUpper, int lUpper) {
	BPLNODE64 *lcurr, *start, *lprev;
	CSBINODE64 *iLow, *iHigh, *iHighStart, *iLowStart; 
	int temp_key;
	void* temp_child;
	int i, j, nLeaf, nHigh, nLow, remainder;

	// first step, populate all the leaf nodes 
	nLeaf=(n+lUpper-1)/lUpper;
	lcurr=(BPLNODE64*) mynew(sizeof(BPLNODE64)*nLeaf);
	lcurr->d_flag=0;
	lcurr->d_num=0;
	lcurr->d_prev=0;
	start=lcurr;

	for (i=0; i<n; i++) {
		if (lcurr->d_num >= lUpper) { // at the beginning of a new node 
#ifdef FIX_HARDCODE
			// fill the empty slots with MAX_KEY
			for (j=lcurr->d_num; j<6; j++)
				lcurr->d_entry[j].d_key=MAX_KEY;
#endif
			lprev=lcurr;
			lcurr++;
			lcurr->d_flag=0;
			lcurr->d_num=0;
			lcurr->d_prev=lprev;
			ASSERT(lprev);
			lprev->d_next=lcurr;
		}
		lcurr->d_entry[lcurr->d_num]=a[i];
		lcurr->d_num++;
	}
	lcurr->d_next=0;
#ifdef FIX_HARDCODE
	// fill the empty slots with MAX_KEY
	for (j=lcurr->d_num; j<6; j++)
		lcurr->d_entry[j].d_key=MAX_KEY;
#endif

	// second step, build the internal nodes, level by level.
	// we can put IUpper keys and IUpper+1 children (implicit) per node
	nHigh=(nLeaf+iUpper)/(iUpper+1);
	remainder=nLeaf%(iUpper+1);
	iHigh=(CSBINODE64*) mynew(sizeof(CSBINODE64)*nHigh);
	iHigh->d_num=0;
	iHigh->d_firstChild=start;
	iHighStart=iHigh;
	lcurr=start;
	for (i=0; i<((remainder==0)?nHigh:(nHigh-1)); i++) {
		iHigh->d_num=iUpper;
		iHigh->d_firstChild=lcurr;
		for (j=0; j<iUpper+1; j++) {
			iHigh->d_keyList[j]=lcurr->d_entry[lcurr->d_num-1].d_key;
			lcurr++;
		}
#ifdef FIX_HARDCODE
		for (j=iUpper+1; j<14; j++)
			iHigh->d_keyList[j]=MAX_KEY;
#endif    
		iHigh++;
	}
	if (remainder==1) {
		//this is a special case, we have to borrow a key from the left node if there is one
		//leaf node remaining.
		iHigh->d_keyList[0]=(iHigh-1)->d_keyList[iUpper];
		(iHigh-1)->d_num--;
		iHigh->d_num=1;
		iHigh->d_firstChild=lcurr-1;
		lcurr++;
#ifdef FIX_HARDCODE
		for (j=1; j<14; j++)
			iHigh->d_keyList[j]=MAX_KEY;
		iHigh++;
#endif    
	}
	else if (remainder>1) {
		iHigh->d_firstChild=lcurr;
		for (i=0; i<remainder; i++) {
			iHigh->d_keyList[i]=lcurr->d_entry[lcurr->d_num-1].d_key;
			lcurr++;
		}
		iHigh->d_num=remainder-1;
#ifdef FIX_HARDCODE
		for (j=remainder; j<14; j++)
			iHigh->d_keyList[j]=MAX_KEY;
		iHigh++;
#endif    
	}
#ifdef FIX_HARDCODE
	(iHigh-1)->d_keyList[(iHigh-1)->d_num]=MAX_KEY;
#endif
	ASSERT((lcurr-nLeaf) == start);

	while (nHigh>1) {
		nLow=nHigh;
		iLow=iHighStart;
		iLowStart=iLow;
		nHigh=(nLow+iUpper)/(iUpper+1);
		remainder=nLow%(iUpper+1);
		iHigh=(CSBINODE64*) mynew(sizeof(CSBINODE64)*nHigh);
		iHigh->d_num=0;
		iHigh->d_firstChild=iLow;
		iHighStart=iHigh;
		for (i=0; i<((remainder==0)?nHigh:(nHigh-1)); i++) {
			iHigh->d_num=iUpper;
			iHigh->d_firstChild=iLow;
			for (j=0; j<iUpper+1; j++) {
				ASSERT(iLow->d_num<14);
				iHigh->d_keyList[j]=iLow->d_keyList[iLow->d_num];
				iLow++;
			}
#ifdef FIX_HARDCODE
			for (j=iUpper+1; j<14; j++)
				iHigh->d_keyList[j]=MAX_KEY;
#endif    
			iHigh++;
		}
		if (remainder==1) { //this is a special case, we have to borrow a key from the left node
			iHigh->d_keyList[0]=(iHigh-1)->d_keyList[iUpper];
			(iHigh-1)->d_num--;
			iHigh->d_num=1;
			iHigh->d_firstChild=iLow-1;
			iLow++;
#ifdef FIX_HARDCODE
			for (j=1; j<14; j++)
				iHigh->d_keyList[j]=MAX_KEY;
			iHigh++;
#endif    
		}
		else if (remainder>1) {
			iHigh->d_firstChild=iLow;
			for (i=0; i<remainder; i++) {
				ASSERT(iLow->d_num<14);
				iHigh->d_keyList[i]=iLow->d_keyList[iLow->d_num];
				iLow++;
			}
			iHigh->d_num=remainder-1;
#ifdef FIX_HARDCODE
			for (j=remainder; j<14; j++)
				iHigh->d_keyList[j]=MAX_KEY;
			iHigh++;
#endif    
		}
#ifdef FIX_HARDCODE
		(iHigh-1)->d_keyList[(iHigh-1)->d_num]=MAX_KEY;
#endif
		ASSERT((iLow-nLow) == iLowStart);
	}

	g_csb_root64=iHighStart;
}

#else

// This version of bulkload allocates a full nodegroup.
// The benefit of doing that is, we never need to deallocate space.
// Every time we have to split, we simply allocate another full nodegroup and
// redistribute the nodes between the two node groups.
// bulk load a CSB+-Tree 
// n: size of the sorted array
// a: sorted leaf array
// iUpper: maximum number of keys for each internal node duing bulkload.
// lUpper: maximum number of keys for each leaf node duing bulkload.
// Note: iUpper has to be less than 14.
void csbBulkLoad64(int n, LPair* a, int iUpper, int lUpper) {
	BPLNODE64 *lcurr, *start, *lprev;
	CSBINODE64 *iLow, *iHigh, *iHighStart, *iLowStart; 
	int temp_key;
	void* temp_child;
	int i, j, k, l, nLeaf, nHigh, nLow, remainder, nHigh1, nLow1, nLeaf1;

	// first step, populate all the leaf nodes 
	nLeaf=(n+lUpper-1)/lUpper;
	nLeaf1=((int)(nLeaf+iUpper)/(iUpper+1))*15;                 // allocate full node group
	lcurr=(BPLNODE64*) mynew(sizeof(BPLNODE64)*nLeaf1);
	lcurr->d_flag=0;
	lcurr->d_num=0;
	lcurr->d_prev=0;
	start=lcurr;
	k=0; 
	for (i=0; i<n; i++) {
		if (lcurr->d_num >= lUpper) { // at the beginning of a new node
#ifdef FIX_HARDCODE
			// fill the empty slots with MAX_KEY
			for (j=lcurr->d_num; j<6; j++)
				lcurr->d_entry[j].d_key=MAX_KEY;
#endif
			k++;
			lprev=lcurr;
			if (k==(iUpper+1)) {
				for (l=iUpper+2; l<=(14+1); l++) { // pad empty nodes
					lcurr++;
					lcurr->d_flag=0;
					lcurr->d_num=-1;
				}
				k=0;
			}
			lcurr++;
			lcurr->d_flag=0;
			lcurr->d_num=0;
			lcurr->d_prev=lprev;
			ASSERT(lprev);
			lprev->d_next=lcurr;
		}
		lcurr->d_entry[lcurr->d_num]=a[i];
		lcurr->d_num++;
	}
	lcurr->d_next=0;
#ifdef FIX_HARDCODE
	// fill the empty slots with MAX_KEY
	for (j=lcurr->d_num; j<6; j++)
		lcurr->d_entry[j].d_key=MAX_KEY;
#endif

	lcurr++;
	while (lcurr < start+nLeaf1) {
		lcurr->d_flag=0;
		lcurr->d_num=-1;
		lcurr++;
	}

	// second step, build the internal nodes, level by level.
	// we can put IUpper keys and IUpper+1 children (implicit) per node
	nHigh=(nLeaf+iUpper)/(iUpper+1);
	remainder=nLeaf%(iUpper+1);
	nHigh1=((int)(nHigh+iUpper)/(iUpper+1))*15;
	iHigh=(CSBINODE64*) mynew(sizeof(CSBINODE64)*nHigh1);
	iHigh->d_num=0;
	iHigh->d_firstChild=start;
	iHighStart=iHigh;
	lcurr=start;
	for (i=0; i<((remainder==0)?nHigh:(nHigh-1)); i++) {
		iHigh->d_num=iUpper;
		iHigh->d_firstChild=lcurr;
		for (j=0; j<iUpper+1; j++) {
			ASSERT((lcurr+j)->d_num>0);
			iHigh->d_keyList[j]=(lcurr+j)->d_entry[(lcurr+j)->d_num-1].d_key;
		}
		lcurr+=15;
#ifdef FIX_HARDCODE
		for (j=iUpper+1; j<14; j++)
			iHigh->d_keyList[j]=MAX_KEY;
#endif    
		iHigh++;
		if (i%(iUpper+1)+1==iUpper+1) {      //skip the reserved empty nodes
#ifndef PRODUCT
			for (j=0; j<14-iUpper; j++)
				(iHigh+j)->d_num=-1;
#endif
			iHigh+=14-iUpper;
		}
	}
	if (remainder==1) {
		//this is a special case, we have to borrow a key from the left node if there is one
		//leaf node remaining.
		if (nHigh%(iUpper+1)==1) {   //find previous node in the previous node group
			iHigh->d_keyList[0]=(iHigh-1-(14-iUpper))->d_keyList[iUpper];
			(iHigh-1-(14-iUpper))->d_num--;
		}
		else { //find previous node in the same node group
			iHigh->d_keyList[0]=(iHigh-1)->d_keyList[iUpper];
			(iHigh-1)->d_num--;
		}
		iHigh->d_num=1;
		iHigh->d_firstChild=lcurr;
		lcurr[1]=lcurr[0];
		lcurr[0]=*(lcurr-1-(14-iUpper));
		(lcurr-1-(14-iUpper))->d_num=-1;
#ifdef FIX_HARDCODE
		//(iHigh-1-(14-iUpper))->d_keyList[(iHigh-1-(14-iUpper))->d_num]=MAX_KEY;
		for (j=1; j<14; j++)
			iHigh->d_keyList[j]=MAX_KEY;
#endif
		iHigh++;
		lcurr+=15;
	}
	else if (remainder>1) {
		iHigh->d_firstChild=lcurr;
		for (i=0; i<remainder; i++) {
			ASSERT((lcurr+i)->d_num>0);
			iHigh->d_keyList[i]=(lcurr+i)->d_entry[(lcurr+i)->d_num-1].d_key;
		}
		iHigh->d_num=remainder-1;
#ifdef FIX_HARDCODE
		for (j=remainder; j<14; j++)
			iHigh->d_keyList[j]=MAX_KEY;
#endif
		iHigh++;
		lcurr+=15;
	}
#ifndef PRODUCT
	while (iHigh<iHighStart+nHigh1) {
		iHigh->d_num=-1;
		iHigh++;
	}
#endif
	ASSERT((lcurr-nLeaf1) == start);

	while (nHigh>1) {
		nLow=nHigh;
		nLow1=nHigh1;
		iLow=iHighStart;
		iLowStart=iLow;
		nHigh=(nLow+iUpper)/(iUpper+1);
		remainder=nLow%(iUpper+1);
		if (nHigh>1)
			nHigh1=((int)(nHigh+iUpper)/(iUpper+1))*15;
		else
			nHigh1=nHigh;
		iHigh=(CSBINODE64*) mynew(sizeof(CSBINODE64)*nHigh1);
		iHigh->d_num=0;
		iHigh->d_firstChild=iLow;
		iHighStart=iHigh;
		for (i=0; i<((remainder==0)?nHigh:(nHigh-1)); i++) {
			iHigh->d_num=iUpper;
			iHigh->d_firstChild=iLow;
			for (j=0; j<iUpper+1; j++) {
				ASSERT(iLow->d_num<14);
				ASSERT(iLow->d_num>=0);
				iHigh->d_keyList[j]=(iLow+j)->d_keyList[(iLow+j)->d_num];
			}
			iLow+=15;
#ifdef FIX_HARDCODE
			for (j=iUpper+1; j<14; j++)
				iHigh->d_keyList[j]=MAX_KEY;
#endif    
			iHigh++;
			if (i%(iUpper+1)+1==iUpper+1) {      //skip the reserved empty nodes
#ifndef PRODUCT
				for (j=0; j<14-iUpper; j++)
					(iHigh+j)->d_num=-1;
#endif
				iHigh+=14-iUpper;
			}
		}
		if (remainder==1) { //this is a special case, we have to borrow a key from the left node
			if (nHigh%(iUpper+1)==1) {   //find previous node in the previous node group
				iHigh->d_keyList[0]=(iHigh-1-(14-iUpper))->d_keyList[iUpper];
				(iHigh-1-(14-iUpper))->d_num--;
			}
			else { //find previous node in the same node group
				iHigh->d_keyList[0]=(iHigh-1)->d_keyList[iUpper];
				(iHigh-1)->d_num--;
			}
			iHigh->d_num=1;
			iHigh->d_firstChild=iLow;
			iLow[1]=iLow[0];
			iLow[0]=*(iLow-1-(14-iUpper));
			(iLow-1-(14-iUpper))->d_num=-1;
#ifdef FIX_HARDCODE
			for (j=1; j<14; j++)
				iHigh->d_keyList[j]=MAX_KEY;
#endif    
			iHigh++;
			iLow+=15;
		}
		else if (remainder>1) {
			iHigh->d_firstChild=iLow;
			for (i=0; i<remainder; i++) {
				ASSERT((iLow+i)->d_num<14);
				ASSERT((iLow+i)->d_num>=0);
				iHigh->d_keyList[i]=(iLow+i)->d_keyList[(iLow+i)->d_num];
			}
			iHigh->d_num=remainder-1;
#ifdef FIX_HARDCODE
			for (j=remainder; j<14; j++)
				iHigh->d_keyList[j]=MAX_KEY;
#endif    
			iHigh++;
			iLow+=15;
		}
#ifndef PRODUCT
		while (iHigh<iHighStart+nHigh1) {
			iHigh->d_num=-1;
			iHigh++;
		}
#endif
		ASSERT((iLow-nLow1) == iLowStart);
	}

	g_csb_root64=iHighStart;
}

#endif

#ifndef PRODUCT
void compareTree(CSBINODE64* root1, CSBINODE64* root2) {
	assert(root1!=0 && root2!=0);
	assert(*root1==*root2);
	if (root1->d_firstChild==0 && root2->d_firstChild==0)
		return;
	assert(root1->d_firstChild!=0 && root2->d_firstChild!=0);
	for (int i=0; i<root1->d_num+1; i++)
		compareTree((CSBINODE64*)root1->d_firstChild+i, (CSBINODE64*)root2->d_firstChild+i);
}
#endif

//csbTilePage:
//In modern computers, main memories are paged. Each page can be 8K large.
//TLB usually has 16 entries. Thus, the size of memory that can fit in TLB
//is about 0.5M. Tiling the nodes in 0.5M can reduced the number of TLB misses.
//This function basically clusters each 3-level subtree (less than 0.5M).
void* csbTilePage(CSBINODE64* firstChild, int nChild) {
	CSBINODE64* root=(CSBINODE64*)g_pool_curr;
	CSBINODE64* curr=(CSBINODE64*)g_pool_curr;
	CSBINODE64* parent=(CSBINODE64*)g_pool_curr;
	CSBINODE64* srcLeft=firstChild;
	CSBINODE64* srcRight=firstChild+nChild;
	CSBINODE64* node;
	CSBINODE64* next_parent;
	int i, j, nNodes=nChild;

	for (node=srcLeft; node<srcRight; node++) {
		*curr=*node;
		curr++;
	}

	for (j=1; j<4; j++) { //Tile a 2 level subtree
		if (IsLeaf(srcLeft)) {
			mynew(sizeof(CSBINODE64)*nNodes);
			return root;
		}
		next_parent=curr;
		for (node=srcLeft; node<srcRight; node++) {
			nNodes+=node->d_num+1;
			parent->d_firstChild=curr;
			parent++;
			for (i=0; i<node->d_num+1; i++) {
				*curr=*((CSBINODE64*)node->d_firstChild+i);
				if (IsLeaf(curr))
					g_space_used++;
				curr++;
			}
		}
		parent=next_parent;
		srcLeft=(CSBINODE64*)srcLeft->d_firstChild;
		srcRight=(CSBINODE64*)(srcRight-1)->d_firstChild+(srcRight-1)->d_num+1;
	}

	mynew(sizeof(CSBINODE64)*nNodes);
	//sync16kMemory();
	if (!IsLeaf(srcLeft)) {
		for (node=srcLeft; node<srcRight; node++) {
			parent->d_firstChild=csbTilePage((CSBINODE64*)node->d_firstChild, node->d_num+1);
			parent++;
		}
	}

	return root;
}

int g_level=0;

void calculateLevel(CSBINODE64* root) {
	g_level++;
	if (!IsLeaf(root))
		calculateLevel((CSBINODE64*)root->d_firstChild);
}

#ifndef PRODUCT
void verifyLevel(CSBINODE64* root, int level) {
	if (IsLeaf(root)) {
		assert(level==g_level);
		return;
	}
	for (int i=0; i<root->d_num+1; i++)
		verifyLevel((CSBINODE64*)root->d_firstChild+i, level+1);
}
#endif

//csbTileCopy:
void csbTileCopy(CSBINODE64** root) {
	CSBINODE64* new_root=(CSBINODE64*)mynew(sizeof(CSBINODE64));

#ifndef PRODUCT
	calculateLevel(*root);
	verifyLevel(*root,1);
	cout<<"pass verify level" <<endl;
#endif
	*new_root=**root;
	new_root->d_firstChild=csbTilePage((CSBINODE64*)(*root)->d_firstChild, (*root)->d_num+1);
#ifndef PRODUCT
	compareTree(*root, new_root);
	cout<<"pass compare tree" <<endl;
#endif
	*root=new_root;
}

#ifndef PRODUCT
void checkCSB64(CSBINODE64* root) {
	int i;
	CSBINODE64* node;

	ASSERT(root);
	ASSERT(validPointer(root));
	if (IsLeaf(root)) {
		return;
	}
	else {
		node=(CSBINODE64*)root->d_firstChild;
		ASSERT(validPointer(node));
		if (IsLeaf(node)) {
			assert(((BPLNODE64*)node)->d_entry[0].d_key<=root->d_keyList[0]);
			assert(((BPLNODE64*)node)->d_entry[((BPLNODE64*)node)->d_num-1].d_key<=root->d_keyList[0]);
		}
		else {
			assert(node->d_keyList[0]<=root->d_keyList[0]);
			assert(node->d_keyList[node->d_num-1]<=root->d_keyList[0]);
		}
		for (i=0; i<root->d_num; i++) {
			node=(CSBINODE64*)root->d_firstChild+i+1;
			if (IsLeaf(node)) {
				assert(((BPLNODE64*)node)->d_entry[0].d_key>=root->d_keyList[i]);
				assert(((BPLNODE64*)node)->d_entry[((BPLNODE64*)node)->d_num-1].d_key>=root->d_keyList[i]);
			}
			else {
				assert(node->d_keyList[0]>=root->d_keyList[i]);
				assert(node->d_keyList[node->d_num-1]>=root->d_keyList[i]);
			}
		}
		for (i=root->d_num; i<14; i++) {
			node=(CSBINODE64*)root->d_firstChild+i+1;
			assert(node->d_num==-1);
		}
		for (i=0; i<=root->d_num; i++) {
			node=(CSBINODE64*)root->d_firstChild+i;
			checkCSB64(node);
		}
	}
}
#endif

// csbInsert64:
// insert a new entry into a CSB+-Tree. A split may occur.
// root: current node being processed.
// parent: the parent of the current node.
// childIndex: current node is the ith child of parent.
// new_entry: the <key, tid> entry to be inserted.
// new_key, new_child: the <new_key, new_child> entry to be inserted at a high level internal node
//
// Each node can have a maximum of 14 keys.
// How to split
// ---------------------------------------------------------
// | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10| 11| 12| 13|  + key + child
// ---------------------------------------------------------
//
// ---------------------------------   -------------------------
// | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |   | 8 | 9 | 10| 11| 12| 13| + key 
// ---------------------------------   -------------------------
// The 7th key in the first node will be promoted.
void csbInsert64(CSBINODE64* root, CSBINODE64* parent, int childIndex, LPair new_entry, int* new_key, void** new_child) {
	int l,h,m,i,j;
#ifdef GCC
	static void* sTable[]={&&l_csbISearch64_1,
		&&l_csbISearch64_1,
		&&l_csbISearch64_2,
		&&l_csbISearch64_3,
		&&l_csbISearch64_4,
		&&l_csbISearch64_5,
		&&l_csbISearch64_6,
		&&l_csbISearch64_7,
		&&l_csbISearch64_8,
		&&l_csbISearch64_9,
		&&l_csbISearch64_10,
		&&l_csbISearch64_11,
		&&l_csbISearch64_12,
		&&l_csbISearch64_13,
		&&l_csbISearch64_14,
		&&l_bpLSearch64_1,  // leaves
		&&l_bpLSearch64_2,
		&&l_bpLSearch64_3,
		&&l_bpLSearch64_4,
		&&l_bpLSearch64_5,
		&&l_bpLSearch64_6};

#endif

	ASSERT(root);
	ASSERT(validPointer(root));
	if (IsLeaf(root)) {    // This is a leaf node
		ASSERT(((BPLNODE64*)root)->d_num<=6);
#ifdef FIX_HARDCODE
		BPLSearch64(root, new_entry.d_key);
#elif VAR_HARDCODE
#ifdef GCC
		goto *sTable[((BPLNODE64*)root)->d_num+14];
		GCCBPLSEARCH64_VAR(((BPLNODE64*)root), new_entry.d_key);
#else    
		l=g_bpLList[((BPLNODE64*)root)->d_num]((BPLNODE64*)root, new_entry.d_key);
#endif
#else
		l=0;
		h=((BPLNODE64*)root)->d_num-1;
		while (l<=h) {
			m=(l+h)>>1;
			if (new_entry.d_key <= ((BPLNODE64*)root)->d_entry[m].d_key)
				h=m-1;
			else
				l=m+1;
		}
#endif
		// by now, d_entry[l-1].d_key < entry.key <= d_entry[l].d_key,
		// l can range from 0 to ((BPLNODE64*)root)->d_num
		// insert entry at the lth position, move everything from l to the right.
		ASSERT(l>=0 && l<=((BPLNODE64*)root)->d_num);
		if (((BPLNODE64*)root)->d_num < 6) {   //we still have enough space in this leaf node.
			for (i=((BPLNODE64*)root)->d_num; i>l; i--)
				((BPLNODE64*)root)->d_entry[i]=((BPLNODE64*)root)->d_entry[i-1];
			((BPLNODE64*)root)->d_entry[l]=new_entry;
			((BPLNODE64*)root)->d_num++;
			*new_child=0;
		}
		else { // we have to split this leaf node
			BPLNODE64 *new_lnode, *old_lnode;
			BPLNODE64 *new_group, *old_group;

			old_group=(BPLNODE64*) parent->d_firstChild;
			ASSERT(root==((CSBINODE64*)parent->d_firstChild)+childIndex);
			if (parent->d_num < 14) { // we don't have to split the parent
				// This bug took me a day. Originally, a node group has parent->d_num+1 nodes.
				// Now we need to allocate space for parent->d_num+2 nodes.
#ifndef PRODUCT
				g_expand++;
#endif
#ifdef FULL_ALLOC
#ifndef PRODUCT
				for (i=0; i<=parent->d_num; i++) {
					assert(old_group[i].d_prev!=old_group+i);
					assert(old_group[i].d_next!=old_group+i);
					if (old_group[i].d_prev) 
						assert((old_group[i].d_prev)->d_entry[0].d_key<=old_group[i].d_entry[0].d_key);
					if (old_group[i].d_next) 
						assert((old_group[i].d_next)->d_entry[0].d_key>=old_group[i].d_entry[0].d_key);
				}
#endif
				for (i=parent->d_num+1; i>childIndex+1; i--)
					old_group[i]=old_group[i-1];
				if (childIndex+1==parent->d_num+1)
					old_group[parent->d_num+1].d_next=old_group[parent->d_num].d_next;
				for (i=parent->d_num+1; i>childIndex; i--) {
					old_group[i].d_prev=old_group+i-1;
					old_group[i-1].d_next=old_group+i;
				}
				if (old_group[parent->d_num+1].d_next)
					(old_group[parent->d_num+1].d_next)->d_prev=old_group+parent->d_num+1;
#ifndef PRODUCT
				for (i=0; i<=parent->d_num+1; i++) {
					assert(old_group[i].d_prev!=old_group+i);
					assert(old_group[i].d_next!=old_group+i);
					if (i!=childIndex+1 && i!=childIndex+2 && old_group[i].d_prev) 
						assert((old_group[i].d_prev)->d_entry[0].d_key<=old_group[i].d_entry[0].d_key);
					if (i!=childIndex && i!=childIndex+1 && old_group[i].d_next) 
						assert((old_group[i].d_next)->d_entry[0].d_key>=old_group[i].d_entry[0].d_key);
				}
#endif
				old_lnode=old_group+childIndex;
				new_lnode=old_group+childIndex+1;
				new_group=old_group;
#else
				new_group=(BPLNODE64*) mynew(sizeof(BPLNODE64)*(parent->d_num+2));
				for (i=0; i<=childIndex; i++) 
					new_group[i]=old_group[i];
				for (i=childIndex+2; i<=parent->d_num+1; i++) 
					new_group[i]=old_group[i-1];
				new_group[0].d_prev=old_group[0].d_prev;
				for (i=1; i<=parent->d_num+1; i++) {
					new_group[i].d_prev=new_group+i-1;
					new_group[i-1].d_next=new_group+i;
				}
				new_group[parent->d_num+1].d_next=old_group[parent->d_num].d_next;
				if (new_group[parent->d_num+1].d_next)
					(new_group[parent->d_num+1].d_next)->d_prev=new_group+parent->d_num+1;
				if (new_group[0].d_prev)
					(new_group[0].d_prev)->d_next=new_group;
				old_lnode=new_group+childIndex;
				new_lnode=new_group+childIndex+1;

				mydelete(old_group, sizeof(BPLNODE64)*parent->d_num);
#endif
			}
			else { // we also have to split parent. We have 15+1 nodes, put 8 in each node group.
#ifndef PRODUCT
				g_split++;
#endif
#ifdef FULL_ALLOC
				new_group=(BPLNODE64*) mynew(sizeof(BPLNODE64)*(14+1));
#else
				new_group=(BPLNODE64*) mynew(sizeof(BPLNODE64)*((14>>1)+1));
#endif
				if (childIndex >= (14>>1)) { // the new node (childIndex+1) belongs to new group
					for (i=(14>>1), j=14; i>=0; i--) 
						if (i != childIndex-(14>>1)) {
							new_group[i]=old_group[j];
							j--;
						}
					if (childIndex==14)    //the new node is the last one
						new_group[(14>>1)].d_next=old_group[14].d_next;
				}
				else { //the new node belongs to the old group
					for (i=(14>>1); i>=0; i--) 
						new_group[i]=old_group[i+(14>>1)];
					for (i=(14>>1); i>childIndex+1; i--)
						old_group[i]=old_group[i-1];
					//old_group[childIndex+1].d_prev=old_group+childIndex;
					//old_group[childIndex+1].d_next=old_group+childIndex+2;
				}
				new_group[0].d_prev=old_group+(14>>1);
				for (i=1; i<=(14>>1); i++) {
					new_group[i].d_prev=new_group+i-1;
					new_group[i-1].d_next=new_group+i;
					old_group[i].d_prev=old_group+i-1;
					old_group[i-1].d_next=old_group+i;
				}
				new_group[(14>>1)].d_next=old_group[14].d_next;
				old_group[(14>>1)].d_next=new_group;
				if (new_group[(14>>1)].d_next)
					(new_group[(14>>1)].d_next)->d_prev=new_group+(14>>1);
				new_lnode=(childIndex+1)>=((14>>1)+1)?
					new_group+childIndex-(14>>1):old_group+childIndex+1;
				old_lnode=(childIndex)>=((14>>1)+1)?
					new_group+childIndex-(14>>1)-1:old_group+childIndex;
#ifndef FULL_ALLOC
				mydelete(old_group+(14>>1)+1, sizeof(BPLNODE64)*(14>>1));
#else
#ifndef PRODUCT
				for (i=(14>>1)+1; i<14+1; i++)
					new_group[i].d_num=-1;
				for (i=(14>>1)+1; i<14+1; i++)
					old_group[i].d_num=-1;
#endif
#endif
			}
			if (l > (6>>1)) { //entry should be put in the new node
				for (i=(6>>1)-1, j=6-1; i>=0; i--) {
					if (i == l-(6>>1)-1) {
						new_lnode->d_entry[i]=new_entry;
					}
					else {
						new_lnode->d_entry[i]=old_lnode->d_entry[j];
						j--;
					}
				}
				//for (i=0; i<(6>>1)+1; i++)
				//  old_lnode->d_entry[i]=((BPLNODE64*)root)->d_entry[i];
			}
			else { //entry should be put in the original node
				for (i=(6>>1)-1; i>=0; i--) 
					new_lnode->d_entry[i]=old_lnode->d_entry[i+(6>>1)];
				for (i=(6>>1); i>l; i--) 
					old_lnode->d_entry[i]=old_lnode->d_entry[i-1];
				old_lnode->d_entry[l]=new_entry;
				//for (i=l-1; i>=0; i--) 
				//  old_lnode->d_entry[i]=((BPLNODE64*)root)->d_entry[i];
			}
			new_lnode->d_num=(6>>1);
			new_lnode->d_flag=0;
			old_lnode->d_num=(6>>1)+1;
			*new_key=old_lnode->d_entry[(6>>1)].d_key;
			*new_child=new_group;
#ifdef FIX_HARDCODE
			for (i=(6>>1)+1; i<6; i++)
				old_lnode->d_entry[i].d_key=MAX_KEY;
			for (i=(6>>1); i<6; i++)
				new_lnode->d_entry[i].d_key=MAX_KEY;
#endif
		}
	}
	else {  //this is an internal node
#ifdef FIX_HARDCODE
		CSBISearch64(root, new_entry.d_key);
#elif VAR_HARDCODE
#ifdef GCC
		goto *sTable[root->d_num];
		GCCCSBISEARCH64_VAR(root, new_entry.d_key);    
#else
		l=g_csbIList[root->d_num](root, new_entry.d_key);
#endif
#else
		l=0;
		h=root->d_num-1;
		while (l<=h) {
			m=(l+h)>>1;
			if (new_entry.d_key <= root->d_keyList[m])
				h=m-1;
			else
				l=m+1;
		}
#endif
		ASSERT(l>=0 && l<=root->d_num);    
		ASSERT(root->d_firstChild);
		csbInsert64(((CSBINODE64*)root->d_firstChild)+l, root, l, new_entry, new_key, new_child);
		if (*new_child) {
			if (root->d_num<14) { // insert the key right here, no further split
				for (i=root->d_num; i>l; i--)
					root->d_keyList[i]=root->d_keyList[i-1];
				root->d_keyList[i]=*new_key;
				root->d_firstChild=*new_child;  // *new_child represents the pointer to the new node group
				root->d_num++;
				*new_child=0;
			}
			else {	// we have to split again
				CSBINODE64 *new_node, *old_node, *new_group;

				if (parent==0) { // now, we need to create a new root 
					g_csb_root64=(CSBINODE64*) mynew(sizeof(CSBINODE64));
#ifdef FULL_ALLOC
					new_group=(CSBINODE64*) mynew(sizeof(CSBINODE64)*(14+1));
#ifndef PRODUCT
					for (i=2; i<14+1; i++)
						new_group[i].d_num=-1;
#endif
#else
					new_group=(CSBINODE64*) mynew(sizeof(CSBINODE64)*2);
#endif
					new_group[0]=*root;
					old_node=new_group;
					new_node=new_group+1;
					g_csb_root64->d_num=1;
					g_csb_root64->d_firstChild=new_group;
					// g_csb_root64->d_keyList[0] to be filled later.
					mydelete(root, sizeof(CSBINODE64)*1);
				}
				else {  // there is a parent
					CSBINODE64 *old_group;

					old_group=(CSBINODE64*) parent->d_firstChild;
					if (parent->d_num < 14) { // no need to split the parent
						// same here, now the new node group has parent->d_num+2 nodes.
#ifndef PRODUCT
						g_expand++;
#endif
#ifdef FULL_ALLOC
						for (i=parent->d_num+1; i>childIndex+1; i--)
							old_group[i]=old_group[i-1];
						old_node=old_group+childIndex;
						new_node=old_group+childIndex+1;
						new_group=old_group;
#else
						new_group=(CSBINODE64*) mynew(sizeof(CSBINODE64)*(parent->d_num+2));
						for (i=0; i<=childIndex; i++) 
							new_group[i]=old_group[i];
						for (i=childIndex+2; i<=parent->d_num+1; i++) 
							new_group[i]=old_group[i-1];
						old_node=new_group+childIndex;
						new_node=new_group+childIndex+1;

						mydelete(old_group, sizeof(CSBINODE64)*parent->d_num);
#endif
					}
					else { // we also have to split parent. We have 15+1 nodes, put 8 nodes in each group.
#ifndef PRODUCT
						g_split++;
#endif
#ifdef FULL_ALLOC
						new_group=(CSBINODE64*) mynew(sizeof(CSBINODE64)*(14+1));
#else
						new_group=(CSBINODE64*) mynew(sizeof(CSBINODE64)*((14>>1)+1));
#endif
						if (childIndex >= (14>>1)) { // the new node belongs to new group
							for (i=(14>>1), j=14; i>=0; i--) 
								if (i != childIndex-(14>>1)) {
									new_group[i]=old_group[j];
									j--;
								}
						}
						else { //the new node belongs to the old group
							for (i=(14>>1); i>=0; i--) 
								new_group[i]=old_group[i+(14>>1)];
							for (i=(14>>1); i>childIndex+1; i--)
								old_group[i]=old_group[i-1];
						}
						new_node=(childIndex+1)>=((14>>1)+1)?
							new_group+childIndex-(14>>1):old_group+childIndex+1;
						old_node=(childIndex)>=((14>>1)+1)?
							new_group+childIndex-(14>>1)-1:old_group+childIndex;
#ifndef FULL_ALLOC
						mydelete(old_group+(14>>1)+1, sizeof(BPLNODE64)*(14>>1));
#else
#ifndef PRODUCT
						for (i=(14>>1)+1; i<14+1; i++)
							new_group[i].d_num=-1;
						for (i=(14>>1)+1; i<14+1; i++)
							old_group[i].d_num=-1;
#endif
#endif
					}
				}
				// we have 14+1 keys, put the first 8 in old_node and the remaining 7 in new_node.
				// the largest key in old_node is then promoted to the parent. 
				if (l > (14>>1)) {     // new_key to be inserted in the new_node
					for (i=(14>>1)-1, j=14-1; i>=0; i--) {
						if (i == l-(14>>1)-1)
							new_node->d_keyList[i]=*new_key;
						else {
							new_node->d_keyList[i]=old_node->d_keyList[j];
							j--;
						}
					}
					//for (i=0; i<(14>>1)+1; i++)
					//  old_node->d_keyList[i]=root->d_keyList[i];
				}
				else {    // new_key to be inserted in the old_node
					for (i=(14>>1)-1; i>=0; i--) 
						new_node->d_keyList[i]=old_node->d_keyList[i+(14>>1)];
					for (i=(14>>1); i>l; i--)
						old_node->d_keyList[i]=old_node->d_keyList[i-1];
					old_node->d_keyList[l]=*new_key;
					//for (i=l-1; i>=0; i--) 
					//  old_node->d_keyList[i]=root->d_keyList[i];
				}
				new_node->d_num=(14>>1);
				new_node->d_firstChild=*new_child;
				old_node->d_num=(14>>1);
				//old_node->d_firstChild=root->d_firstChild;
#ifdef FULL_ALLOC
#ifndef PRODUCT
				for (i=old_node->d_num+1; i<14+1; i++) {
					CSBINODE64* node=(CSBINODE64*)old_node->d_firstChild+i;
					assert(node->d_num==-1);
				}
				for (i=new_node->d_num+1; i<14+1; i++) {
					CSBINODE64* node=(CSBINODE64*)new_node->d_firstChild+i;
					assert(node->d_num==-1);
				}
#endif
#endif

#ifdef FIX_HARDCODE
				for (i=(14>>1); i<14; i++)
					new_node->d_keyList[i]=MAX_KEY;
#endif
				if (parent) 
					*new_key=old_node->d_keyList[14>>1];
				else {
					g_csb_root64->d_keyList[0]=old_node->d_keyList[14>>1];
#ifdef FIX_HARDCODE
					for (i=1; i<14;i++)
						g_csb_root64->d_keyList[i]=MAX_KEY;
#endif
				}
				*new_child=new_group;
			}
		}
	}
}

// search for a key in a CSB+-Tree
// when there are duplicates, the leftmost key of the given value is found.
int csbSearch64(CSBINODE64* root, int key) {
	int l,m,h;
#ifdef GCC
	static void* sTable[]={&&l_csbISearch64_1,
		&&l_csbISearch64_1,
		&&l_csbISearch64_2,
		&&l_csbISearch64_3,
		&&l_csbISearch64_4,
		&&l_csbISearch64_5,
		&&l_csbISearch64_6,
		&&l_csbISearch64_7,
		&&l_csbISearch64_8,
		&&l_csbISearch64_9,
		&&l_csbISearch64_10,
		&&l_csbISearch64_11,
		&&l_csbISearch64_12,
		&&l_csbISearch64_13,
		&&l_csbISearch64_14,
		&&l_bpLSearch64_1,  // leaves
		&&l_bpLSearch64_2,
		&&l_bpLSearch64_3,
		&&l_bpLSearch64_4,
		&&l_bpLSearch64_5,
		&&l_bpLSearch64_6};

#endif

	while (!IsLeaf(root)) {
#ifdef FIX_HARDCODE
		CSBISearch64(root, key);
#elif VAR_HARDCODE
#ifdef GCC
		goto *sTable[root->d_num];
		GCCCSBISEARCH64_VAR(root, key);    
#else
		l=g_csbIList[root->d_num](root, key);
#endif
#else    
		l=0;
		h=root->d_num-1;
		while (l<=h) {
			m=(l+h)>>1;
			if (key <= root->d_keyList[m])
				h=m-1;
			else
				l=m+1;
		}
#endif 
		ASSERT(l>=0 && l<=root->d_num);
		root=(CSBINODE64*) root->d_firstChild+l;
	}

	//now search the leaf
#ifdef FIX_HARDCODE
	BPLSearch64(root, key);
#elif VAR_HARDCODE
#ifdef GCC
	goto *sTable[((BPLNODE64*)root)->d_num+14];
	GCCBPLSEARCH64_VAR(((BPLNODE64*)root), key);
#else    
	l=g_bpLList[((BPLNODE64*)root)->d_num]((BPLNODE64*)root, key);
#endif
#else
	l=0;
	h=((BPLNODE64*)root)->d_num-1;
	while (l<=h) {
		m=(l+h)>>1;
		if (key <= ((BPLNODE64*)root)->d_entry[m].d_key)
			h=m-1;
		else
			l=m+1;
	}
#endif
	// by now, d_entry[l-1].d_key < key <= d_entry[l].d_key,
	// l can range from 0 to ((BPLNODE64*)root)->d_num
	ASSERT(l>=0 && l<=((BPLNODE64*)root)->d_num);
	if (l<((BPLNODE64*)root)->d_num && key==((BPLNODE64*)root)->d_entry[l].d_key)
		return ((BPLNODE64*)root)->d_entry[l].d_tid;
	else
		return 0;
}

// csbDelete64:
// Since a table typically grows rather than shrinks, we implement the lazy version of delete.
// Instead of maintaining the minimum occupancy, we simply locate the key on the leaves and delete it.
// return 1 if the entry is deleted, otherwise return 0.
int csbDelete64(CSBINODE64* root, LPair del_entry) {
	int l,h,m, i;
#ifdef GCC
	static void* sTable[]={&&l_csbISearch64_1,
		&&l_csbISearch64_1,
		&&l_csbISearch64_2,
		&&l_csbISearch64_3,
		&&l_csbISearch64_4,
		&&l_csbISearch64_5,
		&&l_csbISearch64_6,
		&&l_csbISearch64_7,
		&&l_csbISearch64_8,
		&&l_csbISearch64_9,
		&&l_csbISearch64_10,
		&&l_csbISearch64_11,
		&&l_csbISearch64_12,
		&&l_csbISearch64_13,
		&&l_csbISearch64_14,
		&&l_bpLSearch64_1,  // leaves
		&&l_bpLSearch64_2,
		&&l_bpLSearch64_3,
		&&l_bpLSearch64_4,
		&&l_bpLSearch64_5,
		&&l_bpLSearch64_6};

#endif

	while (!IsLeaf(root)) {
#ifdef FIX_HARDCODE
		CSBISearch64(root, del_entry.d_key);
#elif VAR_HARDCODE
#ifdef GCC
		goto *sTable[root->d_num];
		GCCCSBISEARCH64_VAR(root, del_entry.d_key);    
#else
		l=g_csbIList[root->d_num](root, del_entry.d_key);
#endif
#else    
		l=0;
		h=root->d_num-1;
		while (l<=h) {
			m=(l+h)>>1;
			if (del_entry.d_key <= root->d_keyList[m])
				h=m-1;
			else
				l=m+1;
		}
#endif 
		root=(CSBINODE64*) root->d_firstChild+l;
	}

	//now search the leaf
#ifdef FIX_HARDCODE
	BPLSearch64(root, del_entry.d_key);
#elif VAR_HARDCODE
#ifdef GCC
	goto *sTable[((BPLNODE64*)root)->d_num+14];
	GCCBPLSEARCH64_VAR(((BPLNODE64*)root), del_entry.d_key);
#else    
	l=g_bpLList[((BPLNODE64*)root)->d_num]((BPLNODE64*)root, del_entry.d_key);
#endif
#else
	l=0;
	h=((BPLNODE64*)root)->d_num-1;
	while (l<=h) {
		m=(l+h)>>1;
		if (del_entry.d_key <= ((BPLNODE64*)root)->d_entry[m].d_key)
			h=m-1;
		else
			l=m+1;
	}
#endif

	// by now, d_entry[l-1].d_key < key <= d_entry[l].d_key,
	// l can range from 0 to ((BPLNODE64*)root)->d_num
	do {
		while (l<((BPLNODE64*)root)->d_num) {
			if (del_entry.d_key==((BPLNODE64*)root)->d_entry[l].d_key) {
				if (del_entry.d_tid == ((BPLNODE64*)root)->d_entry[l].d_tid) { //delete this entry
					for (i=l; i<((BPLNODE64*)root)->d_num-1; i++)
						((BPLNODE64*)root)->d_entry[i]=((BPLNODE64*)root)->d_entry[i+1];
					((BPLNODE64*)root)->d_num--;
#ifdef FIX_HARDCODE
					((BPLNODE64*)root)->d_entry[((BPLNODE64*)root)->d_num].d_key=MAX_KEY;
#endif
					return 1;
				}
				l++;
			}
			else
				return 0;
		}
		root=(CSBINODE64*) ((BPLNODE64*)root)->d_next;
		l=0;
	}while (root);

	return 0;
}

/*-------------------------------------------------------------------*/
/* Segmented Cache Sensitive B+-Trees                              */
/*-------------------------------------------------------------------*/

// bulk load a segmented CSB+-Tree with 2 segments
// n: size of the sorted array
// a: sorted leaf array
// iUpper: maximum number of keys for each internal node duing bulkload.
// lUpper: maximum number of keys for each leaf node duing bulkload.
// Note: iUpper has to be less than 13.
void gcsbBulkLoad64_2(int n, LPair* a, int iUpper, int lUpper) {
	ASSERT(iUpper < 13);
	csbBulkLoad64(n, a, iUpper, lUpper);
	gcsbAdjust_2(g_csb_root64);
	g_gcsb_root64_2=(GCSBINODE64_2*)g_csb_root64;
}

void gcsbAdjust_2(CSBINODE64* root) {
	int i,j;
	GCSBINODE64_2* p;

	if (IsLeaf(root))
		return;
	// The first segment consists of 6 keys (7 children).
	// The second segment consists of 7 keys (7 children).
	// The unused slots in the first segment are padded with the first key in the second segment.
	// The unused slots in the second segment are padded with the largest key. 
	for (i=0; i<=((CSBINODE64*)root)->d_num; i++) 
		gcsbAdjust_2((CSBINODE64*)root->d_firstChild+i);
	p=(GCSBINODE64_2*)root;
	p->d_num=(short int)(root->d_num);
	p->d_1stNum=(short int) ((p->d_num)>>1);
	// There is at least 1 key in the second segment.
	//  for (i=p->d_num-1, j=p->d_num-p->d_1stNum+5; i>=p->d_1stNum; i--, j--)
	//    p->d_keyList[j]=p->d_keyList[i];
	//  for (i=p->d_1stNum; i<6; i++)
	//    p->d_keyList[i]=p->d_keyList[6];
	p->d_2ndChild=(GCSBINODE64_2*)p->d_firstChild+p->d_1stNum+1;
}

// gcsbInsert64_2:
// insert a new entry into a segmented CSB+-Tree (2 segments). A split may occur.
// root: current node being processed.
// parent: the parent node if root.
// segP: the address of the corresponding segment pointer
// segIndex: index within the segment.
// childIndex: index within in a node
// new_entry: the <key, TID> entry to be inserted.
// new_key, new_child: the <new_key, new_child> entry to be inserted at a high level internal node
//
// each node can have a maximum of 13 keys. 
// How to split
// 13 keys + new_key
//
// first 7 keys in the first node, remaining 7 keys in the second node.
void gcsbInsert64_2(GCSBINODE64_2* root, GCSBINODE64_2* parent, void** segP, int segSize, int segIndex, int childIndex, LPair new_entry, int* new_key, void** new_child) {
	int l,h,m,i,j;
#ifdef GCC
	static void* sTable[]={&&l_csbISearch64_1,
		&&l_csbISearch64_1,
		&&l_csbISearch64_2,
		&&l_csbISearch64_3,
		&&l_csbISearch64_4,
		&&l_csbISearch64_5,
		&&l_csbISearch64_6,
		&&l_csbISearch64_7,
		&&l_csbISearch64_8,
		&&l_csbISearch64_9,
		&&l_csbISearch64_10,
		&&l_csbISearch64_11,
		&&l_csbISearch64_12,
		&&l_csbISearch64_13,
		&&l_csbISearch64_14,
		&&l_bpLSearch64_1,  // leaves
		&&l_bpLSearch64_2,
		&&l_bpLSearch64_3,
		&&l_bpLSearch64_4,
		&&l_bpLSearch64_5,
		&&l_bpLSearch64_6};

#endif

	ASSERT(root);
	ASSERT(validPointer(root));
	//ASSERT(g_csb_root64->d_keyList[0]);
	if (IsLeaf(root)) {    // This is a leaf node
		ASSERT(((BPLNODE64*)root)->d_num<=6);
#ifdef FIX_HARDCODE
		BPLSearch64(root, new_entry.d_key);
#elif VAR_HARDCODE
#ifdef GCC
		goto *sTable[((BPLNODE64*)root)->d_num+14];
		GCCBPLSEARCH64_VAR(((BPLNODE64*)root), new_entry.d_key);
#else    
		l=g_bpLList[((BPLNODE64*)root)->d_num]((BPLNODE64*)root, new_entry.d_key);
#endif
#else
		l=0;
		h=((BPLNODE64*)root)->d_num-1;
		while (l<=h) {
			m=(l+h)>>1;
			if (new_entry.d_key <= ((BPLNODE64*)root)->d_entry[m].d_key)
				h=m-1;
			else
				l=m+1;
		}
#endif
		// by now, d_entry[l-1].d_key < entry.key <= d_entry[l].d_key,
		// l can range from 0 to ((BPLNODE64*)root)->d_num
		// insert entry at the lth position, move everything from l to the right.
		ASSERT(l>=0 && l<=((BPLNODE64*)root)->d_num);
		if (((BPLNODE64*)root)->d_num < 6) {   //we still have enough space in this leaf node.
			for (i=((BPLNODE64*)root)->d_num; i>l; i--)
				((BPLNODE64*)root)->d_entry[i]=((BPLNODE64*)root)->d_entry[i-1];
			((BPLNODE64*)root)->d_entry[l]=new_entry;
			((BPLNODE64*)root)->d_num++;
			*new_child=0;
		}
		else { // we have to split this leaf node
			BPLNODE64 *new_lnode=0, *old_lnode=0;
			BPLNODE64 *new_group=0, *old_group=0;

			if (parent->d_num < 13) { // we don't have to split the parent
#ifndef PRODUCT
				g_expand++;
#endif
				old_group=(BPLNODE64*) *segP;
				new_group=(BPLNODE64*) mynew(sizeof(BPLNODE64)*(segSize+1));
				for (i=0; i<=segIndex; i++) 
					new_group[i]=old_group[i];
				for (i=segIndex+2; i<segSize+1; i++) 
					new_group[i]=old_group[i-1];
				//new_group[0].d_prev=old_group[0].d_prev;
				for (i=1; i<segSize+1; i++) {
					new_group[i].d_prev=new_group+i-1;
					new_group[i-1].d_next=new_group+i;
				}
				new_group[segSize].d_next=old_group[segSize-1].d_next;
				if (new_group[segSize].d_next)
					(new_group[segSize].d_next)->d_prev=new_group+segSize;
				if (new_group[0].d_prev)
					(new_group[0].d_prev)->d_next=new_group;
				old_lnode=new_group+segIndex;
				new_lnode=new_group+segIndex+1;

				*segP=new_group;
				*new_child=new_group;
				mydelete(old_group, sizeof(BPLNODE64)*segSize);
			}
			else {
				// we also have to split parent, we will have 15 nodes in total.
				// The first 7 belong to the old parent, the remaining 8 belong to the new parent.
				BPLNODE64 *cur_seg;

#ifndef PRODUCT
				g_split++;
#endif
				new_group=(BPLNODE64*) mynew(sizeof(BPLNODE64)*15);
				cur_seg=(BPLNODE64*) parent->d_firstChild;
				for (i=0, j=0; i<parent->d_1stNum+1; i++) {
					new_group[j]=cur_seg[i];
					j++;
					if (j==childIndex+1) {
						new_lnode=new_group+j;
						old_lnode=new_group+j-1;
						j++;
					}
				}
				mydelete(cur_seg, sizeof(BPLNODE64)*(parent->d_1stNum+1));
				cur_seg=(BPLNODE64*) parent->d_2ndChild;
				for (i=0; j<15; i++) {
					new_group[j]=cur_seg[i];
					j++;
					if (j==childIndex+1) {
						new_lnode=new_group+j;
						old_lnode=new_group+j-1;
						j++;
					}
				}
				mydelete(cur_seg, sizeof(BPLNODE64)*(parent->d_num-parent->d_1stNum));
				//ASSERT(((BPLNODE64*)root)->d_entry[0].d_key==old_lnode->d_entry[0].d_key);
				//new_group[0].d_prev=old_group+(14>>1);
				for (i=1; i<15; i++) {
					new_group[i].d_prev=new_group+i-1;
					new_group[i-1].d_next=new_group+i;
				}
				new_group[14].d_next=(((BPLNODE64*)parent->d_2ndChild)+parent->d_num-parent->d_1stNum-1)->d_next;
				if (new_group[14].d_next)
					(new_group[14].d_next)->d_prev=new_group+14;
				if (new_group[0].d_prev)
					(new_group[0].d_prev)->d_next=new_group;
				*new_child=new_group+7;
				parent->d_firstChild=new_group;  //first segment has 4 nodes, second has 3 nodes.
				parent->d_2ndChild=new_group+4;
			}
			if (l > (6>>1)) { //entry should be put in the new node
				for (i=(6>>1)-1, j=6-1; i>=0; i--) {
					if (i == l-(6>>1)-1) {
						new_lnode->d_entry[i]=new_entry;
					}
					else {
						new_lnode->d_entry[i]=old_lnode->d_entry[j];
						j--;
					}
				}
				//for (i=0; i<(6>>1)+1; i++)
				//  old_lnode->d_entry[i]=((BPLNODE64*)root)->d_entry[i];
			}
			else { //entry should be put in the original node
				for (i=(6>>1)-1; i>=0; i--) 
					new_lnode->d_entry[i]=old_lnode->d_entry[i+(6>>1)];
				for (i=(6>>1); i>l; i--) 
					old_lnode->d_entry[i]=old_lnode->d_entry[i-1];
				old_lnode->d_entry[l]=new_entry;
				//for (i=l-1; i>=0; i--) 
				//  old_lnode->d_entry[i]=((BPLNODE64*)root)->d_entry[i];
			}
			new_lnode->d_num=(6>>1);
			new_lnode->d_flag=0;
			old_lnode->d_num=(6>>1)+1;
			*new_key=old_lnode->d_entry[(6>>1)].d_key;
#ifdef FIX_HARDCODE
			for (i=(6>>1)+1; i<6; i++)
				old_lnode->d_entry[i].d_key=MAX_KEY;
			for (i=(6>>1); i<6; i++)
				new_lnode->d_entry[i].d_key=MAX_KEY;
#endif
		}
	}
	else {  //this is an internal node
#ifdef FIX_HARDCODE
		GCSBISearch64_2(root, new_entry.d_key);
#elif VAR_HARDCODE
#ifdef GCC
		goto *sTable[root->d_num];
		GCCCSBISEARCH64_VAR(root, new_entry.d_key);    
#else
		// we can use the structure for CSBINODE64 since the keyLists are started from the same position.
		l=g_csbIList[root->d_num]((CSBINODE64*)root, new_entry.d_key);
#endif
#else
		l=0;
		h=root->d_num-1;
		while (l<=h) {
			m=(l+h)>>1;
			if (new_entry.d_key <= root->d_keyList[m])
				h=m-1;
			else
				l=m+1;
		}
#endif
		ASSERT(l>=0 && l<=root->d_num);
		if (l<=root->d_1stNum) 
			gcsbInsert64_2(((GCSBINODE64_2*)root->d_firstChild)+l, root, &root->d_firstChild, root->d_1stNum+1, l, l, new_entry, new_key, new_child);
		else 
			gcsbInsert64_2(((GCSBINODE64_2*)root->d_2ndChild)+l-root->d_1stNum-1, root, &root->d_2ndChild, root->d_num-root->d_1stNum, l-root->d_1stNum-1, l, new_entry, new_key, new_child);
		if (*new_child) {
			if (root->d_num<13) { // insert the key right here, no further split
				for (i=root->d_num; i>l; i--)
					root->d_keyList[i]=root->d_keyList[i-1];
				root->d_keyList[i]=*new_key; // the child pointer has been set at a lower level.
				if (l<=root->d_1stNum)
					root->d_1stNum++;
				root->d_num++;
				*new_child=0;
			}
			else {
				// we have to split the current node into old_node and new_node.
				// the child pointers in old_node has been set properly. we only need to set the child
				// pointers in the new_node.
				GCSBINODE64_2 *new_node=0, *old_node=0, *new_group=0;

				if (parent==0) { // now, we need to create a new root 
					g_gcsb_root64_2=(GCSBINODE64_2*) mynew(sizeof(GCSBINODE64_2));
					new_group=(GCSBINODE64_2*) mynew(sizeof(GCSBINODE64_2)*2);
					new_group[0]=*root;
					old_node=new_group;
					new_node=new_group+1;
					g_gcsb_root64_2->d_num=1;
					g_gcsb_root64_2->d_1stNum=0;
					g_gcsb_root64_2->d_firstChild=new_group;
					g_gcsb_root64_2->d_2ndChild=new_group+1;
					// g_csb_root64->d_keyList[0] to be filled later.
					mydelete(root, sizeof(GCSBINODE64_2)*1);
				}
				else {  // there is a parent
					if (parent->d_num < 13) { // no need to split the parent
						GCSBINODE64_2 *old_group;

#ifndef PRODUCT
						g_expand++;
#endif
						old_group=(GCSBINODE64_2*) *segP;
						new_group=(GCSBINODE64_2*) mynew(sizeof(GCSBINODE64_2)*(segSize+1));
						for (i=0; i<=segIndex; i++) 
							new_group[i]=old_group[i];
						for (i=segIndex+2; i<segSize+1; i++) 
							new_group[i]=old_group[i-1];
						old_node=new_group+segIndex;
						new_node=new_group+segIndex+1;

						*segP=new_group;
						//*new_child=new_group;
						mydelete(old_group, sizeof(GCSBINODE64_2)*segSize);
					}
					else { // we also have to split parent, we will have 15 nodes in total.
						GCSBINODE64_2* cur_seg;

#ifndef PRODUCT
						g_split++;
#endif
						new_group=(GCSBINODE64_2*) mynew(sizeof(GCSBINODE64_2)*15);
						cur_seg=(GCSBINODE64_2*) parent->d_firstChild;
						for (i=0, j=0; i<parent->d_1stNum+1; i++) {
							new_group[j]=cur_seg[i];
							j++;
							if (j==childIndex+1) {
								new_node=new_group+j;
								old_node=new_group+j-1;
								j++;
							}
						}
						mydelete(cur_seg, sizeof(GCSBINODE64_2)*(parent->d_1stNum+1));
						cur_seg=(GCSBINODE64_2*) parent->d_2ndChild;
						for (i=0; j<15; i++) {
							new_group[j]=cur_seg[i];
							j++;
							if (j==childIndex+1) {
								new_node=new_group+j;
								old_node=new_group+j-1;
								j++;
							}
						}
						ASSERT(root->d_keyList[0]==old_node->d_keyList[0]);
						parent->d_firstChild=new_group;
						parent->d_2ndChild=new_group+4;
						mydelete(cur_seg, sizeof(GCSBINODE64_2)*(parent->d_num-parent->d_1stNum));
					}
				}
				// we have 13+1 keys, put the first 7 in old_node and the remaining 7 in new_node.
				// the first 7 children belong to old_node and the remaining 8 belong to new_node.
				// the largest key in old_node is then promoted to the parent. 
				if (l > 6) {     // new_key to be inserted in the new_node
					for (i=6, j=12; i>=0; i--) {
						if (i == l-7)
							new_node->d_keyList[i]=*new_key;
						else {
							new_node->d_keyList[i]=old_node->d_keyList[j];
							j--;
						}
					}
				}
				else {    // new_key to be inserted in the old_node
					for (i=6; i>=0; i--) 
						new_node->d_keyList[i]=old_node->d_keyList[i+6];
					for (i=6; i>l; i--)
						old_node->d_keyList[i]=old_node->d_keyList[i-1];
					old_node->d_keyList[l]=*new_key;
				}
				new_node->d_num=7;
				new_node->d_1stNum=3;
				new_node->d_firstChild=*new_child;
				new_node->d_2ndChild=(GCSBINODE64_2*)(*new_child)+4;
				old_node->d_num=6;
				old_node->d_1stNum=3; // the child pointers have been set in the previous iteration.
#ifndef PRODUCT
				for (i=0; i<new_node->d_1stNum; i++) {
					GCSBINODE64_2* node=(GCSBINODE64_2*)new_node->d_firstChild+i;
					if (IsLeaf(node)) {
						ASSERT(((BPLNODE64*)node)->d_entry[0].d_key <= new_node->d_keyList[i]);
						ASSERT(((BPLNODE64*)node)->d_entry[((BPLNODE64*)node)->d_num-1].d_key <= new_node->d_keyList[i]);
					}
					else {
						ASSERT(node->d_keyList[0] <= new_node->d_keyList[i]);
						ASSERT(node->d_keyList[node->d_num-1] <= new_node->d_keyList[i]);
					}
				}
				GCSBINODE64_2* node=(GCSBINODE64_2*)new_node->d_firstChild+new_node->d_1stNum;
				if (IsLeaf(node)) {
					ASSERT(((BPLNODE64*)node)->d_entry[0].d_key >= new_node->d_keyList[new_node->d_1stNum-1]);
					ASSERT(((BPLNODE64*)node)->d_entry[((BPLNODE64*)node)->d_num-1].d_key >= new_node->d_keyList[new_node->d_1stNum-1]);
				}
				else {
					ASSERT(node->d_keyList[0] >= new_node->d_keyList[new_node->d_1stNum-1]);
					ASSERT(node->d_keyList[node->d_num-1] >= new_node->d_keyList[new_node->d_1stNum-1]);
				}
				for (i=new_node->d_1stNum; i<new_node->d_num; i++) {
					GCSBINODE64_2* node=(GCSBINODE64_2*)new_node->d_2ndChild+i-new_node->d_1stNum;
					if (IsLeaf(node)) {
						ASSERT(((BPLNODE64*)node)->d_entry[0].d_key >= new_node->d_keyList[i]);
						ASSERT(((BPLNODE64*)node)->d_entry[((BPLNODE64*)node)->d_num-1].d_key >= new_node->d_keyList[i]);
					}
					else {
						ASSERT(node->d_keyList[0] >= new_node->d_keyList[i]);
						ASSERT(node->d_keyList[node->d_num-1] >= new_node->d_keyList[i]);
					}
				}
#endif

#ifdef FIX_HARDCODE
				for (i=7; i<13; i++)
					new_node->d_keyList[i]=MAX_KEY;
#endif
				if (parent) 
					*new_key=old_node->d_keyList[6];
				else {
					g_gcsb_root64_2->d_keyList[0]=old_node->d_keyList[6];
#ifdef FIX_HARDCODE
					for (i=1; i<13;i++)
						g_gcsb_root64_2->d_keyList[i]=MAX_KEY;
#endif
				}
				*new_child=new_group+7;
			}
		}
	}
}

// search for a key in a segmented CSB+-Tree
// when there are duplicates, the leftmost key of the given value is found.
int gcsbSearch64_2(GCSBINODE64_2* root, int key) {
	int l,m,h;
#ifdef GCC
	static void* sTable[]={&&l_csbISearch64_1,
		&&l_csbISearch64_1,
		&&l_csbISearch64_2,
		&&l_csbISearch64_3,
		&&l_csbISearch64_4,
		&&l_csbISearch64_5,
		&&l_csbISearch64_6,
		&&l_csbISearch64_7,
		&&l_csbISearch64_8,
		&&l_csbISearch64_9,
		&&l_csbISearch64_10,
		&&l_csbISearch64_11,
		&&l_csbISearch64_12,
		&&l_csbISearch64_13,
		&&l_csbISearch64_14,
		&&l_bpLSearch64_1,  // leaves
		&&l_bpLSearch64_2,
		&&l_bpLSearch64_3,
		&&l_bpLSearch64_4,
		&&l_bpLSearch64_5,
		&&l_bpLSearch64_6};

#endif

	while (!IsLeaf(root)) {
#ifdef FIX_HARDCODE
		GCSBISearch64_2(root, key);
#elif VAR_HARDCODE
#ifdef GCC
		goto *sTable[root->d_num];
		GCCCSBISEARCH64_VAR(root, key);    
#else
		l=g_csbIList[root->d_num]((CSBINODE64*)root, key);
#endif
#else    
		l=0;
		h=root->d_num-1;
		while (l<=h) {
			m=(l+h)>>1;
			if (key <= root->d_keyList[m])
				h=m-1;
			else
				l=m+1;
		}
		ASSERT(l>=0 && l<=13);
#endif 
		if (l<=root->d_1stNum)
			root=(GCSBINODE64_2*) root->d_firstChild+l;
		else
			root=(GCSBINODE64_2*) root->d_2ndChild+l-root->d_1stNum-1;
	}

	//now search the leaf
#ifdef FIX_HARDCODE
	BPLSearch64(root, key);
#elif VAR_HARDCODE
#ifdef GCC
	goto *sTable[((BPLNODE64*)root)->d_num+14];
	GCCBPLSEARCH64_VAR(((BPLNODE64*)root), key);
#else    
	l=g_bpLList[((BPLNODE64*)root)->d_num]((BPLNODE64*)root, key);
#endif
#else
	l=0;
	h=((BPLNODE64*)root)->d_num-1;
	while (l<=h) {
		m=(l+h)>>1;
		if (key <= ((BPLNODE64*)root)->d_entry[m].d_key)
			h=m-1;
		else
			l=m+1;
	}
#endif
	// by now, d_entry[l-1].d_key < key <= d_entry[l].d_key,
	// l can range from 0 to ((BPLNODE64*)root)->d_num
	ASSERT(l>=0 && l<=((BPLNODE64*)root)->d_num);
	if (l<((BPLNODE64*)root)->d_num && key==((BPLNODE64*)root)->d_entry[l].d_key)
		return ((BPLNODE64*)root)->d_entry[l].d_tid;
	else
		return 0;
}

// gcsbDelete64_2:
// Since a table typically grows rather than shrinks, we implement the lazy version of delete.
// Instead of maintaining the minimum occupancy, we simply locate the key on the leaves and delete it.
// return 1 if the entry is deleted, otherwise return 0.
int gcsbDelete64_2(GCSBINODE64_2* root, LPair del_entry) {
	int l,h,m, i;
#ifdef GCC
	static void* sTable[]={&&l_csbISearch64_1,
		&&l_csbISearch64_1,
		&&l_csbISearch64_2,
		&&l_csbISearch64_3,
		&&l_csbISearch64_4,
		&&l_csbISearch64_5,
		&&l_csbISearch64_6,
		&&l_csbISearch64_7,
		&&l_csbISearch64_8,
		&&l_csbISearch64_9,
		&&l_csbISearch64_10,
		&&l_csbISearch64_11,
		&&l_csbISearch64_12,
		&&l_csbISearch64_13,
		&&l_csbISearch64_14,
		&&l_bpLSearch64_1,  // leaves
		&&l_bpLSearch64_2,
		&&l_bpLSearch64_3,
		&&l_bpLSearch64_4,
		&&l_bpLSearch64_5,
		&&l_bpLSearch64_6};

#endif

	while (!IsLeaf(root)) {
#ifdef FIX_HARDCODE
		GCSBISearch64_2(root, del_entry.d_key);
#elif VAR_HARDCODE
#ifdef GCC
		goto *sTable[root->d_num];
		GCCCSBISEARCH64_VAR(root, del_entry.d_key);    
#else
		l=g_csbIList[root->d_num]((CSBINODE64*)root, del_entry.d_key);
#endif
#else    
		l=0;
		h=root->d_num-1;
		while (l<=h) {
			m=(l+h)>>1;
			if (del_entry.d_key <= root->d_keyList[m])
				h=m-1;
			else
				l=m+1;
		}
#endif 
		if (l<=root->d_1stNum)
			root=(GCSBINODE64_2*) root->d_firstChild+l;
		else
			root=(GCSBINODE64_2*) root->d_2ndChild+l-root->d_1stNum-1;
	}

	//now search the leaf
#ifdef FIX_HARDCODE
	BPLSearch64(root, del_entry.d_key);
#elif VAR_HARDCODE
#ifdef GCC
	goto *sTable[((BPLNODE64*)root)->d_num+14];
	GCCBPLSEARCH64_VAR(((BPLNODE64*)root), del_entry.d_key);
#else    
	l=g_bpLList[((BPLNODE64*)root)->d_num]((BPLNODE64*)root, del_entry.d_key);
#endif
#else
	l=0;
	h=((BPLNODE64*)root)->d_num-1;
	while (l<=h) {
		m=(l+h)>>1;
		if (del_entry.d_key <= ((BPLNODE64*)root)->d_entry[m].d_key)
			h=m-1;
		else
			l=m+1;
	}
#endif

	// by now, d_entry[l-1].d_key < key <= d_entry[l].d_key,
	// l can range from 0 to ((BPLNODE64*)root)->d_num
	do {
		while (l<((BPLNODE64*)root)->d_num) {
			if (del_entry.d_key==((BPLNODE64*)root)->d_entry[l].d_key) {
				if (del_entry.d_tid == ((BPLNODE64*)root)->d_entry[l].d_tid) { //delete this entry
					for (i=l; i<((BPLNODE64*)root)->d_num-1; i++)
						((BPLNODE64*)root)->d_entry[i]=((BPLNODE64*)root)->d_entry[i+1];
					((BPLNODE64*)root)->d_num--;
#ifdef FIX_HARDCODE
					((BPLNODE64*)root)->d_entry[((BPLNODE64*)root)->d_num].d_key=MAX_KEY;
#endif
					return 1;
				}
				l++;
			}
			else
				return 0;
		}
		root=(GCSBINODE64_2*) ((BPLNODE64*)root)->d_next;
		l=0;
	}while (root);

	return 0;
}

/*-------------------------------------------------------------------*/
/* Segmented Cache Sensitive B+-Trees with 3 segments              */
/*-------------------------------------------------------------------*/

// bulk load a segmented CSB+-Tree with 3 segments
// n: size of the sorted array
// a: sorted leaf array
// iUpper: maximum number of keys for each internal node duing bulkload.
// lUpper: maximum number of keys for each leaf node duing bulkload.
// Note: iUpper has to be less than 12.
void gcsbBulkLoad64_3(int n, LPair* a, int iUpper, int lUpper) {
	ASSERT(iUpper < 12);
	csbBulkLoad64(n, a, iUpper, lUpper);
	gcsbAdjust_3(g_csb_root64);
	g_gcsb_root64_3=(GCSBINODE64_3*)g_csb_root64;
}

void gcsbAdjust_3(CSBINODE64* root) {
	int i,j;
	GCSBINODE64_3* p;

	if (IsLeaf(root))
		return;
	// The first segment consists of d_1stNum+1 nodes.
	// The second segment consists of d_2ndNum-d_1stNum nodes.
	// The third segment consists of d_num-d_2ndNum nodes.
	for (i=0; i<=((CSBINODE64*)root)->d_num; i++) 
		gcsbAdjust_3((CSBINODE64*)root->d_firstChild+i);
	p=(GCSBINODE64_3*)root;
	p->d_num=(short int)(root->d_num);
	p->d_1stNum=(char) (p->d_num/3);
	p->d_2ndNum=(char) ((p->d_num-p->d_1stNum+1)/2+p->d_1stNum);
	// There is at least 1 key in the second segment.
	//  for (i=p->d_num-1, j=p->d_num-p->d_1stNum+5; i>=p->d_1stNum; i--, j--)
	//    p->d_keyList[j]=p->d_keyList[i];
	//  for (i=p->d_1stNum; i<6; i++)
	//    p->d_keyList[i]=p->d_keyList[6];
	p->d_2ndChild=(GCSBINODE64_3*)p->d_firstChild+p->d_1stNum+1;
	p->d_3rdChild=(GCSBINODE64_3*)p->d_firstChild+p->d_2ndNum+1;
}

// gcsbInsert64_3:
// insert a new entry into a segmented CSB+-Tree (3 segments). A split may occur.
// root: current node being processed.
// parent: the parent node if root.
// segP: the address of the corresponding segment pointer
// segIndex: index within the segment.
// childIndex: index within in a node
// new_entry: the <key, TID> entry to be inserted.
// new_key, new_child: the <new_key, new_child> entry to be inserted at a high level internal node
//
// each node can have a maximum of 12 keys. 
// How to split
// 12 keys + new_key
//
// first 7 keys in the first node, remaining 6 keys in the second node.
void gcsbInsert64_3(GCSBINODE64_3* root, GCSBINODE64_3* parent, void** segP, int segSize, int segIndex, int childIndex, LPair new_entry, int* new_key, void** new_child) {
	int l,h,m,i,j;
#ifdef GCC
	static void* sTable[]={&&l_csbISearch64_1,
		&&l_csbISearch64_1,
		&&l_csbISearch64_2,
		&&l_csbISearch64_3,
		&&l_csbISearch64_4,
		&&l_csbISearch64_5,
		&&l_csbISearch64_6,
		&&l_csbISearch64_7,
		&&l_csbISearch64_8,
		&&l_csbISearch64_9,
		&&l_csbISearch64_10,
		&&l_csbISearch64_11,
		&&l_csbISearch64_12,
		&&l_csbISearch64_13,
		&&l_csbISearch64_14,
		&&l_bpLSearch64_1,  // leaves
		&&l_bpLSearch64_2,
		&&l_bpLSearch64_3,
		&&l_bpLSearch64_4,
		&&l_bpLSearch64_5,
		&&l_bpLSearch64_6};

#endif

	ASSERT(root);
	ASSERT(validPointer(root));
	//ASSERT(g_csb_root64->d_keyList[0]);
	if (IsLeaf(root)) {    // This is a leaf node
		ASSERT(((BPLNODE64*)root)->d_num<=6);
#ifdef FIX_HARDCODE
		BPLSearch64(root, new_entry.d_key);
#elif VAR_HARDCODE
#ifdef GCC
		goto *sTable[((BPLNODE64*)root)->d_num+14];
		GCCBPLSEARCH64_VAR(((BPLNODE64*)root), new_entry.d_key);
#else    
		l=g_bpLList[((BPLNODE64*)root)->d_num]((BPLNODE64*)root, new_entry.d_key);
#endif
#else
		l=0;
		h=((BPLNODE64*)root)->d_num-1;
		while (l<=h) {
			m=(l+h)>>1;
			if (new_entry.d_key <= ((BPLNODE64*)root)->d_entry[m].d_key)
				h=m-1;
			else
				l=m+1;
		}
#endif
		// by now, d_entry[l-1].d_key < entry.key <= d_entry[l].d_key,
		// l can range from 0 to ((BPLNODE64*)root)->d_num
		// insert entry at the lth position, move everything from l to the right.
		ASSERT(l>=0 && l<=((BPLNODE64*)root)->d_num);
		if (((BPLNODE64*)root)->d_num < 6) {   //we still have enough space in this leaf node.
			for (i=((BPLNODE64*)root)->d_num; i>l; i--)
				((BPLNODE64*)root)->d_entry[i]=((BPLNODE64*)root)->d_entry[i-1];
			((BPLNODE64*)root)->d_entry[l]=new_entry;
			((BPLNODE64*)root)->d_num++;
			*new_child=0;
		}
		else { // we have to split this leaf node
			BPLNODE64 *new_lnode=0, *old_lnode=0;
			BPLNODE64 *new_group=0, *old_group=0;

			if (parent->d_num < 12) { // we don't have to split the parent
#ifndef PRODUCT
				g_expand++;
#endif
				old_group=(BPLNODE64*) *segP;
				new_group=(BPLNODE64*) mynew(sizeof(BPLNODE64)*(segSize+1));
				for (i=0; i<=segIndex; i++) 
					new_group[i]=old_group[i];
				for (i=segIndex+2; i<segSize+1; i++) 
					new_group[i]=old_group[i-1];
				//new_group[0].d_prev=old_group[0].d_prev;
				for (i=1; i<segSize+1; i++) {
					new_group[i].d_prev=new_group+i-1;
					new_group[i-1].d_next=new_group+i;
				}
				new_group[segSize].d_next=old_group[segSize-1].d_next;
				if (new_group[segSize].d_next)
					(new_group[segSize].d_next)->d_prev=new_group+segSize;
				if (new_group[0].d_prev)
					(new_group[0].d_prev)->d_next=new_group;
				old_lnode=new_group+segIndex;
				new_lnode=new_group+segIndex+1;

				*segP=new_group;
				*new_child=new_group;
				mydelete(old_group, sizeof(BPLNODE64)*segSize);
			}
			else {
				// we also have to split parent, we will have 14 nodes in total.
				// The first 7 belong to the old parent, the remaining 7 belong to the new parent.
				BPLNODE64 *cur_seg;

#ifndef PRODUCT
				g_split++;
#endif
				new_group=(BPLNODE64*) mynew(sizeof(BPLNODE64)*14);
				cur_seg=(BPLNODE64*) parent->d_firstChild;
				for (i=0, j=0; i<parent->d_1stNum+1; i++) {
					new_group[j]=cur_seg[i];
					j++;
					if (j==childIndex+1) {
						new_lnode=new_group+j;
						old_lnode=new_group+j-1;
						j++;
					}
				}
				mydelete(cur_seg, sizeof(BPLNODE64)*(parent->d_1stNum+1));
				cur_seg=(BPLNODE64*) parent->d_2ndChild;
				for (i=0; i<parent->d_2ndNum-parent->d_1stNum; i++) {
					new_group[j]=cur_seg[i];
					j++;
					if (j==childIndex+1) {
						new_lnode=new_group+j;
						old_lnode=new_group+j-1;
						j++;
					}
				}
				mydelete(cur_seg, sizeof(BPLNODE64)*(parent->d_2ndNum-parent->d_1stNum));
				cur_seg=(BPLNODE64*) parent->d_3rdChild;
				for (i=0; j<14; i++) {
					new_group[j]=cur_seg[i];
					j++;
					if (j==childIndex+1) {
						new_lnode=new_group+j;
						old_lnode=new_group+j-1;
						j++;
					}
				}
				mydelete(cur_seg, sizeof(BPLNODE64)*(parent->d_num-parent->d_2ndNum));
				//ASSERT(((BPLNODE64*)root)->d_entry[0].d_key==old_lnode->d_entry[0].d_key);
				//new_group[0].d_prev=old_group+(14>>1);
				for (i=1; i<14; i++) {
					new_group[i].d_prev=new_group+i-1;
					new_group[i-1].d_next=new_group+i;
				}
				new_group[13].d_next=(((BPLNODE64*)parent->d_3rdChild)+parent->d_num-parent->d_2ndNum-1)->d_next;
				if (new_group[13].d_next)
					(new_group[13].d_next)->d_prev=new_group+13;
				if (new_group[0].d_prev)
					(new_group[0].d_prev)->d_next=new_group;
				*new_child=new_group+7;
				parent->d_firstChild=new_group;  //first segment has 3 nodes, second and third each has 2 nodes.
				parent->d_2ndChild=new_group+3;
				parent->d_3rdChild=new_group+5;
			}
			if (l > (6>>1)) { //entry should be put in the new node
				for (i=(6>>1)-1, j=6-1; i>=0; i--) {
					if (i == l-(6>>1)-1) {
						new_lnode->d_entry[i]=new_entry;
					}
					else {
						new_lnode->d_entry[i]=old_lnode->d_entry[j];
						j--;
					}
				}
				//for (i=0; i<(6>>1)+1; i++)
				//  old_lnode->d_entry[i]=((BPLNODE64*)root)->d_entry[i];
			}
			else { //entry should be put in the original node
				for (i=(6>>1)-1; i>=0; i--) 
					new_lnode->d_entry[i]=old_lnode->d_entry[i+(6>>1)];
				for (i=(6>>1); i>l; i--) 
					old_lnode->d_entry[i]=old_lnode->d_entry[i-1];
				old_lnode->d_entry[l]=new_entry;
				//for (i=l-1; i>=0; i--) 
				//  old_lnode->d_entry[i]=((BPLNODE64*)root)->d_entry[i];
			}
			new_lnode->d_num=(6>>1);
			new_lnode->d_flag=0;
			old_lnode->d_num=(6>>1)+1;
			*new_key=old_lnode->d_entry[(6>>1)].d_key;
#ifdef FIX_HARDCODE
			for (i=(6>>1)+1; i<6; i++)
				old_lnode->d_entry[i].d_key=MAX_KEY;
			for (i=(6>>1); i<6; i++)
				new_lnode->d_entry[i].d_key=MAX_KEY;
#endif
		}
	}
	else {  //this is an internal node
#ifdef FIX_HARDCODE
		GCSBISearch64_3(root, new_entry.d_key);
#elif VAR_HARDCODE
#ifdef GCC
		goto *sTable[root->d_num];
		GCCCSBISEARCH64_VAR(root, new_entry.d_key);    
#else
		// we can use the structure for CSBINODE64 since the keyLists are started from the same position.
		l=g_csbIList[root->d_num]((CSBINODE64*)root, new_entry.d_key);
#endif
#else
		l=0;
		h=root->d_num-1;
		while (l<=h) {
			m=(l+h)>>1;
			if (new_entry.d_key <= root->d_keyList[m])
				h=m-1;
			else
				l=m+1;
		}
#endif
		ASSERT(l>=0 && l<=root->d_num);
		if (l<=root->d_1stNum) 
			gcsbInsert64_3(((GCSBINODE64_3*)root->d_firstChild)+l, root, &root->d_firstChild, root->d_1stNum+1, l, l, new_entry, new_key, new_child);
		else if (l<=root->d_2ndNum) {
			int si=l-root->d_1stNum-1;
			gcsbInsert64_3(((GCSBINODE64_3*)root->d_2ndChild)+si, root, &root->d_2ndChild, root->d_2ndNum-root->d_1stNum, si, l, new_entry, new_key, new_child);
		}
		else {
			int si=l-root->d_2ndNum-1;
			gcsbInsert64_3(((GCSBINODE64_3*)root->d_3rdChild)+si, root, &root->d_3rdChild, root->d_num-root->d_2ndNum, si, l, new_entry, new_key, new_child);
		}
		if (*new_child) {
			if (root->d_num<12) { // insert the key right here, no further split
				for (i=root->d_num; i>l; i--)
					root->d_keyList[i]=root->d_keyList[i-1];
				root->d_keyList[i]=*new_key; // the child pointer has been set at a lower level.
				if (l<=root->d_1stNum) {
					root->d_1stNum++;
					root->d_2ndNum++;
				}
				else if (l<=root->d_2ndNum)
					root->d_2ndNum++;
				root->d_num++;
				*new_child=0;
#ifndef PRODUCT
				for (i=0; i<root->d_num-1; i++)
					assert(root->d_keyList[i]<=root->d_keyList[i+1]);
				for (i=0; i<root->d_1stNum; i++) {
					GCSBINODE64_3* node=(GCSBINODE64_3*)root->d_firstChild+i;
					if (IsLeaf(node)) {
						ASSERT(((BPLNODE64*)node)->d_entry[0].d_key <= root->d_keyList[i]);
						ASSERT(((BPLNODE64*)node)->d_entry[((BPLNODE64*)node)->d_num-1].d_key <= root->d_keyList[i]);
					}
					else {
						ASSERT(node->d_keyList[0] <= root->d_keyList[i]);
						ASSERT(node->d_keyList[node->d_num-1] <= root->d_keyList[i]);
					}
				}
				GCSBINODE64_3* node=(GCSBINODE64_3*)root->d_firstChild+root->d_1stNum;
				if (root->d_1stNum>0) {
					if (IsLeaf(node)) {
						ASSERT(((BPLNODE64*)node)->d_entry[0].d_key >= root->d_keyList[root->d_1stNum-1]);
						ASSERT(((BPLNODE64*)node)->d_entry[((BPLNODE64*)node)->d_num-1].d_key >= root->d_keyList[root->d_1stNum-1]);
					}
					else {
						ASSERT(node->d_keyList[0] >= root->d_keyList[root->d_1stNum-1]);
						ASSERT(node->d_keyList[node->d_num-1] >= root->d_keyList[root->d_1stNum-1]);
					}
				}
				else  {
					assert(root->d_1stNum==0);
					if (IsLeaf(node)) {
						ASSERT(((BPLNODE64*)node)->d_entry[0].d_key <= root->d_keyList[root->d_1stNum]);
						ASSERT(((BPLNODE64*)node)->d_entry[((BPLNODE64*)node)->d_num-1].d_key <= root->d_keyList[root->d_1stNum]);
					}
					else {
						ASSERT(node->d_keyList[0] <= root->d_keyList[root->d_1stNum]);
						ASSERT(node->d_keyList[node->d_num-1] <= root->d_keyList[root->d_1stNum]);
					}
				}

				for (i=root->d_1stNum; i<root->d_2ndNum; i++) {
					GCSBINODE64_3* node=(GCSBINODE64_3*)root->d_2ndChild+i-root->d_1stNum;
					if (IsLeaf(node)) {
						ASSERT(((BPLNODE64*)node)->d_entry[0].d_key >= root->d_keyList[i]);
						ASSERT(((BPLNODE64*)node)->d_entry[((BPLNODE64*)node)->d_num-1].d_key >= root->d_keyList[i]);
					}
					else {
						ASSERT(node->d_keyList[0] >= root->d_keyList[i]);
						ASSERT(node->d_keyList[node->d_num-1] >= root->d_keyList[i]);
					}
				}
				for (i=root->d_2ndNum; i<root->d_num; i++) {
					GCSBINODE64_3* node=(GCSBINODE64_3*)root->d_3rdChild+i-root->d_2ndNum;
					if (IsLeaf(node)) {
						ASSERT(((BPLNODE64*)node)->d_entry[0].d_key >= root->d_keyList[i]);
						ASSERT(((BPLNODE64*)node)->d_entry[((BPLNODE64*)node)->d_num-1].d_key >= root->d_keyList[i]);
					}
					else {
						ASSERT(node->d_keyList[0] >= root->d_keyList[i]);
						ASSERT(node->d_keyList[node->d_num-1] >= root->d_keyList[i]);
					}
				}
#endif
			}
			else {
				// we have to split the current node into old_node and new_node.
				// the child pointers in old_node has been set properly. we only need to set the child
				// pointers in the new_node.
				GCSBINODE64_3 *new_node=0, *old_node=0, *new_group=0;

				if (parent==0) { // now, we need to create a new root 
					g_gcsb_root64_3=(GCSBINODE64_3*) mynew(sizeof(GCSBINODE64_3));
					new_group=(GCSBINODE64_3*) mynew(sizeof(GCSBINODE64_3)*2);
					new_group[0]=*root;
					old_node=new_group;
					new_node=new_group+1;
					g_gcsb_root64_3->d_num=1;
					g_gcsb_root64_3->d_1stNum=0;
					g_gcsb_root64_3->d_2ndNum=1;
					g_gcsb_root64_3->d_firstChild=new_group;
					g_gcsb_root64_3->d_2ndChild=new_group+1;
					g_gcsb_root64_3->d_3rdChild=0;
					// g_csb_root64->d_keyList[0] to be filled later.
					mydelete(root, sizeof(GCSBINODE64_3)*1);
				}
				else {  // there is a parent
					if (parent->d_num < 12) { // no need to split the parent
						GCSBINODE64_3 *old_group;

#ifndef PRODUCT
						g_expand++;
#endif
						old_group=(GCSBINODE64_3*) *segP;
						new_group=(GCSBINODE64_3*) mynew(sizeof(GCSBINODE64_3)*(segSize+1));
						for (i=0; i<=segIndex; i++) 
							new_group[i]=old_group[i];
						for (i=segIndex+2; i<segSize+1; i++) 
							new_group[i]=old_group[i-1];
						old_node=new_group+segIndex;
						new_node=new_group+segIndex+1;

						*segP=new_group;
						//*new_child=new_group;
						mydelete(old_group, sizeof(GCSBINODE64_3)*segSize);
					}
					else { // we also have to split parent, we will have 14 nodes in total.
						GCSBINODE64_3* cur_seg;

#ifndef PRODUCT
						g_split++;
#endif
						new_group=(GCSBINODE64_3*) mynew(sizeof(GCSBINODE64_3)*14);
						cur_seg=(GCSBINODE64_3*) parent->d_firstChild;
						for (i=0, j=0; i<parent->d_1stNum+1; i++) {
							new_group[j]=cur_seg[i];
							j++;
							if (j==childIndex+1) {
								new_node=new_group+j;
								old_node=new_group+j-1;
								j++;
							}
						}
						mydelete(cur_seg, sizeof(GCSBINODE64_3)*(parent->d_1stNum+1));
						cur_seg=(GCSBINODE64_3*) parent->d_2ndChild;
						for (i=0; i<parent->d_2ndNum-parent->d_1stNum; i++) {
							new_group[j]=cur_seg[i];
							j++;
							if (j==childIndex+1) {
								new_node=new_group+j;
								old_node=new_group+j-1;
								j++;
							}
						}
						mydelete(cur_seg, sizeof(GCSBINODE64_3)*(parent->d_2ndNum-parent->d_1stNum));
						cur_seg=(GCSBINODE64_3*) parent->d_3rdChild;
						for (i=0; j<14; i++) {
							new_group[j]=cur_seg[i];
							j++;
							if (j==childIndex+1) {
								new_node=new_group+j;
								old_node=new_group+j-1;
								j++;
							}
						}
						//ASSERT(root->d_keyList[0]==old_node->d_keyList[0]);
						mydelete(cur_seg, sizeof(GCSBINODE64_3)*(parent->d_num-parent->d_2ndNum));
						parent->d_firstChild=new_group;  //first segment has 3 nodes, second and third each has 2 nodes.
						parent->d_2ndChild=new_group+3;
						parent->d_3rdChild=new_group+5;
					}
				}
				// we have 12+1 keys, put the first 7 in old_node and the remaining 6 in new_node.
				// the first 7 children belong to old_node and the remaining 7 belong to new_node.
				// the largest key in old_node is then promoted to the parent. 
				if (l > 6) {     // new_key to be inserted in the new_node
					for (i=5, j=11; i>=0; i--) {
						if (i == l-7)
							new_node->d_keyList[i]=*new_key;
						else {
							new_node->d_keyList[i]=old_node->d_keyList[j];
							j--;
						}
					}
				}
				else {    // new_key to be inserted in the old_node
					for (i=5; i>=0; i--) 
						new_node->d_keyList[i]=old_node->d_keyList[i+6];
					for (i=6; i>l; i--)
						old_node->d_keyList[i]=old_node->d_keyList[i-1];
					old_node->d_keyList[l]=*new_key;
				}
				new_node->d_num=6;
				new_node->d_1stNum=2;
				new_node->d_2ndNum=4;
				new_node->d_firstChild=*new_child;
				new_node->d_2ndChild=(GCSBINODE64_3*)(*new_child)+3;
				new_node->d_3rdChild=(GCSBINODE64_3*)(*new_child)+5;
				old_node->d_num=6;
				old_node->d_1stNum=2; // the child pointers have been set in the previous iteration.
				old_node->d_2ndNum=4; // the child pointers have been set in the previous iteration.
#ifndef PRODUCT
				for (i=0; i<old_node->d_num-1; i++)
					assert(old_node->d_keyList[i]<=old_node->d_keyList[i+1]);
				for (i=0; i<new_node->d_num-1; i++)
					assert(new_node->d_keyList[i]<=new_node->d_keyList[i+1]);
				for (i=0; i<new_node->d_1stNum; i++) {
					GCSBINODE64_3* node=(GCSBINODE64_3*)new_node->d_firstChild+i;
					if (IsLeaf(node)) {
						ASSERT(((BPLNODE64*)node)->d_entry[0].d_key <= new_node->d_keyList[i]);
						ASSERT(((BPLNODE64*)node)->d_entry[((BPLNODE64*)node)->d_num-1].d_key <= new_node->d_keyList[i]);
					}
					else {
						ASSERT(node->d_keyList[0] <= new_node->d_keyList[i]);
						ASSERT(node->d_keyList[node->d_num-1] <= new_node->d_keyList[i]);
					}
				}
				GCSBINODE64_3* node=(GCSBINODE64_3*)new_node->d_firstChild+new_node->d_1stNum;
				if (new_node->d_1stNum>0) {
					if (IsLeaf(node)) {
						ASSERT(((BPLNODE64*)node)->d_entry[0].d_key >= new_node->d_keyList[new_node->d_1stNum-1]);
						ASSERT(((BPLNODE64*)node)->d_entry[((BPLNODE64*)node)->d_num-1].d_key >= new_node->d_keyList[new_node->d_1stNum-1]);
					}
					else {
						ASSERT(node->d_keyList[0] >= new_node->d_keyList[new_node->d_1stNum-1]);
						ASSERT(node->d_keyList[node->d_num-1] >= new_node->d_keyList[new_node->d_1stNum-1]);
					}
				}
				else  {
					assert(new_node->d_1stNum==0);
					if (IsLeaf(node)) {
						ASSERT(((BPLNODE64*)node)->d_entry[0].d_key <= new_node->d_keyList[new_node->d_1stNum]);
						ASSERT(((BPLNODE64*)node)->d_entry[((BPLNODE64*)node)->d_num-1].d_key <= new_node->d_keyList[new_node->d_1stNum]);
					}
					else {
						ASSERT(node->d_keyList[0] <= new_node->d_keyList[new_node->d_1stNum]);
						ASSERT(node->d_keyList[node->d_num-1] <= new_node->d_keyList[new_node->d_1stNum]);
					}
				}

				for (i=new_node->d_1stNum; i<new_node->d_2ndNum; i++) {
					GCSBINODE64_3* node=(GCSBINODE64_3*)new_node->d_2ndChild+i-new_node->d_1stNum;
					if (IsLeaf(node)) {
						ASSERT(((BPLNODE64*)node)->d_entry[0].d_key >= new_node->d_keyList[i]);
						ASSERT(((BPLNODE64*)node)->d_entry[((BPLNODE64*)node)->d_num-1].d_key >= new_node->d_keyList[i]);
					}
					else {
						ASSERT(node->d_keyList[0] >= new_node->d_keyList[i]);
						ASSERT(node->d_keyList[node->d_num-1] >= new_node->d_keyList[i]);
					}
				}
				for (i=new_node->d_2ndNum; i<new_node->d_num; i++) {
					GCSBINODE64_3* node=(GCSBINODE64_3*)new_node->d_3rdChild+i-new_node->d_2ndNum;
					if (IsLeaf(node)) {
						ASSERT(((BPLNODE64*)node)->d_entry[0].d_key >= new_node->d_keyList[i]);
						ASSERT(((BPLNODE64*)node)->d_entry[((BPLNODE64*)node)->d_num-1].d_key >= new_node->d_keyList[i]);
					}
					else {
						ASSERT(node->d_keyList[0] >= new_node->d_keyList[i]);
						ASSERT(node->d_keyList[node->d_num-1] >= new_node->d_keyList[i]);
					}
				}
#endif

#ifdef FIX_HARDCODE
				for (i=6; i<12; i++)
					new_node->d_keyList[i]=MAX_KEY;
#endif
				if (parent) 
					*new_key=old_node->d_keyList[6];
				else {
					g_gcsb_root64_3->d_keyList[0]=old_node->d_keyList[6];
#ifdef FIX_HARDCODE
					for (i=1; i<12;i++)
						g_gcsb_root64_3->d_keyList[i]=MAX_KEY;
#endif
				}
				*new_child=new_group+7;
			}
		}
	}
}

// search for a key in a segmented CSB+-Tree (3 segments)
// when there are duplicates, the leftmost key of the given value is found.
int gcsbSearch64_3(GCSBINODE64_3* root, int key) {
	int l,m,h;
#ifdef GCC
	static void* sTable[]={&&l_csbISearch64_1,
		&&l_csbISearch64_1,
		&&l_csbISearch64_2,
		&&l_csbISearch64_3,
		&&l_csbISearch64_4,
		&&l_csbISearch64_5,
		&&l_csbISearch64_6,
		&&l_csbISearch64_7,
		&&l_csbISearch64_8,
		&&l_csbISearch64_9,
		&&l_csbISearch64_10,
		&&l_csbISearch64_11,
		&&l_csbISearch64_12,
		&&l_csbISearch64_13,
		&&l_csbISearch64_14,
		&&l_bpLSearch64_1,  // leaves
		&&l_bpLSearch64_2,
		&&l_bpLSearch64_3,
		&&l_bpLSearch64_4,
		&&l_bpLSearch64_5,
		&&l_bpLSearch64_6};

#endif

	while (!IsLeaf(root)) {
#ifdef FIX_HARDCODE
		GCSBISearch64_3(root, key);
#elif VAR_HARDCODE
#ifdef GCC
		goto *sTable[root->d_num];
		GCCCSBISEARCH64_VAR(root, key);    
#else
		l=g_csbIList[root->d_num]((CSBINODE64*)root, key);
#endif
#else    
		l=0;
		h=root->d_num-1;
		while (l<=h) {
			m=(l+h)>>1;
			if (key <= root->d_keyList[m])
				h=m-1;
			else
				l=m+1;
		}
		ASSERT(l>=0 && l<=13);
#endif 
		if (l<=root->d_1stNum)
			root=(GCSBINODE64_3*) root->d_firstChild+l;
		else if (l<=root->d_2ndNum)
			root=(GCSBINODE64_3*) root->d_2ndChild+l-root->d_1stNum-1;
		else
			root=(GCSBINODE64_3*) root->d_3rdChild+l-root->d_2ndNum-1;
	}

	//now search the leaf
#ifdef FIX_HARDCODE
	BPLSearch64(root, key);
#elif VAR_HARDCODE
#ifdef GCC
	goto *sTable[((BPLNODE64*)root)->d_num+14];
	GCCBPLSEARCH64_VAR(((BPLNODE64*)root), key);
#else    
	l=g_bpLList[((BPLNODE64*)root)->d_num]((BPLNODE64*)root, key);
#endif
#else
	l=0;
	h=((BPLNODE64*)root)->d_num-1;
	while (l<=h) {
		m=(l+h)>>1;
		if (key <= ((BPLNODE64*)root)->d_entry[m].d_key)
			h=m-1;
		else
			l=m+1;
	}
#endif
	// by now, d_entry[l-1].d_key < key <= d_entry[l].d_key,
	// l can range from 0 to ((BPLNODE64*)root)->d_num
	ASSERT(l>=0 && l<=((BPLNODE64*)root)->d_num);
	if (l<((BPLNODE64*)root)->d_num && key==((BPLNODE64*)root)->d_entry[l].d_key)
		return ((BPLNODE64*)root)->d_entry[l].d_tid;
	else
		return 0;
}

// gcsbDelete64_3:
// Since a table typically grows rather than shrinks, we implement the lazy version of delete.
// Instead of maintaining the minimum occupancy, we simply locate the key on the leaves and delete it.
// return 1 if the entry is deleted, otherwise return 0.
int gcsbDelete64_3(GCSBINODE64_3* root, LPair del_entry) {
	int l,h,m, i;
#ifdef GCC
	static void* sTable[]={&&l_csbISearch64_1,
		&&l_csbISearch64_1,
		&&l_csbISearch64_2,
		&&l_csbISearch64_3,
		&&l_csbISearch64_4,
		&&l_csbISearch64_5,
		&&l_csbISearch64_6,
		&&l_csbISearch64_7,
		&&l_csbISearch64_8,
		&&l_csbISearch64_9,
		&&l_csbISearch64_10,
		&&l_csbISearch64_11,
		&&l_csbISearch64_12,
		&&l_csbISearch64_13,
		&&l_csbISearch64_14,
		&&l_bpLSearch64_1,  // leaves
		&&l_bpLSearch64_2,
		&&l_bpLSearch64_3,
		&&l_bpLSearch64_4,
		&&l_bpLSearch64_5,
		&&l_bpLSearch64_6};

#endif

	while (!IsLeaf(root)) {
#ifdef FIX_HARDCODE
		GCSBISearch64_3(root, del_entry.d_key);
#elif VAR_HARDCODE
#ifdef GCC
		goto *sTable[root->d_num];
		GCCCSBISEARCH64_VAR(root, del_entry.d_key);    
#else
		l=g_csbIList[root->d_num]((CSBINODE64*)root, del_entry.d_key);
#endif
#else    
		l=0;
		h=root->d_num-1;
		while (l<=h) {
			m=(l+h)>>1;
			if (del_entry.d_key <= root->d_keyList[m])
				h=m-1;
			else
				l=m+1;
		}
#endif 
		if (l<=root->d_1stNum)
			root=(GCSBINODE64_3*) root->d_firstChild+l;
		else if (l<=root->d_2ndNum)
			root=(GCSBINODE64_3*) root->d_2ndChild+l-root->d_1stNum-1;
		else
			root=(GCSBINODE64_3*) root->d_3rdChild+l-root->d_2ndNum-1;
	}

	//now search the leaf
#ifdef FIX_HARDCODE
	BPLSearch64(root, del_entry.d_key);
#elif VAR_HARDCODE
#ifdef GCC
	goto *sTable[((BPLNODE64*)root)->d_num+14];
	GCCBPLSEARCH64_VAR(((BPLNODE64*)root), del_entry.d_key);
#else    
	l=g_bpLList[((BPLNODE64*)root)->d_num]((BPLNODE64*)root, del_entry.d_key);
#endif
#else
	l=0;
	h=((BPLNODE64*)root)->d_num-1;
	while (l<=h) {
		m=(l+h)>>1;
		if (del_entry.d_key <= ((BPLNODE64*)root)->d_entry[m].d_key)
			h=m-1;
		else
			l=m+1;
	}
#endif

	// by now, d_entry[l-1].d_key < key <= d_entry[l].d_key,
	// l can range from 0 to ((BPLNODE64*)root)->d_num
	do {
		while (l<((BPLNODE64*)root)->d_num) {
			if (del_entry.d_key==((BPLNODE64*)root)->d_entry[l].d_key) {
				if (del_entry.d_tid == ((BPLNODE64*)root)->d_entry[l].d_tid) { //delete this entry
					for (i=l; i<((BPLNODE64*)root)->d_num-1; i++)
						((BPLNODE64*)root)->d_entry[i]=((BPLNODE64*)root)->d_entry[i+1];
					((BPLNODE64*)root)->d_num--;
#ifdef FIX_HARDCODE
					((BPLNODE64*)root)->d_entry[((BPLNODE64*)root)->d_num].d_key=MAX_KEY;
#endif
					return 1;
				}
				l++;
			}
			else
				return 0;
		}
		root=(GCSBINODE64_3*) ((BPLNODE64*)root)->d_next;
		l=0;
	}while (root);

	return 0;
}

/*****************************************************************************/
/* FULL CSS-Trees.                                                           */
/*****************************************************************************/

// cssBulkLoad64:
// num: size of the sorted array
// a1: sorted leaf array
// *nInternal: is the index of the first non-internal nodes, starting from 0. 
// *thres is the index of the first node where the key should be found in the first
//  half of a1.
//17-way search (each node has 16 keys)
void cssBulkLoad64(int num, LPair* a1, int** a2, int* nInternal, int* thres) {
	//a1 has ordered data. *a2 is the final heap
	int k, B, x, nonLeaf, mask, nodeSize, branch;
	int i,j , node, child, l, exp9k;
	int* b; 
	int mbits=4;             //16 integers per node
	int entryPerLeaf=8;      //8 pairs per leaf node
	int keyPNode=16;         //keys per internal node

	nodeSize=1<<mbits;
	mask=nodeSize-1;
	branch=nodeSize+1;
	B=ceil((double)num/(double)entryPerLeaf);
	k=ceil(log10((double)B)/log10((double)branch));
	exp9k=pow(branch,k);
	x=(exp9k-B)/nodeSize;

	nonLeaf=((exp9k-1)>>mbits)-x;
	*nInternal=nonLeaf;
	b=(int*)mynew(nonLeaf*64);
	*thres=((exp9k-1)>>4);

	for (i=(*nInternal)-1;i>=0;i--) {
		for (j=i*keyPNode+keyPNode-1; j>=i*keyPNode; j--) {
			child=branch*i+(j&mask)+1;
			while (child<nonLeaf) 
				child=branch*child+branch;
			l=child-(*thres);     // l is the node index
			l=l*entryPerLeaf;     // l is the entry index
			if (l<0) {
				l+=num;
				b[j]=a1[l+entryPerLeaf-1].d_key;
			}
			else {
				if (l<num-x*entryPerLeaf)
					b[j]=a1[l+entryPerLeaf-1].d_key;
				else
					b[j]=a1[num-x*entryPerLeaf-1].d_key;
			}
		}
	}
	*a2=b;
}

int cssSearch64(LPair* a1, int num, int key, int* a2, int nInternal, int thres) {
	//a1 is the sorted leaf array with num elements.
	//a2 is the top level index structure
	// *nInternal: is the index of the first non-internal nodes, starting from 0. 
	// *thres is the index of the first node where the key should be found in the first
	//  half of a1.
	//17-way search (each node has 16 keys)
	int l, node;
	int* p;
	LPair* p1;

	node=0;
	while (node<nInternal) {
		p=a2+(node<<4);
		if (*(p+8)>=key) {
			if (*(p+3)>=key) {
				if (*(p+1)>=key) {
					if (*p>=key)
						l=1;
					else
						l=2;
				}
				else {  //*(p+1]<key
					if (*(p+2)>=key)
						l=3;
					else
						l=4;
				}
			}
			else { //if (*(p+3]<key) 
				if (*(p+5)>=key) {
					if (*(p+4)>=key)
						l=5;
					else
						l=6;
				}
				else {
					if (*(p+6)>=key)
						l=7;
					else {
						if (*(p+7)>=key)
							l=8;
						else
							l=9;
					}
				}
			}
		}
		else { //(*(p+8)<key) {
			if (*(p+12)>=key) {
				if (*(p+10)>=key) {
					if (*(p+9)>=key)
						l=10;
					else
						l=11;
				}
				else {  //*(p+1]<key
					if (*(p+11)>=key)
						l=12;
					else
						l=13;
				}
			}
			else { //if (*(p+3]<key) 
				if (*(p+14)>=key) {
					if (*(p+13)>=key)
						l=14;
					else
						l=15;
				}
				else {
					if (*(p+15)>=key)
						l=16;
					else 
						l=17;
				}
			}
		}
		node=node*17+l; 
		}
		l=node-thres;
		l=(l<<3);     // l is the entry index
		if (l<0) 
			l+=num;
		//now at the  leaves
		p1=a1+l;
		LEAF64(p1, key);

		if (l<8 && key==(p1+l)->d_key)
			return (p1+l)->d_tid;
		else
			return 0;
	}

	// bpGetMaxKey: this is a testing function to verify that all the leaf nodes are
	// linked properly.
	int bpGetMaxKey(BPINODE64* root) {
		BPLNODE64* p;

		while (!IsLeaf(root))
			root=(BPINODE64*) root->d_entry[0].d_child;
		p=(BPLNODE64*)root;
		while (p->d_next) {
			ASSERT(validPointer(p));
			p=p->d_next;
		}
		return p->d_entry[p->d_num-1].d_key;
	}

	// bpGetMinKey: this is a testing function to verify that all the leaf nodes are
	// linked properly.
	int bpGetMinKey(BPINODE64* root) {
		BPLNODE64* p;

		while (!IsLeaf(root))
			root=(BPINODE64*) root->d_entry[NumKey(root)].d_child;
		p=(BPLNODE64*)root;
		while (p->d_prev) {
			ASSERT(validPointer(p));
			p=p->d_prev;
		}
		return p->d_entry[0].d_key;
	}

	// csbGetMaxKey: this is a testing function to verify that all the leaf nodes are
	// linked properly.
	int csbGetMaxKey(CSBINODE64* root) {
		BPLNODE64* p;

		while (!IsLeaf(root))
			root=(CSBINODE64*) root->d_firstChild;
		p=(BPLNODE64*)root;
		while (p->d_next) {
			ASSERT(validPointer(p));
			p=p->d_next;
		}
		return p->d_entry[p->d_num-1].d_key;
	}

	// csbGetMinKey: this is a testing function to verify that all the leaf nodes are
	// linked properly.
	int csbGetMinKey(CSBINODE64* root) {
		BPLNODE64* p;

		while (!IsLeaf(root))
			root=(CSBINODE64*) root->d_firstChild+root->d_num;
		p=(BPLNODE64*)root;
		while (p->d_prev) {
			ASSERT(validPointer(p));
			p=p->d_prev;
		}
		return p->d_entry[0].d_key;
	}

	// gcsb2GetMinKey: this is a testing function to verify that all the leaf nodes are
	// linked properly.
	int gcsb2GetMinKey(GCSBINODE64_2* root) {
		BPLNODE64* p;

		while (!IsLeaf(root))
			root=(GCSBINODE64_2*) root->d_2ndChild+root->d_num-root->d_1stNum-1;
		p=(BPLNODE64*)root;
		while (p->d_prev) {
			ASSERT(validPointer(p));
			p=p->d_prev;
		}
		return p->d_entry[0].d_key;
	}

	void getBPStatInfo(BPINODE64* root, int level) {
		int i;

		if (IsLeaf(root)) {
			g_stat_rec.d_NLNode++;
			g_stat_rec.d_NTotalSlots+=6;
			g_stat_rec.d_NUsedSlots+=((BPLNODE64*)root)->d_num;
			if (g_stat_rec.d_level==0)
				g_stat_rec.d_level=level;
			else
				ASSERT(level==g_stat_rec.d_level);
			g_stat_rec.d_leafSpace+=64;
		}
		else { //this is an internal node
			g_stat_rec.d_NINode++;
			g_stat_rec.d_NTotalSlots+=7;
			g_stat_rec.d_NUsedSlots+=NumKey(root);
			g_stat_rec.d_internalSpace+=64;
			for (i=0; i<=NumKey(root); i++)
				getBPStatInfo((BPINODE64*) root->d_entry[i].d_child, level+1);
		}
	}

	void getCSBStatInfo(CSBINODE64* root, int level) {
		int i;

		if (IsLeaf(root)) {
			g_stat_rec.d_NLNode++;
			g_stat_rec.d_NTotalSlots+=6;
			g_stat_rec.d_NUsedSlots+=((BPLNODE64*)root)->d_num;
			if (g_stat_rec.d_level==0)
				g_stat_rec.d_level=level;
			else
				ASSERT(level==g_stat_rec.d_level);
			g_stat_rec.d_leafSpace+=64;
		}
		else { //this is an internal node
			g_stat_rec.d_NINode++;
			g_stat_rec.d_NTotalSlots+=14;
			g_stat_rec.d_NUsedSlots+=root->d_num;
			g_stat_rec.d_internalSpace+=64;
#ifdef FULL_ALLOC
			if (IsLeaf(root->d_firstChild)) 
				g_stat_rec.d_leafSpace+=64*(15-root->d_num-1);
			else
				g_stat_rec.d_internalSpace+=64*(15-root->d_num-1);
#endif    
			for (i=0; i<=root->d_num; i++)
				getCSBStatInfo((CSBINODE64*) root->d_firstChild+i, level+1);
		}
	}

	void getGCSB2StatInfo(GCSBINODE64_2* root, int level) {
		int i;

		if (IsLeaf(root)) {
			g_stat_rec.d_NLNode++;
			g_stat_rec.d_NTotalSlots+=6;
			g_stat_rec.d_NUsedSlots+=((BPLNODE64*)root)->d_num;
			g_stat_rec.d_NUsedSlots+=((BPLNODE64*)root)->d_num;
			if (g_stat_rec.d_level==0)
				g_stat_rec.d_level=level;
			else
				ASSERT(level==g_stat_rec.d_level);
			g_stat_rec.d_leafSpace+=64;
		}
		else { //this is an internal node
			g_stat_rec.d_NINode++;
			g_stat_rec.d_NTotalSlots+=13;
			g_stat_rec.d_NUsedSlots+=root->d_num;
			g_stat_rec.d_internalSpace+=64;
			g_stat_rec.d_segRatio+=(root->d_1stNum+1>=root->d_num-root->d_1stNum)?
				(double)(root->d_1stNum+1)/(double)(root->d_num-root->d_1stNum) :
				(double)(root->d_num-root->d_1stNum)/(double)(root->d_1stNum+1);
			for (i=0; i<=root->d_1stNum; i++)
				getGCSB2StatInfo((GCSBINODE64_2*) root->d_firstChild+i, level+1);
			for (i=0; i<root->d_num-root->d_1stNum; i++)
				getGCSB2StatInfo((GCSBINODE64_2*) root->d_2ndChild+i, level+1);
		}
	}

	void getGCSB3StatInfo(GCSBINODE64_3* root, int level) {
		int i;

		if (IsLeaf(root)) {
			g_stat_rec.d_NLNode++;
			g_stat_rec.d_NTotalSlots+=6;
			g_stat_rec.d_NUsedSlots+=((BPLNODE64*)root)->d_num;
			g_stat_rec.d_NUsedSlots+=((BPLNODE64*)root)->d_num;
			if (g_stat_rec.d_level==0)
				g_stat_rec.d_level=level;
			else
				ASSERT(level==g_stat_rec.d_level);
			g_stat_rec.d_leafSpace+=64;
		}
		else { //this is an internal node
			g_stat_rec.d_NINode++;
			g_stat_rec.d_NTotalSlots+=12;
			g_stat_rec.d_NUsedSlots+=root->d_num;
			g_stat_rec.d_internalSpace+=64;
			/* g_stat_rec.d_segRatio+=(root->d_1stNum+1>=root->d_num-root->d_1stNum)?
			   (double)(root->d_1stNum+1)/(double)(root->d_num-root->d_1stNum) :
			   (double)(root->d_num-root->d_1stNum)/(double)(root->d_1stNum+1);
			   */
			for (i=0; i<=root->d_1stNum; i++)
				getGCSB3StatInfo((GCSBINODE64_3*) root->d_firstChild+i, level+1);
			for (i=0; i<root->d_2ndNum-root->d_1stNum; i++)
				getGCSB3StatInfo((GCSBINODE64_3*) root->d_2ndChild+i, level+1);
			for (i=0; i<root->d_num-root->d_2ndNum; i++)
				getGCSB3StatInfo((GCSBINODE64_3*) root->d_3rdChild+i, level+1);
		}
	}

	struct TestRec {
		int  d_type;     //insert 1, search 2, delete 3
		LPair d_entry;
	};

	int main(int argc, char* argv[]) {
		LPair *a1, *a2;
		TestRec* a3;
		int i, ntest, num, seed, iupper, lupper;
		int sea_count=0, del_count=0, nInsert=0, nSearch=0, nDelete=0;
		struct timeval tv,tv0;
		double tdiff;
		double pinsert, psearch, val;
		int value;
		int tempKey;
		void* tempChild;
		int nInternal, thres;
		int* css_dir;

		ASSERT(sizeof(BPLNODE64)==64);
		ASSERT(sizeof(BPINODE64)==64);
		ASSERT(sizeof(CSBINODE64)==64);
		ASSERT(sizeof(GCSBINODE64_2)==64);
		ASSERT(sizeof(GCSBINODE64_3)==64);
		if (argc!=10 && argc!=11) {
			cout<<"Usage: csb #element #tests pinsert psearch P|L|G2|G3|C uniform|random|skewed iupper lupper seed [i]"<<endl;
			cout<<"Usage:  0     1       2      3        4        5        6                     7     8      9   10"<<endl;
			cout<<"pinsert---percentage of insertion in the tests"<<endl;
			cout<<"psearch---percentage of search in the tests"<<endl;
			cout<<"C---CSS-Tree"<<endl;
			cout<<"P---B+ tree"<<endl;
			cout<<"L---CSB+-Tree"<<endl;
			cout<<"G2---segmented (2 segments) CSB+-Tree"<<endl;
			cout<<"G3---segmented (3 segments) CSB+-Tree"<<endl;
			cout<<"uniform--keys are from 1 to #elements"<<endl;
			cout<<"random--keys are randomly chosen from 1 to RANGE (10000000), so duplicates are possible"<<endl;
			cout<<"skewed--keys are skewly chosen from 1 to #elements"<<endl;
			cout<<"ordered--searching keys are sequential, keys are random"<<endl;
			cout<<"upper---maximum keys in each leaf node during bulk load"<<endl;
			cout<<"[i]--- if specified, bulkload half of the element and insert the other half."<<endl;
			/*********************Important notes********************************************/
			cout<<"iUpper has to be 6 or smaller for B+-Trees"<<endl;
			cout<<"iUpper has to be 13 or smaller for CSB+-Trees (each node can have 14 keys at most though)"<<endl;
			cout<<"iUpper has to be 12 or smaller for SCSB+-Trees with two segments"<<endl;
			cout<<"iUpper has to be 11 or smaller for SCSB+-Trees with three segments"<<endl;
			cout<<"lUpper has to be 6 or smaller"<<endl;
			/********************************************************************************/
			exit(1);
		}

		sscanf(argv[1],"%d",&num);
		sscanf(argv[2],"%d",&ntest);
		sscanf(argv[3],"%lf",&pinsert);
		sscanf(argv[4],"%lf",&psearch);
		sscanf(argv[7],"%d",&iupper);
		sscanf(argv[8],"%d",&lupper);
#ifndef PC
		init_memory(7000000);
#else
		init_memory(7000000);
#endif
		if (argv[5][0]=='C' || argv[5][0]=='c') {
			a1=(LPair*)mynew(sizeof(LPair)*num);
			syncMemory();
		}
		else {
			a1=new LPair[num];
		}

		sscanf(argv[9],"%d",&seed);
		srandom(seed);

		for (i=0; i<num; i++) {
			if (argv[6][0]=='u')
				a1[i].d_key=i;
			else if (argv[6][0]=='r')
				a1[i].d_key=random()%RANGE;
			else if (argv[6][0]=='s')
				a1[i].d_key=(int)(pow((double)i, 1.3)/10.0);
			//a1[i]=(int)(sqrt((double)i)*10.0);
			a1[i].d_tid=i+1;
		}

		if (argc==10) { // use all the elements for bulkload
			qsort(a1, num, sizeof(LPair), pairComp);
			if (argv[5][0]=='P' ||argv[5][0]=='p') 
				bpBulkLoad64(num, a1, iupper, lupper);
			else if (argv[5][0]=='L' ||argv[5][0]=='l') {
				csbBulkLoad64(num, a1, iupper, lupper);
				if (argv[5][1]=='T' ||argv[5][1]=='t')
					csbTileCopy(&g_csb_root64);
			}
			else if (!strcmp(argv[5], "G2") || !strcmp(argv[5], "g2")) 
				gcsbBulkLoad64_2(num, a1, iupper, lupper);
			else if (!strcmp(argv[5], "G3") || !strcmp(argv[5], "g3")) 
				gcsbBulkLoad64_3(num, a1, iupper, lupper);
			else if (argv[5][0]=='C' ||argv[5][0]=='c') 
				cssBulkLoad64(num, a1, &css_dir, &nInternal, &thres);
			else {
				cout<<"unsupported type of index"<<endl;
				exit(0);
			}
		}
		else { // use half of the elements for bulkload and the other half for insertion
			qsort(a1, num/2, sizeof(LPair), pairComp);
			if (argv[5][0]=='P' ||argv[5][0]=='p') {
				bpBulkLoad64(num/2, a1, iupper, lupper);
				for (i=num/2;i<num;i++) {
					bpInsert64(g_bp_root64, a1[i], &tempKey, &tempChild);
				}
			}
			else if (argv[5][0]=='L' ||argv[5][0]=='l') {
				csbBulkLoad64(num/2, a1, iupper, lupper);
#ifndef PRODUCT
#if FULL_ALLOC
				checkCSB64(g_csb_root64);
#endif
#endif
				for (i=num/2;i<num;i++) {
					csbInsert64(g_csb_root64, 0, 0, a1[i], &tempKey, &tempChild);
				}
			}
			else if (!strcmp(argv[5], "G2") || !strcmp(argv[5], "g2")) {
				gcsbBulkLoad64_2(num/2, a1, iupper, lupper);
				for (i=num/2;i<num;i++) {
					gcsbInsert64_2(g_gcsb_root64_2, 0, 0, 0, 0, 0, a1[i], &tempKey, &tempChild);
				}
			}
			else if (!strcmp(argv[5], "G3") || !strcmp(argv[5], "g3")) {
				gcsbBulkLoad64_3(num/2, a1, iupper, lupper);
				for (i=num/2;i<num;i++) {
					gcsbInsert64_3(g_gcsb_root64_3, 0, 0, 0, 0, 0, a1[i], &tempKey, &tempChild);
				}
			}
			else if (argv[5][0]=='C' ||argv[5][0]=='c') {
				cout<<"Can't insert into CSS-Trees"<<endl;
				exit(0);
			}
			else {
				cout<<"unsupported type of index"<<endl;
				exit(0);
			}
		}

#ifndef PRODUCT
#if FULL_ALLOC
		checkCSB64(g_csb_root64);
#endif
#endif

		a2=new LPair[ntest];
		a3=new TestRec[ntest];
		for (i=0;i<ntest;i++) {
			value=random()%1000;
			val=(double)value/1000.0;
			if (val < pinsert) { //generate an insertion
				a3[i].d_type=1;
				if (argv[6][0]=='u')
					a3[i].d_entry.d_key=num+nInsert;
				else if (argv[6][0]=='r') 
					a3[i].d_entry.d_key=random()%RANGE;
				a3[i].d_entry.d_tid=num+i+1;
				a2[nInsert]=a3[i].d_entry;
				nInsert++;
			}
			else if (val >= 1.0-psearch) { //generate a search
				a3[i].d_type=2;
				if (nSearch==0) 
					a3[i].d_entry.d_key=a1[num-1].d_key;
				else {
					value=random()%(num+nInsert);
					a3[i].d_entry.d_key=(value>=num)?a2[value-num].d_key : a1[value].d_key;
				}
				nSearch++;
			}
			else { //generate a deletion
				a3[i].d_type=3;
				value=random()%(num+nInsert);
				a3[i].d_entry.d_key=(value>=num)?a2[value-num].d_key : a1[value].d_key;
				a3[i].d_entry.d_tid=(value>=num)?a2[value-num].d_tid : a1[value].d_tid;
				nDelete++;
			}
		}

		if (!(argv[5][0]=='C' || argv[5][0]=='c'))
			delete [] a1;
		delete [] a2;

		//Let's start the real test now
#ifndef PRODUCT
		g_split=0;
		g_expand=0;
#endif
		gettimeofday(&tv0,0);
		if (argv[5][0]=='P' ||argv[5][0]=='p') {
			for (i=0;i<ntest;i++) {
				if (a3[i].d_type == 1)
					bpInsert64(g_bp_root64, a3[i].d_entry, &tempKey, &tempChild);
				else if (a3[i].d_type == 2) {
					sea_count+=(bpSearch64(g_bp_root64, a3[i].d_entry.d_key)>0 ? 1 : 0);
				}
				else 
					del_count+=bpDelete64(g_bp_root64, a3[i].d_entry);
			}
		}
		else if (argv[5][0]=='L' ||argv[5][0]=='l') {
			for (i=0;i<ntest;i++) {
				if (a3[i].d_type == 1)
					csbInsert64(g_csb_root64, 0, 0, a3[i].d_entry, &tempKey, &tempChild);
				else if (a3[i].d_type == 2) {
					sea_count+=(csbSearch64(g_csb_root64, a3[i].d_entry.d_key)>0 ? 1 : 0);
				}
				else 
					del_count+=csbDelete64(g_csb_root64, a3[i].d_entry);
			}
		}
		else if (!strcmp(argv[5], "G2") || !strcmp(argv[5], "g2")) {
			for (i=0;i<ntest;i++) {
				if (a3[i].d_type == 1)
					gcsbInsert64_2(g_gcsb_root64_2, 0, 0, 0, 0, 0, a3[i].d_entry, &tempKey, &tempChild);
				else if (a3[i].d_type == 2) { 
					sea_count+=(gcsbSearch64_2(g_gcsb_root64_2, a3[i].d_entry.d_key)>0 ? 1 : 0);
				}
				else {
					del_count+=gcsbDelete64_2(g_gcsb_root64_2, a3[i].d_entry);
				}
			}
		} 
		else if (!strcmp(argv[5], "G3") || !strcmp(argv[5], "g3")) {
			for (i=0;i<ntest;i++) {
				if (a3[i].d_type == 1) {
					gcsbInsert64_3(g_gcsb_root64_3, 0, 0, 0, 0, 0, a3[i].d_entry, &tempKey, &tempChild);
				}
				else if (a3[i].d_type == 2) { 
					sea_count+=(gcsbSearch64_3(g_gcsb_root64_3, a3[i].d_entry.d_key)>0 ? 1 : 0);
				}
				else {
					del_count+=gcsbDelete64_3(g_gcsb_root64_3, a3[i].d_entry);
				}
			}
		} 
		else if (argv[5][0]=='C' ||argv[5][0]=='c') {
			for (i=0;i<ntest;i++) {
				if (a3[i].d_type == 1)
					assert(0);
				else if (a3[i].d_type == 2) { 
					sea_count+=(cssSearch64(a1, num, a3[i].d_entry.d_key, css_dir, nInternal, thres)>0 ? 1 : 0);
				}
				else {
					assert(0);
				}
			}
		}
		gettimeofday(&tv,0);

		tdiff = (tv.tv_sec-tv0.tv_sec)+(tv.tv_usec-tv0.tv_usec)*0.000001;
#ifndef SPACE
		cout <<num<<"\t"<<tdiff<<"\t"<<nInsert<<"\t"<<nDelete<<"\t"<<nSearch<<"\t"<<psearch<<"\t"<<sea_count<<"\t"<<del_count<<endl;
#else
#ifdef PRODUCT
		if (argv[5][0]=='P' ||argv[5][0]=='p') 
			getBPStatInfo(g_bp_root64,0);
		else if (argv[5][0]=='L' ||argv[5][0]=='l') 
			getCSBStatInfo(g_csb_root64,0);
		else if (!strcmp(argv[5],"G2") || !strcmp(argv[5],"g2")) 
			getGCSB2StatInfo(g_gcsb_root64_2,0);
		else if (!strcmp(argv[5],"G3") || !strcmp(argv[5],"g3")) 
			getGCSB3StatInfo(g_gcsb_root64_3,0);
		cout <<g_stat_rec.d_internalSpace<<"\t"<<g_stat_rec.d_leafSpace<<"\t"<<g_stat_rec.d_leafSpace+g_stat_rec.d_internalSpace<<endl;
#endif
#endif

#ifndef PRODUCT
		if (argv[5][0]=='P' ||argv[5][0]=='p') 
			getBPStatInfo(g_bp_root64,0);
		else if (argv[5][0]=='L' ||argv[5][0]=='l') 
			getCSBStatInfo(g_csb_root64,0);
		else if (!strcmp(argv[5],"G2") || !strcmp(argv[5],"g2")) 
			getGCSB2StatInfo(g_gcsb_root64_2,0);
		cout <<"NumInsert:"<<"\t"<<nInsert<<"\t"<<"NumSearch:"<<"\t"<<nSearch<<"\t"<<"NumDelete:"<<"\t"<<nDelete<<endl;
		cout <<"NumInternalNode: "<<"\t"<<g_stat_rec.d_NINode<<endl;
		cout <<"NumLeafNode: "<<"\t"<<g_stat_rec.d_NLNode<<endl; 
		cout <<"NumTotalSlots: "<<"\t"<<g_stat_rec.d_NTotalSlots<<endl;
		cout <<"NumUsedSlots: "<<"\t"<<g_stat_rec.d_NUsedSlots<<endl;
		cout <<"Occupancy: "<<"\t"<<(double)g_stat_rec.d_NUsedSlots/(double)g_stat_rec.d_NTotalSlots<<endl;
		cout <<"NumLevels: "<<"\t"<<g_stat_rec.d_level<<endl;
		cout <<"NumExpands: "<<"\t"<<g_expand<<endl;
		cout <<"NumSplits: "<<"\t"<<g_split<<endl;
		if (argv[5][0]=='G' || argv[5][0]=='g')
			cout <<"SegRatio: "<<"\t"<<g_stat_rec.d_segRatio/(double)g_stat_rec.d_NINode<<endl;
		cout <<"SpaceUsed: "<<"\t"<<g_space_used<<endl;
		if (argv[5][0]=='P' ||argv[5][0]=='p')
			cout <<"MinKey:"<<"\t"<<bpGetMinKey(g_bp_root64)<<"\tMaxKey:"<<"\t"<<bpGetMaxKey(g_bp_root64)<<endl;
		else if (argv[5][0]=='L' ||argv[5][0]=='l') 
			cout <<"MinKey:"<<"\t"<<csbGetMinKey(g_csb_root64)<<"\tMaxKey:"<<"\t"<<csbGetMaxKey(g_csb_root64)<<endl;
		else if (!strcmp(argv[5],"G2") || !strcmp(argv[5],"g2")) 
			cout <<"MinKey:"<<"\t"<<gcsb2GetMinKey(g_gcsb_root64_2)<<"\tMaxKey:"<<"\t"<<csbGetMaxKey((CSBINODE64*)g_gcsb_root64_2)<<endl;
#endif
	}


