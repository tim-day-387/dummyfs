/* Eric McCreath, 2006-2020, GPL
 * Alex Barilaro, 2020
 * Timothy Day, 2022
 * (based on the simplistic RAM filesystem McCreath 2001)
 */

#ifndef BLOCK
#define BLOCK

struct inode *dummyfs_new_inode(const struct inode *, umode_t, unsigned short);
unsigned long dummyfs_inode_block_index(struct super_block *, unsigned long, int);
unsigned long dummyfs_empty_block(struct super_block *);
unsigned long dummyfs_alloc_data(struct super_block *, struct dummyfs_block *, unsigned long);
char *dummyfs_map_data(struct super_block *, struct dummyfs_inode *, unsigned int);
void dummyfs_dealloc_data(struct super_block *, unsigned long);
int dummyfs_readblock(struct super_block *, unsigned long, struct dummyfs_block *);
int dummyfs_writeblock(struct super_block *, unsigned long, struct dummyfs_block *);
int dummyfs_read_inode(struct super_block *, unsigned long, struct dummyfs_inode *);
int dummyfs_write_inode(struct super_block *, unsigned long, struct dummyfs_inode *);
int dummyfs_empty_inode(struct super_block *);
int dummyfs_write_data(struct super_block *, struct dummyfs_inode *, unsigned char *, unsigned long);

#endif
