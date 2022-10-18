/* Eric McCreath, 2006-2020, GPL
 * Alex Barilaro, 2020
 * (based on the simplistic RAM filesystem McCreath 2001)
 *
 * This code all pertains to the manipulation of blocks on a vvsfs disk.
 */

#include "vvsfs.h"


/*
 * Read a block from the superblock/block device.
 *
 * Returns size of block read.
 */
static int vvsfs_readblock(struct super_block *sb,
                           unsigned long block_index,
                           struct vvsfs_block *block)
{
        struct buffer_head *bh;

        if (DEBUG)
                printk("vvsfs - readblock : %lu\n", block_index);

        bh = sb_bread(sb, block_index); // Move to the correct position on the device
        memcpy((void *) block, (void *) bh->b_data, BLOCKSIZE); // Read bytes from position
        brelse(bh);

        if (DEBUG)
                printk("vvsfs - readblock done : %lu\n", block_index);

        return BLOCKSIZE;
}

/*
 * Write a block to the block device.
 *
 * Returns size of block written.
 */
static int vvsfs_writeblock(struct super_block *sb,
                            unsigned long block_index,
                            struct vvsfs_block *block)
{
        struct buffer_head *bh;

        if (DEBUG)
                printk("vvsfs - writeblock : %lu\n", block_index);

        bh = sb_bread(sb,block_index); // Move to the correct position on the device
        memcpy(bh->b_data, block, BLOCKSIZE); // Read block struct to position
        mark_buffer_dirty(bh);
        sync_dirty_buffer(bh); // Initiate write to actual device
        brelse(bh);
            
        if (DEBUG)
                printk("vvsfs - writeblock done: %lu\n", block_index);

        return BLOCKSIZE;
}

/*
 * Get (or write) the block index of an inode (i.e.: the block
 * containing the inode metadata).
 *
 * Returns the block index of the inode (including on writes).
 */
static unsigned long vvsfs_inode_block_index(struct super_block *sb,
		                             unsigned long ino,
					     int writing)
{
	struct vvsfs_inode_table table;
	unsigned long table_num;
	unsigned long table_index = TABLE_BLOCK_INDEX;
	unsigned long inode_index;
	long long entry;

	if (DEBUG)
		printk("vvsfs - %s inode %lu block index",
		    ((writing) ? "writing" : "getting"),
		    ino);

	/*
	 * If the inode number is larger than the maximum index of an
	 * inode table's entries (i.e.: the max array index), then we'll
	 * need to follow the linked list of tables beginning at the first
	 * inode table. To do that, we'll need to know how many tables away
	 * the inode index is.
	 */
	if (ino >= MAX_TABLE_SIZE)
		table_num = (unsigned long) (ino / MAX_TABLE_SIZE);
	else
		table_num = 0;
	if (table_num != 0)
		entry = ((signed long long) ino) - (table_num * MAX_TABLE_SIZE);
	else
		entry = ino;

	if (DEBUG)
		printk("vvsfs - ino %lu is at table %lu at entry %lld", ino, table_num, entry);

	// Follow the linked list of inode tables
	vvsfs_readblock(sb, TABLE_BLOCK_INDEX, (struct vvsfs_block *) &table);
	while (table_num > 0) {
		table_index = table.b_next;
		vvsfs_readblock(sb, table.b_next, (struct vvsfs_block *) &table);
		table_num--;
	}
	inode_index = table.t_table[entry];

	// Make any changes to the entry, if requested
	if (writing) {
		table.t_table[entry] = writing;
		vvsfs_writeblock(sb, table_index, (struct vvsfs_block *) &table);
	}

	if (DEBUG)
		printk("vvsfs - done %s inode %lu index",
		    ((writing) ? "writing" : "getting"),
		    ino);

	return inode_index;
}

/*
 * Get an inode block (i.e.: the block containing all the
 * inode metadata on disk) from the block device using only
 * the inode number.
 *
 * Returns the size of the block read.
 */
