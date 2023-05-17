/* SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) OR BSD-2-Clause) */
/*
 * Copyright (c) 2023 NVIDIA CORPORATION. All rights reserved
 */
#include <arpa/inet.h>
#include <getopt.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <rdma/rdma_cma.h>
#include <infiniband/mlx5dv.h>

#include "sig_test.h"

enum {
	ROLE_NONE,
	ROLE_SERVER,
	ROLE_CLIENT,
};

#define SERVER_PORT  6006

struct sig_param param;

static int my_role = ROLE_NONE;
static struct sockaddr server;

static struct rdma_event_channel *ech;
static struct rdma_cm_id *cmid;
struct ibv_pd *pd;
struct ibv_cq *cq;
struct ibv_qp *qp;

static struct rdma_cm_id *client_id; /* Used by server */

static int create_self_qp(struct rdma_cm_id *id)
{
	struct mlx5dv_qp_init_attr mlx5_qp_attr = {};
        struct ibv_qp_init_attr_ex qp_attr = {};

	qp_attr.qp_type = IBV_QPT_RC;
	qp_attr.sq_sig_all = 0;
	qp_attr.send_cq = cq;
	qp_attr.recv_cq = cq;
	qp_attr.cap.max_send_wr = 32;
	qp_attr.cap.max_recv_wr = 32;
	qp_attr.cap.max_send_sge = 1;
	qp_attr.cap.max_recv_sge = 1;
	qp_attr.cap.max_inline_data = 64;

	qp_attr.pd = pd;
	qp_attr.comp_mask = IBV_QP_INIT_ATTR_PD;
	qp_attr.comp_mask |= IBV_QP_INIT_ATTR_SEND_OPS_FLAGS;
	qp_attr.send_ops_flags = IBV_QP_EX_WITH_RDMA_WRITE | IBV_QP_EX_WITH_SEND |
		IBV_QP_EX_WITH_RDMA_READ | IBV_QP_EX_WITH_LOCAL_INV;

	/* Signature attributes */
	mlx5_qp_attr.comp_mask = MLX5DV_QP_INIT_ATTR_MASK_SEND_OPS_FLAGS;
	mlx5_qp_attr.send_ops_flags = MLX5DV_QP_EX_WITH_MKEY_CONFIGURE;

	qp = mlx5dv_create_qp(id->verbs, &qp_attr, &mlx5_qp_attr);
	if (!qp) {
		err("mlx5dv_create_qp: %s\n", strerror(errno));
		return errno;
	}

	return 0;
}

static int modify_qp(struct rdma_cm_id *id, struct ibv_qp *qp)
{
	struct ibv_qp_attr qp_attr = {};
	int qp_attr_mask, ret;

	qp_attr.qp_state = IBV_QPS_INIT;
	ret = rdma_init_qp_attr(id, &qp_attr, &qp_attr_mask);
	if (ret)
		return ret;

	ret = ibv_modify_qp(qp, &qp_attr, qp_attr_mask);
	if (ret)
		return ret;

	qp_attr.qp_state = IBV_QPS_RTR;
	ret = rdma_init_qp_attr(id, &qp_attr, &qp_attr_mask);
	if (ret)
		return ret;

	ret = ibv_modify_qp(qp, &qp_attr, qp_attr_mask);
	if (ret)
		return ret;

	qp_attr.qp_state = IBV_QPS_RTS;
	ret = rdma_init_qp_attr(id, &qp_attr, &qp_attr_mask);
	if (ret)
		return ret;

	return ibv_modify_qp(qp, &qp_attr, qp_attr_mask);
}

static int create_res(struct rdma_cm_id *id)
{
	int err;

	pd = ibv_alloc_pd(id->verbs);
	if (!pd) {
		perror("ibv_alloc_pd");
		return errno;
	}

	cq = ibv_create_cq(id->verbs, 32, NULL, NULL, 0);
	if (!cq) {
		perror("ibv_create_cq");
		goto fail_create_cq;
	}

	err = create_self_qp(id);
	if (err)
		goto fail_create_qp;

	return 0;

fail_create_qp:
	ibv_destroy_cq(cq);
fail_create_cq:
	ibv_dealloc_pd(pd);
	return -1;
}

