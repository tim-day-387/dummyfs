/* Eric McCreath, 2006-2020, GPL
 * Alex Barilaro, 2020
 * Timothy Day, 2022
 * (based on the simplistic RAM filesystem McCreath 2001)
 */

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int
main (int argc, char *argv[])
{
  int dfile, filesize;

  if (argc != 3)
    {
      printf ("usage : truncate <name> <size>\n");
      return 1;
    }

  dfile = open (argv[1], O_RDWR | O_CREAT);

  if (sscanf (argv[2], "%d", &filesize) != 1)
    {
      printf ("Problem with number format\n");
      return 1;
    }

  if (ftruncate (dfile, filesize))
    {
      printf ("Problem with ftruncate\n");
      return 1;
    }

  return 0;
}
