#include "swap.h"
#include <stdbool.h>
#include <bitmap.h>
#include "threads/vaddr.h"
#include "devices/block.h"


static struct block* swap_block;
static struct bitmap* swap_map;

/*
	defined in block.h
	#define BLOCK_SECTOR_SIZE 512
	sector isze is 512 byte.
*/

static size_t SECTORS_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;

void swap_init(){
	swap_block = block_get_role(BLOCK_SWAP);
	if(swap_block == NULL){
		PANIC("no swap block allocated.");
	}

	//bitmap_size == page repository size
	int bitmap_size = block_size (swap_block) / SECTORS_PER_PAGE;
	swap_map = bitmap_create(bitmap_size);
	if(swap_map == NULL){
		PANIC("no swap bitmap created.");
	}

	bitmap_set_all(swap_map, false);
}

void swap_in(size_t used_index, void* kaddr){
	int i;
	for(i=0; i<SECTORS_PER_PAGE; ++i){
		block_read(swap_block, used_index*SECTORS_PER_PAGE+i, kaddr+i*BLOCK_SECTOR_SIZE);
	}

	bitmap_flip(swap_block, used_index);
}

size_t swap_out(void* kaddr){
	int i;
	size_t index = bitmap_scan_and_flip(swap_map, 0, 1, false);
	if(index == BITMAP_ERROR){
		return SWAP_ERROR;
	}
	for(i=0; i<SECTORS_PER_PAGE; ++i){
		block_write(swap_block, index*SECTORS_PER_PAGE+i, kaddr+i*BLOCK_SECTOR_SIZE);
	}

	return index;
}