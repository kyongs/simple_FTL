#include "header.h"

struct blockmap_str {
	int block_map[NUM_LBNS];
	int spare_block_idx; 
};

struct blockmap_stats {
	int copyback_count;
};

struct blockmap_str blockmap_str;
struct blockmap_stats blockmap_stats;

/* statistics print*/
static void blockmap_print_stats(void)
{
	printf("blockmap_stats: block copyback count:       %d\n",
	        blockmap_stats.copyback_count);
}

/* read operation */
static int blockmap_read_op(int lpn)
{
	int lbn, pbn, page_offset;
	int data;
	
	// validation check
	assert(lpn < NUM_LPNS);

	// lbn, page_offset 계산
	lbn = lpn / PAGES_PER_BLOCK;
	page_offset = lpn % PAGES_PER_BLOCK;

	// block map table에서 pbn을 가져옴
	pbn = blockmap_str.block_map[lbn];

	// nand에서 데이터 읽음
	data = nand_page_read(pbn, page_offset);

	return data;
}

/* copy block to spare block and overwrite data */
static void copy_block(int *lbn, int *page_offset, int *data) {
	
	int old_block_idx = blockmap_str.block_map[*lbn];
	int new_block_idx = blockmap_str.spare_block_idx;

	// old block의 모든 원소를 new block으로 복사
	for (int page_idx=0; page_idx<PAGES_PER_BLOCK; page_idx++){
		// 해당 page_offset에는 data write
		if(page_idx == page_offset){
			nand_page_program(new_block_idx, page_offset, data);
			continue;
		}

		int data_lpn = nand_page_read(old_block_idx, page_idx);
		if (data_lpn > 0 ){
			nand_page_program(new_block_idx, page_idx, data_lpn);
			blockmap_stats.copyback_count++;
		}
	}

	//blockmap update
	blockmap_str.block_map[*lbn] = new_block_idx;

	//예전 block을 spare block으로
	nand_block_erase(old_block_idx);
	blockmap_str.spare_block_idx = old_block_idx;

}

/* write operation */
static void blockmap_write_op(int lpn, int data)
{
	int pbn, lbn, page_offset;

	// validation check
	assert(lpn < NUM_LPNS);

	// calculates block idx and page offset 
	lbn = lpn / PAGES_PER_BLOCK;
	page_offset = lpn % PAGES_PER_BLOCK;
	if (VERBOSE_MODE)
		printf("%s:%d lpn %d data %d\n", __func__, __LINE__, lpn, data);

	//blockmap table에서 pbn 얻음
	pbn = blockmap_str.block_map[lbn];

	// nand에서 pbn에 해당하는 값을 읽는다 (실제 데이터)
	int nand_data = nand_page_read(pbn, page_offset);


	// 만약 page가 비어있으면 write후 return
	if (nand_data < 0) {
		nand_page_program(pbn, page_offset, data);
	} else {
		//nand에서 해당 값에 이미 다른 원소가 들어있다면
		//block 전체를 free block에 복사하면서 덮어쓴다.
		copy_block(&lbn, &page_offset, &data);
	}
}

/* Initialization*/
void blockmap_init(struct ftl_operation *op)
{
	blockmap_str.spare_block_idx = BLOCKS_PER_NAND -1; 

	// block-map table 초기화
	for (int block_num = 0; block_num < NUM_LBNS; block_num++)
		blockmap_str.block_map[block_num] = block_num;

	op->write_op = blockmap_write_op;
	op->read_op = blockmap_read_op;
	op->print_stats = blockmap_print_stats;
}
