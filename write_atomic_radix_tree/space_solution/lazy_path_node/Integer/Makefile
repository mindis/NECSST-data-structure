BIN_DIR = ./
CFLAGS = -c

all:
	gcc $(CFLAGS) art_lpn.c -lrt -O3
	gcc $(CFLAGS) main.c -lrt -O3
	gcc -o $(BIN_DIR)ART_LPN_synthetic_1024M_noflush art_lpn.o main.o -lrt -O3
	rm *.o
