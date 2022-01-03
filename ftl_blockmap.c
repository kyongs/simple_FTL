#include "header.h"

struct blockmap_str {
	int block_map[BLOCKS_PER_NAND];

	/* ... */
};

static void blockmap_print_stats(void)
{
	/* ... */
}

static int blockmap_read_op(int lpn)
{
	/* ... */
	return 0;
}

static void blockmap_write_op(int lpn, int data)
{
	/* ... */
}

void blockmap_init(struct ftl_operation *op)
{
	/* ... */

	op->write_op = blockmap_write_op;
	op->read_op = blockmap_read_op;
	op->print_stats = blockmap_print_stats;
}
