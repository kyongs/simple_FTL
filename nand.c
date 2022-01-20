#include "header.h"

struct page {
	int pbn, page_offset;	/* physical block number, page offset in block */ 
	int lpn; 		/* logical page number for debug */
};

struct block {
	struct page page[PAGES_PER_BLOCK];
	int last_page_offset; /* current page offset, for debug */
};

struct nand {
	struct block block[BLOCKS_PER_NAND];
};

struct nand_stats {
	int total_nand_page_program;
	int total_nand_page_read;
	int total_block_erase_count;
};

struct nand nand;
struct nand_stats stats;

/* Initizalization */
void nand_init(void)
{
	int block_idx, page_idx;

	for (block_idx = 0; block_idx < (BLOCKS_PER_NAND); block_idx++) {
		nand.block[block_idx].last_page_offset = 0;

		for (page_idx = 0; page_idx < PAGES_PER_BLOCK; page_idx++) {
			nand.block[block_idx].page[page_idx].pbn = block_idx;
			nand.block[block_idx].page[page_idx].page_offset = page_idx;
			nand.block[block_idx].page[page_idx].lpn = -1; /* -1 means invalid page */
		}
	}

	stats.total_nand_page_program = 0;
	stats.total_nand_page_read = 0;
	stats.total_block_erase_count = 0;
}

//nand에 write
void nand_page_program(int block_idx, int page_idx, int data_lpn)
{
	//validation check
	assert(block_idx < BLOCKS_PER_NAND);
	assert(page_idx < PAGES_PER_BLOCK);
	assert(nand.block[block_idx].last_page_offset == page_idx);

	// nand block 내 해당되는 idx에 삽입
	nand.block[block_idx].page[page_idx].lpn = data_lpn;
	nand.block[block_idx].last_page_offset++;

	stats.total_nand_page_program++;
}

int nand_page_read(int block_idx, int page_idx)
{
	assert(block_idx < BLOCKS_PER_NAND);
	assert(page_idx < PAGES_PER_BLOCK);

	stats.total_nand_page_read++;

	return nand.block[block_idx].page[page_idx].lpn;
}

/* 블록 안에 원소들을 모두 초기화, 모두 invalid 시킴 */
void nand_block_erase(int block_idx)
{
	int page_idx;

	assert(block_idx < BLOCKS_PER_NAND);

	nand.block[block_idx].last_page_offset = 0;

	for (page_idx = 0; page_idx < PAGES_PER_BLOCK; page_idx++)
		nand.block[block_idx].page[page_idx].lpn = -1; /* -1 means invalid page */

	stats.total_block_erase_count++;
}

int nand_print_stats(void)
{
	printf("nand_stats: total_nand_page_program:: %d\n",
	       stats.total_nand_page_program);
	printf("nand_stats: total_nand_page_read::    %d\n",
	       stats.total_nand_page_read);
	printf("nand_stats: total_block_erase_count:: %d\n",
	       stats.total_block_erase_count);

	return stats.total_nand_page_program;
}
