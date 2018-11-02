# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(my-test-remove) begin
(my-test-remove) create "deleteme"
(my-test-remove) open "deleteme"
(my-test-remove) remove "deleteme"
(my-test-remove) Write the file with a different process should fail.
(my-test-remove) exec child 1 of 1: "child-syn-wrt 0"
load: child-syn-wrt: open failed
(my-test-remove) close "deleteme"
(my-test-remove) end
EOF
pass;