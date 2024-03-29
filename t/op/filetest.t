#!./perl

# There are few filetest operators that are portable enough to test.
# See pod/perlport.pod for details.

BEGIN {
    require './test.pl';
}

use Config;
plan(tests => 24);

our ($bad_chmod, $oldeuid);

ok( -d 'op' );
ok( -f 'TEST' );
ok( !-f 'op' );
ok( !-d 'TEST' );
ok( -r 'TEST' );

# make sure TEST is r-x
try { chmod 0555, 'TEST' or die "chmod 0555, 'TEST' failed: $^OS_ERROR" };
chomp ($bad_chmod = $^EVAL_ERROR);

$oldeuid = $^EUID;		# root can read and write anything
eval '$^EUID = 1';		# so switch uid (may not be implemented)

print $^STDOUT, "# oldeuid = $oldeuid, euid = $^EUID\n";

SKIP: do {
    if (!config_value("d_seteuid")) {
	skip('no seteuid');
    } 
    elsif (config_value("config_args") =~m/Dmksymlinks/) {
	skip('we cannot chmod symlinks');
    }
    elsif ($bad_chmod) {
	skip( $bad_chmod );
    }
    else {
	ok( !-w 'TEST' );
    }
};

# Scripts are not -x everywhere so cannot test that.

eval '$> = $oldeuid';	# switch uid back (may not be implemented)

# this would fail for the euid 1
# (unless we have unpacked the source code as uid 1...)
ok( -r 'op' );

# this would fail for the euid 1
# (unless we have unpacked the source code as uid 1...)
SKIP: do {
    if (config_value("d_seteuid")) {
	ok( -w 'op' );
    } else {
	skip('no seteuid');
    }
};

ok( -x 'op' ); # Hohum.  Are directories -x everywhere?

is( "$(join ' ', grep { -r }, qw(foo io noo op zoo))", "io op" );

# Test stackability of filetest operators

ok( defined( -f -d 'TEST' ) && ! -f -d _ );
ok( !defined( -e 'zoo' ) );
ok( !defined( -e -d 'zoo' ) );
ok( !defined( -f -e 'zoo' ) );
ok( -f -e 'TEST' );
ok( -e -f 'TEST' );
ok( defined(-d -e 'TEST') );
ok( defined(-e -d 'TEST') );
ok( ! -f -d 'op' );
ok( -x -d -x 'op' );
ok( (-s -f 'TEST' +> 1), "-s returns real size" );
ok( -f -s 'TEST' == 1 );

# test that _ is a bareword after filetest operators

-f 'TEST';
ok( -f _ );
sub _ { "this is not a file name" }
ok( -f _ );
