/* Eric McCreath, 2006-2020, GPL
 * Alex Barilaro, 2020
 * Timothy Day, 2022
 * (based on the simplistic RAM filesystem McCreath 2001)
 */

#include <linux/blkdev.h>
#include <linux/statfs.h>
#include <linux/version.h>

#include "block.h"
#include "inode.h"
#include "logging.h"
#include "mod.h"

#define FNM "inode"

/*
 * Create an inode in a directory.
 *
 * Returns 0 on success.
 */
int
dummyfs_create (struct inode *dir, struct dentry *dentry, umode_t mode,
                unsigned short inode_mode)
{
  struct dummyfs_inode dir_data;
  int num_listings;
  struct dummyfs_dir_listing *listing;
  struct inode *inode;
  unsigned char *listings;

  log_info (FNM, "create -> %s", dentry->d_name.name);

  // Establish a default 644 mode if one wasn't given
  if (!mode)
    mode = S_IRUGO | S_IWUGO;

  /*
   * Create a dummyfs inode on disk and instantiate a corresponding
   * VFS inode, making sure to create the right kind (dir or
   * regular file) and assign the right VFS inode operations to it
   */
  if (IM_IS_DIR (inode_mode))
    inode = dummyfs_new_inode (dir, mode | S_IFDIR, inode_mode);
  else
    inode = dummyfs_new_inode (dir, mode | S_IFREG, inode_mode);
  if (!inode)
    return -ENOSPC;
  if (IM_IS_DIR (inode_mode))
    {
      inode->i_op = &dummyfs_dir_inode_operations;
      inode->i_fop = &dummyfs_dir_operations;
      inode->i_mode = mode | S_IFDIR;
    }
  else
    {
      inode->i_op = &dummyfs_file_inode_operations;
      inode->i_fop = &dummyfs_file_operations;
      inode->i_mode = mode;
    }

  // Make sure we've got a directory to put this in
  if (!dir)
    return -1;

  /*
   * dummyfs stores dentries as a dir_listing, which is just a name/inode
   * number pair. These listings make up a directory's data. To add a new one,
   * we'll need to map the directory's existing data (listings) into memory,
   * and append a new name/inode number pair onto the end.
   */
  dummyfs_read_inode (dir->i_sb, dir->i_ino, &dir_data);
  num_listings = dir_data.i_size / sizeof (struct dummyfs_dir_listing);
  listings = dummyfs_map_data (dir->i_sb, &dir_data,
                               sizeof (struct dummyfs_dir_listing));
  listing
      = (struct dummyfs_dir_listing *)(listings
                                       + (num_listings
                                          * sizeof (
                                              struct dummyfs_dir_listing)));

  /*
   * Append the new name/inode number pair onto the end of the listings data,
   * and write the dummyfs inode + data blocks back out to disk
   */
  strncpy (listing->l_name, dentry->d_name.name, dentry->d_name.len);
  listing->l_name[dentry->d_name.len] = '\0';
  listing->l_ino = inode->i_ino;
  dummyfs_write_data (dir->i_sb, &dir_data, listings,
                      (num_listings + 1)
                          * sizeof (struct dummyfs_dir_listing));

  // Update the directory's VFS inode and clean up
  dir->i_size = dir_data.i_size;
  mark_inode_dirty (dir);
  vfree (listings);              // Free the listings from memory
  d_instantiate (dentry, inode); // Couple the VFS dentry with the VFS inode

  log_info (FNM, "file created -> %ld", inode->i_ino);
  return 0;
}

/*
 * Write to a file.
 *
 * Returns the size of the write.
 *
 * Works by reading in an inode and appending a series
 * of bytes (from userspace) to its data field.  */
