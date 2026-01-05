/* gcc -Wall -o t test.c  -libverbs -lmlx5 */
#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <infiniband/verbs.h>
#include <infiniband/mlx5dv.h>

#include "mlx5_ifc.h"

void test(struct ibv_context *ibctx)
{
	uint16_t opmod = MLX5_SET_HCA_CAP_OP_MOD_GENERAL_DEVICE | HCA_CAP_OPMOD_GET_CUR;
	uint32_t in[DEVX_ST_SZ_DW(query_hca_cap_in)] = {};
	uint32_t out[DEVX_ST_SZ_DW(query_hca_cap_out)] = {};
	int ret, log_max_qp_wr;

	DEVX_SET(query_hca_cap_in, in, opcode, MLX5_CMD_OP_QUERY_HCA_CAP);
	DEVX_SET(query_hca_cap_in, in, op_mod, opmod);

	printf("=DEBUG:%s:%d: tryint to do query_hca_cap on ibdev %s...\n", __func__, __LINE__, ibctx->device->name);
	ret = mlx5dv_devx_general_cmd(ibctx, in, sizeof(in), out, sizeof(out));
	if (ret)
		perror("failed\n");

	log_max_qp_wr = DEVX_GET(query_hca_cap_out, out, capability.cmd_hca_cap.log_max_qp_sz);
	printf("=DEBUG:%s:%d: Succeeded, CAP.log_max_qp_sz %d, max qp WQE %d\n", __func__, __LINE__, log_max_qp_wr, 1 << log_max_qp_wr);
}


int main(void)
{
        struct ibv_device **dev_list;
	struct ibv_context *ibctx;

	dev_list = ibv_get_device_list(NULL);
	if (!dev_list) {
		perror("ibv_get_device_list()");
		return -errno;
	}

	ibctx = ibv_open_device(dev_list[0]);
	test(ibctx);

	ibv_close_device(ibctx);
	ibv_free_device_list(dev_list);
	return 0;
}
