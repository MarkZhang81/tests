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

#define NIPQUAD(addr) \
	((unsigned char *)&addr)[0], \
		((unsigned char *)&addr)[1], \
		((unsigned char *)&addr)[2], \
		((unsigned char *)&addr)[3]

static inline void dump_sockaddr_in(const char *head, struct sockaddr_in *sin)
{
	if (head)
		dump("%s:\n", head);
        dump("\t.sin_family: %d\n", sin->sin_family);
        dump("\t.sin_port: %d\n", be16toh(sin->sin_port));
        dump("\t.sin_addr: %d.%d.%d.%d\n", NIPQUAD(sin->sin_addr.s_addr));
}

static inline void dump_addrinfo(struct rdma_addrinfo *ai, int n)
{
        dump("addrinfo[%d]:\n", n);
        dump("  ai_flags: 0x%x\n", ai->ai_flags);
        dump("  ai_family: 0x%x\n", ai->ai_family);
        dump("  ai_port_space: 0x%x\n", ai->ai_port_space);
        dump("  ai_src_len: %d\n", ai->ai_src_len);
	if (ai->ai_src_addr) {
		if (ai->ai_family == AF_IB)
			dump_sockaddr_ib("  ai_src_addr (IB)",
					 (struct sockaddr_ib *)ai->ai_src_addr);
		else if (ai->ai_family == AF_INET)
			dump_sockaddr_in("  ai_src_addr (INET)",
					   (struct sockaddr_in *)ai->ai_src_addr);
	}
        dump("  ai_dst_len: %d\n", ai->ai_dst_len);
	if (ai->ai_dst_addr) {
		if (ai->ai_family == AF_IB)
			dump_sockaddr_ib("  ai_dst_addr (IB)",
					 (struct sockaddr_ib *)ai->ai_dst_addr);
		else if (ai->ai_family == AF_INET)
			dump_sockaddr_in("  ai_dst_addr (INET)",
					   (struct sockaddr_in *)ai->ai_dst_addr);
	}
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
