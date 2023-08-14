/* SPDX-License-Identifier: ((GPL-2.0 WITH Linux-syscall-note) OR BSD-2-Clause) */
/*
 * Copyright (c) 2023 NVIDIA CORPORATION. All rights reserved
 */

#include <stdlib.h>
#include <unistd.h>

#include <infiniband/mlx5dv.h>

#include "sig_test.h"

enum {
	SIG_FLAG_WIRE = 1 << 0,
	SIG_FLAG_MEM = 1 << 1,
};

static unsigned int sig_block_size = 512;
static unsigned int sig_pi_size = 8;	/* For t10dif */
static unsigned int sig_num_blocks = 1;

static struct ibv_mr *data_mr, *pi_mr;
static unsigned char *data_buf, *pi_buf;

static struct mlx5dv_mkey *sig_mkey;

static void dump_data_buf(void)
{
	unsigned char *p = data_buf;
	int i;

	printf("Data buf:\n");
	for (i = 0; i < sig_num_blocks; i++) {
		p = data_buf + sig_block_size * i;
		printf("  block %02x: %02x %02x %02x %02x %02x %02x %02x %02x ...\n", i,
		       p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
	}
}

static void dump_data_buf_with_pi(void)
{
	unsigned char *p = data_buf;
	int i;

	printf("Data buf:\n");
	for (i = 0; i < sig_num_blocks; i++) {
		p = data_buf + (sig_block_size + sig_pi_size) * i;
		printf("  block %02x: %02x %02x %02x %02x %02x %02x %02x %02x ...\n", i,
		       p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
		p = p + sig_block_size;
		printf("        pi: %02x %02x %02x %02x %02x %02x %02x %02x\n",
		       p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
	}
}

static void dump_pi(void)
{
	unsigned char *p = pi_buf;
	int i;

	printf("PI buf:\n");
	for (i = 0; i < sig_num_blocks; i++) {
		p = pi_buf + sig_pi_size * i;
		printf("  block %02x: %02x %02x %02x %02x %02x %02x %02x %02x...\n", i,
		       p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
	}
}

static int is_sig_supported(struct ibv_context *ctx, struct sig_param *param)
{
	struct mlx5dv_context dv_ctx = {
		.comp_mask = MLX5DV_CONTEXT_MASK_SIGNATURE_OFFLOAD,
	};
	uint32_t prot;
	int ret;

	if (!mlx5dv_is_supported(ctx->device)) {
		err("Device %s doesn't support mlx5dv\n", ibv_get_device_name(ctx->device));
		return -1;
	}

	ret = mlx5dv_query_device(ctx, &dv_ctx);
	if (ret) {
		perror("mlx5dv_query_device");
		return ret;
	}

	prot = dv_ctx.sig_caps.block_prot;
	info("sig cap: CRC %d, T10DIF %d, NVMEDIF %d\n",
	     !!(prot & MLX5DV_SIG_PROT_CAP_T10DIF),
	     !!(prot & MLX5DV_SIG_PROT_CAP_CRC),
	     !!(prot & MLX5DV_SIG_PROT_CAP_NVMEDIF));

	if (param->check_copy_en_mask && !(prot & MLX5DV_SIG_PROT_CAP_NVMEDIF))
		err("check_copy_en_mask is not supported!!!\n");
	return 0;
}

static int poll_completion(struct ibv_cq *cq, enum ibv_wc_opcode expected)
{
	struct ibv_wc wc = {};
	int ret;

	do {
		ret = ibv_poll_cq(cq, 1, &wc);
		usleep(1000 * 100);
	} while (ret == 0);

	if (wc.status != IBV_WC_SUCCESS) {
		err("CQE status %d(%s), opcode %d(%s), expected %s\n",
		    wc.status, ibv_wc_status_str(wc.status),
		    wc.opcode, wc_opcode_str(wc.opcode), wc_opcode_str(expected));
		return -1;
	}

	if (wc.opcode != expected) {
		err("CQE opcode %d(%s), expected %s\n",
		    wc.opcode, wc_opcode_str(wc.opcode), wc_opcode_str(expected));
		return -1;
	}

	info("Expected complection received, opcode %d(%s), byte_len %d qp_num %d src_qp %d\n",
	     expected, wc_opcode_str(expected), wc.byte_len, wc.qp_num, wc.src_qp);
	return 0;
}

static struct mlx5dv_mkey *create_sig_mkey(struct ibv_pd *pd)
{
	struct mlx5dv_mkey_init_attr mkey_attr = {};
	struct mlx5dv_mkey *mkey;

	mkey_attr.pd = pd;
	mkey_attr.max_entries = 1;
	mkey_attr.create_flags = MLX5DV_MKEY_INIT_ATTR_FLAGS_INDIRECT |
		MLX5DV_MKEY_INIT_ATTR_FLAGS_BLOCK_SIGNATURE;

	mkey = mlx5dv_create_mkey(&mkey_attr);
	if (!mkey)
		perror("mlx5dv_create_mkey");

	return mkey;
}

static void set_sig_domain_t10dif(struct mlx5dv_sig_block_domain *domain,
				  struct mlx5dv_sig_t10dif *dif)
{
	memset(dif, 0, sizeof(*dif));
	dif->bg_type = MLX5DV_SIG_T10DIF_CRC;
	dif->bg = 0xffff;
	dif->app_tag = 0x5678;
	dif->ref_tag = 0xabcdef90;
	dif->flags = MLX5DV_SIG_T10DIF_FLAG_REF_REMAP | MLX5DV_SIG_T10DIF_FLAG_APP_ESCAPE;

	memset(domain, 0, sizeof(*domain));
	domain->sig.dif = dif;
	domain->sig_type = MLX5DV_SIG_TYPE_T10DIF;
	domain->block_size = (sig_block_size == 512) ?
		MLX5DV_BLOCK_SIZE_512 : MLX5DV_BLOCK_SIZE_4096;
}

static void set_sig_domain_nvmedif(struct mlx5dv_sig_block_domain *domain,
				   struct mlx5dv_sig_nvmedif *dif)
{
	domain->sig_type = MLX5DV_SIG_TYPE_NVMEDIF;
	domain->sig.nvmedif = dif;
	domain->block_size = (sig_block_size == 512) ?
		MLX5DV_BLOCK_SIZE_512 : MLX5DV_BLOCK_SIZE_4096;

	dif->format = MLX5DV_SIG_NVMEDIF_FORMAT_32;
	dif->seed = UINT32_MAX;
	dif->storage_tag = 0x11223344dead5566;
	dif->ref_tag = 0x55667788beef9900;
	dif->sts = 20;

	dif->flags = MLX5DV_SIG_NVMEDIF_FLAG_APP_REF_ESCAPE;
	dif->app_tag = 0x6a43;
	dif->app_tag_check = 0xf;
	dif->storage_tag_check = 0x3f;
}

static int config_sig_mkey(struct ibv_qp *qp, struct mlx5dv_mkey *mkey,
			   struct mlx5dv_sig_block_attr *sig_attr, int mode)
{
	struct ibv_qp_ex *qpx = ibv_qp_to_qp_ex(qp);
	struct mlx5dv_qp_ex *dv_qp = mlx5dv_qp_ex_from_ibv_qp_ex(qpx);
	struct mlx5dv_mkey_conf_attr conf_attr = {};
	uint32_t access_flags = IBV_ACCESS_LOCAL_WRITE |
		IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
	struct mlx5dv_mr_interleaved mr_interleaved[2] = {};
	struct ibv_sge sge;
	int ret;

	ibv_wr_start(qpx);
	qpx->wr_id = 0x1314;
	qpx->wr_flags = IBV_SEND_SIGNALED | IBV_SEND_INLINE;

	mlx5dv_wr_mkey_configure(dv_qp, mkey, 3, &conf_attr);
	mlx5dv_wr_set_mkey_access_flags(dv_qp, access_flags);

	if ((mode & SIG_FLAG_WIRE) && (mode & SIG_FLAG_MEM)) {
		info("interleave mode\n");
		/* Data */
		mr_interleaved[0].addr = (uint64_t)data_mr->addr;
		mr_interleaved[0].bytes_count = sig_block_size;
		mr_interleaved[0].bytes_skip = 0;
		mr_interleaved[0].lkey = data_mr->lkey;

		/* Pi */
		mr_interleaved[1].addr = (uint64_t)pi_mr->addr;
		mr_interleaved[1].bytes_count = sig_pi_size;
		mr_interleaved[1].bytes_skip = 0;
		mr_interleaved[1].lkey = pi_mr->lkey;

		mlx5dv_wr_set_mkey_layout_interleaved(dv_qp, sig_num_blocks, 2, mr_interleaved);
	} else {
		sge.addr = (uint64_t)data_mr->addr;
		sge.lkey = data_mr->lkey;
		//sge.length = data_mr->length;
		sge.length = sig_block_size * sig_num_blocks;
		//sge.length = sig_block_size * 2;
		mlx5dv_wr_set_mkey_layout_list(dv_qp, 1, &sge);
	}

	mlx5dv_wr_set_mkey_sig_block(dv_qp, sig_attr);
	ret = ibv_wr_complete(qpx);
	if (ret)
		perror("ibv_wr_complete mkey configure WR");

	return ret;
}

static int reg_sig_mkey_t10dif(struct ibv_qp *qp, struct ibv_cq *cq,
			       struct mlx5dv_mkey *mkey, int mode,
			       struct sig_param *param)
{
	struct mlx5dv_sig_block_domain dwire = {}, dmem = {};
	struct mlx5dv_sig_t10dif wire_t10dif = {}, mem_t10dif = {};
	struct mlx5dv_sig_block_attr sig_attr = {
		.check_mask = MLX5DV_SIG_MASK_T10DIF_GUARD | MLX5DV_SIG_MASK_T10DIF_APPTAG | MLX5DV_SIG_MASK_T10DIF_REFTAG,
	};
	int ret;

	if (mode & SIG_FLAG_MEM) {
		sig_attr.mem = &dmem;
		set_sig_domain_t10dif(&dmem, &mem_t10dif);
	}
	if (mode & SIG_FLAG_WIRE) {
		sig_attr.wire = &dwire;
		set_sig_domain_t10dif(&dwire, &wire_t10dif);
	}

	if (param && param->check_copy_en_mask) {
		sig_attr.check_mask = 0;
		sig_attr.comp_mask = MLX5DV_SIG_BLOCK_COMP_MASK_CHECK_COPY_EN;
	}

	if (config_sig_mkey(qp, mkey, &sig_attr, mode))
		return -1;

	info("Mkey configure MR posted, wailting for completion...\n");
	ret = poll_completion(cq, IBV_WC_DRIVER1);
	if (ret) {
		err("poll_completion(IBV_WC_DRIVER1) failed.\n");
		return ret;
	}

	return 0;
}

static int reg_sig_mkey_nvmedif(struct ibv_qp *qp, struct ibv_cq *cq,
				struct mlx5dv_mkey *mkey, int mode,
				struct sig_param *param)
{
	struct mlx5dv_sig_block_domain dwire = {}, dmem = {};
	struct mlx5dv_sig_nvmedif wire_dif = {}, mem_dif = {};
	struct mlx5dv_sig_block_attr sig_attr = {};
	int ret;

	if (!param->check_copy_en_mask) {
		err("check_copy_en_mask is needed for nvme\n");
		return -EINVAL;
	}

	sig_attr.comp_mask = MLX5DV_SIG_BLOCK_COMP_MASK_CHECK_COPY_EN;
	sig_attr.check_copy_en.guard_check_en = 1;
	sig_attr.check_copy_en.ref_tag_check_en = 1;
	sig_attr.check_copy_en.app_tag_check_en = 1;
	sig_attr.check_copy_en.storage_tag_check_en = 1;
	sig_attr.check_copy_en.storage_tag_copy_en = 0xff;

	if (mode & SIG_FLAG_MEM) {
		sig_attr.mem = &dmem;
		set_sig_domain_nvmedif(&dmem, &mem_dif);
	}
	if (mode & SIG_FLAG_WIRE) {
		sig_attr.wire = &dwire;
		set_sig_domain_nvmedif(&dwire, &wire_dif);
	}

	if (config_sig_mkey(qp, mkey, &sig_attr, mode))
		return -1;

	info("Mkey configure MR posted, wailting for completion...\n");
	ret = poll_completion(cq, IBV_WC_DRIVER1);
	if (ret) {
		err("poll_completion(IBV_WC_DRIVER1) failed.\n");
		return ret;
	}

	return 0;
}

#if 0
static int inv_sig_mkey(struct ibv_qp *qp, struct ibv_cq *cq,
			struct mlx5dv_mkey *mkey)
{
        struct ibv_qp_ex *qpx = ibv_qp_to_qp_ex(qp);
	int rc;

	ibv_wr_start(qpx);
	qpx->wr_id = 0;
	qpx->wr_flags = IBV_SEND_SIGNALED;
	ibv_wr_local_inv(qpx, mkey->rkey);
	rc = ibv_wr_complete(qpx);
	if (rc) {
		err("Local invalidate sig MKEY: %s\n", strerror(rc));
		return -1;
	}

	if (poll_completion(cq, IBV_WC_LOCAL_INV)) {
		err("Failed to invalidete sig MKEY\n");
		return -1;
	}

	info("Sig MKEY is invalidated\n");
	return rc;
}
#endif

static int alloc_mr(struct ibv_pd *pd)
{
	int ret, flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
		IBV_ACCESS_REMOTE_WRITE;
	ssize_t len;

	/* Larger data buffer is allocated so that it can be used to receive data with pi */
	len = (sig_block_size + sig_pi_size) * sig_num_blocks;
	data_buf = malloc(len);
	if (!data_buf) {
		perror("malloc data");
		return errno;
	}

	data_mr = ibv_reg_mr(pd, data_buf, len, flags);
	if (!data_mr) {
		perror("ibv_reg_mr data");
		ret = errno;
		goto fail_reg_data_mr;
	}

	len = sig_pi_size * sig_num_blocks;
	pi_buf = malloc(len);
	if (!pi_buf) {
		perror("malloc pi");
		ret = errno;
		goto fail_malloc_pi_buf;
	}

	pi_mr = ibv_reg_mr(pd, pi_buf, len, flags);
	if (!pi_mr) {
		perror("ibv_reg_mr pi");
		ret = errno;
		goto fail_reg_pi_mr;
	}

	info("Block size %d, pi size %d, block num %d\n", sig_block_size, sig_pi_size, sig_num_blocks);
	return 0;

fail_reg_pi_mr:
	free(pi_buf);
fail_malloc_pi_buf:
	ibv_dereg_mr(data_mr);
fail_reg_data_mr:
	free(data_buf);

	return ret;
}


static void dealloc_mr(void)
{
	if (data_mr) {
		ibv_dereg_mr(data_mr);
		free(data_buf);
	}

	if (pi_mr) {
		ibv_dereg_mr(pi_mr);
		free(pi_buf);
	}
}

static int create_sig_res(struct ibv_pd *pd)
{
	int ret;

	ret = alloc_mr(pd);
	if (ret)
		return ret;

	sig_mkey = create_sig_mkey(pd);
	if (!sig_mkey) {
		ret = errno;
		goto fail_create_sig_mkey;
	}

	info("Sig res created.\n\n");
	return 0;

fail_create_sig_mkey:
	dealloc_mr();

	return ret;
}

static void destroy_sig_res(void)
{
	mlx5dv_destroy_mkey(sig_mkey);
	dealloc_mr();
}

static int do_recv(struct ibv_qp *qp, struct ibv_cq *cq,
		   struct mlx5dv_mkey *sig_mkey)
{
	struct ibv_recv_wr wr = {}, *bad_wr = NULL;
	struct ibv_sge sge = {};
	static int wr_id = 200;
	int ret;

	if (sig_mkey) {
		sge.addr = 0;
		sge.length = (sig_block_size + sig_pi_size) * sig_num_blocks;
		sge.lkey = sig_mkey->lkey;
	} else {
		sge.addr = (uint64_t)data_buf;
		sge.length = data_mr->length;
		sge.lkey = data_mr->lkey;
	}

	wr.wr_id = wr_id++;
	wr.next = NULL;
	wr.sg_list = &sge;
	wr.num_sge = 1;

	ret = ibv_post_recv(qp, &wr, &bad_wr);
	if (ret) {
		perror("ibv_post_recv");
		return ret;
	}

	return poll_completion(cq, IBV_WC_RECV);
}

static void init_buf(unsigned char *buf, ssize_t len, unsigned char c)
{
	int i;

	for (i = 0; i < len; i ++)
		buf[i] = c;
}

static void init_send_buf(unsigned char *buf, unsigned int block_size,
			  unsigned int block_num, unsigned char c)
{
	int i;

	for (i = 0; i < block_num; i++)
		init_buf(buf + i * block_size, block_size, c++);
}

static int check_sig_mkey(struct mlx5dv_mkey *mkey)
{
	struct mlx5dv_mkey_err err_info;
	const char *sig_err_str = "";
	int sig_err, ret;

	ret = mlx5dv_mkey_check(mkey, &err_info);
	if (ret) {
		perror("mlx5dv_mkey_check");
		return ret;
	}

	sig_err = err_info.err_type;
	switch (sig_err) {
	case MLX5DV_MKEY_NO_ERR:
		break;
	case MLX5DV_MKEY_SIG_BLOCK_BAD_REFTAG:
		sig_err_str = "REF_TAG";
		break;
	case MLX5DV_MKEY_SIG_BLOCK_BAD_APPTAG:
		sig_err_str = "APP_TAG";
		break;
	case MLX5DV_MKEY_SIG_BLOCK_BAD_GUARD:
		sig_err_str = "BLOCK_GUARD";
		break;
	default:
		err("unknown sig error %d\n", sig_err);
		break;
	}

	if (!sig_err)
		info("SIG status: OK\n");
	else
		err("SIG ERROR: %s: expected 0x%lx, actual 0x%lx, offset %lu\n",
		    sig_err_str, err_info.err.sig.expected_value,
		    err_info.err.sig.actual_value, err_info.err.sig.offset);
	return 0;
}

int start_sig_test_server(struct ibv_pd *pd, struct ibv_qp *qp,
			  struct ibv_cq *cq, struct sig_param *param)
{
	ssize_t recv_len = (sig_block_size + sig_pi_size) * sig_num_blocks;
	int ret;

	ret = is_sig_supported(pd->context, param);
	if (ret)
		return ret;

	if (param->block_num)
		sig_num_blocks = param->block_num;

	if (param->block_size)
		sig_block_size = param->block_size;

	if (param->pi_size)
		sig_pi_size = param->pi_size;

	ret = create_sig_res(pd);
	if (ret)
		return ret;

	init_buf(data_buf, recv_len, 0);
	init_buf(pi_buf, sig_pi_size * sig_num_blocks, 0);
	info("Receving data without mkey...\n");
	ret = do_recv(qp, cq, NULL);
	if (ret)
		goto out;
	dump_data_buf();
	info("Done\n\n");

	init_buf(data_buf, recv_len, 0);
	info("Receving data (sent with mkey enabled)...\n");
	ret = do_recv(qp, cq, NULL);
	if (ret)
		goto out;
	dump_data_buf_with_pi();
	info("Done\n\n");

	info("Register sig mkey...\n");
	if (param->sig_type == MLX5DV_SIG_TYPE_T10DIF)
		ret = reg_sig_mkey_t10dif(qp, cq, sig_mkey,
					  SIG_FLAG_MEM | SIG_FLAG_WIRE, param);
	else if (param->sig_type == MLX5DV_SIG_TYPE_NVMEDIF)
		ret = reg_sig_mkey_nvmedif(qp, cq, sig_mkey,
					   SIG_FLAG_MEM | SIG_FLAG_WIRE, param);
	else
		ret = -EOPNOTSUPP;
	if (ret)
		goto out;

	init_buf(data_buf, recv_len, 0);
	info("Receving data (sent with mkey enabled)...\n");
	ret = do_recv(qp, cq, sig_mkey);
	if (ret)
		goto out;

	dump_data_buf();
	dump_pi();
	info("Done\n\n");

out:
	destroy_sig_res();
	return ret;
}

/* Client sends the same data with mkey twice; In the server side:
 * - First time: Receives raw data (without mkey configured)
 * - Second time: Receives clear data (with mkey configured and pi striped)
 */
static int do_send(struct ibv_qp *qp, struct ibv_cq *cq, ssize_t data_len,
		   struct mlx5dv_mkey *sig_mkey)
{
	struct ibv_send_wr wr = {}, *bad_wr = NULL;
	struct ibv_sge sge = {};
	static int wr_id = 100;
	int ret;

	if (sig_mkey) {
		sge.addr = 0;
		sge.length = data_len + sig_pi_size * sig_num_blocks;
		sge.lkey = sig_mkey->lkey;
	} else {
		sge.addr = (uint64_t)data_buf;
		sge.length = data_len;
		sge.lkey = data_mr->lkey;
	}

	wr.wr_id = wr_id++;
	wr.next = NULL;
	wr.sg_list = &sge;
	wr.num_sge = 1;
	wr.opcode = IBV_WR_SEND;
	wr.send_flags = IBV_SEND_SIGNALED;

	ret = ibv_post_send(qp, &wr, &bad_wr);
	if (ret) {
		perror("ibv_post_send");
		return ret;
	}

	ret = poll_completion(cq, IBV_WC_SEND);
	return ret;
}

int start_sig_test_client(struct ibv_pd *pd, struct ibv_qp *qp,
			  struct ibv_cq *cq, struct sig_param *param)
{
	ssize_t send_len = sig_block_size * sig_num_blocks;
	int ret;

	ret = is_sig_supported(pd->context, param);
	if (ret)
		return ret;

	if (param->block_num)
		sig_num_blocks = param->block_num;

	if (param->block_size)
		sig_block_size = param->block_size;

	if (param->pi_size)
		sig_pi_size = param->pi_size;

	ret = create_sig_res(pd);
	if (ret)
		return ret;

	init_send_buf(data_buf, sig_block_size, sig_num_blocks, 0x5a);

	usleep(1000*500);
	info("Send data (%ld bytes) without mkey...\n", send_len);
	ret = do_send(qp, cq, send_len, NULL);
	if (ret)
		goto out;
	info ("Done\n\n");

	init_send_buf(data_buf, sig_block_size, sig_num_blocks, 0xc0);
	info("Register sig mkey (WIRE)...\n");
	if (param->sig_type == MLX5DV_SIG_TYPE_T10DIF)
		ret = reg_sig_mkey_t10dif(qp, cq, sig_mkey, SIG_FLAG_WIRE, param);
	else if (param->sig_type == MLX5DV_SIG_TYPE_NVMEDIF)
		ret = reg_sig_mkey_nvmedif(qp, cq, sig_mkey,
					   SIG_FLAG_MEM | SIG_FLAG_WIRE, param);
	else
		ret = -EOPNOTSUPP;
	if (ret)
		goto out;
	info("Done\n\n");

	usleep(1000 * 500);
	info("Send data (%ld bytes) with mkey (server receives without mkey)...\n", send_len);
	ret = do_send(qp, cq, send_len, sig_mkey);
	if (ret)
		goto out;
#if 0
	ret = inv_sig_mkey(qp, cq, sig_mkey);
	if (ret)
		goto out;
#endif
	info ("Done\n\n");

	ret = check_sig_mkey(sig_mkey);
	if (ret < 0)
		goto out;

	usleep(1000 * 500);
	info("Send data (%ld bytes) with mkey (server receives *with* mkey)...\n", send_len);
	ret = do_send(qp, cq, send_len, sig_mkey);
	if (ret)
		goto out;
	info ("Done\n\n");

	ret = check_sig_mkey(sig_mkey);
	if (ret < 0)
		goto out;

out:
	destroy_sig_res();
	return ret;
}
