/* Eric McCreath, 2006-2020, GPL
 * Alex Barilaro, 2020
 * Timothy Day, 2022
 * (based on the simplistic RAM filesystem McCreath 2001)
 */

#include <linux/blkdev.h>
#include <linux/module.h>
#include <linux/statfs.h>

#include "block.h"
#include "inode.h"
#include "logging.h"
#include "mod.h"

static void
dummyfs_put_super (struct super_block *sb)
{
  log_info ("put_super");
  return;
}

static int
dummyfs_statfs (struct dentry *dentry, struct kstatfs *buf)
{
  log_info ("statfs");

  buf->f_namelen = MAX_NAME_SIZE;
  return 0;
}

struct file_operations dummyfs_file_operations = {
  .read = dummyfs_file_read,
  .write = dummyfs_file_write,
};

struct inode_operations dummyfs_file_inode_operations = {
  //       truncate: dummyfs_truncate,
};

struct file_operations dummyfs_dir_operations = {
  .llseek = generic_file_llseek,
  .read = generic_read_dir,
  .iterate = dummyfs_readdir,
  .fsync = generic_file_fsync,
};

struct inode_operations dummyfs_dir_inode_operations = {
  .create = dummyfs_file_create,
  .lookup = dummyfs_lookup,
  .unlink = dummyfs_unlink,
  .mkdir = dummyfs_mkdir,
  .rmdir = dummyfs_rmdir,
  .link = dummyfs_link,
};

struct super_operations dummyfs_ops = {
  .statfs = dummyfs_statfs,
  .put_super = dummyfs_put_super,
};

static struct dentry *
dummyfs_mount (struct file_system_type *fs_type, int flags,
               const char *dev_name, void *data)
{
  return mount_bdev (fs_type, flags, dev_name, data, dummyfs_fill_super);
}

struct file_system_type dummyfs_type = {
  .owner = THIS_MODULE,
  .name = "dummyfs",
  .mount = dummyfs_mount,
  .kill_sb = kill_block_super,
  .fs_flags = FS_REQUIRES_DEV,
};

static int __init
dummyfs_init (void)
{
  log_info ("registering dummyfs");
  return register_filesystem (&dummyfs_type);
}

static void __exit
dummyfs_exit (void)
{
  log_info ("unregistering dummyfs");
  unregister_filesystem (&dummyfs_type);
}

module_init (dummyfs_init);
module_exit (dummyfs_exit);
MODULE_LICENSE ("GPL");
