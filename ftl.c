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

#include "ftl.h"
#include <stdlib.h> 
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define N_GC_BLOCKS 1

#define DATA_BLOCK 1
#define TR_BLOCK 2

/*
 * CMT, GTD
 */
typedef struct {
	bool valid;
	u32 map_page;
	u32 map_entry[N_MAP_ENTRIES_PER_PAGE];
	bool dirty;
	u32 ref_time;
}CMT_t;

CMT_t **CMT;
static u32 GTD[N_BANKS][N_MAP_PAGES_PB];

/*
 * Buffer
 */
u32 **buffer;
u32 *buffer_list;
u32 *buffer_count;
bool **buffer_sector_valid;


/*
 * State of physical memory
 */
typedef struct PAGE_STATE{
	bool write;
	bool valid;
}PAGE_STATE;

typedef struct BLOCK_STATE{
	u32 nvalid;
	u32 area;
	bool full;
}BLOCK_STATE;

PAGE_STATE ***page_state;
BLOCK_STATE **blk_state;


u32 current_block_map[N_BANKS];
u32 current_block_user[N_BANKS];

static u32 ref_time = 0;

static void map_garbage_collection(u32 bank);
/* DFTL simulator
 * you must make CMT, GTD to use L2P cache
 * you must increase stats.cache_hit value when L2P is in CMT
 * when you can not find L2P in CMT, you must flush cache 
 * and load target L2P in NAND through GTD and increase stats.cache_miss value
 */


/*
 *	Initialize CMT
 */
static void init_CMT(u32 bank, u32 cache_slot)
{
	CMT[bank][cache_slot].dirty = false;
	memset(CMT[bank][cache_slot].map_entry, -1, MAP_ENTRY_SIZE * N_MAP_ENTRIES_PER_PAGE);
	CMT[bank][cache_slot].map_page = -1;
	CMT[bank][cache_slot].ref_time = -1;
	CMT[bank][cache_slot].valid = false;
	return;
}

