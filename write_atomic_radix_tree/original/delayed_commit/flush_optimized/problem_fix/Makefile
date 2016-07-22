BIN_DIR = ./
CFLAGS = -c

all:
	gcc $(CFLAGS) w_radix_tree.c -lrt
	gcc $(CFLAGS) main.c -lrt
	gcc -o $(BIN_DIR)w_radix_tree w_radix_tree.o main.o -lrt
	rm *.o
