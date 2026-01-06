CC := gcc
LD := gcc
CFLAGS := -Wall -g

LIBS := -lrdmacm -libverbs -lmlx5 -lpthread

all: reg_mr_test

reg_mr_test: reg_mr_test.o
	$(LD) $(LD_FLAGS) -o $@ $^ $(LIBS)

%.o: %.c $(HEADERS) Makefile
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o reg_mr_test 2>/dev/null
