/* Check the correctness of function tell. */

#include <random.h>
#include <string.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

char buf1[50];

void
test_main (void)
{
  const char *file_name = "checktell";
  int fd;

  CHECK (create (file_name, 2 * sizeof buf1), "create \"%s\"", file_name);
  CHECK ((fd = open (file_name)) > 1, "open \"%s\"", file_name);
  random_bytes (buf1, sizeof buf1);
  CHECK (write (fd, buf1, sizeof buf1) > 0, "write \"%s\"", file_name);

  msg ("the current position should be \"%d\"", tell(fd));
  CHECK (tell (fd) == sizeof(buf1), "tell \"%s\"", file_name);

  msg ("seek \"%s\" to 0", file_name);
  seek (fd, 0);

  msg ("the current position should be \"%d\"", tell(fd));
  CHECK (tell (fd) == 0, "tell \"%s\"", file_name);

  msg ("close \"%s\"", file_name);
  close (fd);
}