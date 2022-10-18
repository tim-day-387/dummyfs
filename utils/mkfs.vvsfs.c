/*
 *  mkfs.vvsfs - constructs an initial empty file system
 * Eric McCreath 2006 GPL
 *
 * To compile :
 *     gcc mkfs.vvsfs.c -o mkfs.vvsfs
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "../vvsfs/vvsfs.h"

char* device_name;
int device;

static void die(char *mess)
{
        fprintf(stderr,"Exit : %s\n",mess);
        exit(1);
}

static void usage(void)
{
        die("Usage : mkfs.vvsfs <device name>)");
}

int main(int argc, char ** argv)
{

        if (argc != 2)
                usage();

        // open the device for reading and writing
        device_name = argv[1];
        device = open(device_name,O_RDWR);

        off_t pos=0;
        struct vvsfs_block block;
	struct vvsfs_inode_table *table;
	struct vvsfs_inode *inode;
	unsigned long numblocks = (unsigned long) (lseek(device, 0L, SEEK_END) / BLOCKSIZE);
        int i;
        int k;

	pos = lseek(device, 0L, SEEK_SET);
	printf("device has %lu blocks to write\n", numblocks);
	printf("inode data size is %lu\n", MAX_INODE_DATA_SIZE);
	printf("inode size itself is %lu\n", sizeof(struct vvsfs_inode));
	printf("block data size is %lu\n", MAX_BLOCK_DATA_SIZE);
	printf("block size itself is %lu\n", sizeof(struct vvsfs_block));
	printf("table data size is %lu\n", MAX_TABLE_SIZE);
	printf("table size itself is %lu\n", sizeof(struct vvsfs_inode_table));

	// FIXME: We don't check that numblocks <= u32max

        for (i = 0; i < numblocks; i++) {  // write each of the blocks

                printf("writing %lu : ",i);

		// Fill out the inode table block
		if (i == TABLE_BLOCK_INDEX) { 
			printf("inode table block\n");
			table = (struct vvsfs_inode_table *) &block;
			block.b_mode = BM_TABLE;
			table->t_numblocks = numblocks;
			for (k = 0; k < MAX_TABLE_SIZE; k++) {
				table->t_table[k] = BM_UNALLOCATED;
			}
			table->b_next = BM_UNALLOCATED;
			table->t_table[0] = ROOT_DIR_BLOCK_INDEX;
		}

		// Fill out the root directory inode block
		else if (i == ROOT_DIR_BLOCK_INDEX) {
			printf("root dir inode block\n");
			block.b_mode = BM_INODE;
			inode = (struct vvsfs_inode *) &block;
			inode->i_ino = 0;
			inode->i_mode = IM_DIR;
			inode->i_links = 1;
			inode->i_size = 0;
			for (k = 0; k < MAX_INODE_DATA_SIZE; k++)
				inode->i_data[k] = 0;
			inode->b_next = BM_UNALLOCATED;
		}

		// Fill out empty blocks
		else {
			printf("empty block\n");
			block.b_mode = BM_EMPTY;
			for (k = 0; k < MAX_BLOCK_DATA_SIZE; k++)
				block.b_data[k] = BM_UNALLOCATED;
			block.b_next = BM_UNALLOCATED;
		}

		// Move the file pointer to the correct block
		if (pos != lseek(device,pos,SEEK_SET))
			die("seek set failed");

		// Write the block to the device
		if (sizeof(struct vvsfs_block) !=
		    write(device, &block, BLOCKSIZE))
			die("inode write failed");
		pos += BLOCKSIZE;
        }

        close(device);
        return 0;
}