static void map_write(u32 bank, u32 map_page, u32 cache_slot)
{
	/* you use this function when you must flush
	 * cache from CMT to NAND MAP area
	 * flush cache with LRU policy and fix GTD!!
	 */

	// get TR block
	u32 M_ppn = GTD[bank][map_page];

	// invalid old translate block
	if (M_ppn != -1) 
	{
		u32 old_bank = M_ppn / N_PPNS_PB;
		u32 old_block = (M_ppn - (N_PPNS_PB * bank)) / PAGES_PER_BLK;
		u32 old_page = (M_ppn - (N_PPNS_PB * bank)) % PAGES_PER_BLK;

		page_state[old_bank][old_block][old_page].valid = false;
		(blk_state[old_bank][old_block].nvalid)--;
	}

	// map garbage collection trigger
	u32 nfull_tr = 0;
	for (int j = 0 ; j < BLKS_PER_BANK ; j++) {
		if (blk_state[bank][j].full == true 
			&& blk_state[bank][j].area == TR_BLOCK) 
			nfull_tr++;
	}

	if (nfull_tr == N_MAP_BLOCKS_PB - N_GC_BLOCKS) {
		map_garbage_collection(bank);
	}
	
	u32 M_block = 0;
	u32 M_page = 0;

	// find new map ppn
	if (current_block_map[bank] == -1) {
		M_block = 0;
		while (blk_state[bank][M_block].full == true
				|| blk_state[bank][M_block].area == DATA_BLOCK) 
			M_block++;
		current_block_map[bank] = M_block;
	} else {
		M_block = current_block_map[bank];
	}
	blk_state[bank][M_block].area = TR_BLOCK;
	
	M_page = 0;
	while (page_state[bank][M_block][M_page].write == true) {
		M_page++;
	}
	M_ppn = (N_PPNS_PB * bank) + (PAGES_PER_BLK * M_block) + M_page;

	// write new translate block
	u32 *map_entry = malloc(PAGE_DATA_SIZE);
	memset(map_entry, 0, PAGE_DATA_SIZE);

	for (int i = 0; i < N_MAP_ENTRIES_PER_PAGE; i++) {
		map_entry[i] = CMT[bank][cache_slot].map_entry[i];
	}

	u32 *M_vpn = &map_page;
	nand_write(bank, M_block, M_page, map_entry, M_vpn);
	stats.map_write++;

	free(map_entry);

	page_state[bank][M_block][M_page].write = true;
	page_state[bank][M_block][M_page].valid = true;
	(blk_state[bank][M_block].nvalid)++;

	if (M_page == PAGES_PER_BLK - 1) {
		blk_state[bank][M_block].full = true;
		current_block_map[bank] = -1;
	}

	// modify CMT, GTD

	GTD[bank][map_page] = M_ppn;
	init_CMT(bank, cache_slot);

	return;
}
static void map_read(u32 bank, u32 map_page, u32 cache_slot)
{
	/* you use this function when you must load 
	 * L2P from NAND MAP area to CMT
	 * find L2P MAP with GTD and fill CMT!!
	 */

	u32 M_ppn = GTD[bank][map_page];

	u32 spare_lpn;
	
	u32 old_bank = M_ppn / N_PPNS_PB;
	u32 old_block = (M_ppn - (N_PPNS_PB * bank)) / PAGES_PER_BLK;
	u32 old_page = (M_ppn - (N_PPNS_PB * bank)) % PAGES_PER_BLK;

	nand_read(old_bank, old_block, old_page, CMT[bank][cache_slot].map_entry, &spare_lpn);
	stats.map_read++;

	CMT[bank][cache_slot].map_page = map_page;
	CMT[bank][cache_slot].valid = true;
	CMT[bank][cache_slot].dirty = false;
	CMT[bank][cache_slot].ref_time = ref_time;
}

