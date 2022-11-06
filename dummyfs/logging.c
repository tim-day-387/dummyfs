/* Timothy Day, 2022
 * (based on the simplistic RAM filesystem McCreath 2001)
 */

#include <asm/uaccess.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/cred.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/statfs.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/vmalloc.h>

#include "logging.h"

int
log_info (char *string, ...)
{
  va_list valist;
  char log_msg[MAX_LOG_LENGTH] = "";

  va_start (valist, string);

  strcat (log_msg, "dummyfs: ");
  strcat (log_msg, string);
  strcat (log_msg, "\n");

  printk (log_msg, valist);

  va_end (valist);

  return 0;
}
