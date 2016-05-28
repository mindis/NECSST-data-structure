BIN_DIR=./
CFLAGS= -c 

all:
	gcc $(CFLAGS) wbtree.c -lrt
	gcc $(CFLAGS) main.c -lrt
	gcc -o $(BIN_DIR)wbtree wbtree.o main.o -lrt
	rm *.o
