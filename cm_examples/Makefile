CC := gcc
LD := gcc
CFLAGS := -Wall -g

LIBS := -lrdmacm -libverbs -lmlx5 -lpthread

all: client

client: client.o
	$(LD) $(LD_FLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $^

clean:
	rm -f *.o client
