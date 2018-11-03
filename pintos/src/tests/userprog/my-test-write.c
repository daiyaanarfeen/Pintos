/* Test write on output */

#include <stdio.h>
#include <syscall.h>
#include "tests/main.h"

void
test_main (void)
{
  char buf[16] = "Hello World!\n";
  write (STDOUT_FILENO, &buf, 13);
}