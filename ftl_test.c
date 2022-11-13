/*
 * Project1 : Custom DFTL Simulator
 *  - Embedded Systems Design, ICE3028 (Fall, 2022)
 *
 * Nov. 1, 2022.
 *
 * TA: Jinwoo Jeong, Jeeyoon Jung
 * Prof: Dongkun Shin
 * Embedded Software Laboratory
 * Sungkyunkwan University
 * http://nyx.skku.ac.kr
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "ftl.h"


struct ftl_stats stats;

static void show_info(void)
{
	printf("Bank: %d\n", N_BANKS);
	printf("Blocks / Bank: %d blocks\n", BLKS_PER_BANK);
	printf("Pages / Block: %d pages\n", PAGES_PER_BLK);
	printf("Sectors per Page: %lu\n", SECTORS_PER_PAGE);
	printf("OP ratio: %d%%\n", OP_RATIO);
	printf("Physical Blocks: %d\n", N_BLOCKS);
	printf("User Blocks: %d\n", (int)N_USER_BLOCKS);
	printf("OP Blocks: %d\n", (int)N_OP_BLOCKS);
	printf("PPNs: %d\n", N_PPNS);
	printf("LPNs: %d\n", (int)N_LPNS);
	printf("\n");
}

static u32 get_data()
{
	return rand() & 0xff;
}

static void show_stat(void)
{
	printf("\nResults ------\n");
	printf("Host read: %d, writes: %d\n", stats.host_read, stats.host_write);
	printf("Nand read: %d, writes: %d\n", stats.nand_read, stats.nand_write);
	printf("GC read: %d, writes: %d\n", stats.gc_read, stats.gc_write);
	printf("Number of GCs: %d\n", stats.gc_cnt);
	printf("MAP read : %ld, MAP writes : %ld\n", stats.map_read, stats.map_write);
	printf("Number of MAP GCs : %d\n", stats.map_gc_cnt);
	printf("Number of MAP GC read : %ld, Number of MAP GC write : %ld\n",stats.map_gc_read, stats.map_gc_write);
	printf("Valid pages per GC: %.2f pages\n", (double)stats.gc_write / stats.gc_cnt);
	printf("Valid pages per Map GC: %.2f pages\n", (double)stats.map_gc_write / stats.map_gc_cnt);
	printf("Cache hit rate : %.2f %%\n", (double)(stats.cache_hit*100. / (stats.cache_hit + stats.cache_miss)));
	printf("WAF: %.2f\n", (double)((stats.nand_write + stats.gc_write + stats.map_write + stats.map_gc_write) * 8.0 / stats.host_write));
	printf("RAF : %.2f\n", (double)((stats.nand_read + stats.gc_read + stats.map_read + stats.map_gc_read) * 8.0 / stats.host_read));

}

int main(int argc, char **argv)
{
	if (argc >= 2 && !freopen(argv[1], "r", stdin)) {
		perror("freopen in");
		return EXIT_FAILURE;
	}
	if (argc >= 3 && !freopen(argv[2], "w", stdout)) {
		perror("freopen out");
		return EXIT_FAILURE;
	}

	int seed;
	if (scanf("S %d", &seed) < 1) {
		fprintf(stderr, "wrong input format\n");
		return EXIT_FAILURE;
	}
	srand(seed);

	ftl_open();
	show_info();

	while (1) {
		int i;
		char op;
		u32 lba;
		u32 nsect;
		u32 *buf;
		if (scanf(" %c", &op) < 1)
			break;
		switch (op) {
		case 'R':
			scanf("%d %d", &lba, &nsect);
                        assert(lba >= 0 && lba + nsect <= N_LPNS * SECTORS_PER_PAGE);
			buf = malloc(SECTOR_SIZE * nsect);
			ftl_read(lba, nsect, buf);
			printf("Read(%u,%u): [ ", lba, nsect);
			for (i = 0; i < nsect; i++)
				printf("%2x ", buf[i]);
			printf("]\n");
                        free(buf);
			break;
		case 'W':
			scanf("%d %d", &lba, &nsect);
                        assert(lba >= 0 && lba + nsect <= N_LPNS * SECTORS_PER_PAGE);
			buf = malloc(SECTOR_SIZE * nsect);
			for (i = 0; i < nsect; i++)
				buf[i] = get_data();
			ftl_write(lba, nsect, buf);
			printf("Write(%u,%u): [ ", lba, nsect);
			for (i = 0; i < nsect; i++)
				printf("%2x ", buf[i]);
			printf("]\n");
                        free(buf);
			break;
		default:
			fprintf(stderr, "Wrong op type\n");
			return EXIT_FAILURE;
		}
	}

	show_stat();
	return 0;
}
