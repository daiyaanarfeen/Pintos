# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(my-test-write) begin
Hello World!
(my-test-write) end
my-test-write: exit(0)
EOF
pass;
