CC=/opt/SUNWspro/bin/CC
GCC=g++
CFLAG=-fast -xO5 -DPRODUCT
CFLAG_FIX=-fast -xO5 -DPRODUCT -DFIX_HARDCODE
CFLAG_VAR=-fast -xO5 -DPRODUCT -DVAR_HARDCODE
CFLAG_VAR_GCC= -O -DPRODUCT -DVAR_HARDCODE -DGCC
CFLAG_space=-fast -xO5 -DPRODUCT -DFIX_HARDCODE -DSPACE 
CFLAG_FULL=-fast -xO5 -DPRODUCT -DFIX_HARDCODE -DFULL_ALLOC 
CFLAG_FULL_space=-fast -xO5 -DPRODUCT -DFIX_HARDCODE -DFULL_ALLOC -DSPACE
CFLAG_g=-g
CFLAG_FIX_g=-g -DFIX_HARDCODE 
CFLAG_VAR_g=-g -DVAR_HARDCODE 
CFLAG_FULL_g=-g -DFIX_HARDCODE -DFULL_ALLOC 

all: csb csb_fix csb_var csb_var_gcc csb_full 

csb: csb.c
	$(CC) $(CFLAG) -o csb csb.c

csb_fix: csb.c
	$(CC) $(CFLAG_FIX) -o csb_fix csb.c

csb_var: csb.c
	$(CC) $(CFLAG_VAR) -o csb_var csb.c

csb_var_gcc: csb.c
	$(GCC) $(CFLAG_VAR_GCC) -o csb_var_gcc csb.c

csb_space: csb.c
	$(CC) $(CFLAG_space) -o csb_space csb.c

csb_full: csb.c
	$(CC) $(CFLAG_FULL) -o csb_full csb.c

csb_full_space: csb.c
	$(CC) $(CFLAG_FULL_space) -o csb_full_space csb.c

csb_g: csb.c
	$(CC) $(CFLAG_g) -o csb_g csb.c

csb_fix_g: csb.c
	$(CC) $(CFLAG_FIX_g) -o csb_fix_g csb.c

csb_var_g: csb.c
	$(CC) $(CFLAG_VAR_g) -o csb_var_g csb.c

csb_full_g: csb.c
	$(CC) $(CFLAG_FULL_g) -o csb_full_g csb.c

csb.s: csb.c
	$(CC) $(CFLAG) -S -o csb.s csb.c

clean:
	rm csb csb_g csb_fix csb_fix_g *.o
