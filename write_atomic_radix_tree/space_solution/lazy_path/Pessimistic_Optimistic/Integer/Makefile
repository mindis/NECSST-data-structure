BIN_DIR = ./
CFLAGS = -c

all:
	gcc $(CFLAGS) art_lp.c -lrt -O3
	gcc $(CFLAGS) main.c -lrt -O3
	gcc -o $(BIN_DIR)ART_LP_synthetic_1024M_noflush art_lp.o main.o -lrt -O3
	rm *.o
