#!./perl
# Tests for caller()

BEGIN {
    require './test.pl';
    plan( tests => 76 );
}

my @c;

print $^STDOUT, "# Tests with caller(0)\n";

@c = @( caller(0) );
ok( (!nelems @c), "caller(0) in main program" );

try { @c = @( caller(0) ) };
is( @c[3], "(eval)", "subroutine name in an eval \{\}" );
is( @c[4], undef, "args undef in an eval \{\}" );

eval q{ @c = (Caller(0))[3] };
is( @c[3], "(eval)", "subroutine name in an eval ''" );
is( @c[4], undef, "args undef in an eval ''" );

sub { @c = @( caller(0) ) } -> ();
is( @c[3], undef, "anonymous subroutine name" );
ok( defined @c[4], "hasargs defined with anon sub" );

# Bug 20020517.003, used to dump core
sub foo { @c = @( caller(0) ) }
my $fooref = \(delete %main::{foo});
*$fooref -> ();
is( @c[3], "main::foo", "unknown subroutine name" );
ok( defined @c[4], "args true with unknown sub" );

print $^STDOUT, "# Tests with caller(1)\n";

sub f { @c = @( caller(1) ) }

sub callf { f(); }
callf();
is( @c[3], "main::callf", "subroutine name" );
ok( defined @c[4], "args true with callf()" );

try { f() };
is( @c[3], "(eval)", "subroutine name in an eval \{\}" );
is( @c[4], undef, "args undef in an eval \{\}" );

eval q{ f() };
is( @c[3], "(eval)", "subroutine name in an eval ''" );
is( @c[4], undef, "args false in an eval ''" );

sub { f() } -> ("myarg");
is( @c[3], 'main::__ANON__', "anonymous subroutine name" );
ok( ( nelems(@c[4]) == 1 and @c[4][0] eq "myarg" ),
    "args is correct with anon sub" );

sub foo2 { f() }
my $fooref2 = \(delete %main::{foo2});
*$fooref2 -> ();
is( @c[3], "main::foo2", "unknown subroutine name" );
ok( defined @c[4], "hasargs true with unknown sub" );

# See if caller() returns the correct warning mask

sub show_bits
{
    my $in = shift;
    my $out = '';
    foreach (@(unpack('W*', $in))) {
        $out .= sprintf('\x%02x', $_);
    }
    return $out;
}

sub check_bits
{
    local our $Level = $Level + 2;
    my @($got, $exp, $desc) =  @_;
    if (! ok($got eq $exp, $desc)) {
        diag('     got: ' . show_bits($got));
        diag('expected: ' . show_bits($exp));
    }
}

sub testwarn {
    my $w = shift;
    my $id = shift;
    check_bits( @(caller(0))[9], $w, "warnings match caller ($id)");
}

do {
    use bytes;
    no warnings;
    # Build the warnings mask dynamically
    my ($default, $registered);
    BEGIN {
	for my $i (0..$warnings::LAST_BIT/2 - 1) {
	    vec($default, $i, 2, 1);
	}
	$registered = $default;
	vec($registered, $warnings::LAST_BIT/2, 2, 1);
    }
    BEGIN { check_bits( $^WARNING_BITS, "\0" x 12, 'all bits off via "no warnings"' ) }
    testwarn("\0" x 12, 'no bits');

    use warnings;
    BEGIN { check_bits( $^WARNING_BITS, $default,
			'default bits on via "use warnings"' ); }
    BEGIN { testwarn($default, 'all'); }
    # run-time :
    # the warning mask has been extended by warnings::register
    testwarn($registered, 'ahead of w::r');

    use warnings::register;
    BEGIN { check_bits( $^WARNING_BITS, $registered,
			'warning bits on via "use warnings::register"' ) }
    testwarn($registered, 'following w::r');
};


# The next two cases test for a bug where caller ignored evals if
# the DB::sub glob existed but &DB::sub did not (for example, if 
# $^P had been set but no debugger has been loaded).  The tests
# thus assume that there is no &DB::sub: if there is one, they 
# should both pass  no matter whether or not this bug has been
# fixed.

my $debugger_test =  q<
    my @stackinfo = @(caller(0));
    return nelems @stackinfo;
>;

sub pb { return @(caller(0))[3] }

my $i = eval $debugger_test;
is( $i, 11, "do not skip over eval (and caller returns 10 elements)" );

is( eval 'pb()', 'main::pb', "actually return the right function name" );

my $saved_perldb = $^PERLDB;
$^PERLDB = 16;
$^PERLDB = $saved_perldb;

$i = eval $debugger_test;
is( $i, 11, 'do not skip over eval even if $^P had been on at some point' );
is( eval 'pb()', 'main::pb', 'actually return the right function name even if $^P had been on at some point' );

print $^STDOUT, "# caller can now return the compile time state of \%^H\n";

sub hint_exists {
    my $key = shift;
    my $level = shift;
    my @results = @( caller($level||0) );
    exists @results[10]->{$key};
}

sub hint_fetch {
    my $key = shift;
    my $level = shift;
    my @results = @( caller($level||0) );
    @results[10]->{?$key};
}

$::testing_caller = 1;

do './op/caller.pl' or die $^EVAL_ERROR;
