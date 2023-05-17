/* SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) OR BSD-2-Clause) */
/*
 * Copyright (c) 2023 NVIDIA CORPORATION. All rights reserved
 */

#define err(args...) fprintf(stderr, ##args)
#define info(args...) fprintf(stdout, ##args)
#define dbg(args...) fprintf(stdout, ##args)

struct sig_param {
	bool check_copy_en_mask;
};

static inline const char *wc_opcode_str(enum ibv_wc_opcode opcode)
{
	const char *str;

	switch (opcode) {
	case IBV_WC_RDMA_WRITE:
		str = "RDMA_WRITE";
		break;
	case IBV_WC_SEND:
		str = "SEND";
		break;
	case IBV_WC_RDMA_READ:
		str = "RDMA_READ";
		break;
	case IBV_WC_LOCAL_INV:
		str = "LOCAL_INV";
		break;
	case IBV_WC_RECV:
		str = "RECV";
		break;
	case IBV_WC_DRIVER1:
		str = "DRIVER1";
		break;
	default:
		str = "UNKNOWN";
	};

	return str;
}


int start_sig_test_server(struct ibv_pd *pd, struct ibv_qp *qp,
			  struct ibv_cq *cq, struct sig_param *param);
int start_sig_test_client(struct ibv_pd *pd, struct ibv_qp *qp,
			  struct ibv_cq *cq, struct sig_param *param);

