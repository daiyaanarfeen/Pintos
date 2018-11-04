/* Test write on output */

#include <stdio.h>
#include <syscall.h>
#include "tests/main.h"
#include "tests/lib.h"

void
test_main (void)
{
  int status;
  status = wait(exec("bad-tell-3"));
  if (status != -1)
    {
      fail("Tell should have failed on invalid fd\n");
    }
  status = wait(exec("bad-tell-1"));
  if (status != -1)
    {
      fail("Tell should have failed on stdin\n");
    }
  status = wait(exec("bad-tell-2"));
  if (status != -1)
    {
      fail("Tell should have failed on stdout\n");
    }
  msg("Passed!");
}