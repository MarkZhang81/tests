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

static struct ibv_pd *pd;
static struct ibv_cq *cq;
static struct ibv_mr *mr;
static char buf[BUFSIZE];

static int setup_qp_client(void)
{
	int ret;

	pd = ibv_alloc_pd(cm_id->verbs);
	if (!pd) {
		perror("ibv_alloc_pd");
		return -1;
	}

	mr = ibv_reg_mr(pd, buf, BUFSIZE, IBV_ACCESS_LOCAL_WRITE);
	if (!mr) {
		perror("ibv_reg_mr");
		return -1;
	}

	cq = ibv_create_cq(cm_id->verbs, 32, NULL, NULL, 0);
	if (!cq) {
		perror("ibv_create_cq");
		return -1;
	}

	struct ibv_qp_init_attr init_attr = {
		.cap = {
			.max_send_wr = 32,
			.max_recv_wr = 32,
			.max_send_sge = 1,
			.max_recv_sge = 1,
			.max_inline_data = 64,
		},
		.qp_type = IBV_QPT_RC,
		.send_cq = cq,
		.recv_cq = cq,
	};

	ret = rdma_create_qp(cm_id, pd, &init_attr);
	if (ret) {
		perror("rdma_create_qp");
		return -1;
	}

	return 0;
}

static void dump_addrinfo(struct rdma_addrinfo *ai, int n)
{
	struct sockaddr_ib *sib = (struct sockaddr_ib *)ai->ai_dst_addr;

        dump("addrinfo[%d]:\n", n);
        dump("  ai_flags: 0x%x\n", ai->ai_flags);
        dump("  ai_family: 0x%x\n", ai->ai_family);
        dump("  ai_port_space: 0x%x\n", ai->ai_port_space);
        dump("  ai_dst_len: %d\n", ai->ai_dst_len);
        dump("  ai_dst_addr.sib_pkey: 0x%x(be16)\n", sib->sib_pkey);
        dump("  ai_dst_addr.sib_addr: 0x%04x:0x%04x:0x%04x:0x%04x:0x%04x:0x%04x:0x%04x:0x%04x\n",
	     htobe16(sib->sib_addr.sib_addr16[0]), htobe16(sib->sib_addr.sib_addr16[1]),
	     htobe16(sib->sib_addr.sib_addr16[2]), htobe16(sib->sib_addr.sib_addr16[3]),
	     htobe16(sib->sib_addr.sib_addr16[4]), htobe16(sib->sib_addr.sib_addr16[5]),
	     htobe16(sib->sib_addr.sib_addr16[6]), htobe16(sib->sib_addr.sib_addr16[7]));
}
static int start_cm_client(void)
{
	struct rdma_addrinfo hints = {}, *rai = NULL;
	struct rdma_conn_param param = {};
        struct rdma_cm_event *e;
	int err, n = 0;

	INFO("Start client with IB resolve_addrinfo..");
	ech = rdma_create_event_channel();
	if (ech == NULL) {
		perror("rdma_create_event_channel");
		return -1;
	}

	err = rdma_create_id(ech, &cm_id, NULL, RDMA_PS_IB);
	if (err) {
		perror("rdma_create_id");
		return err;
	}

#if 0
	struct sockaddr_in sin = {};

	/* bind_addr() before resolve_addrinfo() */
	sin.sin_family = AF_INET;
	sin.sin_port   = htons(CM_EXAMPLE_SERVER_PORT);
	err = inet_pton(AF_INET, CM_EXAMPLE_LOCAL_IP, &sin.sin_addr);
	if (err != 1) {
		perror("inet_pton");
		return err;
	}
	err = rdma_bind_addr(cm_id, (struct sockaddr *)&sin);
#endif
	hints.ai_flags = RAI_NUMERICHOST | RAI_FAMILY | RAI_PASSIVE;
	hints.ai_family = AF_IB;
	hints.ai_port_space = RDMA_PS_IB;
	err = rdma_getaddrinfo(CM_EXAMPLE_CLIENT_GID,
			       CM_EXAMPLE_SERVER_PORT_STR, &hints, &rai);
	if (err) {
		perror("rdma_getaddrinfo");
		return err;
	}

	err = rdma_bind_addr(cm_id, rai->ai_src_addr);
        if (err) {
		perror("rdma_bind_addr");
		return err;
	}
	INFO("bind_addr(%s) done", CM_EXAMPLE_CLIENT_GID);
	rdma_freeaddrinfo(rai);
	rai = NULL;

	hints.ai_flags = RAI_SA;
	err = rdma_resolve_addrinfo(cm_id, NULL,
				    CM_EXAMPLE_IB_SERVICE_ID, &hints);
	if (err) {
		perror("rdma_resolve_addrinfo");
		return err;
	}

	err = rdma_get_cm_event(ech, &e);
	if (err || (e->event != RDMA_CM_EVENT_ADDRINFO_RESOLVED)) {
		ERR("Expects RDMA_CM_EVENT_ADDRINFO_RESOLVED get %d(%s) status %d, err %d",
		    e->event, rdma_event_str(e->event), e->status, err);
		return -1;
	}
	err = rdma_ack_cm_event(e);
	if (err) {
		perror("rdma_ack_cm_event");
		return err;
	}
	INFO("rdma_resolve_addrinfo done");

	err = rdma_query_addrinfo(cm_id, &rai);
	if (err || !rai) {
		ERR("rdma_query_addrinfo, err %d info %p", err, rai);
		return err;
	}

	do {
		dump_addrinfo(rai, n);
		rai = rai->ai_next;
		n++;
	} while (rai);

	err = rdma_resolve_route(cm_id, 5000);
	if (err) {
		perror("rdma_resolve_route");
		return err;
	}

	err = rdma_get_cm_event(ech, &e);
	if (err || (e->event != RDMA_CM_EVENT_ROUTE_RESOLVED)) {
		ERR("Expects RDMA_CM_EVENT_ROUTE_RESOLVED get %d(%s) status %d, err %d",
		    e->event, rdma_event_str(e->event), e->status, err);
		return -1;
	}
	err = rdma_ack_cm_event(e);
	if (err) {
		perror("rdma_ack_cm_event");
		return err;
	}
	INFO("rdma_resolve_route done");

	err = setup_qp_client();
	if (err)
		return err;
	INFO("setup qp done");

	err = rdma_connect(cm_id, &param);
	if (err) {
		perror("rdma_connect");
		return err;
	}

	err = rdma_get_cm_event(ech, &e);
	if (err || (e->event != RDMA_CM_EVENT_ESTABLISHED)) {
		ERR("Expects RDMA_CM_EVENT_ESTABLISHED get %d(%s) status %d, err %d",
		    e->event, rdma_event_str(e->event), e->status, err);
		return -1;
	}
	err = rdma_ack_cm_event(e);
	if (err) {
		perror("rdma_ack_cm_event");
		return err;
	}
	INFO("Connection established");

	return 0;
}

