#include <stdio.h>
#include <stdlib.h>

int data[1024 * 1024];
int data_udpsport[0xFFFF+1];

int argv_to_int(char *argv)
{
	printf("argv[1] = '%s' (%d)\n", argv, *argv);
	char *p;
	long num = strtol(argv, &p, 10);
	return num;
}

unsigned long get_udp_sport(unsigned long fl)
{
	unsigned long fl_low = fl & 0x03fff, fl_high = fl & 0xFC000;

	fl_low ^= fl_high >> 14;
	return (unsigned long)(fl_low | 0xC000);
}

unsigned long get_flow_lable_folding_16_8(unsigned long sport, unsigned long dport)
{
	unsigned long fl;

	fl = sport * dport;
	fl ^= fl >> 16;
	fl ^= fl >> 8;
	fl &= 0xFFFFF;

	return fl;
}

unsigned long get_flow_lable_unfolding(unsigned long sport, unsigned long dport)
{
	unsigned long fl;

	fl = sport * dport;
	fl &= 0xFFFFF;

	return fl;
}

unsigned long get_flow_lable_multiple31(unsigned long sport, unsigned long dport)
{
	unsigned long fl;

	fl = sport * 31 + dport;
	fl &= 0xFFFFF;

	return fl;
}

/* 1st arg: Folding or Nonfloding hash */
/* 2nd arg: dump to csv */
int main(int argc, char **argv)
{
        unsigned long src, dst;
        unsigned long fl, udp_sport;
	FILE *fp;
	int hash_fold = 1;

	if (argc > 1) {
		if (1 == argv_to_int(argv[1]))
			hash_fold = 0;
	}
        printf("%s hash\n", hash_fold ? "Folding" : "Non-folding");

	if (argc > 2) {
		fp = fopen("./hashout.csv", "w");
        	printf("dump output to csv: hashout.csv, fp %p\n", fp);
	}

	if (argc > 2) {
        	fprintf(fp, "%s hash\nbucket,hits\n", hash_fold ? "Folding" : "Non-folding");
	} else {
        	printf("%s hash\nbucket\thits\n", hash_fold ? "Folding" : "Non-folding");
	}

	int dst_start = 0x4800, dst_end = dst_start + 0;
        for (src = 1024; src < 0xFFFF; src++)
                for (dst = dst_start; dst <= dst_end; dst++) {
			fl = get_flow_lable_multiple31(src, dst);
			//fl = get_flow_lable_unfolding(src, dst);
			udp_sport = get_udp_sport(fl);
                        data[fl]++;
			data_udpsport[udp_sport] ++;
                }
        int i, c=0;
        //for (i = 0; i < 1024 * 1024; i++) {
        for (i = 0xC000; i < 0xFFFF + 1; i++) {
		int d = data_udpsport[i];
		if (1 || d<100 || d>1500) {
			c++;
			if (argc > 2) {
				fprintf(fp, "%d,%d\n", i, d);
			} else {
		                printf("%#x\t%d\n", i, d);
			}
		}
	}

	if (argc > 2) {
		fprintf(fp, "c=%d\n", c);
		fclose(fp);
	} else {
		printf("c=%d\n", c);
	}

        return 0;
}
