#!./perl

require './test.pl';

plan (9);

my $blank = "";
eval {select undef, $blank, $blank, 0};
is ($@, "");
eval {select $blank, undef, $blank, 0};
is ($@, "");
eval {select $blank, $blank, undef, 0};
is ($@, "");

eval {select "", $blank, $blank, 0};
is ($@, "");
eval {select $blank, "", $blank, 0};
is ($@, "");
eval {select $blank, $blank, "", 0};
is ($@, "");

dies_like( sub {select "a", $blank, $blank, 0},
           qr/^Modification of a read-only value attempted/);
dies_like( sub {select $blank, "a", $blank, 0},
           qr/^Modification of a read-only value attempted/);
dies_like( sub {select $blank, $blank, "a", 0},
           qr/^Modification of a read-only value attempted/);
