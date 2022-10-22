/* Eric McCreath, 2006-2020, GPL
 * Alex Barilaro, 2020
 * (based on the simplistic RAM filesystem McCreath 2001)
 *
 * This code pertains to the manipulation of VFS inodes and
 * reflecting VFS data back to a dummyfs disk.
 */

struct inode *dummyfs_iget(struct super_block *, unsigned long);
struct dentry *dummyfs_lookup(struct inode *, struct dentry *, unsigned int);
ssize_t dummyfs_file_write(struct file *, const char *, size_t, loff_t *);
ssize_t dummyfs_file_read(struct file *, char *, size_t, loff_t *);
int dummyfs_create(struct inode *, struct dentry*, umode_t, unsigned short);
int dummyfs_unlink(struct inode *, struct dentry *);
int dummyfs_rmdir(struct inode *, struct dentry *);
int dummyfs_readdir(struct file *, struct dir_context *);
int dummyfs_link(struct dentry *, struct inode *, struct dentry *);
int dummyfs_file_create(struct inode *, struct dentry *, umode_t, bool);
int dummyfs_mkdir(struct inode *, struct dentry *, umode_t);
int dummyfs_fill_super(struct super_block *, void *, int);
