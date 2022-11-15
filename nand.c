/*
 * Lab #1 : NAND Simulator
 *  - Embedded Systems Design, ICE3028 (Fall, 2022)
 *
 * Sep. 13, 2022.
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
#include <stdbool.h>

#include "ftl3.h"
#include "nand.h"

/*
 * define your own data structure for NAND flash implementation
 */

typedef struct page{
	unsigned int data[PAGE_DATA_SIZE / sizeof(unsigned int)];
	unsigned int spare[PAGE_SPARE_SIZE / sizeof(unsigned int)];
}page;
page ***memory;

bool ***meta_data;

#define WRITING -2
#define NOWRITING -1
int *pre_write;
int *info;


unsigned int initial_data[PAGE_DATA_SIZE / sizeof(unsigned int)];
unsigned int initial_spare[PAGE_SPARE_SIZE / sizeof(unsigned int)];

/*
 * initialize the NAND flash memory
 * @nbanks: number of bank
 * @nblks: number of blocks per bank
 * @npages: number of pages per block
 *
 * Returns:
 *   0 on success
 *   NAND_ERR_INVALID if given dimension is invalid
 */
int nand_init(int nbanks, int nblks, int npages)
{
	if (nbanks <= 0 || 
		nblks <= 0 || 
		npages <= 0) {
		return NAND_ERR_INVALID;
	}

	for (int i = 0; i < PAGE_DATA_SIZE / sizeof(unsigned int); i++) {
		initial_data[i] = 0xffffffff;
	}
	for (int i = 0; i < PAGE_SPARE_SIZE / sizeof(unsigned int); i++) {
		initial_spare[i] = 0xffffffff;
	}

	memory = malloc((sizeof(page **)) * nbanks);
	meta_data = malloc(sizeof(bool **) * nbanks);
	for (int depth = 0; depth < nbanks; depth++)
	{
		memory[depth] = malloc(sizeof(page *) * nblks);
		meta_data[depth] = malloc(sizeof(bool *) * nblks);

		for (int row = 0; row < nblks; row++)
		{
			memory[depth][row] = malloc(sizeof(page) * npages);
			meta_data[depth][row] = malloc(sizeof(bool) * npages);

		}
	}

	for (int depth = 0; depth < nbanks; depth++)
	{
		for (int row = 0; row < nblks; row++)
		{
			for (int column = 0; column < npages; column++)
			{
				for (int ndata = 0; ndata < PAGE_DATA_SIZE / sizeof(unsigned int); ndata++) {
					memory[depth][row][column].data[ndata] = 0xffffffff;
				}
				for (int nspare = 0; nspare < PAGE_SPARE_SIZE / sizeof(unsigned int); nspare++) {
					memory[depth][row][column].spare[nspare] = 0xffffffff;
				}
				meta_data[depth][row][column] = false;
			}
		} 
	}

	pre_write = malloc(sizeof(int) * 3);
	info = malloc(sizeof(int) * 3);
	for (int i = 0 ; i < 4 ; i++) {
		pre_write[i] = -1;
	}

	info[0] = nbanks;
	info[1] = nblks;
	info[2] = npages;
	return NAND_SUCCESS;
}

/*
 * write data and spare into the NAND flash memory page
 *
 * Returns:
 *   0 on success
 *   NAND_ERR_INVALID if target flash page address is invalid
 *   NAND_ERR_OVERWRITE if target page is already written
 *   NAND_ERR_POSITION if target page is empty but not the position to be written
 */
