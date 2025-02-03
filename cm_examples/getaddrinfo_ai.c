#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <infiniband/ib.h>
#include <rdma/rdma_cma.h>

#include "helper.h"
#include "params.h"

void do_getaddrinfo_ai_test(void)
{
	struct rdma_addrinfo *rai = NULL, hints = {};
	char *service = CM_EXAMPLE_IB_SERVICE_ID;
	int ret, n = 0;

	INFO("getaddrinfo(RAI_SA) node = NULL, service = \"%s\", ", service);
	hints.ai_flags = RAI_SA;
	ret = rdma_getaddrinfo(NULL, service, &hints, &rai);
	if (ret) {
		perror("rdma_getaddrinfo");
		return;
	}

	do {
		dump_addrinfo(rai, n);
		rai = rai->ai_next;
		n++;
	} while (rai);

	rdma_freeaddrinfo(rai);
}

int main(int argc, char *argv[])
{
	do_getaddrinfo_ai_test();
	return 0;
}