static void send_data(void)
{

	struct ibv_sge sg_list = {
		.addr   = (uintptr_t)buf,
		.length = 0,
		.lkey   = mr->lkey,
	};
	struct ibv_send_wr wr = {
		.next       = NULL,
		.sg_list    = &sg_list,
		.num_sge    = 1,
		.opcode     = IBV_WR_SEND,
		.send_flags = IBV_SEND_SIGNALED, /* XXX */
	}, *bad_wr;
	int err, size, msgid = 0, num_comp;
	struct ibv_wc wc;

	do {
		INFO("Sleep 2 seconds to wait server to prepare..");
		sleep(2);
		snprintf(buf, BUFSIZE, "(%d)A message from client %s",
			 msgid++, CM_EXAMPLE_CLIENT_GID);
		size = strlen(buf);
		sg_list.length = size;

		err = ibv_post_send(cm_id->qp, &wr, &bad_wr);
		if (err) {
			perror("ibv_post_send");
			return;
		}
		INFO("Sent(size %d): `%s'", size, buf);

		do {
			num_comp = ibv_poll_cq(cq, 1, &wc);
		} while (num_comp == 0);

		if (num_comp < 0) {
			perror("ibv_poll_cq");
			return;
		}

		if (wc.status != IBV_WC_SUCCESS) {
			INFO("Failed status %s(%x) for wr_id %d\n",
			    ibv_wc_status_str(wc.status), wc.status, (int)wc.wr_id);
			return;
		}
	} while (0);
}

int main(int argc, char *argv[])
{
	int ret;
	ret = start_cm_client();
	if (ret)
		return ret;

	send_data();

	rdma_disconnect(cm_id);
	rdma_destroy_qp(cm_id);
	ibv_destroy_cq(cq);
	ibv_dereg_mr(mr);
	ibv_dealloc_pd(pd);
	rdma_destroy_id(cm_id);

	return 0;
}