static int vvsfs_read_inode(struct super_block *sb,
                           unsigned long inum,
                           struct vvsfs_inode *inode)
{
	unsigned long inode_block_index;

        if (DEBUG)
                printk("vvsfs - reading inode %lu\n", inum);

	inode_block_index = vvsfs_inode_block_index(sb, inum, false);
	vvsfs_readblock(sb, inode_block_index, (struct vvsfs_block *) inode);
	
        if (DEBUG)
                printk("vvsfs - done reading inode %lu\n", inum);

        return BLOCKSIZE;
}

/*
 * Write an inode block (i.e.: the block containing all the
 * inode metadata on disk) to the block device using only
 * the inode number.
 *
 * Returns the size of the block written.
 */
static int vvsfs_write_inode(struct super_block *sb,
                           unsigned long inum,
                           struct vvsfs_inode *inode)
{
	unsigned long inode_block_index;

        if (DEBUG)
                printk("vvsfs - writing inode %lu\n", inum);

	inode_block_index = vvsfs_inode_block_index(sb, inum, false);
	vvsfs_writeblock(sb, inode_block_index, (struct vvsfs_block *) inode);

        if (DEBUG)
                printk("vvsfs - done writing inode %lu\n", inum);

        return BLOCKSIZE;
}


/*
 * Find the index of the first empty block on disk.
 *
 * Returns 0 if no empty blocks are found, or the index
 * of the block if found.
 */
static unsigned long vvsfs_empty_block(struct super_block *sb)
{
	struct vvsfs_inode_table table;
        struct vvsfs_block block;
        unsigned long k;

	/*
	 * We'll need to read the first inode table to check how many blocks
	 * the device has. Once we have that, we have the maximum bound for a
	 * linear search through the blocks on the device.
	 */
	vvsfs_readblock(sb, TABLE_BLOCK_INDEX, (struct vvsfs_block *) &table);
        for (k = 0; k < table.t_numblocks; k++) {
                vvsfs_readblock(sb,k,&block);
                if (BM_IS_EMPTY(block.b_mode))
			return k;
        }
        return 0;
}

/*
 * Find the first unallocated entry in the inode table linked
 * list.
 *
 * Returns the inode number if one is unallocated, and returns 0
 * if no unallocated inodes are left (i.e.: every inode table is
 * full, and there's no more room to make another inode table).
 */
static int vvsfs_empty_inode(struct super_block *sb)
{
	struct vvsfs_inode_table table;
	unsigned long table_index;
	unsigned long table_num = 0;
	unsigned long new_table_index;
	int k;

	if (DEBUG)
		printk("vvsfs - finding an empty inode");

	// Traverse the linked list of inode tables to spot an unallocated entry
	table_index = TABLE_BLOCK_INDEX;
	vvsfs_readblock(sb, TABLE_BLOCK_INDEX, (struct vvsfs_block *) &table);
	while (true) {
		for (k = 0; k < MAX_TABLE_SIZE; k++) { // Search through a table
			printk("vvsfs - table[%d] is %lu", k, table.t_table[k]);
			if (BM_IS_UNALLOCATED(table.t_table[k])) {
				if (DEBUG)
					printk("vvsfs - done empty inode");
				return k + (table_num*MAX_TABLE_SIZE);
			}
		}
		if (!BM_IS_UNALLOCATED(table.b_next)) { // Move to the next table, if present
			table_index = table.b_next;
			vvsfs_readblock(sb, table.b_next, (struct vvsfs_block *) &table);
			table_num++;
		} else {
			break;
		}
	}

	// If we get to here, we need to make a new inode table
	if (DEBUG)
		printk("vvsfs - creating a new inode table");
	new_table_index = vvsfs_empty_block(sb); // Get a new empty block
	if (!new_table_index) {
		printk("vvsfs - no free blocks to allocate a new table!");
		return 0;
	}
	table.b_next = new_table_index; // Update the current table to point to the new one
	vvsfs_writeblock(sb, table_index, (struct vvsfs_block *) &table);

	// Fill out the fields for the new table and write it
	for (k = 0; k < MAX_TABLE_SIZE; k++)
		table.t_table[k] = BM_UNALLOCATED;
	table.b_next = BM_UNALLOCATED;
	vvsfs_writeblock(sb, new_table_index, (struct vvsfs_block *) &table);
	table_num++;

	if (DEBUG)
		printk("vvsfs - done finding empty inode");

	return (table_num * MAX_TABLE_SIZE);
}