ssize_t
dummyfs_file_write (struct file *filp, const char *buf, size_t count,
                    loff_t *ppos)
{

  struct dummyfs_inode file_data; // dir_data;
  struct inode *inode = filp->f_path.dentry->d_inode;
  unsigned char *data;
  // struct inode * dir = filp->f_path.dentry->d_parent->d_inode;
  ssize_t pos;
  unsigned long extra;
  struct super_block *sb;

  log_info (FNM, "file write, count -> %zu, ppos -> %Ld", count, *ppos);

  /*
   * These error checks ensure no shenanigans (writing beyond the end/beginning
   * of a file, writing directly to a directory's data, et cetera) are about to
   * happen.
   */
  if (!inode)
    {
      log_info (FNM, "problem with file inode");
      return -EINVAL;
    }
  if (!(S_ISREG (inode->i_mode)))
    {
      log_info (FNM, "not regular file");
      return -EINVAL;
    }
  if (*ppos > inode->i_size || count <= 0)
    {
      log_info (FNM, "attempting to write over the end of a file");
      return 0;
    }

  // Read the inode block in
  sb = inode->i_sb;
  dummyfs_read_inode (sb, inode->i_ino, &file_data);

  // If we're appending, move our start position to the end of the file
  if (filp->f_flags & O_APPEND)
    pos = inode->i_size;
  else // Otherwise, put it where it's been specified
    pos = *ppos;

  /*
   * If the file is about to grow beyond its original size, work
   * out how much it will grow by (so that we can have some padding
   * to write the extra data to when we map the data blocks to memory)
   */
  extra = ((pos + count) <= file_data.i_size)
              ? 0
              : ((pos + count) - file_data.i_size);

  // Map the file data to memory and copy the data from userspace to that
  // memory
  data = dummyfs_map_data (sb, &file_data, extra);
  if (copy_from_user (data + pos, buf, count)) // Append to the file
    return -ENOSPC;
  *ppos = pos;
  buf += count;

  // Write the inode + data blocks back out to the device
  dummyfs_write_data (sb, &file_data, data, file_data.i_size + extra);
  inode->i_size = file_data.i_size;
  mark_inode_dirty (inode);

  vfree (data); // Free the data from memory

  log_info (FNM, "file write, done -> %zu, ppos -> %Ld", count, *ppos);

  return count;
}

/*
 * Read data from a file.
 *
 * Returns the size of the read.
 */
ssize_t
dummyfs_file_read (struct file *filp, char *buf, size_t count, loff_t *ppos)
{

  struct dummyfs_inode file_data;
  struct inode *inode = filp->f_path.dentry->d_inode;
  unsigned char *data;
  ssize_t offset, size;
  struct super_block *sb;

  log_info (FNM, "file read, count -> %zu, ppos -> %Ld", count, *ppos);

  /*
   * These error checks ensure no shenanigans (reading beyond the end/beginning
   * of a file, reading a directory's data directly, et cetera) are about to
   * happen.
   */
  if (!inode)
    {
      log_info (FNM, "problem with file inode");
      return -EINVAL;
    }

  if (!(S_ISREG (inode->i_mode)))
    {
      log_info (FNM, "not regular file");
      return -EINVAL;
    }
  if (*ppos > inode->i_size || count <= 0)
    {
      log_info (FNM, "attempting to read beyond the start/end of a file");
      return 0;
    }

  // Map the data to memory (refer to dummyfs_file_write for an explanation of
  // how dummyfs stores data as linked lists)
  sb = inode->i_sb;
  dummyfs_read_inode (sb, inode->i_ino, &file_data);
  data = dummyfs_map_data (sb, &file_data, 0);

  size = MIN (inode->i_size - *ppos,
              count); // Ensure we only read up to end of file, not past it

  offset = *ppos;
  *ppos += size;

  log_info (FNM, "copying bytes to userspace -> %u, size -> %ld",
            file_data.i_size, size);

  // Copy the data from memory to userspace
  if (copy_to_user (buf, data + offset, size))
    return -EIO;
  buf += size;

  vfree (data); // Free the data from memory

  log_info (FNM, "done file read");
  return size;
}

/*
 * Remove a listing from a directory (and remove the corresponding
 * inode from disk, if the listing was the last reference to it).
 *
 * Returns 0 on success.
 */