int nand_write(int bank, int blk, int page, void *data, void *spare)
{
	if (bank < 0 || blk < 0 || page < 0 ||
		bank >= info[0] || blk >= info[1] || page >= info[2]) {
		return NAND_ERR_INVALID;
	}

	/*
	if (memcmp(memory[bank][blk][page].data, initial_data, sizeof(initial_data)) &&
		memcmp(memory[bank][blk][page].spare, initial_spare, sizeof(initial_spare))) {
		return NAND_ERR_OVERWRITE;
	}
	*/
	if (meta_data[bank][blk][page] == true) {
		return NAND_ERR_OVERWRITE;
	}

	if ((pre_write[3] == WRITING && 
	    (pre_write[0] == bank && pre_write[1] == blk) &&
	   !(pre_write[2] == page - 1)))
	{
		return NAND_ERR_POSITION;
	}

	/*
	for (int i = 0 ; i < info[2] ; i++) {
		if (memcmp(memory[bank][blk][i].data, initial_data, sizeof(initial_data)) ||
			memcmp(memory[bank][blk][i].spare, initial_spare, sizeof(initial_spare)))
		{
            if (i == page - 1)
                break;
            else
                NAND_ERR_POSITION;
		}
		if (i == info[2] - 1 && page != 0) 
		{
			return NAND_ERR_POSITION;
		}
	}
	*/
	for (int i = 0 ; i < info[2] ; i++) {
		if (meta_data[bank][blk][i] == true)
		{
            if (i == page - 1)
                break;
            else
                NAND_ERR_POSITION;
		}
		if (i == info[2] - 1 && page != 0) 
		{
			printf("+ [%d] ", page);
			return NAND_ERR_POSITION;
		}
	}

	memcpy(memory[bank][blk][page].data, data, sizeof(memory[bank][blk][page].data));
	memcpy(memory[bank][blk][page].spare, spare, sizeof(memory[bank][blk][page].spare));

	pre_write[0] = bank;
	pre_write[1] = blk;
	pre_write[2] = page;
	pre_write[3] = WRITING;

	meta_data[bank][blk][page] = true;

	u32 a = memory[bank][blk][page].spare[0];
/* 	if (a == 3583) {
		printf("+++ \n\n\n\n");
					for (int n = 0 ; n < SECTORS_PER_PAGE ; n++)
						printf("%2x ", memory[bank][blk][page].data[n]);
		} */
	


	//if (a == 255 || a == 256) {
/* 		for (int k = 0 ; k < N_BANKS; k++) {
			for (int l = 0 ; l < BLKS_PER_BANK ; l++) {
				for (int m = 0 ; m < PAGES_PER_BLK ; m++) {
					for (int n = 0 ; n < SECTORS_PER_PAGE ; n++)
						printf("%2x ", memory[k][l][m].data[n]);
				}
				printf("\n\n");
			}
			printf("===============\n");
		} */
	//}
			


	return NAND_SUCCESS;
}


/*
 * read data and spare from the NAND flash memory page
 *
 * Returns:
 *   0 on success
 *   NAND_ERR_INVALID if target flash page address is invalid
 *   NAND_ERR_EMPTY if target page is empty
 */
int nand_read(int bank, int blk, int page, void *data, void *spare)
{
	if (bank < 0 || blk < 0 || page < 0 ||
		bank >= info[0] || blk >= info[1] || page >= info[2]) {
		return NAND_ERR_INVALID;
	}

	pre_write[3] = NOWRITING;
	
	/*
	if (!memcmp(memory[bank][blk][page].data, initial_data, sizeof(initial_data)) &&
		!memcmp(memory[bank][blk][page].spare, initial_spare, sizeof(initial_spare))) 
	{
		return NAND_ERR_EMPTY;
	}
	*/

	if (meta_data[bank][blk][page] == false) {
		return NAND_ERR_EMPTY;
	}

	memcpy(data, memory[bank][blk][page].data, sizeof(memory[bank][blk][page].data));
	memcpy(spare, memory[bank][blk][page].spare, sizeof(memory[bank][blk][page].spare));

	return NAND_SUCCESS;
}

/*
 * erase the NAND flash memory block
 *
 * Returns:
 *   0 on success
 *   NAND_ERR_INVALID if target flash block address is invalid
 *   NAND_ERR_EMPTY if target block is already erased
 */
int nand_erase(int bank, int blk)
{
	pre_write[3] = NOWRITING;
	if (bank < 0 || blk < 0 ||
		bank >= info[0] || blk >= info[1]) {
		return NAND_ERR_INVALID;
	}

	/*
	for (int i = 0 ; i < info[2] ; i++) {
		if (memcmp(memory[bank][blk][i].data, initial_data, sizeof(initial_data)) ||
			memcmp(memory[bank][blk][i].spare, initial_spare, sizeof(initial_spare)))
		{
			break;
		}
		if (i == info[2] - 1) 
			return NAND_ERR_EMPTY;
	}
	*/
	for (int i = 0 ; i < info[2] ; i++) {
		if (meta_data[bank][blk][i] == true)
		{
			break;
		}
		if (i == info[2] - 1) 
			return NAND_ERR_EMPTY;
	}

	
	for (int i = 0 ; i < info[2] ; i++) {
		memcpy(memory[bank][blk][i].data, initial_data, sizeof(initial_data));
		memcpy(memory[bank][blk][i].spare, initial_spare, sizeof(initial_spare));

		meta_data[bank][blk][i] = false;
	}

	return NAND_SUCCESS;
}