/*
 * Initialise a new inode on disk and return a VFS inode.
 */
struct inode *vvsfs_new_inode(const struct inode *dir, umode_t mode, unsigned short inode_mode)
{
        struct vvsfs_inode block;
        struct super_block *sb;
        struct inode *inode;
        unsigned long block_index;
	unsigned long new_inode_number;
	int k;

        if (DEBUG)
	        printk("vvsfs - new inode\n");

        if (!dir)
	        return NULL;
        sb = dir->i_sb;

	// Initialise a new VFS inode struct
        inode = new_inode(sb);
        if (!inode)
	        return NULL;

	// Find a new inode in the superblock
	new_inode_number = vvsfs_empty_inode(sb);
	if (new_inode_number == 0) {
		printk("vvsfs - inode table is full\n");
		return NULL;
	}

	// Find an empty block in the superblock
        block_index = vvsfs_empty_block(sb);
        if (block_index == 0) {
        	printk("vvsfs - no empty blocks left\n");
	        return NULL;
        }

	// Initialise the inode on disk with plain metadata
	block.b_mode = BM_INODE;
	block.i_ino = new_inode_number;
	block.i_kind = inode_mode;
	block.i_mode = mode;
	block.i_uid = current_fsuid().val;
	block.i_gid = current_fsuid().val;
	block.i_links = 1;
	block.i_size = 0;
	for (k = 0; k < MAX_INODE_DATA_SIZE; k++)
		block.i_data[k] = 0;
	block.b_next = BM_UNALLOCATED;
	vvsfs_writeblock(sb, block_index, (struct vvsfs_block *) &block);

	// Add the block index to the inode table
	vvsfs_inode_block_index(sb, block.i_ino, block_index);

	// Initialise the VFS inode metadata
        inode_init_owner(inode, dir, mode);
        inode->i_ino = new_inode_number;
        inode->i_ctime = inode->i_mtime = inode->i_atime = current_time(inode);
        inode->i_op = NULL;
        insert_inode_hash(inode);

	if (DEBUG)
		printk("vvsfs - done new inode");

        return inode;
}

/*
 * Map out a file's data into memory (with padding appended,
 * if requested).
 *
 * Returns a pointer to the data in memory.
 */