int
dummyfs_unlink (struct inode *dir, struct dentry *dentry)
{

  int num_listings, k, l;
  struct dummyfs_inode dir_data;
  unsigned long file_data_index;
  struct inode *inode;
  unsigned char *listings;
  struct dummyfs_dir_listing *listing, *last_listing;

  log_info (FNM, "unlink -> %s", dentry->d_name.name);

  // Retrieve the parent directory's inode metadata and listings
  dummyfs_read_inode (dir->i_sb, dir->i_ino, &dir_data);
  num_listings
      = dir_data.i_size
        / sizeof (struct dummyfs_dir_listing); // Get an upper bounds for the
                                               // later linear search
  listings = dummyfs_map_data (dir->i_sb, &dir_data, 0);

  // Find the listing in the parent directory's data and delete it
  // Search through the parent directory's listings for the one we're trying to
  // remove
  for (k = 0; k < num_listings; k++)
    {
      listing = (struct dummyfs_dir_listing
                     *)((listings) + k * sizeof (struct dummyfs_dir_listing));

      // Check for a match
      if ((strlen (listing->l_name) == dentry->d_name.len)
          && strncmp (listing->l_name, dentry->d_name.name, dentry->d_name.len)
                 == 0)
        {

          // Replace this listing with the last listing (zero out data first)
          for (l = 0; l < MAX_NAME_SIZE; l++)
            listing->l_name[l] = 0;
          last_listing = (struct dummyfs_dir_listing
                              *)((listings)
                                 + (num_listings - 1)
                                       * sizeof (struct dummyfs_dir_listing));
          strncpy (listing->l_name, last_listing->l_name,
                   strlen (last_listing->l_name));
          listing->l_ino = last_listing->l_ino;

          /*
           * Destroy the last listing (so we don't have duplicate data, but
           * also for the case that the last listing is the listing we want to
           * destroy)
           */
          for (l = 0; l < MAX_NAME_SIZE; l++)
            last_listing->l_name[l] = 0;
          last_listing->l_ino = 0;

          break;
        }
    }

  // Write out the truncated directory listings to disk (and shrink the
  // directory's size)
  dummyfs_write_data (dir->i_sb, &dir_data, listings,
                      (num_listings * sizeof (struct dummyfs_dir_listing)));
  dir_data.i_size = dir_data.i_size - sizeof (struct dummyfs_dir_listing);
  dummyfs_write_inode (dir->i_sb, dir->i_ino, &dir_data);

  // Retrieve the VFS inode so we can check how many links it has left
  inode = dentry->d_inode;
  if (!inode)
    {
      log_info (FNM,
                "dentry has no inode attached, can't perform disk removal");
      log_info (
          FNM,
          "may have orphaned inode in VFS/on disk that can't be accessed");
      return -EACCES;
    }

  // Remove inode and data blocks from superblock if the last link is gone
  if (inode->i_nlink == 1)
    {
      log_info (FNM, "inode has no links left, emptying out inode on disk");
      file_data_index = dummyfs_inode_block_index (
          dir->i_sb, inode->i_ino,
          BM_UNALLOCATED); // Remove the inode table entry
      dummyfs_dealloc_data (dir->i_sb,
                            file_data_index); // Deallocate data blocks
    }

  // Update the VFS file inode
  inode_dec_link_count (inode);
  mark_inode_dirty (inode);

  // Update the VFS directory inode
  dir->i_size = dir_data.i_size;
  mark_inode_dirty (dir);

  return 0;
}

/*
 * Remove a directory inode. This is just a wrapper for the regular
 * unlink operation that checks to see if a directory is empty.
 *
 * Returns 0 on success.
 */
int
dummyfs_rmdir (struct inode *dir, struct dentry *dentry)
{
  struct dummyfs_inode dir_data;
  struct inode *del = dentry->d_inode;
  int num_dirs;

  log_info (FNM, "rmdir -> %s", dentry->d_name.name);

  dummyfs_read_inode (dir->i_sb, del->i_ino, &dir_data);
  num_dirs = dir_data.i_size / sizeof (struct dummyfs_dir_listing);
  if (num_dirs == 0)
    {
      dummyfs_unlink (dir, dentry);
    }
  else
    {
      log_info (FNM, "cannot unlink directory with files -> %d", num_dirs);
      log_info (FNM, "done rmdir");
      return -ENOTEMPTY;
    }

  log_info (FNM, "done rmdir");
  return 0;
}

/*
 * Read the listings in a directory and emit them.
 *
 * Returns 0 on success.
 */
