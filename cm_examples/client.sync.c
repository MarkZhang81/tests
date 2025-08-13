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

#define CLIENT_RESOLVE_IB_SERVICE_VERSION "0.1"

static struct rdma_event_channel *ech;
static struct rdma_cm_id *cm_id;

static struct ibv_pd *pd;
static struct ibv_cq *cq;
static struct ibv_mr *mr;
static char buf[BUFSIZE];

static int start_cm_client_sync(void)
{
	struct rdma_addrinfo hints = {}, *rai = NULL;
	struct sockaddr_ib sib = {};
	struct in6_addr a6;
	int ret, n = 0;

	ret = rdma_create_id(NULL, &cm_id, NULL, RDMA_PS_IB);
	if (ret) {
		perror("rdma_create_id sync mode");
		return ret;
	}

	ret = inet_pton(AF_INET6, CM_EXAMPLE_CLIENT_GID, &a6);
	if (ret != 1) {
		ERR("inet_pton failed, err %d", ret);
		return ret;
	}

	sib.sib_family = AF_IB;
	ib_addr_set(&sib.sib_addr, a6.s6_addr32[0], a6.s6_addr32[1], a6.s6_addr32[2], a6.s6_addr32[3]);
	dump_sockaddr_ib("src_addr", &sib);
	ret = rdma_bind_addr(cm_id, (struct sockaddr *)&sib);
	if (ret) {
		perror("rdma_bind_addr");
		return ret;
	}
	INFO("bind_addr(%s) done", CM_EXAMPLE_CLIENT_GID);

	hints.ai_flags = RAI_SA;
	ret = rdma_resolve_addrinfo(cm_id, NULL, CM_EXAMPLE_IB_SERVICE_ID, &hints);
	if (ret) {
		perror("rdma_resolve_addrinfo");
		return ret;
	}

	INFO("rdma_resolve_addrinfo done");

	ret = rdma_query_addrinfo(cm_id, &rai);
	if (ret || !rai) {
		ERR("rdma_query_addrinfo, err %d info %p", ret, rai);
		return ret;
	}

	do {
		dump_addrinfo(rai, n);
		rai = rai->ai_next;
		n++;
	} while (rai);

	rdma_freeaddrinfo(rai);

	return 0;
}

int main(int argc, char *argv[])
{
	INFO("rdma_resolve_addrinfo sync mode test...");
	start_cm_client_sync();
	return 0;
}
