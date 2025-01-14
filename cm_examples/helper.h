#include <stdio.h>
#include <sys/time.h>

#include "params.h"

#define BUFSIZE 4096

#define INFO printf
#define ERR printf

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
