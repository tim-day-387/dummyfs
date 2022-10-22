/* Eric McCreath, 2006-2020, GPL
 * Alex Barilaro, 2020
 * (based on the simplistic RAM filesystem McCreath 2001)
 *
 * This code pertains to the actual dummyfs module setup and
 * plugging code into the VFS.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/statfs.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>
#include <linux/cred.h>

#include "inode.c"

static void dummyfs_put_super(struct super_block *sb)
{
        if (DEBUG)
                printk("dummyfs - put_super\n");
        return;
}

static int dummyfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
        if (DEBUG)
                printk("dummyfs - statfs\n");

        buf->f_namelen = MAX_NAME_SIZE;
        return 0;
}

static struct file_operations dummyfs_file_operations =
{
	read:   dummyfs_file_read,
	write:  dummyfs_file_write,
};

static struct inode_operations dummyfs_file_inode_operations =
{
//       truncate: dummyfs_truncate,
};

static struct file_operations dummyfs_dir_operations =
{
	.llseek  = generic_file_llseek, 
	.read    = generic_read_dir,
	.iterate = dummyfs_readdir,
	.fsync   = generic_file_fsync,
};

static struct inode_operations dummyfs_dir_inode_operations =
{
	create: dummyfs_file_create,
	lookup: dummyfs_lookup,
	unlink: dummyfs_unlink,
	mkdir: dummyfs_mkdir,
	rmdir: dummyfs_rmdir,
	link: dummyfs_link,
};

static struct super_operations dummyfs_ops =
{
	statfs:     dummyfs_statfs,
	put_super:  dummyfs_put_super,
};

static struct dentry *dummyfs_mount(struct file_system_type *fs_type,
                                  int flags,
                                  const char *dev_name,
                                  void *data)
{
	return mount_bdev(fs_type, flags, dev_name, data, dummyfs_fill_super);
}

static struct file_system_type dummyfs_type =
{
	.owner      = THIS_MODULE,
	.name       = "dummyfs",
	.mount      = dummyfs_mount,
	.kill_sb    = kill_block_super,
	.fs_flags   = FS_REQUIRES_DEV,
};

static int __init dummyfs_init(void)
{
	printk("Registering dummyfs\n");
	return register_filesystem(&dummyfs_type);
}

static void __exit dummyfs_exit(void)
{
	printk("Unregistering the dummyfs.\n");
	unregister_filesystem(&dummyfs_type);
}

module_init(dummyfs_init);
module_exit(dummyfs_exit);
MODULE_LICENSE("GPL");