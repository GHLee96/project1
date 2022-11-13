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
#pragma once


#include "nand.h"

//#define NO_CACHE
typedef unsigned int 		u32;

#define SECTOR_SIZE					sizeof(u32)
#define N_BANKS						8
#define BLKS_PER_BANK				40
#define PAGES_PER_BLK				16
#define SECTORS_PER_PAGE			(PAGE_DATA_SIZE / sizeof(u32))

#define OP_RATIO					7

#define N_BUFFERS					(10)
#define BUFFER_SIZE					(N_BUFFERS*SECTORS_PER_PAGE*SECTOR_SIZE)

#define MAP_ENTRY_SIZE				(sizeof(u32))
#define N_MAP_ENTRIES_PER_PAGE		(PAGE_DATA_SIZE / MAP_ENTRY_SIZE)

/* Per Bank */
#define CMT_RATIO					5
#define CMT_SIZE_PB					(BLKS_PER_BANK * PAGES_PER_BLK * sizeof(u32) * CMT_RATIO / 100) // 5% of total map table

#define N_CACHED_MAP_PAGE_PB_TEMP	((const int)(CMT_SIZE_PB / (MAP_ENTRY_SIZE * N_MAP_ENTRIES_PER_PAGE)))
#define N_CACHED_MAP_PAGE_PB		((0<N_CACHED_MAP_PAGE_PB_TEMP)?(N_CACHED_MAP_PAGE_PB_TEMP):(1))

#define N_PPNS_PB					(BLKS_PER_BANK * PAGES_PER_BLK)
#define N_MAP_PAGES_PB				(N_PPNS_PB / N_MAP_ENTRIES_PER_PAGE)

#define N_MAP_BLOCKS_PB				(N_MAP_PAGES_PB / PAGES_PER_BLK)
#define N_USER_BLOCKS_PB			(((BLKS_PER_BANK - N_MAP_BLOCKS_PB) * 100) / (100 + OP_RATIO))
#define N_OP_BLOCKS_PB				(BLKS_PER_BANK - N_MAP_BLOCKS_PB - N_USER_BLOCKS_PB)

#define MAP_OP_RATIO				(0.4)
#define USER_OP_RATIO				(0.6)

#define N_USER_OP_BLOCKS_PB			((int)(N_OP_BLOCKS_PB*USER_OP_RATIO))
#define N_MAP_OP_BLOCKS_PB			(N_OP_BLOCKS_PB - N_USER_OP_BLOCKS_PB)

#define N_PHY_DATA_BLK				(N_USER_OP_BLOCKS_PB + N_USER_BLOCKS_PB)
#define N_PHY_MAP_BLK				(N_MAP_OP_BLOCKS_PB + N_MAP_BLOCKS_PB)

#define N_LPNS_PB					((N_USER_BLOCKS_PB) * PAGES_PER_BLK)

#define N_PPNS						(N_PPNS_PB * N_BANKS)
#define N_BLOCKS					(BLKS_PER_BANK * N_BANKS)
#define N_MAP_BLOCKS				(N_MAP_BLOCKS_PB * N_BANKS)
#define N_USER_BLOCKS				(N_USER_BLOCKS_PB * N_BANKS)
#define N_OP_BLOCKS					(N_OP_BLOCKS_PB * N_BANKS)

#define N_LPNS						(N_LPNS_PB * N_BANKS)
#define N_LBAS						(N_LPNS * SECTORS_PER_PAGE)

struct ftl_stats {
	int gc_cnt;
	int map_gc_cnt;
	long host_write, host_read;
	long nand_write, nand_read;
	long gc_write, gc_read;
	long map_write, map_read;
	long map_gc_write, map_gc_read;
	long cache_hit;
	long cache_miss;
};

extern struct ftl_stats stats;

void ftl_open();
void ftl_write(u32 lba, u32 num_sectors, u32 *write_buffer);
void ftl_read(u32 lba, u32 num_sectors, u32 *read_buffer);