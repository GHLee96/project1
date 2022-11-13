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


static void map_write(u32 bank, u32 map_page, u32 cache_slot)
{

}
static void map_read(u32 bank, u32 map_page, u32 cache_slot)
{
}

static void map_garbage_collection(u32 bank)
{

}
static void garbage_collection(u32 bank)
{
}
void ftl_open()
{

}

void ftl_read(u32 lba, u32 nsect, u32 *read_buffer)
{	

}

void ftl_write(u32 lba, u32 nsect, u32 *write_buffer)
{
	stats.host_write += lba; 
}
