/*
 * view.dummyfs - print a summary of the data in the entire file system
 *
 * Eric McCreath 2006 GPL
 * To compile :
 *     gcc view.dummyfs.c -o view.dummyfs
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "../dummyfs/dummyfs.h"

char* device_name;
int device;

static void die(char *mess)
{
        fprintf(stderr,"Exit : %s\n",mess);
        exit(1);
}

static void usage(void)
{
        die("Usage : view.dummyfs <device name>)");
}

int main(int argc, char ** argv)
{
        if (argc != 2)
                usage();

        // open the device for reading
        device_name = argv[1];
        device = open(device_name,O_RDONLY);

        off_t pos=0;
        struct dummyfs_block block;
	struct dummyfs_inode *inode;
	struct dummyfs_inode_table *table;
	int numblocks;
        int i;

	// Get the number of blocks on the filesystem
	lseek(device, TABLE_BLOCK_INDEX*sizeof(struct dummyfs_block), SEEK_SET);
	read(device, &block, sizeof(struct dummyfs_block));
	table = (struct dummyfs_inode_table *) &block;
	numblocks = table->t_numblocks;
	printf("Device has %d blocks\n", numblocks);
	lseek(device, pos, SEEK_SET);

        for (i = 0; i < numblocks; i++) {  // read each of the blocks

		if (pos != lseek(device,pos,SEEK_SET))
			die("seek set failed");

		if (BLOCKSIZE != read(device, &block, BLOCKSIZE))
			die("inode read failed");

		if (BM_IS_EMPTY(block.b_mode))
			printf("%2d: Empty block\n", i);
		else if (BM_IS_INODE(block.b_mode)) {
			inode = (struct dummyfs_inode *) &block;
			printf("%2d: Inode %u : %s : %u bytes : next block is %s\n",
					i,
					inode->i_ino,
					(IM_IS_DIR(inode->i_mode) ? "Dir" : "Reg"),
					inode->i_size,
					(BM_IS_UNALLOCATED(inode->b_next) ? "unallocated" : "allocated"));
		}
		else if (i == TABLE_BLOCK_INDEX) {
			table = (struct dummyfs_inode_table *) &block;
			printf("%2d : Inode table : %d blocks : next block is %s\n",
					i,
					table->t_numblocks,
					(BM_IS_UNALLOCATED(table->b_next) ? "unallocated" : "allocated"));
		}

		pos += BLOCKSIZE;
        }
        close(device);
        return 0;
}

