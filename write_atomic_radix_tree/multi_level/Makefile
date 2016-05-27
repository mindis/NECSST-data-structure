BIN_DIR = ./
CFLAGS = -c

all:
	gcc $(CFLAGS) wm_radix_tree.c -lrt
	gcc $(CFLAGS) main.c -lrt
	gcc -o $(BIN_DIR)wm_radix_tree wm_radix_tree.o main.o -lrt
	rm *.o
