#!./perl

require "./test.pl";
plan( tests => 4 );

use warnings;

my $start = time;
my $sleep_says = sleep 3;
my $diff = time - $start;

cmp_ok( $sleep_says, '+>=', 2,  'Sleep says it slept at least 2 seconds' );
cmp_ok( $sleep_says, '+<=', 10, '... and no more than 10' );

cmp_ok( $diff, '+>=', 2,  'Actual time diff is at least 2 seconds' );
cmp_ok( $diff, '+<=', 10, '... and no more than 10' );
