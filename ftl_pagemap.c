#include "header.h"

struct pagemap_str {
	
	int page_map[NUM_LPNS];
	int num_invalid_pages[BLOCKS_PER_NAND];

	int current_block;	 /* current write block */
	int current_page_offset; /* current write page offset in block */

	int free_block_count; /* total free block count */

	int gc_free_block; /* free block index for GC */
};

struct pagemap_stats {
	int gc_count;
	int copyback_count;
};

struct pagemap_str pagemap_str;
struct pagemap_stats pagemap_stats;

static void pagemap_print_stats(void)
{
	printf("pagemap_stats: garbage collection count::   %d\n",
	       pagemap_stats.gc_count);
	printf("pagemap_stats: page copyback count:         %d\n",
	       pagemap_stats.copyback_count);
}

static int pagemap_get_ppn(int lpn)
{
	return pagemap_str.page_map[lpn];
}

/* pagemap update*/
static void pagemap_set_ppn(int lpn, int ppn)
{
	int old_block_idx, old_ppn;

	assert(lpn < NUM_LPNS);
	assert(ppn < NUM_PPNS);

	/* increase invalidation count of previous block */
	old_ppn = pagemap_str.page_map[lpn];

	// 이전에 기록된 적이 없다면 바로 page table들 update
	if (old_ppn < 0)
		goto set_page_table;

	// 업데이트 이전 블록 idx 가져와서 invalidation count 올림
	old_block_idx = old_ppn / PAGES_PER_BLOCK;
	assert(old_block_idx < BLOCKS_PER_NAND);

	pagemap_str.num_invalid_pages[old_block_idx]++;
	if (VERBOSE_MODE) {
		("%s:%d old_block %d invalid_count %d\n",
		       __func__, __LINE__, old_block_idx,
		       pagemap_str.num_invalid_pages[old_block_idx]);
	}

	/* set page table */
	set_page_table:
		pagemap_str.page_map[lpn] = ppn;
}

static int pagemap_garbage_collection(void)
{
	int block_idx, page_idx, ppn_in_victim, ppn_in_map;
	int data_lpn;
	int gc_free_block, gc_current_page_offset;
	int victim_block;
	int max_invalid_pages;

	victim_block = -1;
	max_invalid_pages = -1;

	/* the block with max invalidation pages selected as the victim  - time complexity O(n)*/
	for (block_idx = 0; block_idx < (BLOCKS_PER_NAND); block_idx++){
		if(max_invalid_pages < pagemap_str.num_invalid_pages[block_idx]){
			victim_block = block_idx;
			max_invalid_pages = pagemap_str.num_invalid_pages[block_idx];
		}
	}

	if (VERBOSE_MODE)
		printf("%s:%d victim_block %d max_invalid_pages %d\n",
		       __func__, __LINE__, victim_block, max_invalid_pages);

	// validation check
	assert(victim_block != -1);
	assert(victim_block < BLOCKS_PER_NAND);
	assert(victim_block != pagemap_str.gc_free_block);
	assert(max_invalid_pages != 0);

	gc_free_block = pagemap_str.gc_free_block;
	assert(pagemap_str.num_invalid_pages[gc_free_block] == 0);

	gc_current_page_offset = 0;

	/* copy valid page from victim to gc_free_block */ // victim page를 읽음
	for (page_idx = 0; page_idx < PAGES_PER_BLOCK ; page_idx++) {
		data_lpn = nand_page_read(victim_block, page_idx);
		ppn_in_map = pagemap_get_ppn(data_lpn);

		//victim내 ppn 인덱스를 p2l 테이블에 바로 접근하여 lpn 확인함, 인덱스 접근 후 -1 아니면 바로 pagemap_set_ppn하면됨
		ppn_in_victim = victim_block * PAGES_PER_BLOCK + page_idx;
		
		if (ppn_in_map != ppn_in_victim) //같지않으면 invalid된 page라는 뜻이므로 continue
			continue;

		// nand에 write
		nand_page_program(gc_free_block, gc_current_page_offset, data_lpn);
		// page map update
		pagemap_set_ppn(data_lpn, gc_free_block * PAGES_PER_BLOCK + gc_current_page_offset);
		
		gc_current_page_offset++;
		pagemap_stats.copyback_count++;
	}
	assert(gc_current_page_offset == (PAGES_PER_BLOCK - max_invalid_pages));

	/* erase victim block and set next gc free block */
	nand_block_erase(victim_block);
	pagemap_str.num_invalid_pages[victim_block] = 0;
	pagemap_str.gc_free_block = victim_block;

	/* set current gc free block to current block */
	pagemap_str.current_block = gc_free_block;
	pagemap_str.current_page_offset = gc_current_page_offset;
	pagemap_str.num_invalid_pages[gc_free_block] = 0;

	pagemap_stats.gc_count++;
}

/* 
	Gets free ppn 
	IF data is full, garbage collection is executed
*/
static void pagemap_get_free_ppn(int *block_idx, int *page_offset)
{
	if (pagemap_str.current_page_offset >= PAGES_PER_BLOCK) {
		// 기존 block이 다 차서 다음 block으로 offset 포인터를 옮기는 과정
		if (pagemap_str.free_block_count) { 
			pagemap_str.free_block_count--;
			pagemap_str.current_block++;
			pagemap_str.current_page_offset = 0;
		} else {
			// spare block 제외한 모든 block이 가득찼으면 gc 수행
			pagemap_garbage_collection();
		}
	}

	*block_idx = pagemap_str.current_block;
	*page_offset= pagemap_str.current_page_offset;

	pagemap_str.current_page_offset++;
}

static void pagemap_write_op(int lpn, int data)
{
	int block_idx, page_offset;
	int ppn;

	/* get free physical page index or do garbage-collect */
	pagemap_get_free_ppn(&block_idx, &page_offset);

	/* write data in nand block */
	nand_page_program(block_idx, page_offset, data);

	//calculates ppn
	ppn = block_idx * PAGES_PER_BLOCK + page_offset;
	
	/* pagemap update */
	pagemap_set_ppn(lpn, ppn);

	if (VERBOSE_MODE)
		printf("%s:%d lpn %d ppn %d\n", __func__, __LINE__, lpn, ppn);
}

static int pagemap_read_op(int lpn)
{
	int ppn, data;
	int pbn, page_offset;

	/* get physical page number from page-map table */
	ppn = pagemap_get_ppn(lpn);

	pbn = ppn / PAGES_PER_BLOCK;	      /* get physical block number */
	page_offset = ppn % PAGES_PER_BLOCK;  /* get page offset in block */

	data = nand_page_read(pbn, page_offset);

	return data;
}

/* pagemap initialization */
void pagemap_init(struct ftl_operation *op)
{
	int block_idx, page_idx;

	pagemap_str.current_block = 0;
	pagemap_str.current_page_offset = 0;

	pagemap_str.gc_free_block = BLOCKS_PER_NAND - 1; //현재 spare block number: Block#4
	pagemap_str.free_block_count = BLOCKS_PER_NAND - 2; // 3

	// Total Block 개수: 5
	// 모든 블록에 대해 invalid number 0으로 초기화
	for (block_idx = 0; block_idx < (BLOCKS_PER_NAND); block_idx++)
		pagemap_str.num_invalid_pages[block_idx] = 0;

	// l2p page_map을 -1로 채움
	for (page_idx = 0; page_idx < NUM_LPNS; page_idx++)
		pagemap_str.page_map[page_idx] = -1;

	op->write_op = pagemap_write_op;
	op->read_op = pagemap_read_op;
	op->print_stats = pagemap_print_stats;
}