static void map_garbage_collection(u32 bank)
{
	/*stats.map_gc_cnt++ every map_garbage_collection call*/
	/*stats.map_gc_write++ every nand_write call*/
	int victim = 0;
	int min_nvalid = PAGES_PER_BLK;
	u32 *valid_page = malloc(PAGE_DATA_SIZE);
	u32 M_vpn;
	u32 M_ppn;
	int page;

	int block = 0;
	while (blk_state[bank][block].full == true
			|| blk_state[bank][block].area == DATA_BLOCK) 
	{
		block++;
	}
	current_block_map[bank] = block;
	blk_state[bank][block].area = TR_BLOCK;



	for (int j = 0 ; j < BLKS_PER_BANK ; j++) {
		if (blk_state[bank][j].full == true && 
		   (blk_state[bank][j].nvalid < min_nvalid) &&
		   blk_state[bank][j].area == TR_BLOCK) 
		{
			min_nvalid = blk_state[bank][j].nvalid;
			victim = j;
		}
	}

	for (int j = 0 ; j < PAGES_PER_BLK ; j++) {
		if (page_state[bank][victim][j].valid == true) {
			nand_read(bank, victim, j, valid_page, &M_vpn);
			stats.map_gc_read ++;

			page = 0;
			while (page_state[bank][block][page].write == true) {
				page++;
			}
			

			M_ppn = (N_PPNS_PB * bank) + (PAGES_PER_BLK * block) + page;	
			GTD[bank][M_vpn] = M_ppn;

			nand_write(bank, block, page, valid_page, &M_vpn);
			stats.map_gc_write++;

			page_state[bank][victim][j].valid = false;

			page_state[bank][block][page].write = true;
			page_state[bank][block][page].valid = true;

			blk_state[bank][block].nvalid++;
		}
	}

	nand_erase(bank, victim);
	blk_state[bank][victim].full = false;
	blk_state[bank][victim].nvalid = 0;
	blk_state[bank][victim].area = 0;
	
	for (int i = 0 ; i < PAGES_PER_BLK ; i++) {
		page_state[bank][victim][i].write = false;
		page_state[bank][victim][i].valid = false;
	}

	free(valid_page);

	stats.map_gc_cnt++;
	return;
}
static void garbage_collection(u32 bank)
{
	/* stats.gc_cnt++ every garbage_collection call*/
	/* stats.gc_write++ every nand_write call*/

	int victim = 0;
	int min_nvalid = PAGES_PER_BLK + 1;
	u32 *valid_page = malloc(PAGE_DATA_SIZE);
	u32 *map_data = malloc(PAGE_DATA_SIZE);
	u32 spare;
	int page;

	int block = 0;
	while (blk_state[bank][block].full == true 
			|| blk_state[bank][block].area == TR_BLOCK) 
	{
		block++;
	}
	current_block_user[bank] = block;	
	blk_state[bank][block].area = DATA_BLOCK;


	for (int j = 0 ; j < BLKS_PER_BANK ; j++) {
		if (blk_state[bank][j].full == true && 
		   (blk_state[bank][j].nvalid < min_nvalid) &&
		   blk_state[bank][j].area == DATA_BLOCK) 
		{
			min_nvalid = blk_state[bank][j].nvalid;
			victim = j;
		}
	}

	for (int j = 0 ; j < PAGES_PER_BLK ; j++) {

		if (page_state[bank][victim][j].valid == true) {
			nand_read(bank, victim, j, valid_page, &spare);
			stats.gc_read ++;

			page = 0;
			while (page_state[bank][block][page].write == true) {
				page++;
			}
			u32 D_ppn = (N_PPNS_PB * bank) + (PAGES_PER_BLK * block) + page;	
			// PMT[spare] = ppn;

			u32 map_page = spare / (N_BANKS * N_MAP_ENTRIES_PER_PAGE);
			u32 map_offset = spare % (N_BANKS * N_MAP_ENTRIES_PER_PAGE) / (N_BANKS);

			// Data ppn 바꾸기
			u32 cmt_index = -1;
			for (int j = 0; j < N_CACHED_MAP_PAGE_PB; j++) {
				if (CMT[bank][j].map_page == map_page)
					cmt_index = j;
			}

			if (cmt_index != -1)
			{
				// CMT에 있을 때, CMT update
				CMT[bank][cmt_index].ref_time = ref_time;
				CMT[bank][cmt_index].map_page = map_page;
				CMT[bank][cmt_index].map_entry[map_offset] = D_ppn;
				CMT[bank][cmt_index].valid = true;
				CMT[bank][cmt_index].dirty = true;
			}
			else
			{
				// CMT에 없을 때, Map update and GTD update

				// map garbage collection trigger
				u32 nfull_tr = 0;
				for (int j = 0 ; j < BLKS_PER_BANK ; j++) {
					if (blk_state[bank][j].full == true 
						&& blk_state[bank][j].area == TR_BLOCK)
						nfull_tr++;
				}


				if (nfull_tr == N_MAP_BLOCKS_PB - N_GC_BLOCKS) {
					map_garbage_collection(bank);
				}

				// get TR block
				u32 M_ppn = GTD[bank][map_page];

				// invalid old translate block, read map data
				if (M_ppn != -1)
				{
					u32 old_bank = M_ppn / N_PPNS_PB;
					u32 old_block = (M_ppn - (N_PPNS_PB * bank)) / PAGES_PER_BLK;
					u32 old_page = (M_ppn - (N_PPNS_PB * bank)) % PAGES_PER_BLK;

					page_state[old_bank][old_block][old_page].valid = false;
					(blk_state[old_bank][old_block].nvalid)--;

					u32 *idle;
					nand_read(old_bank, old_block, old_page, map_data, &idle);
					stats.gc_read++;

					map_data[map_offset] = D_ppn;
				}
				
				u32 M_block = 0;
				u32 M_page = 0;

				// find new map ppn
				if (current_block_map[bank] == -1) {
					M_block = 0;
					while (blk_state[bank][M_block].full == true
							|| blk_state[bank][M_block].area == DATA_BLOCK) 
						M_block++;
					current_block_map[bank] = M_block;
				} else {
					M_block = current_block_map[bank];
				}
				blk_state[bank][M_block].area = TR_BLOCK;
				
				M_page = 0;
				while (page_state[bank][M_block][M_page].write == true) {
					M_page++;
				}
				M_ppn = (N_PPNS_PB * bank) + (PAGES_PER_BLK * M_block) + M_page;

				// write new translate block				
				u32 *M_vpn = &map_page;
				nand_write(bank, M_block, M_page, map_data, M_vpn);
				stats.gc_write++;

				page_state[bank][M_block][M_page].write = true;
				page_state[bank][M_block][M_page].valid = true;
				(blk_state[bank][M_block].nvalid)++;

				if (M_page == PAGES_PER_BLK - 1) {
					blk_state[bank][M_block].full = true;
					current_block_map[bank] = -1;
				}

				// GTD update
				GTD[bank][map_page] = M_ppn;
			}

			nand_write(bank, block, page, valid_page, &spare);
			stats.gc_write++;

			page_state[bank][victim][j].valid = false;

			page_state[bank][block][page].write = true;
			page_state[bank][block][page].valid = true;

			blk_state[bank][block].nvalid++;
		}
	}

	nand_erase(bank, victim);
	blk_state[bank][victim].full = false;
	blk_state[bank][victim].nvalid = 0;
	blk_state[bank][victim].area = 0;
	
	for (int i = 0 ; i < PAGES_PER_BLK ; i++) {
		page_state[bank][victim][i].write = false;
		page_state[bank][victim][i].valid = false;
	}

	free(valid_page);
	free(map_data);

	stats.gc_cnt++;
	return;
}
void ftl_open()
{
	nand_init(N_BANKS, BLKS_PER_BANK, PAGES_PER_BLK);

	CMT = malloc(sizeof(CMT_t *) * N_BANKS);
	for (int depth = 0; depth < N_BANKS; depth++)
	{
		CMT[depth] = malloc(sizeof(CMT_t) * N_CACHED_MAP_PAGE_PB);
	}

	for (int depth = 0; depth < N_BANKS; depth++)
	{
		for (int row = 0; row < N_CACHED_MAP_PAGE_PB; row++)
		{
			init_CMT(depth, row);
		}

		for (int map_page = 0; map_page < N_MAP_PAGES_PB; map_page++) 
		{
			GTD[depth][map_page] = -1;
		}
	}

	buffer = malloc(sizeof(u32 *) * N_BUFFERS);
	buffer_list = malloc(sizeof(u32) * N_BUFFERS);
	buffer_count = malloc(sizeof(u32));
	buffer_sector_valid = malloc(sizeof(bool *) * N_BUFFERS);

	for (int depth = 0; depth < N_BUFFERS; depth++) {
		buffer[depth] = malloc(BUFFER_SIZE / N_BUFFERS);
		buffer_sector_valid[depth] = malloc(sizeof(bool) * SECTORS_PER_PAGE);
	}

	for (int depth = 0; depth < N_BUFFERS; depth++)
	{
		for (int row = 0; row < SECTORS_PER_PAGE; row++)
		{
			buffer_sector_valid[depth][row] = false;
		} 
	}

	for (int i = 0 ; i < N_BUFFERS ; i++) {
		memset(buffer[i], -1, BUFFER_SIZE / N_BUFFERS);
		buffer_list[i] = -1;
	}
	*buffer_count = 0;

	page_state = malloc(sizeof(PAGE_STATE **) * N_BANKS);
	blk_state = malloc(sizeof(BLOCK_STATE *) * N_BANKS);
	for (int depth = 0; depth < N_BANKS; depth++)
	{
		page_state[depth] = malloc(sizeof(PAGE_STATE *) * BLKS_PER_BANK);
		blk_state[depth] = malloc(sizeof(BLOCK_STATE) * BLKS_PER_BANK);
		for (int row = 0; row < BLKS_PER_BANK; row++)
		{
			page_state[depth][row] = malloc(sizeof(PAGE_STATE) * PAGES_PER_BLK);
		}
	}

	for (int depth = 0; depth < N_BANKS; depth++)
	{
		current_block_map[depth] = -1;
		current_block_user[depth] = -1;
		for (int row = 0; row < BLKS_PER_BANK; row++)
		{
			for (int column = 0; column < PAGES_PER_BLK; column++)
			{
				page_state[depth][row][column].write = false;
				page_state[depth][row][column].valid = false;
			}
			blk_state[depth][row].nvalid = 0;
			blk_state[depth][row].full = false;
			blk_state[depth][row].area = 0;
		} 
	}
}