static char *vvsfs_map_data(struct super_block *sb,
		            struct vvsfs_inode *inode,
		            unsigned int extra)
{
	struct vvsfs_block disk_data;
	unsigned char *mem_data = vmalloc(inode->i_size + extra);
	unsigned char *eof = mem_data+inode->i_size+extra;
	unsigned char *pos = mem_data;

	if (DEBUG)
		printk("vvsfs - mapping %lu+%lu data from inode %lu",
		    inode->i_size,
		    extra,
		    inode->i_ino);

	// Copy the inode's inline data
	memcpy(pos, inode->i_data, MIN(MAX_INODE_DATA_SIZE, inode->i_size + extra));
	pos += MIN(MAX_INODE_DATA_SIZE, inode->i_size + extra);

	/*
	 * Files on disk are stored using linked lists of data blocks. Some data
	 * is stored in the inode block. 
	 * Once the inline data has been copied to memory, we need to traverse the
	 * linked list of data blocks for the file until we hit a block whose b_next
	 * field is unallocated. That is, we need to keep going until we hit the end
	 * of the linked list.
	 */
	if (BM_IS_UNALLOCATED(inode->b_next)) { // If the inline data is all there was...
		if (DEBUG) {
			printk("vvsfs - inode had no extra data blocks");
			printk("vvsfs - done map data");
		}
		return mem_data;
	} else {
		if (DEBUG)
			printk("vvsfs - inode has extra data blocks");
		vvsfs_readblock(sb, inode->b_next, &disk_data);
		while (true) { // Traverse the linked list
			memcpy(pos, disk_data.b_data, MIN(MAX_BLOCK_DATA_SIZE, eof-pos));
			pos += MIN(MAX_BLOCK_DATA_SIZE, eof-pos);
			if (!BM_IS_UNALLOCATED(disk_data.b_next)) 
				vvsfs_readblock(sb, disk_data.b_next, &disk_data);
			else // Stop when we hit the end
				break;
		}
		if (DEBUG)
			printk("vvsfs - done map data");
		return mem_data;
	}

}

/*
 * Allocate a data block on disk (to fill up with juicy data)
 * and point an existing block to that to form a linked list.
 *
 * Returns the index of the newly-allocated block if an
 * allocation was made, and returns 0 if the allocation
 * failed.
 */
static unsigned long vvsfs_alloc_data(struct super_block *sb,
		                      struct vvsfs_block *prev,
				      unsigned long prev_index)
{
	struct vvsfs_block new;
	unsigned long new_index;
	int k;

	if (DEBUG)
		printk("vvsfs - allocating new data block\n");

	// Check to see if our work is already done
	if (!BM_IS_UNALLOCATED(prev->b_next)) {
		printk("vvsfs - current block already has a successor!\n");
		if (DEBUG)
			printk("vvsfs - done allocation\n");
		return prev->b_next;
	}

	// Sigh... If we're here, we actually need to do something.
	new_index = vvsfs_empty_block(sb);
	if (new_index == 0) { // Report failures
		printk("vvsfs - no empty blocks left!\n");
	} else { 
		prev->b_next = new_index; // Point the previous block to the new one
		vvsfs_writeblock(sb, prev_index, prev);
		new.b_mode = BM_DATA; // Write out empty data to the newly-allocated block
		                      // (so there aren't any nasty artefacts from memory)
		for (k = 0; k < MAX_BLOCK_DATA_SIZE; k++) 
			new.b_data[k] = 0;
		new.b_next = BM_UNALLOCATED;
		vvsfs_writeblock(sb, new_index, &new);
	}

	if (DEBUG)
		printk("vvsfs - done allocation\n");
	return new_index;

}

/*
 * Deallocate (mark as empty) every block in a linked list
 * of data blocks for a file.
 */
static void vvsfs_dealloc_data(struct super_block *sb,
			       unsigned long block_index)
{
	struct vvsfs_block block;
	unsigned long next;
	int k;

	if (DEBUG)
		printk("vvsfs - deallocating data blocks, starting with %lu",
		       block_index);

	/*
	 * Traverse the linked list of data blocks, zeroing
	 * everything out, until we hit the end (i.e.: a block
	 * whose b_next field is unallocated).
	 */
	while (true) {
		vvsfs_readblock(sb, block_index, &block);
		next = block.b_next;
		block.b_mode = BM_EMPTY;
		for (k = 0; k < MAX_BLOCK_DATA_SIZE; k++)
			block.b_data[k] = BM_UNALLOCATED;
		block.b_next = BM_UNALLOCATED;
		vvsfs_writeblock(sb, block_index, &block);
		block_index = next;
		if (BM_IS_UNALLOCATED(block_index)) // Break if we hit the end
			break;
	}

	if (DEBUG)
		printk("vvsfs - done deallocating data blocks");
}

