#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <rdma/rdma_cma.h>

#include "helper.h"
#include "params.h"

static struct rdma_event_channel *ech;

static struct rdma_cm_id *listen_id, *client_id;
static struct ibv_pd *pd;
static struct ibv_mr *mr;
static struct ibv_cq *cq;

static char buf[BUFSIZE];

static int server_port_space = RDMA_PS_TCP;

static int setup_qp_server(struct rdma_cm_id *cid)
{
	int ret;

	pd = ibv_alloc_pd(cid->verbs);
	if (!pd) {
		perror("ibv_alloc_pd");
		return -1;
	}

	mr = ibv_reg_mr(pd, buf, BUFSIZE, IBV_ACCESS_LOCAL_WRITE);
	if (!mr) {
		perror("ibv_reg_mr");
		return -1;
	}

	cq = ibv_create_cq(cid->verbs, 32, NULL, NULL, 0);
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

	ret = rdma_create_qp(cid, pd, &init_attr);
	if (ret) {
		perror("rdma_create_qp");
		return -1;
	}

	return 0;
}

static int server_init(void)
{
	struct rdma_addrinfo hints = {}, *rai = NULL;
	struct rdma_conn_param param = {};
	struct sockaddr_in sin = {};
	struct rdma_cm_event *e;
	struct sockaddr *sa;
	int err;

	if (server_port_space == RDMA_PS_IB) {
		hints.ai_flags = RAI_NUMERICHOST | RAI_FAMILY | RAI_PASSIVE;
		hints.ai_family = AF_IB;
		hints.ai_port_space = RDMA_PS_IB;
		err = rdma_getaddrinfo(NULL, CM_EXAMPLE_SERVER_PORT_STR, &hints, &rai);
		if (err) {
			perror("rdma_getaddrinfo");
			return err;
		}
		sa = rai->ai_src_addr;
		dump_sockaddr_ib("src_addr", (struct sockaddr_ib *)rai->ai_src_addr);

	//r = rdma_getaddrinfo(NULL, "7471", &hints, &rai);
	} else if (server_port_space == RDMA_PS_TCP) {
		sin.sin_family = AF_INET;
		sin.sin_port = htons(CM_EXAMPLE_SERVER_PORT);
		sin.sin_addr.s_addr = htonl(INADDR_ANY);
		sa = (struct sockaddr *)&sin;
	} else {
		ERR("Unsupported port_space %d", server_port_space);
		return -1;
	}

	ech = rdma_create_event_channel();
	if (ech == NULL) {
		perror("rdma_create_event_channel");
		return errno;
	}

	err = rdma_create_id(ech, &listen_id, NULL, server_port_space);
	if (err) {
		perror("rdma_create_id");
		return err;
	}

	err = rdma_bind_addr(listen_id, sa);
        if (err) {
		perror("rdma_bind_addr");
		return err;
	}
	INFO("bound done, port space 0x%x port %s (in resolve ib service tests this port must be low 16-bit of the registered serviceID)",
	     server_port_space, CM_EXAMPLE_SERVER_PORT_STR);
	if (rai)
		rdma_freeaddrinfo(rai);

	err = rdma_listen(listen_id, 10);
        if (err) {
		perror("rdma_listen");
		return err;
	}

	err = rdma_get_cm_event(ech, &e);
	if (err || (e->event != RDMA_CM_EVENT_CONNECT_REQUEST)) {
		ERR("Expects RDMA_CM_EVENT_CONNECT_REQUEST get %d(%s): %d",
		    e->event, rdma_event_str(e->event), err);
		return -1;
	}
	client_id = e->id;

	err = rdma_ack_cm_event(e);
	if (err) {
		perror("rdma_ack_cm_event");
		return err;
	}
	INFO("Get an incoming request");

	err = setup_qp_server(client_id);
	if (err)
		return err;
	INFO("setup_qp_server done");

	err = rdma_accept(client_id, &param);
	if (err) {
		perror("rdma_accept");
		return err;
	}
	err = rdma_get_cm_event(ech, &e);
	if (err || (e->event != RDMA_CM_EVENT_ESTABLISHED)) {
		ERR("Expects RDMA_CM_EVENT_ESTABLISHED get %d(%s): %d",
		    e->event, rdma_event_str(e->event), err);
		return -1;
	}
	err = rdma_ack_cm_event(e);
	if (err) {
		perror("rdma_ack_cm_event");
		return err;
	}

	INFO("Server connection established");
	return 0;
}