void ftl_read(u32 lba, u32 nsect, u32 *read_buffer)
{	
	int bank;
	int D_bank;
	int D_block;
	int D_page;
	int M_bank;
	int M_block;
	int M_page;
	u32 offset;
	u32 size;
	u32 D_ppn = 0;
	u32 M_ppn = 0;
	u32 *read_data = malloc(PAGE_DATA_SIZE);
	int *lpn = malloc(sizeof(int));
	
	int end_page = (lba + nsect) / SECTORS_PER_PAGE;
	if ((lba + nsect) % SECTORS_PER_PAGE != 0)
		end_page++;

	int start_page = lba / SECTORS_PER_PAGE;
	int npage = end_page - start_page;

	for (int i = 0 ; i < npage; i++) {
	
		*lpn = (lba / SECTORS_PER_PAGE) + i;
		bank = *lpn % N_BANKS;

		u32 map_page = *lpn / (N_BANKS * N_MAP_ENTRIES_PER_PAGE);
		u32 map_offset = *lpn % (N_BANKS * N_MAP_ENTRIES_PER_PAGE) / (N_BANKS);
	 	u32 cmt_index = -1;
		for (int j = 0; j < N_CACHED_MAP_PAGE_PB; j++) {
			if (CMT[bank][j].map_page == map_page)
				cmt_index = j;
		}

		memset(read_data, -1, SECTOR_SIZE * SECTORS_PER_PAGE);

		if (cmt_index == -1) 
		{
			// CMT에 없을 때 (miss)
			stats.cache_miss++;

			if (GTD[bank][map_page] == -1)
			{
				// NAND에 없을 때
				memset(read_data, -1, SECTOR_SIZE * SECTORS_PER_PAGE);
			}
			else
			{
				// NAND에 있을 때
				u32 spare_lpn;
				M_ppn = GTD[bank][map_page];
				
				M_bank = M_ppn / N_PPNS_PB;
				M_block = (M_ppn - (N_PPNS_PB * bank)) / PAGES_PER_BLK;
				M_page = (M_ppn - (N_PPNS_PB * bank)) % PAGES_PER_BLK;		

				u32 *read_data_map = malloc(PAGE_DATA_SIZE);
				memset(read_data_map, -1, SECTOR_SIZE * SECTORS_PER_PAGE);

				nand_read(M_bank, M_block, M_page, read_data_map, &spare_lpn);
				stats.nand_read++;

				D_ppn = read_data_map[map_offset];
				free(read_data_map);

				D_bank = D_ppn / N_PPNS_PB;
				D_block = (D_ppn - (N_PPNS_PB * bank)) / PAGES_PER_BLK;
				D_page = (D_ppn - (N_PPNS_PB * bank)) % PAGES_PER_BLK;
				nand_read(D_bank, D_block, D_page, read_data, &spare_lpn);
				stats.nand_read++;

				// CMT update

				u32 i_slot = 0;
				u32 n_vacant_slot = 0;

				// 빈 slot 개수
				while (i_slot < N_CACHED_MAP_PAGE_PB)
				{
					if (CMT[bank][i_slot].valid == false)
						n_vacant_slot++;
					i_slot++;
				}


				if (n_vacant_slot == 0) 
				{
					// slot 가득 찼을 때

					u32 i_min_val = CMT[bank][0].ref_time;
					u32 i_min = 0;

					for (int j = 1; j < N_CACHED_MAP_PAGE_PB; j++)
					{
						if (CMT[bank][j].ref_time < i_min_val)
						{
							i_min = j;
							i_min_val = CMT[bank][j].ref_time;
						}
					}

					// dirty bit check
					if (CMT[bank][i_min].dirty == true)
					{
						// map garbage collection trigger
						u32 nfull_tr = 0;
						for (int j = 0 ; j < BLKS_PER_BANK ; j++) {
							if (blk_state[bank][j].full == true 
								&& blk_state[bank][j].area == TR_BLOCK) 
								nfull_tr++;
						}

						if (nfull_tr == N_MAP_BLOCKS_PB - N_GC_BLOCKS) {
							map_garbage_collection(bank);
						}

						// flush
						map_write(bank, CMT[bank][i_min].map_page, i_min);					
					}

					// load or make
					map_read(bank, map_page, i_min);
				}
				else
				{
					// slot 빈자리 있을 때
					u32 vacant_slot = 0;
					i_slot = 0;
					while (CMT[bank][i_slot].valid == true) 
					{
						vacant_slot++;
						i_slot++;
					}
					map_read(bank, map_page, vacant_slot);
				}
			}
		}
		else
		{
			// CMT에 있을 때 (hit)
			D_ppn = CMT[bank][cmt_index].map_entry[map_offset];
			
			if (D_ppn == -1)
			{
				memset(read_data, -1, SECTOR_SIZE * SECTORS_PER_PAGE);
			}
			
			u32 spare_lpn;

			D_bank = D_ppn / N_PPNS_PB;
			D_block = (D_ppn - (N_PPNS_PB * bank)) / PAGES_PER_BLK;
			D_page = (D_ppn - (N_PPNS_PB * bank)) % PAGES_PER_BLK;

			nand_read(D_bank, D_block, D_page, read_data, &spare_lpn);
			stats.nand_read++;
			stats.cache_hit++;
		}

		if (i == 0) {
			offset = lba % SECTORS_PER_PAGE;
			if (nsect + offset < SECTORS_PER_PAGE)
				size = nsect * SECTOR_SIZE;
			else
				size = PAGE_DATA_SIZE - offset * SECTOR_SIZE;
			memcpy(read_buffer, read_data + offset, size);
			read_buffer += size / SECTOR_SIZE;
		} else if (i == npage - 1) {
			offset = (lba + nsect) % SECTORS_PER_PAGE;
			if (offset == 0)
				size = PAGE_DATA_SIZE;
			else
				size = offset * SECTOR_SIZE;
			memcpy(read_buffer, read_data, size);
			read_buffer += size / SECTOR_SIZE;
		} else {
			size = PAGE_DATA_SIZE;
			memcpy(read_buffer, read_data, size);
			read_buffer += SECTORS_PER_PAGE;
		}
	}

	free(read_data);
	free(lpn);
	stats.host_read += nsect;
	return;
}

