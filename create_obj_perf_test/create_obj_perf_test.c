#include <errno.h>
#include <getopt.h>
#include <malloc.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include <infiniband/verbs.h>

#define info(args...) fprintf(stdout, ##args)
#define err(args...) fprintf(stderr, ##args)

#define dump(args...) fprintf(stdout, ##args)

static unsigned int task_num;
static unsigned int inst_num_per_task;

struct perf_inst {
	struct ibv_pd *pd;
	struct ibv_mr *mr;
	struct ibv_cq *cq;
	struct ibv_qp *qp;

	uint64_t tm_pd, tm_mr, tm_cq, tm_qp;
};

struct perf_task {
	pthread_t tid;
	struct ibv_context *ibctx;
	struct perf_inst *insts;

	uint64_t create_tm_used;
};

static struct perf_task *tasks;
static const char *dev_name;

struct ibv_device **dev_list;
static struct ibv_device *ibdev;

uint64_t prog_create_tm_used;

struct timeval tv_prog_start, tv_prog_done;
static int num_task_create_done;
sem_t sem;

static void show_usage(char *prog)
{
	printf("Usage: %s -t <task_num> -n <instance_num_per_task> -d <ib_device>\n", prog);
}

static int parse_opt(int argc, char *argv[])
{
	static const struct option long_opts[] = {
		{"help", 0, NULL, 'h'},
		{"device", 0, NULL, 'd'},
		{"task-num", 0, NULL, 't'},
		{"instance-num-per-task", 0, NULL, 'n'},
		{},
	};
	int i, op, ret = 0;


	while ((op = getopt_long(argc, argv, "ht:n:d:", long_opts, NULL)) != -1) {
		switch (op) {
		case 'h':
			show_usage(argv[0]);
			exit(1);

		case 'd':
			dev_name = optarg;
			break;

		case 't':
			task_num = atoi(optarg);
			break;

		case 'n':
			inst_num_per_task = atoi(optarg);
			break;

		default:
			err("Unknown option %c\n", op);
			show_usage(argv[0]);
			return EINVAL;
		}
	}

	if (!dev_name) {
		err("Error: IB device is not specified\n");
		show_usage("argv[0]");
		return EINVAL;
	}

	if (!task_num || !inst_num_per_task) {
		err("Error: Invalid task number %d or per-task instance number %d\n", task_num, inst_num_per_task);
		show_usage(argv[0]);
		return EINVAL;
	}

	dev_list = ibv_get_device_list(NULL);
	if (!dev_list) {
		err("ibv_get_device_list failed %d\n", errno);
		return errno;
	}

	for (i = 0; dev_list[i] != NULL; i++) {
		if (strncmp(dev_name, ibv_get_device_name(dev_list[i]),
			    strlen(dev_name)) == 0)
			break;
	}

	if (!dev_list[i]) {
		err("Device not found %s\n", dev_name);
		return EINVAL;
	}

	ibdev = dev_list[i];
	info("Device %s; Task number: %d; Per-taks instance number: %d\n", dev_name, task_num, inst_num_per_task);
	return ret;
}


static uint64_t get_time_used(const struct timeval *t1, const struct timeval *t2)
{
	return (t2->tv_sec - t1->tv_sec) * 1000000 + (t2->tv_usec - t1->tv_usec);
}

static char buf[1024];
static void *task_run(void *arg)
{
	struct perf_task *task = (struct perf_task *)arg;
	struct timeval tt0, tt1, t0, t1, t2, t3, t4;
	struct perf_inst *inst;
	struct ibv_qp_init_attr qp_init_attr = {};
	int i;

	task->ibctx = ibv_open_device(ibdev);
	if (!task->ibctx) {
		err("ibv_open_device failed %d, task abort\n", errno);
		return NULL;
	}

	gettimeofday(&tt0, NULL);
	for (i = 0; i < inst_num_per_task; i++) {
		inst = &task->insts[i];

		gettimeofday(&t0, NULL);
		inst->pd = ibv_alloc_pd(task->ibctx);
		if (!inst->pd) {
			perror("ibv_alloc_pd failed, abort\n");
			goto fail;
		}

		gettimeofday(&t1, NULL);
		inst->mr = ibv_reg_mr(inst->pd, buf, sizeof(buf), IBV_ACCESS_LOCAL_WRITE);
		if (!inst->mr) {
			perror("ibv_reg_mr failed, abort\n");
			goto fail;
		}

		gettimeofday(&t2, NULL);
		inst->cq = ibv_create_cq(task->ibctx, 128, NULL, NULL, 0);
		if (!inst->cq) {
			perror("ibv_create_cq failed, abort\n");
			goto fail;
		}

		qp_init_attr.send_cq = inst->cq;
		qp_init_attr.recv_cq = inst->cq;
		qp_init_attr.qp_type = IBV_QPT_RC;

		qp_init_attr.cap.max_send_wr = 32;
		qp_init_attr.cap.max_recv_wr = 32;
		qp_init_attr.cap.max_send_sge = 1;
		qp_init_attr.cap.max_recv_sge = 1;
		qp_init_attr.cap.max_inline_data = 64;

		gettimeofday(&t3, NULL);
		inst->qp = ibv_create_qp(inst->pd, &qp_init_attr);
		if (!inst->qp) {
			perror("ibv_create_qp failed, abort\n");
		}

		gettimeofday(&t4, NULL);

		inst->tm_pd = get_time_used(&t0, &t1);
		inst->tm_mr = get_time_used(&t1, &t2);
		inst->tm_cq = get_time_used(&t2, &t3);
		inst->tm_qp = get_time_used(&t3, &t4);
	}

	gettimeofday(&tt1, NULL);
	task->create_tm_used = get_time_used(&tt0, &tt1);

	num_task_create_done++;
	if (num_task_create_done >= task_num) {
		gettimeofday(&tv_prog_done, NULL);
		sem_post(&sem);
	}

	return NULL;

fail:
	exit(errno);
	return NULL;
}

