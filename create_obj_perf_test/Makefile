CC := gcc
LD := gcc
CFLAGS := -Wall -g

LIBS := -lrdmacm -libverbs -lmlx5 -lpthread

all: create_obj_perf_test

create_obj_perf_test: create_obj_perf_test.o
	$(LD) $(LD_FLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $^

clean:
	rm -f *.o create_obj_perf_test
