/* Eric McCreath, 2006-2020, GPL
 * Alex Barilaro, 2020
 * (based on the simplistic RAM filesystem McCreath 2001)
 *
 * This code pertains to the actual vvsfs module setup and
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

static void vvsfs_put_super(struct super_block *sb)
{
        if (DEBUG)
                printk("vvsfs - put_super\n");
        return;
}

static int vvsfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
        if (DEBUG)
                printk("vvsfs - statfs\n");

        buf->f_namelen = MAX_NAME_SIZE;
        return 0;
}

static struct file_operations vvsfs_file_operations =
{
	read:   vvsfs_file_read,
	write:  vvsfs_file_write,
};

static struct inode_operations vvsfs_file_inode_operations =
{
//       truncate: vvsfs_truncate,
};

static struct file_operations vvsfs_dir_operations =
{
	.llseek  = generic_file_llseek, 
	.read    = generic_read_dir,
	.iterate = vvsfs_readdir,
	.fsync   = generic_file_fsync,
};

static struct inode_operations vvsfs_dir_inode_operations =
{
	create: vvsfs_file_create,
	lookup: vvsfs_lookup,
	unlink: vvsfs_unlink,
	mkdir: vvsfs_mkdir,
	rmdir: vvsfs_rmdir,
	link: vvsfs_link,
};

static struct super_operations vvsfs_ops =
{
	statfs:     vvsfs_statfs,
	put_super:  vvsfs_put_super,
};

static struct dentry *vvsfs_mount(struct file_system_type *fs_type,
                                  int flags,
                                  const char *dev_name,
                                  void *data)
{
	return mount_bdev(fs_type, flags, dev_name, data, vvsfs_fill_super);
}

static struct file_system_type vvsfs_type =
{
	.owner      = THIS_MODULE,
	.name       = "vvsfs",
	.mount      = vvsfs_mount,
	.kill_sb    = kill_block_super,
	.fs_flags   = FS_REQUIRES_DEV,
};

static int __init vvsfs_init(void)
{
	printk("Registering vvsfs\n");
	return register_filesystem(&vvsfs_type);
}

static void __exit vvsfs_exit(void)
{
	printk("Unregistering the vvsfs.\n");
	unregister_filesystem(&vvsfs_type);
}

module_init(vvsfs_init);
module_exit(vvsfs_exit);
MODULE_LICENSE("GPL");
