#include "syscall.h"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  tell (1);
  fail ("should have exited with -1");
}