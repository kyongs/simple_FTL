#include "header.h"

struct main_stats {
	int total_read_lpn;
	int total_write_lpn;
};

static struct main_stats main_stats;

static void main_mode_0(struct ftl_operation *ftl, char *filename);
static void main_mode_1(struct ftl_operation *ftl, int issue_count);
static int read_lpn(struct ftl_operation *op, int lpn);

/*
 * argv[0] - execution filename
 * argv[1] - mode
 *	0 = input-text-file mode
 *	1 = random generating lpn mode
 * argv[2] - input text filename
 */
int main(int argc, char *argv[])
{
	int mode, wrong, i;
	int total_nand_page_program;
	double waf;
	struct ftl_operation op = { NULL, NULL, NULL };

	if (argc != 3) {
		printf("..help: %s <mode> <argument>\n", argv[0]);
		printf("..mode(0) - %s 0 <filename>\n", argv[0]);
		printf("..mode(1) - %s 1 <random request count>\n", argv[0]);
		return -1;
	}

	main_stats.total_read_lpn = 0;
	main_stats.total_write_lpn = 0;

	pagemap_init(&op);
//      blockmap_init(&op);

	mode = atoi(argv[1]);
	if (mode != 0 && mode != 1) {
		printf("mode %d error..\n", mode);
		return -1;
	}

	switch (mode) {
	case 0:
		/*
		 * input-file mode
		 * argv[2] - input filename
		 */
		main_mode_0(&op, argv[2]);
		break;
	case 1:
		/*
		 * random write mode
		 * argv[2] - request count
		 */
		main_mode_1(&op, atoi(argv[2]));
		break;
	default:
		assert(0);
	}

	/* print statistics */
	printf("Print ftl stats::\n");
	op.print_stats();
	total_nand_page_program = nand_print_stats();
	printf("total write lpn count:: %d\n", main_stats.total_write_lpn);
	waf = ((double) total_nand_page_program) / main_stats.total_write_lpn;
	printf("Write Amplification Factor:: %.4lf\n", waf);

	/* verify all ranges */
	for (i = 0, wrong = 0; i < NUM_LPNS; i++) {
		int data_lpn = read_lpn(&op, i);
		if (data_lpn != i) {
			if (VERBOSE_MODE)
				printf("verify:: lpn %d has wrong data (%d)\n",
				       i, data_lpn);
			wrong++;
		}
	}
	printf("wrong lpn count:: %d\n", wrong);
}

static void write_lpn(struct ftl_operation *op, int lpn, int data)
{
	main_stats.total_write_lpn++;
	op->write_op(lpn, data);
}

static int read_lpn(struct ftl_operation *op, int lpn)
{
	main_stats.total_read_lpn++;
	return op->read_op(lpn);
}

static void main_mode_0(struct ftl_operation *op, char *filename)
{
	FILE *fp;
	int lpn;

	fp = fopen(filename, "r");
	if (!fp) {
		printf("mode_0 filename %s error.. fd(%p)\n", filename, fp);
		return;
	}

	while (fscanf(fp, "%d", &lpn) != EOF) {
		assert(lpn < NUM_LPNS);
		write_lpn(op, lpn, lpn);
	}

	fclose(fp);
}

static void main_mode_1(struct ftl_operation *op, int issue_count)
{
	int i;

	/* fillout whole logical space to eliminate null lpn */
	printf("write whole LPN range..\n");
	for (i = 0; i < NUM_LPNS; i++)
		write_lpn(op, i, i);

	printf("write random LPN as count %d\n", issue_count);
	srand(0); // srand(time(NULL));
	for (i = 0; i < issue_count; i++) {
		int lpn = rand() % NUM_LPNS;
		write_lpn(op, lpn, lpn);
	}
}