int
dummyfs_readdir (struct file *filp, struct dir_context *ctx)
{
  struct inode *inode;
  struct dummyfs_inode dir_data;
  unsigned char *listings;
  int num_listings;
  struct dummyfs_dir_listing
      *listing; // Points to the current name/inode pair (dentry)
  int error, k;

  log_info (FNM, "readdir");

  // Map the directory's listings into memory
  inode = file_inode (filp);
  dummyfs_read_inode (inode->i_sb, inode->i_ino, &dir_data);
  num_listings = dir_data.i_size / sizeof (struct dummyfs_dir_listing);
  listings = dummyfs_map_data (inode->i_sb, &dir_data, 0);

  log_info (FNM, "number of entries -> %d, fpos -> %Ld", num_listings,
            filp->f_pos);

  // Loop through each listing and emit it
  error = 0;
  k = 0;
  listing = (struct dummyfs_dir_listing *)listings;
  while (!error && filp->f_pos < dir_data.i_size && k < num_listings)
    {
      log_info (FNM, "adding name -> %s, ino -> %d", listing->l_name,
                listing->l_ino);

      if (listing->l_ino)
        {
          if (!dir_emit (ctx, listing->l_name,
                         strnlen (listing->l_name, MAX_NAME_SIZE),
                         listing->l_ino, DT_UNKNOWN))
            return 0;
        }
      ctx->pos
          += sizeof (struct dummyfs_dir_listing); // Move to the next listing

      k++;
      listing++;
    }

  // update_atime(i);
  vfree (listings); // Free the listings from memory
  log_info (FNM, "done readdir");

  return 0;
}

/*
 * Create a hard link to an existing file (inode).
 *
 * Returns 0 on success.
 */
int
dummyfs_link (struct dentry *old_dentry, struct inode *dir,
              struct dentry *dentry)
{
  struct dummyfs_inode data;
  int num_listings;
  struct dummyfs_dir_listing *listing;
  struct inode *inode;
  unsigned char *listings;

  log_info (FNM, "link -> %s", dentry->d_name.name);

  // Get the existing inode
  inode = d_inode (old_dentry);
  if (!inode)
    return -ENOSPC;

  if (!dir)
    return -1;

  if (!dir)
    return -1;

  // Get the directory's listings
  dummyfs_read_inode (dir->i_sb, dir->i_ino, &data);
  num_listings = data.i_size / sizeof (struct dummyfs_dir_listing);
  listings = dummyfs_map_data (dir->i_sb, &data,
                               sizeof (struct dummyfs_dir_listing));
  listing
      = (struct dummyfs_dir_listing *)(listings
                                       + (num_listings
                                          * sizeof (
                                              struct dummyfs_dir_listing)));

  // Append a new listing with the same inode as the inode we retrieved earlier
  strncpy (listing->l_name, dentry->d_name.name, dentry->d_name.len);
  listing->l_name[dentry->d_name.len] = '\0';
  listing->l_ino = inode->i_ino;
  dummyfs_write_data (dir->i_sb, &data, listings,
                      (num_listings + 1)
                          * sizeof (struct dummyfs_dir_listing));
  dir->i_size = data.i_size;

  // Update the VFS parent directory
  mark_inode_dirty (dir);
  vfree (listings);

  // Increment the inode block's links field
  dummyfs_read_inode (dir->i_sb, inode->i_ino, &data);
  data.i_links++;
  dummyfs_write_inode (dir->i_sb, inode->i_ino, &data);

  // Update the VFS inode and couple it to the new dentry
  inode_inc_link_count (inode);
  mark_inode_dirty (inode);
  d_instantiate (dentry, inode);

  log_info (FNM, "link created -> %ld", inode->i_ino);
  return 0;
}

/*
 * Find a specific filename (listing) in a directory
 * and create a VFS dentry from it.
 *
 * Returns NULL on success (the dentry is stored in the
 * dentry struct passed as a parameter).
 */
struct dentry *
dummyfs_lookup (struct inode *dir, struct dentry *dentry, unsigned int flags)
{
  int num_listings, k;
  struct dummyfs_inode dir_data;
  struct inode *inode = NULL;
  unsigned char *listings;
  struct dummyfs_dir_listing *listing;

  log_info (FNM, "lookup in dir with ino -> %lu", dir->i_ino);

  // Map the directory's listings to memory
  dummyfs_read_inode (dir->i_sb, dir->i_ino, &dir_data);
  num_listings = dir_data.i_size / sizeof (struct dummyfs_dir_listing);
  listings = dummyfs_map_data (dir->i_sb, &dir_data, 0);

  /*
   * Loop through listings until a match is found between the name in the
   * pair and the name of the file we're trying to find.
   */
  for (k = 0; k < num_listings; k++)
    {
      listing = (struct dummyfs_dir_listing
                     *)((listings) + k * sizeof (struct dummyfs_dir_listing));

      if ((strlen (listing->l_name) == dentry->d_name.len)
          && strncmp (listing->l_name, dentry->d_name.name, dentry->d_name.len)
                 == 0)
        {

          inode = dummyfs_iget (
              dir->i_sb,
              listing->l_ino); // Create a VFS inode from the disk data
          if (!inode)
            return ERR_PTR (-EACCES);

          d_add (dentry, inode);
          return NULL;
        }
    }

  d_add (dentry, inode);
  vfree (listings); // Free the listings from memory

  log_info (FNM, "done lookup");

  return NULL;
}