/*
 * Write out a file's data to a linked list of data blocks
 * (beginning with the inline data in its inode block).
 *
 * Returns the amount of data written.
 */
static int vvsfs_write_data(struct super_block *sb,
		            struct vvsfs_inode *inode,
			    unsigned char *data,
			    unsigned long size)
{
	struct vvsfs_block block;
	unsigned long required;
	long long remainder_size;
	unsigned long block_index = vvsfs_inode_block_index(sb, inode->i_ino, false);
	unsigned char *eof = data+size;
	unsigned char *pos = data;

	if (DEBUG)
		printk("vvsfs - writing data (%lu bytes)", size);

	// Calculate how many blocks we'll need beyond the inode's inline data
	remainder_size = ((signed long long) size) - MAX_INODE_DATA_SIZE;
	required = (remainder_size > 0)
		? DIV_ROUND_UP(remainder_size, MAX_BLOCK_DATA_SIZE)
		: 0;
	if (DEBUG)
		printk("vvsfs - data write needs %lu blocks after inode block", required);

	/*
	 * Allocate the extra blocks we need.
	 *
	 * We'll need to treat the inode block specially, because it stores less data than pure
	 * data blocks, so we'll need to use different values for the math involved allocating
	 * blocks and truncating data.
	 */
	if (required) {
		block_index = vvsfs_alloc_data(sb, (struct vvsfs_block *) inode, block_index);
		if (block_index == 0) { // If there are no more empty blocks, truncate the data
			                // by marking the eof as earlier than it actually is.
			printk("vvsfs - will only write what I can fit");
			eof = data + MAX_INODE_DATA_SIZE;
			required = 0;
		} else {
			vvsfs_readblock(sb, block_index, &block);
			pos += MAX_INODE_DATA_SIZE;
			required--;
		}
		while (required > 0) {
			block_index = vvsfs_alloc_data(sb, &block, block_index);
			if (block_index == 0) {
				printk("vvsfs - will only write what I can fit");
				eof = pos + MAX_BLOCK_DATA_SIZE;
				required = 0;
			} else {
				vvsfs_readblock(sb, block_index, &block);
				pos += MAX_BLOCK_DATA_SIZE;
				required--;
			}
		}
	}

	// Time to write to disk!
	if (DEBUG)
		printk("vvsfs - beginning writes to disk");
	
	// Reset all the pointers to be back to the start (we adjusted them
	// in case we needed to truncate data in the above allocation loop)
	pos = data; 
	block_index = vvsfs_inode_block_index(sb, inode->i_ino, false);

	// Copy out the inline data first
	memcpy(inode->i_data, pos, MIN(MAX_INODE_DATA_SIZE, eof - pos));
	inode->i_size = eof - data;
	vvsfs_writeblock(sb, block_index, (struct vvsfs_block *) inode);
	pos += MIN(MAX_INODE_DATA_SIZE, eof - pos);

	// Iterate through the linked list, copying data, until we hit the end
	if (!BM_IS_UNALLOCATED(inode->b_next)) {
		block_index = inode->b_next;
		vvsfs_readblock(sb, inode->b_next, &block);
		while (pos != eof) {

			/*
			 * Only copy as much data as we have, rather than just ripping
			 * out a BLOCK_DATA_SIZE worth of bytes out of memory regardless
			 * of whether they store meaningful data or not (your file
			 * reads will thank us later)
			 */
			memcpy(block.b_data, pos, MIN(MAX_BLOCK_DATA_SIZE, eof - pos));
			vvsfs_writeblock(sb, block_index, &block);
			pos += MIN(MAX_BLOCK_DATA_SIZE, eof - pos);
			block_index = block.b_next;
			if (!BM_IS_UNALLOCATED(block.b_next))
				vvsfs_readblock(sb, block.b_next, &block);
		}
	}

	if (DEBUG)
		printk("vvsfs - done write data");

	return pos - data;
	
}
