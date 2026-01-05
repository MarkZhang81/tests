/* gcc -Wall -o t reg_mr_test.c  -libverbs */
#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <infiniband/verbs.h>
//#include <infiniband/mlx5dv.h>
//#include "mlx5_ifc.h"

struct ibv_pd *pd;
char *buf1, *buf2;
struct ibv_mr *mr1, *mr2;

void test(struct ibv_context *ibctx)
{
	int buflen = 4096 * 4;
	struct ibv_mr_init_attr in = {};

	printf("=DEBUG:%s:%d: Doing test on ibdev %s\n", __func__, __LINE__, ibctx->device->name);
	pd = ibv_alloc_pd(ibctx);
	if (!pd) {
		perror("ibv_alloc_pd");
		exit(1);
	}

	buf1 = malloc(buflen);
	buf2 = malloc(buflen);
	if (!buf1 || !buf2) {
		perror("malloc");
		exit(1);
	}

	mr1 = ibv_reg_mr(pd, buf1, buflen, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);
	if (!mr1)
		perror("ibv_reg_mr");
	else
		printf("=DEBUG:%s:%d: ibv_reg_mr succeeded\n", __func__, __LINE__);

	in.access = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
	in.length = buflen;
	in.comp_mask = IBV_REG_MR_MASK_ADDR;
	in.addr = buf2;
	mr2 = ibv_reg_mr_ex(pd, &in);
	if (!mr2)
		perror("ibv_reg_mr_ex");
	else
		printf("=DEBUG:%s:%d: ibv_reg_mr_ex succeeded\n", __func__, __LINE__);
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