static void destroy_qp(struct rdma_cm_id *id)
{
	ibv_destroy_qp(qp);
	ibv_destroy_cq(cq);
	ibv_dealloc_pd(pd);
}

static int setup_connection_server(void)
{
	struct rdma_conn_param param = {}, *cparam;
	struct rdma_cm_event *e;
	struct sockaddr_in sin;
	bool done;
	int err;

	sin.sin_family = AF_INET;
	sin.sin_port = htons(SERVER_PORT);
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	err = rdma_bind_addr(cmid, (struct sockaddr *)&sin);
	if (err) {
		perror("rdma_bind_addr");
		return errno;
	}

	err = rdma_listen(cmid, 5);
	if (err) {
		perror("rdma_listen");
		return errno;
	}

	done = false;
	while (!done) {
		err = rdma_get_cm_event(ech, &e);
		if (err) {
			perror("rdma_get_cm_event");
			return errno;
		}

		switch (e->event) {
		case RDMA_CM_EVENT_CONNECT_REQUEST:
			cparam = &e->param.conn;
			client_id = e->id;
			err = create_res(client_id);
			if (err)
				return err;

			info("Connect request comes; Created lqpn %d, rqpn %d\n",
			     qp->qp_num, cparam->qp_num);

			err = modify_qp(client_id, qp);
			if (err) {
				perror("modify_qp");
				goto fail;
			}

			param.qp_num = qp->qp_num;
			err = rdma_accept(client_id, &param);
			if (err) {
				perror("rdma_accept");
				goto fail;
			}
			break;

		case RDMA_CM_EVENT_ESTABLISHED:
			info("Connection established.\n");
			done = true;
			break;

		default:
			err("Unexpected event %d\n", e->event);
			break;
		}

		err = rdma_ack_cm_event(e);
		if (err) {
			perror("rdma_ack_cm_event");
			return err;
		}
	}

	return 0;

fail:
	destroy_qp(client_id);
	return err;
}

static int run_server(void)
{
	int ret;

	info("Server started...\n");
	ret = setup_connection_server();
	if (ret)
		return ret;

	ret = start_sig_test_server(pd, qp, cq, &param);

	destroy_qp(client_id);
	return ret;
}

static int setup_connection_client(void)
{
	struct rdma_conn_param param = {}, *cparam;
	struct rdma_cm_event *e;
	int ret, done;

	ret = rdma_resolve_addr(cmid, NULL, &server, 1000);
	if (ret) {
		perror("rdma_resolve_addr");
		return ret;
	}

	done = false;
	while (!done) {
		ret = rdma_get_cm_event(ech, &e);
		if (ret) {
			perror("rdma_get_cm_event");
			return errno;
		}

		switch (e->event) {
		case RDMA_CM_EVENT_ADDR_RESOLVED:
			ret = rdma_resolve_route(cmid, 2000);
			if (ret) {
				perror("rdma_resolve_route");
				return errno;
			}
			break;

		case RDMA_CM_EVENT_ROUTE_RESOLVED:
			ret = create_res(cmid);
			if (ret)
				return ret;

			param.qp_num = qp->qp_num;
			ret = rdma_connect(cmid, &param);
			if (ret) {
				perror("rdma_connect");
				goto fail;
			}
			break;

		case RDMA_CM_EVENT_CONNECT_RESPONSE:
		/* case RDMA_CM_EVENT_ESTABLISHED: */
			cparam = &e->param.conn;
			ret = modify_qp(cmid, qp);
			if (ret) {
				perror("modify_qp");
				goto fail;
			}

			ret = rdma_establish(cmid);
			if (ret) {
				perror("rdma_establish");
				goto fail;
			}

			info("Connection established, lqpn %d, rqpn %d\n",
			     qp->qp_num, cparam->qp_num);

			done = true;
			break;

		default:
			err("Unknown event %d, status %d, ignored\n",
			    e->event, e->status);
			break;
		}

		ret = rdma_ack_cm_event(e);
		if (ret) {
			perror("rdma_ack_cm_event");
			return ret;
		}
	}

	return 0;

fail:
	destroy_qp(cmid);
	return -1;
}