#if 0
#define DATA_PER_LINE 16
static char dbuf[256] = {};
static void print_line(int line_id, const char *addr, int len, int unit)
{
	int i;

	sprintf(dbuf, "%04x:", line_id * DATA_PER_LINE);
	for (i = 0; i < len; i++)
		sprintf(dbuf + strlen(dbuf), " %02x", addr[i]);

	sprintf(dbuf + strlen(dbuf), "\n");
	printf("%s", dbuf);
}

static void DUMP(const char *info, const char *addr, int len)
{
	int pos = 0, left = len, line_id = 0;
	int unit = 1;

	if (info)
		printf("====> %s\n", info);
	else
		printf("====>\n");
	do {
		if (left >= DATA_PER_LINE) {
			print_line(line_id, addr + pos, DATA_PER_LINE, unit);
			line_id++;
			pos += DATA_PER_LINE;
			left -= DATA_PER_LINE;
		} else {
			print_line(line_id, addr + pos, left, unit);
			break;
		}
	} while (left);
	printf("<====\n");
}
#endif

static void recv_data(struct rdma_cm_id *cid)
{
	struct ibv_sge sg_list = {
		.addr   = (uintptr_t)buf,
		.length = BUFSIZE,
		.lkey   = mr->lkey,
	};
	struct ibv_recv_wr wr = {
		.wr_id   = 12345,
		.next    = NULL,
		.sg_list = &sg_list,
		.num_sge = 1,
	}, *bad_wr;
	struct ibv_wc wc;
        int num_comp, ret;

	ret = ibv_post_recv(cid->qp, &wr, &bad_wr);
	if (ret) {
		perror("ibv_post_recv");
		return;
	}

	/*
	static struct ibv_port_attr port_attr = {};
	ret = ibv_query_port(cid->verbs, cid->port_num, &port_attr);
	if (ret) {
		ERR("ibv_query_port failed");
		return;
	}
	*/

	INFO("Now waiting for dataexpecting wr_id = %ld...", wr.wr_id);
	do {
		num_comp = ibv_poll_cq(cq, 1, &wc);
		//num_comp = ibv_exp_poll_cq(cq, 1, &wc, sizeof(wc));
	} while (num_comp == 0);

	if (num_comp < 0) {
		perror("ibv_poll_cq");
		return;
	}
	INFO("ibv_poll_cq() returns %d wr_id %d\n", num_comp, (int)wr.wr_id);
	if (wc.status != IBV_WC_SUCCESS) {
		ERR("Failed status %s(%x) for wr_id %d\n",
		    ibv_wc_status_str(wc.status), wc.status, (int)wc.wr_id);
		return;
	}
	INFO("String received: `%s'", buf);
}

static void server_uninit(void)
{
	rdma_disconnect(client_id);
	rdma_destroy_qp(client_id);
	ibv_destroy_cq(cq);
	ibv_dereg_mr(mr);
	ibv_dealloc_pd(pd);
	rdma_destroy_id(client_id);
	rdma_destroy_id(listen_id);
}

static int parse_opt(int argc, char *argv[])
{
	int op;

	while ((op = getopt(argc, argv, "P:")) != -1) {
		switch (op) {
		case 'P':
			if (!strncasecmp("ib", optarg, 2)) {
				server_port_space = RDMA_PS_IB;
				INFO("Set port space to RDMA_PS_IB");
			} else if (strncasecmp("tcp", optarg, 3)) {
				fprintf(stderr, "Warning: unknown port space format: %s\n", optarg);
				return -EINVAL;
			}
			break;
		default:
			dump("Usage: server [-P <port_space>]\n");
			dump("Examples: server -P ib\n");
			return -EINVAL;
		}
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int ret;

	ret = parse_opt(argc, argv);
	if (ret)
		return ret;

	ret = server_init();
	if (ret)
		return ret;

	recv_data(client_id);

	sleep(2);
	server_uninit();
	return 0;
}
