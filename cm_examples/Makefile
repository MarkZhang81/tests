CC := gcc
LD := gcc
CFLAGS := -Wall -g

LIBS := -lrdmacm -libverbs -lmlx5 -lpthread
HEADERS := params.h helper.h

all: client server client_resolve_ib_service resolve_dns write_event getaddrinfo_ai getaddrinfo_ai_2

client: client.o
	$(LD) $(LD_FLAGS) -o $@ $^ $(LIBS)

server: server.o
	$(LD) $(LD_FLAGS) -o $@ $^ $(LIBS)

client_resolve_ib_service: client_resolve_ib_service.o
	$(LD) $(LD_FLAGS) -o $@ $^ $(LIBS)

resolve_dns: resolve_dns.o
	$(LD) $(LD_FLAGS) -o $@ $^ $(LIBS)

write_event: write_event.o
	$(LD) $(LD_FLAGS) -o $@ $^ $(LIBS)

getaddrinfo_ai: getaddrinfo_ai.o
	$(LD) $(LD_FLAGS) -o $@ $^ $(LIBS)

getaddrinfo_ai_2: getaddrinfo_ai_2.o
	$(LD) $(LD_FLAGS) -o $@ $^ $(LIBS)

%.o: %.c $(HEADERS) Makefile
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o client server client_resolve_ib_service resolve_dns write_event getaddrinfo_ai 2>/dev/null
