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
void do_write_event_test(void)
{
	uint64_t arg = 0x55667788aabbccdd;
	int status = 0x11223344;
        struct rdma_cm_event *e;
	int ret;

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

	ret = rdma_write_cm_event(cm_id, RDMA_CM_EVENT_USER, status, arg);
	if (ret) {
		perror("rdma_write_cm_event");
		goto out_event;
	}

	ret = rdma_get_cm_event(ech, &e);
	if (ret) {
		perror("rdma_get_cm_event");
		goto out_event;
	}

	INFO("event/status/arg: Expected 0x%x 0x%x 0x%lx, Get 0x%x 0x%x 0x%lx",
	     RDMA_CM_EVENT_USER, status, arg, e->event, e->status, e->param.arg);

	ret = rdma_ack_cm_event(e);
	if (ret)
		perror("rdma_ack_cm_event");

out_event:
	rdma_destroy_id(cm_id);
out_create_id:
	rdma_destroy_event_channel(ech);
	return;
}

int main(int argc, char *argv[])
{
	do_write_event_test();
	return 0;
}
