/* Check a removed file cannot be accessed by another process. */

#include <random.h>
#include <string.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

#include <stdio.h>

char buf1[1234];

void
test_main (void)
{
  const char *file_name = "deleteme";
  int fd;

  CHECK (create (file_name, sizeof buf1), "create \"%s\"", file_name);
  CHECK ((fd = open (file_name)) > 1, "open \"%s\"", file_name);
  CHECK (remove (file_name), "remove \"%s\"", file_name);

  msg("Write the file with a different process should fail.");
  int status = wait(exec("w-closed"));
  if (status == -1)
  {
    fail("Child exited abnormally");
  }

  msg ("close \"%s\"", file_name);
  close (fd);
}
