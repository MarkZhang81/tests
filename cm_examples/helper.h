#include <stdio.h>
#include <sys/time.h>

#include <infiniband/ib.h>
#include <rdma/rdma_cma.h>

#include "params.h"

#define BUFSIZE 4096

#if defined(LOG_LEVEL)
#define INFO(fmt, args...) \
	printf("=MZINFO:%s:%d: " fmt "\n", __func__, __LINE__, ##args)
#else
#define INFO
#endif

#define ERR(fmt, args...) \
	fprintf(stderr, "=MZERR:%s:%d(%d:%s) " fmt "\n", __func__, __LINE__, errno, strerror(errno), ##args)

#define dump printf

static inline void dump_sockaddr_ib(const char *head, struct sockaddr_ib *sib)
{
	if (head)
		dump("%s:\n", head);
        dump("\t.sib_family: 0x%x\n", sib->sib_family);
        dump("\t.sib_pkey: 0x%x\n", be16toh(sib->sib_pkey));
        dump("\t.sib_addr: %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n",
	     htobe16(sib->sib_addr.sib_addr16[0]), htobe16(sib->sib_addr.sib_addr16[1]),
	     htobe16(sib->sib_addr.sib_addr16[2]), htobe16(sib->sib_addr.sib_addr16[3]),
	     htobe16(sib->sib_addr.sib_addr16[4]), htobe16(sib->sib_addr.sib_addr16[5]),
	     htobe16(sib->sib_addr.sib_addr16[6]), htobe16(sib->sib_addr.sib_addr16[7]));
        dump("\t.sib_sid: 0x%lx\n", be64toh(sib->sib_sid));
        dump("\t.sib_sid_mask: 0x%lx\n", be64toh(sib->sib_sid_mask));
        dump("\t.sib_scope_id: 0x%lx\n", be64toh(sib->sib_scope_id));
}

static inline void dump_addrinfo(struct rdma_addrinfo *ai, int n)
{
        dump("addrinfo[%d]:\n", n);
        dump("  ai_flags: 0x%x\n", ai->ai_flags);
        dump("  ai_family: 0x%x\n", ai->ai_family);
        dump("  ai_port_space: 0x%x\n", ai->ai_port_space);
        dump("  ai_src_len: %d\n", ai->ai_src_len);
	if (ai->ai_src_addr && ai->ai_family == AF_IB)
		dump_sockaddr_ib("  ai_src_addr", (struct sockaddr_ib *)ai->ai_src_addr);
        dump("  ai_dst_len: %d\n", ai->ai_dst_len);
	if (ai->ai_dst_addr && ai->ai_family == AF_IB)
		dump_sockaddr_ib("  ai_dst_addr", (struct sockaddr_ib *)ai->ai_dst_addr);
}

/*
static inline void INFO(const char *s)
{
	struct timeval tv;

	if (!LOG_LEVEL)
		return;

	gettimeofday(&tv, NULL);
	printf("INFO: %ld.%03ld: %s\n", tv.tv_sec, tv.tv_usec/1000, s);
}

static inline void ERR(const char *s)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	fprintf(stderr, "ERR: %ld.%03ld: %s\n", tv.tv_sec, tv.tv_usec/1000, s);
}
*/
