# -*- perl -*-
use strict;
use warnings;
use tests::tests;
use tests::random;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(my-test-tell) begin
(my-test-tell) create "checktell"
(my-test-tell) open "checktell"
(my-test-tell) write "checktell"
(my-test-tell) the current position should be "50"
(my-test-tell) tell "checktell"
(my-test-tell) seek "checktell" to 0
(my-test-tell) the current position should be "0"
(my-test-tell) tell "checktell"
(my-test-tell) close "checktell"
(my-test-tell) end
EOF
pass;