
use TestInit;
use Config;

BEGIN {
    if (%Config{'extensions'} !~ m/\bXS\/APItest\b/) {
        print "1..0 # Skip: XS::APItest was not built\n";
        exit 0;
    }
}

use Test::More tests => 10;

BEGIN { use_ok('XS::APItest') };

#########################

my $rv;

$XS::APItest::exception_caught = undef;

$rv = try { apitest_exception(0) };
is($@, '');
ok(defined $rv);
is($rv, 42);
is($XS::APItest::exception_caught, 0);

$XS::APItest::exception_caught = undef;

$rv = try { apitest_exception(1) };
is($@->{description}, "boo\n");
ok(not defined $rv);
is($XS::APItest::exception_caught, 1);

$rv = try { mycroak("foobar\n"); 1 };
is($@->{description}, "foobar\n", 'croak');
ok(not defined $rv);