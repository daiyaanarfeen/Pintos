#include "syscall.h"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  char *file_name = "deleteme";
  int fd = open(file_name);
  if (fd != -1) {
  	fail("You managed to open a removed file!\n");
  }
}