void ftl_write(u32 lba, u32 nsect, u32 *write_buffer)
{
	/* stats.nand_write++ every nand_write call*/
	int *lpn = malloc(sizeof(int));
	u32 D_ppn = 0;
	int bank;
	int D_block;
	int D_page;
	int old_D_ppn = -1;
	int old_bank;
	int old_block;
	int old_page;
	u32 *write_data = malloc(PAGE_DATA_SIZE);

	int end_page = (lba + nsect) / SECTORS_PER_PAGE;
	if ((lba + nsect) % SECTORS_PER_PAGE != 0)
		end_page++;

	int start_page = lba / SECTORS_PER_PAGE;
	int npage = end_page - start_page;
	int offset;
	int size;


	for (int i = 0 ; i < npage; i++) {
		memset(write_data, -1, PAGE_DATA_SIZE);

		*lpn = (lba / SECTORS_PER_PAGE) + i;
		bank = *lpn % N_BANKS;

		u32 nfull_data = 0;
		for (int j = 0 ; j < BLKS_PER_BANK ; j++) {
			if (blk_state[bank][j].full == true 
				&& blk_state[bank][j].area == DATA_BLOCK) 
			{
				nfull_data++;
			}
		}

		if (nfull_data == N_USER_BLOCKS_PB - N_GC_BLOCKS) {
			garbage_collection(bank);
		}

		// data ppn
		if (current_block_user[bank] == -1) {
			D_block = 0;
			while (blk_state[bank][D_block].full == true
					|| blk_state[bank][D_block].area == TR_BLOCK) 
			{
				D_block++;
			}
			current_block_user[bank] = D_block;
		} else {
			D_block = current_block_user[bank];
		}

		D_page = 0;
		while (page_state[bank][D_block][D_page].write == true) {
			D_page++;
		}
		D_ppn = (N_PPNS_PB * bank) + (PAGES_PER_BLK * D_block) + D_page;
		blk_state[bank][D_block].area = DATA_BLOCK;

		u32 map_page = *lpn / (N_BANKS * N_MAP_ENTRIES_PER_PAGE);
		u32 map_offset = *lpn % (N_BANKS * N_MAP_ENTRIES_PER_PAGE) / (N_BANKS);
	 	u32 cmt_index = -1;

		for (int j = 0; j < N_CACHED_MAP_PAGE_PB; j++) {
			if (CMT[bank][j].map_page == map_page)
				cmt_index = j;
		}

		if (cmt_index == -1) 
		{
			// CMT에 없을 때 (miss)
			stats.cache_miss++;

			u32 i_slot = 0;
			u32 n_vacant_slot = 0;

			// 빈 slot 개수
			while (i_slot < N_CACHED_MAP_PAGE_PB)
			{
				if (CMT[bank][i_slot].valid == false)
					n_vacant_slot++;
				i_slot++;
			}

			if (n_vacant_slot == 0) 
			{
				// slot 가득 찼을 때

				u32 i_min_val = CMT[bank][0].ref_time;
				u32 i_min = 0;

				for (int j = 1; j < N_CACHED_MAP_PAGE_PB; j++)
				{
					if (CMT[bank][j].ref_time < i_min_val)
					{
						i_min = j;
						i_min_val = CMT[bank][j].ref_time;
					}
				}

				// dirty bit check
				if (CMT[bank][i_min].dirty == true)
				{
					// map garbage collection trigger
					u32 nfull_tr = 0;
					for (int j = 0 ; j < BLKS_PER_BANK ; j++) {
						if (blk_state[bank][j].full == true 
							&& blk_state[bank][j].area == TR_BLOCK) 
							nfull_tr++;
					}

					if (nfull_tr == N_MAP_BLOCKS_PB - N_GC_BLOCKS) {
						map_garbage_collection(bank);
					}

					// flush
					map_write(bank, CMT[bank][i_min].map_page, i_min);						
				}

				init_CMT(bank, i_min);
				
				// load or make
				if (GTD[bank][map_page] != -1)
				{
					map_read(bank, map_page, i_min);
					old_D_ppn = CMT[bank][i_min].map_entry[map_offset];
				}
				else
				{
					old_D_ppn = CMT[bank][i_min].map_entry[map_offset];

					CMT[bank][i_min].ref_time = ref_time;
					CMT[bank][i_min].map_page = map_page;
					CMT[bank][i_min].valid = true;
					CMT[bank][i_min].dirty = false;
				}		 
				CMT[bank][i_min].map_entry[map_offset] = D_ppn;
				CMT[bank][i_min].dirty = true;
			}
			else
			{
				// slot 빈자리 있을 때
				u32 vacant_slot = 0;
				i_slot = 0;
				while (CMT[bank][i_slot].valid == true) 
				{
					vacant_slot++;
					i_slot++;
				}

				if (GTD[bank][map_page] != -1)
				{
					map_read(bank, map_page, vacant_slot);
					old_D_ppn = CMT[bank][vacant_slot].map_entry[map_offset];
				}
				else
				{
					old_D_ppn = CMT[bank][vacant_slot].map_entry[map_offset];

					CMT[bank][vacant_slot].ref_time = ref_time;
					CMT[bank][vacant_slot].map_page = map_page;
					CMT[bank][vacant_slot].map_entry[map_offset] = D_ppn;
					CMT[bank][vacant_slot].valid = true;
					CMT[bank][vacant_slot].dirty = true;
				}
			}
		} 
		else 
		{
			// CMT에 있을 때 (hit)
			stats.cache_hit++;

			old_D_ppn = CMT[bank][cmt_index].map_entry[map_offset];

			CMT[bank][cmt_index].ref_time = ref_time;
			CMT[bank][cmt_index].map_page = map_page;
			CMT[bank][cmt_index].map_entry[map_offset] = D_ppn;
			CMT[bank][cmt_index].valid = true;
			CMT[bank][cmt_index].dirty = true;
		}

		// old data invalid, load
		if (old_D_ppn != -1)
		{
			u32 spare_lpn;
			
			old_bank = old_D_ppn / N_PPNS_PB;
			old_block = (old_D_ppn - (N_PPNS_PB * bank)) / PAGES_PER_BLK;
			old_page = (old_D_ppn - (N_PPNS_PB * bank)) % PAGES_PER_BLK;

			page_state[old_bank][old_block][old_page].valid = false;
			if (blk_state[old_bank][old_block].nvalid > 0)
				blk_state[old_bank][old_block].nvalid--;

			nand_read(old_bank, old_block, old_page, write_data, &spare_lpn);
			stats.nand_read++;
		}

		// write data page
		if (i == 0) {
			offset = lba % SECTORS_PER_PAGE;
			
			if (nsect + offset < SECTORS_PER_PAGE)
				size = nsect * SECTOR_SIZE;
			else 
				size = PAGE_DATA_SIZE - offset * SECTOR_SIZE;
			memcpy(write_data + offset, write_buffer, size);
			write_buffer += size / SECTOR_SIZE;
		} else if (i == npage - 1) {
			offset = (lba + nsect) % SECTORS_PER_PAGE;
			if (offset == 0) {
				size = PAGE_DATA_SIZE;
				memcpy(write_data, write_buffer, size);
			}
			else {
				size = offset * SECTOR_SIZE;
				memcpy(write_data, write_buffer, size);
				write_buffer += SECTORS_PER_PAGE;
			}
				
		} else {
			size = PAGE_DATA_SIZE;
			memcpy(write_data, write_buffer, size);
			write_buffer += SECTORS_PER_PAGE;
		}

		nand_write(bank, D_block, D_page, write_data, lpn);
		stats.nand_write++;

		page_state[bank][D_block][D_page].write = true;
		page_state[bank][D_block][D_page].valid = true;
		(blk_state[bank][D_block].nvalid)++;

		if (D_page == PAGES_PER_BLK - 1) {
			blk_state[bank][D_block].full = true;
			current_block_user[bank] = -1;
		}
	}
	free(lpn);
	free(write_data);
	stats.host_write += nsect;
	ref_time++;
	return;
}