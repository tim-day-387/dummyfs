/* Timothy Day, 2022
 * (based on the simplistic RAM filesystem McCreath 2001)
 */

#include <linux/module.h>

#include "logging.h"

static void log_error (char *error_msg);

static const char title[] = "dummyfs>";
static const char spacer[] = ": ";
static const char newline[] = "\n";

int
log_info (char *file, char *string, ...)
{
  char log_msg[MAX_LOG_LENGTH] = "";
  int log_len = strlen (title) + strlen (file) + strlen (spacer)
                + strlen (string) + strlen (newline);

  if (log_len >= MAX_LOG_LENGTH)
    {
      log_error ("log message too long");

      return 0;
    }
  else
    {
      va_list valist;

      va_start (valist, string);

      strcat (log_msg, title);
      strcat (log_msg, file);
      strcat (log_msg, spacer);
      strcat (log_msg, string);
      strcat (log_msg, newline);

      printk (log_msg, valist);

      va_end (valist);

      return 0;
    }
}

static void
log_error (char *error_msg)
{
  const char log_error_title[] = "log_error: ";

  printk ("%s%s%s%s", title, log_error_title, error_msg, newline);
}
