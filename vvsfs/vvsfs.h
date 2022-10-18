#define BLOCKSIZE 	512
#define BLOCKSIZE_BITS 	8
#define MAX_NAME_SIZE 	40

#define BLOCK_HEADER_SIZE 	(sizeof(__u8))
#define BLOCK_TRAILER_SIZE 	(sizeof(__u32))
#define TABLE_HEADER_SIZE 	(3*sizeof(__u8) + sizeof(__u32))
#define INODE_HEADER_SIZE 	(2*sizeof(__u8) + 3*sizeof(__u16) + 2*sizeof(__u32))
#define MAX_BLOCK_DATA_SIZE 	(BLOCKSIZE - BLOCK_HEADER_SIZE - BLOCK_TRAILER_SIZE)
#define MAX_TABLE_SIZE 		((BLOCKSIZE - BLOCK_HEADER_SIZE - TABLE_HEADER_SIZE - BLOCK_TRAILER_SIZE) / (sizeof(__u32)))
#define MAX_INODE_DATA_SIZE 	((BLOCKSIZE - BLOCK_HEADER_SIZE - INODE_HEADER_SIZE - BLOCK_TRAILER_SIZE)-8)

#define TABLE_BLOCK_INDEX	0
#define ROOT_DIR_BLOCK_INDEX 	1

#define BM_EMPTY 	0x01
#define BM_TABLE 	0x02
#define BM_INODE 	0x04
#define BM_DATA 	0x08
#define BM_UNALLOCATED 	0xff
#define BM_RESERVED 	0x20

#define BM_IS_EMPTY(a) 		(BM_EMPTY & a)
#define BM_IS_TABLE(a) 		(BM_TABLE & a)
#define BM_IS_INODE(a) 		(BM_INODE & a)
#define BM_IS_DATA(a) 		(BM_DATA & a)
#define BM_IS_UNALLOCATED(a) 	((BM_UNALLOCATED & a)==BM_UNALLOCATED)
#define BM_IS_RESERVED(a) 	(BM_RESERVED & a)

#define IM_REG 	0x1
#define IM_DIR 	0x2

#define IM_IS_REG(a) 	(IM_REG & a)
#define IM_IS_DIR(a) 	(IM_DIR & a)

#define MIN(a,b) 	(((a)<(b))?(a):(b))

#define true 	1
#define false 	0

#define DEBUG 1

#include <linux/types.h>

struct vvsfs_block
{
	__u8 	b_mode;
	unsigned char 	b_data[MAX_BLOCK_DATA_SIZE];
	__u32 	b_next;
};

struct vvsfs_inode_table
{
	__u8 	b_mode;
	__u8 	t_padding[3];
	__u32 	t_numblocks;
	__u32 	t_table[MAX_TABLE_SIZE];
	__u32 	b_next;
};
	

struct vvsfs_inode
{
	__u8 	b_mode;
	__u32 	i_ino;
	__u8 	i_kind;
	__u16 	i_mode;
	__u16 	i_uid;
	__u16 	i_gid;
	__u8 	i_links;
	__u32	i_size;
	unsigned char 	i_data[MAX_INODE_DATA_SIZE];
	__u32 	b_next;
};


struct vvsfs_dir_listing
{
        char 	l_name[MAX_NAME_SIZE+1];
        __u32 l_ino;
};