static int run_client(void)
{
	const char *p = NULL;
	char ipstr[64] = {};
	int ret;

	switch(server.sa_family) {
	case AF_INET:
		p = inet_ntop(AF_INET, &(((struct sockaddr_in *)&server)->sin_addr),
			      ipstr, sizeof(ipstr));
		break;

	case AF_INET6:
		p = inet_ntop(AF_INET, &(((struct sockaddr_in *)&server)->sin_addr),
			      ipstr, sizeof(ipstr));
		break;
	default:
		err("Unsupported AF: %d\n", server.sa_family);
		break;
	}
	if (!p) {
		err("Failed to get ip addr: %d\n", errno);
		return -1;
	}

	info("Client started, server %s port %d...\n", ipstr, SERVER_PORT);

	ret = setup_connection_client();
	if (ret)
		return ret;


	ret = start_sig_test_client(pd, qp, cq, &param);

	destroy_qp(cmid);
	return ret;
}

static void show_usage(char *program)
{
	printf("Usage: %s [OPTION]\n", program);
	printf("  [-c, --client]       - Run as the client\n");
	printf("  [-s, --server]       - Run as the server\n");
	printf("  [-m, --enable-check-copy-en-mask]       - Enable the check-copy-en-mask flag\n");
	printf("  [-h, --help]         - Show help\n");
	printf("Example:\n");
	printf("  Server: %s -s\n", program);
	printf("  Client: %s -c <server_ip>\n", program);
}

static int get_addr(char *ip, struct sockaddr *addr)
{
	struct addrinfo *res;
	int ret;

	ret = getaddrinfo(ip, NULL, NULL, &res);
	if (ret) {
		err("getaddrinfo failed (%s) - invalid hostname or IP address '%s'\n",
		    gai_strerror(ret), ip);
		return ret;
	}

	if (res->ai_family == PF_INET)
		memcpy(addr, res->ai_addr, sizeof(struct sockaddr_in));
	else if (res->ai_family == PF_INET6)
		memcpy(addr, res->ai_addr, sizeof(struct sockaddr_in6));
	else
		ret = -1;

	freeaddrinfo(res);
	return ret;
}

static int parse_opt(int argc, char *argv[])
{
	static const struct option long_opts[] = {
		{"version", 0, NULL, 'v'},
		{"help", 0, NULL, 'h'},
		{"client", 0, NULL, 'c'},
		{"server", 0, NULL, 's'},
		{"enable-check-copy-en-mask", 0, NULL, 'm'},
		{},
	};
	int op, err;

	while ((op = getopt_long(argc, argv, "hvcsm", long_opts, NULL)) != -1) {
                switch (op) {
		case 'v':
			//printf("%s %s\n", PROJECT_NAME, PROJECT_VERSION);
			exit(0);

		case 'h':
			show_usage(argv[0]);
			exit(0);

		case 'c':
			my_role = ROLE_CLIENT;
			err = get_addr(argv[argc - 1], &server);
			if (err) {
				err("Failed to get server IP address\n");
				exit(err);
			}
			if (server.sa_family == AF_INET)
				((struct sockaddr_in *)&server)->sin_port = htons(SERVER_PORT);
			else
				((struct sockaddr_in6 *)&server)->sin6_port = htons(SERVER_PORT);
			break;

		case 's':
			my_role = ROLE_SERVER;
			break;

		case 'm':
			param.check_copy_en_mask = true;
			info("check_copy_en_mask flag is enabled\n");
			break;

		default:
			err("Unknown option %c\n", op);
			show_usage(argv[0]);
			return -1;
		}
	}

	if (my_role == ROLE_NONE) {
		err("role not set\n");
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int ret;

	ret = parse_opt(argc, argv);
	if (ret)
		return ret;


	ech = rdma_create_event_channel();
	if (!ech) {
		perror("rdma_create_event_channel");
		return errno;
	}

	ret = rdma_create_id(ech, &cmid, NULL, RDMA_PS_TCP);
	if (ret) {
		perror("rdma_create_id");
		ret = errno;
		goto fail_create_id;
	}

	if (my_role == ROLE_SERVER)
		ret = run_server();
	else
		ret = run_client();

	rdma_destroy_id(cmid);

fail_create_id:
	rdma_destroy_event_channel(ech);
	return ret;
}