static int start_tasks(void)
{
	int i, ret;

	tasks = calloc(task_num, sizeof(*tasks));
	if (!tasks) {
		err("Calloc(%d, %ld) failed: %d\n", task_num, sizeof(*tasks), errno);
		exit(errno);
	}

	gettimeofday(&tv_prog_start, NULL);

	for (i = 0; i < task_num; i++) {
		tasks[i].insts = calloc(inst_num_per_task, sizeof(*tasks[i].insts));
		if (!tasks[i].insts) {
			err("Calloc(%d, %ld) failed: %d\n", inst_num_per_task, sizeof(*tasks[i].insts), errno);
			exit(errno);
		}

		ret = pthread_create(&tasks[i].tid, NULL, task_run, tasks + i);
		if (ret) {
			err("Failed to start task %d: %d\n", i, errno);
			exit(errno);
		}
		info("Task %d has been started...\n", i);
	}

	return 0;
}

static void do_statistic_instance(void)
{
	struct perf_inst *inst;
	uint64_t tpd = 0, tmr = 0, tcq = 0, tqp = 0;
	uint64_t mpd = 0, mmr = 0, mcq = 0, mqp = 0;
	int i, j;

	for (i = 0; i < task_num; i++) {
		for (j = 0; j < inst_num_per_task; j++) {
			inst = &tasks[i].insts[j];
			tpd += inst->tm_pd;
			tmr += inst->tm_mr;
			tcq += inst->tm_cq;
			tqp += inst->tm_qp;

			if (inst->tm_pd > mpd)
				mpd = inst->tm_pd;
			if (inst->tm_mr)
				mmr = inst->tm_mr;
			if (inst->tm_cq)
				mcq = inst->tm_cq;
			if (inst->tm_qp)
				mqp = inst->tm_qp;
		}
	}

	dump("Maximum and average time used for each step(in mini-seconds):\n");
	tpd /= (task_num * inst_num_per_task);
	dump("  alloc_pd:  %03ld.%03ld	%03ld.%03ld\n", mpd / 1000, mpd % 1000, tpd / 1000, tpd % 1000);
	tmr /= (task_num * inst_num_per_task);
	dump("  reg_mr:    %03ld.%03ld	%03ld.%03ld\n", mmr / 1000, mmr % 1000, tmr / 1000, tmr % 1000);
	tcq /= (task_num * inst_num_per_task);
	dump("  create_cq: %03ld.%03ld	%03ld.%03ld\n", mcq / 1000, mcq % 1000, tcq / 1000, tcq % 1000);
	tqp /= (task_num * inst_num_per_task);
	dump("  create_qp: %03ld.%03ld	%03ld.%03ld\n", mqp / 1000, mqp % 1000, tqp / 1000, tqp % 1000);
}

static void do_statistic_task(void)
{
	uint64_t total = 0, max = 0;
	int i;

	for (i = 0; i < task_num; i++) {
		total += tasks[i].create_tm_used;
		if (tasks[i].create_tm_used > max)
			max = tasks[i].create_tm_used;
	}

	dump("\nMaximum and average time used for each task(in mini-seconds):\n");
	total /= task_num;
	dump("  %03ld.%03ld  %03ld.%03ld\n", max / 1000, max % 1000, total / 1000, total % 1000);

}

static void do_statistic(void)
{
	uint64_t tm_used;

	dump("\n");
	dump("********* Statistic  **********\n");
	dump("Total instance number %d\n", task_num * inst_num_per_task);
	do_statistic_instance();
	do_statistic_task();

	tm_used = get_time_used(&tv_prog_start, &tv_prog_done);
	dump("\nProgram wise: %ld.%03ld seconds\n\n", tm_used / 1000000, (tm_used % 1000000)/ 1000);
}

static void cleanup(void)
{
	struct perf_inst *inst;
	int i, j;

	for (i = 0; i < task_num; i++) {
		for (j = 0; j < inst_num_per_task; j++) {
			inst = &tasks[i].insts[j];
			ibv_destroy_qp(inst->qp);
			ibv_destroy_cq(inst->cq);
			ibv_dereg_mr(inst->mr);
			ibv_dealloc_pd(inst->pd);
		}
		free(tasks[i].insts);
		ibv_close_device(tasks[i].ibctx);
	}

	free(tasks);
	ibv_free_device_list(dev_list);
}

int main(int argc, char *argv[])
{
	int ret;

	ret = parse_opt(argc, argv);
	if (ret)
		return ret;

	sem_init(&sem, 0, 0);
	ret = start_tasks();
	if (ret)
		return ret;

	sem_wait(&sem);
	info("All tasks done\n");
	do_statistic();

	info("Test done, now cleanup...\n\n");
	cleanup();
	return 0;
}
