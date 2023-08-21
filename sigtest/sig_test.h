/* SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) OR BSD-2-Clause) */
/*
 * Copyright (c) 2023 NVIDIA CORPORATION. All rights reserved
 */

#define err(args...) fprintf(stderr, ##args)
#define info(args...) fprintf(stdout, ##args)
#define dbg(args...) fprintf(stdout, ##args)

struct sig_param {
	enum mlx5dv_sig_type sig_type;

	bool check_copy_en_mask;
	unsigned int block_num;
	unsigned int block_size;
	unsigned int pi_size;

	enum mlx5dv_sig_nvmedif_format nvme_fmt;
	unsigned int sts;	/* Storage tag size, for nvmedif only */
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


int verify_sts(enum mlx5dv_sig_nvmedif_format nvme_fmt, unsigned int sts);
unsigned int get_default_sts(enum mlx5dv_sig_nvmedif_format nvme_fmt);

