# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(my-test-tell) begin
(bad-tell-3) begin
bad-tell-3: exit(-1)
(bad-tell-1) begin
bad-tell-1: exit(-1)
(bad-tell-2) begin
bad-tell-2: exit(-1)
(my-test-tell) Passed!
(my-test-tell) end
my-test-tell: exit(0)
EOF
pass;
