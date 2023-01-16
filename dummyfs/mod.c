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

#define FNM "mod"

MODULE_LICENSE ("GPL");

static void
dummyfs_put_super (struct super_block *sb)
{
  log_info (FNM, "put_super");
  return;
}

static int
dummyfs_statfs (struct dentry *dentry, struct kstatfs *buf)
{
  log_info (FNM, "statfs");

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

static struct inode *
dumdbfs_make_inode (struct super_block *sb, int mode,
                    const struct file_operations *fops)
{
  struct inode *inode;
  inode = new_inode (sb);
  if (!inode)
    {
      return NULL;
    }
  inode->i_mode = mode;
  inode->i_atime = inode->i_mtime = inode->i_ctime = current_time (inode);
  inode->i_fop = fops;
  inode->i_ino = get_next_ino ();
  return inode;
};

static int
dumdbfs_open (struct inode *inode, struct file *filp)
{
  filp->private_data = inode->i_private;
  return 0;
};

static ssize_t
dumdbfs_read_file (struct file *filp, char *buf, size_t count, loff_t *offset)
{
  atomic_t *counter = (atomic_t *)filp->private_data;
  int v, len;
  char tmp[TMPSIZE];

  v = atomic_read (counter);
  if (*offset > 0)
    v -= 1;
  else
    atomic_inc (counter);
  len = snprintf (tmp, TMPSIZE, "%d\n", v);
  if (*offset > len)
    return 0;
  if (count > len - *offset)
    count = len - *offset;

  if (copy_to_user (buf, tmp + *offset, count))
    return -EFAULT;
  *offset += count;
  return count;
};

static ssize_t
dumdbfs_write_file (struct file *filp, const char *buf, size_t count,
                    loff_t *offset)
{
  atomic_t *counter = (atomic_t *)filp->private_data;
  char tmp[TMPSIZE];

  if (*offset != 0)
    return -EINVAL;

  if (count >= TMPSIZE)
    return -EINVAL;
  memset (tmp, 0, TMPSIZE);
  if (copy_from_user (tmp, buf, count))
    return -EFAULT;

  log_info (FNM, "%s", tmp);

  atomic_set (counter, simple_strtol (tmp, NULL, 10));
  return count;
};

static struct file_operations dumdbfs_file_ops = {
  .open = dumdbfs_open,
  .read = dumdbfs_read_file,
  .write = dumdbfs_write_file,
};

const struct inode_operations dumdbfs_inode_operations = {
  .setattr = simple_setattr,
  .getattr = simple_getattr,
};

static struct dentry *
dumdbfs_create_file (struct super_block *sb, struct dentry *dir,
                     const char *name, atomic_t *counter)
{
  struct dentry *dentry;
  struct inode *inode;

  dentry = d_alloc_name (dir, name);
  if (!dentry)
    goto out;
  inode = dumdbfs_make_inode (sb, S_IFREG | 0644, &dumdbfs_file_ops);
  if (!inode)
    goto out_dput;
  inode->i_private = counter;

  d_add (dentry, inode);
  return dentry;

out_dput:
  dput (dentry);
out:
  return 0;
};

static atomic_t counter;

static void
dumdbfs_create_files (struct super_block *sb, struct dentry *root)
{
  atomic_set (&counter, 0);
  dumdbfs_create_file (sb, root, "counter", &counter);
};

static struct super_operations dumdbfs_s_ops = {
  .statfs = simple_statfs,
  .drop_inode = generic_delete_inode,
};

static int
dumdbfs_fill_super (struct super_block *sb, void *data, int silent)
{
  struct inode *root;
  struct dentry *root_dentry;

  sb->s_blocksize = VMACACHE_SIZE;
  sb->s_blocksize_bits = VMACACHE_SIZE;
  sb->s_magic = DUMDBFS_MAGIC;
  sb->s_op = &dumdbfs_s_ops;

  root = dumdbfs_make_inode (sb, S_IFDIR | 0755, &simple_dir_operations);
  inode_init_owner (root, NULL, S_IFDIR | 0755);
  if (!root)
    goto out;
  root->i_op = &simple_dir_inode_operations;

  set_nlink (root, 2);
  root_dentry = d_make_root (root);
  if (!root_dentry)
    goto out_iput;

  dumdbfs_create_files (sb, root_dentry);
  sb->s_root = root_dentry;
  return 0;

out_iput:
  iput (root);
out:
  return -ENOMEM;
}

static struct dentry *
dumdbfs_get_super (struct file_system_type *fst, int flags,
                   const char *devname, void *data)
{
  return mount_nodev (fst, flags, data, dumdbfs_fill_super);
}

static struct file_system_type dumdbfs_type = {
  .owner = THIS_MODULE,
  .name = "dumdbfs",
  .mount = dumdbfs_get_super,
  .kill_sb = kill_litter_super,
};

static int __init
dummyfs_init (void)
{
  int rc;

  log_info (FNM, "registering dummyfs");

  rc = register_filesystem (&dumdbfs_type);

  if (rc != 0)
    goto out;

  rc = register_filesystem (&dummyfs_type);

out:
  return rc;
}

static void __exit
dummyfs_exit (void)
{
  log_info (FNM, "unregistering dummyfs");
  unregister_filesystem (&dumdbfs_type);
  unregister_filesystem (&dummyfs_type);
}

module_init (dummyfs_init);
module_exit (dummyfs_exit);
