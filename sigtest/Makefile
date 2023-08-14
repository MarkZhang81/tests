CC := gcc
LD := gcc
CFLAGS := -Wall -g

LIBS := -lrdmacm -libverbs -lmlx5

all: sigtest

sigtest: main.o sig_test.o
	$(LD) $(LD_FLAGS) -o $@ $^ $(LIBS)

%.o: %.c sig_test.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o sigtest