/*
 * Create a file in a directory. Just a wrapper to
 * dummyfs_create that specifies the inode is a file.
 *
 * Returns 0 on success.
 */
int
dummyfs_file_create (struct inode *dir, struct dentry *dentry, umode_t mode,
                     bool excl)
{
  return dummyfs_create (dir, dentry, mode, IM_REG);
}

/*
 * Create a directory in a directory. Just a wrapper to
 * dummyfs_create that specifies the inode is a directory.
 *
 * Returns 0 on success.
 */
int
dummyfs_mkdir (struct inode *dir, struct dentry *dentry, umode_t mode)
{
  return dummyfs_create (dir, dentry, mode, IM_DIR);
}

/*
 * Instantiate a VFS inode from on-disk dummyfs data.
 *
 * Returns the inode on success.
 */
struct inode *
dummyfs_iget (struct super_block *sb, unsigned long ino)
{
  struct inode *inode;
  struct dummyfs_inode v_inode;

  log_info (FNM, "iget, ino -> %lu", ino);
  log_info (FNM, "iget, super -> %p", sb);

  inode = iget_locked (sb, ino);
  if (!inode)
    return ERR_PTR (-ENOMEM);
  if (!(inode->i_state & I_NEW))
    return inode;

  // Read the inode block in
  dummyfs_read_inode (inode->i_sb, inode->i_ino, &v_inode);

  // Populate the VFS inode's fields
  inode->i_size = v_inode.i_size;
  // inode->i_uid = (kuid_t) v_inode.i_uid;
  // inode->i_gid = (kgid_t) v_inode.i_gid;
  inode->i_ctime = inode->i_mtime = inode->i_atime = current_time (inode);

  // Assign the correct inode operations
  if (IM_IS_DIR (v_inode.i_kind))
    {
      inode->i_mode = v_inode.i_mode | S_IFDIR;
      inode->i_op = &dummyfs_dir_inode_operations;
      inode->i_fop = &dummyfs_dir_operations;
    }
  else
    {
      inode->i_mode = v_inode.i_mode | S_IFREG;
      inode->i_op = &dummyfs_file_inode_operations;
      inode->i_fop = &dummyfs_file_operations;
    }

  unlock_new_inode (inode);
  return inode;
}

/*
 * Create a VFS superblock from on-disk dummyfs data.
 * This is a pretty simple function that just makes the root directory a
 * regular ol' inode to the VFS. The superblock for dummyfs is very minimal.
 *
 * Returns 0 on success.
 */
int
dummyfs_fill_super (struct super_block *s, void *data, int silent)
{
  struct inode *i;
  struct dummyfs_inode inode;
  // struct dummyfs_inode_table table;
  int hblock;
  // int *numblocks = malloc(sizeof(int));

  log_info (FNM, "fill super");

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
  s->s_flags = MS_NOSUID | MS_NOEXEC;
#else
  s->s_flags = ST_NOSUID | SB_NOEXEC;
#endif
  s->s_op = &dummyfs_ops;

  i = new_inode (s);

  i->i_sb = s;
  i->i_ino = 0;
  i->i_flags = 0;
  i->i_mode = S_IRUGO | S_IWUGO | S_IXUGO | S_IFDIR;
  i->i_op = &dummyfs_dir_inode_operations;
  i->i_fop = &dummyfs_dir_operations;
  log_info (FNM, "inode number -> %lu, at -> %p", i->i_ino, i);

  hblock = bdev_logical_block_size (s->s_bdev);
  if (hblock > BLOCKSIZE)
    {
      log_info (FNM, "device blocks are too small");
      return -1;
    }

  set_blocksize (s->s_bdev, BLOCKSIZE);
  s->s_blocksize = BLOCKSIZE;
  s->s_blocksize_bits = BLOCKSIZE_BITS;
  s->s_root = d_make_root (i);

  dummyfs_readblock (s, ROOT_DIR_BLOCK_INDEX, (struct dummyfs_block *)&inode);
  i->i_size = inode.i_size;

  return 0;
}
