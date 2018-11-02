/* Check a removed file cannot be accessed by another process. */

#include <random.h>
#include <string.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

#include <stdio.h>
#include "tests/filesys/base/syn-write.h"

char buf1[1234];

void
test_main (void)
{
  const char *file_name = "deleteme";
  int fd;

  CHECK (create (file_name, sizeof buf1), "create \"%s\"", file_name);
  CHECK ((fd = open (file_name)) > 1, "open \"%s\"", file_name);
  CHECK (remove (file_name), "remove \"%s\"", file_name);

  char cmd_line[128];
  pid_t child;
  snprintf (cmd_line, sizeof cmd_line, "%s %zu", "child-syn-wrt", 0);

  msg("Write the file with a different process should fail.");

  CHECK ((child = exec (cmd_line)) == PID_ERROR,
          "exec child %zu of %zu: \"%s\"", 1, 1, cmd_line);

  msg ("close \"%s\"", file_name);
  close (fd);
}
