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

static struct rdma_event_channel *ech;
static struct rdma_cm_id *cm_id;

void do_resolve_dns_test(void)
{
	char *node = "www.nvidia.com", *service = "http";
	struct rdma_addrinfo *rai = NULL;
        struct rdma_cm_event *e;
	int ret, n = 0;

	INFO("Resolve node = \"%s\", service = \"%s\", ", node, service);
	ech = rdma_create_event_channel();
	if (ech == NULL) {
		perror("rdma_create_event_channel");
		return;
	}

	ret = rdma_create_id(ech, &cm_id, NULL, RDMA_PS_IB);
	if (ret) {
		perror("rdma_create_id");
		goto out_create_id;
	}

	ret = rdma_resolve_addrinfo(cm_id, node, service, NULL);
	if (ret) {
		perror("rdma_resolve_addrinfo");
		goto out_event;
	}


	ret = rdma_get_cm_event(ech, &e);
	if (ret) {
		perror("rdma_get_cm_event");
		goto out_event;
	}

	INFO("event/status/arg: Expected(event status arg) 0x%x 0 0, Get 0x%x 0x%x 0x%lx",
	     RDMA_CM_EVENT_ADDRINFO_RESOLVED, e->event, e->status, e->param.arg);

	ret = rdma_ack_cm_event(e);
	if (ret) {
		perror("rdma_ack_cm_event");
		goto out_event;
	}

	if (e->event != RDMA_CM_EVENT_ADDRINFO_RESOLVED) {
		ERR("Unexpected event received.\n");
		goto out_event;
	}

	ret = rdma_query_addrinfo(cm_id, &rai);
	if (ret) {
		perror("rdma_query_addrinfo");
		goto out_event;
	}

	do {
		dump_addrinfo(rai, n);
		rai = rai->ai_next;
		n++;
	} while (rai);

	rdma_freeaddrinfo(rai);

out_event:
	rdma_destroy_id(cm_id);
out_create_id:
	rdma_destroy_event_channel(ech);
	return;
}

int main(int argc, char *argv[])
{
	do_resolve_dns_test();
	return 0;
}
