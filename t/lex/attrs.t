#!./perl

# Regression tests for attributes.pm and the C< : attrs> syntax.

use warnings;

BEGIN {
    require './test.pl';
}

plan 'no_plan';

${^WARN_HOOK} = sub { die @_ };

our ($anon1, $anon2, $anon3);

sub eval_ok ($;$) {
    eval shift;
    is( $@, '', @_);
}

eval_ok 'sub t1 ($) : locked { $_[0]++ }';
eval_ok 'sub t2 : locked { $_[0]++ }';
eval_ok 'sub t3 ($) : locked ;';
eval_ok 'sub t4 : locked ;';
eval_ok '$anon1 = sub ($) : locked:method { $_[0]++ }';
eval_ok '$anon2 = sub : locked : method { $_[0]++ }';
eval_ok '$anon3 = sub : method { $_[0]->[1] }';

eval 'sub e1 ($) : plugh ;';
like $@->message, qr/^Invalid CODE attributes?: ["']?plugh["']? at/;

eval 'sub e2 ($) : plugh(0,0) xyzzy ;';
like $@->message, qr/^Invalid CODE attributes: ["']?plugh\(0,0\)["']? /;

eval 'sub e3 ($) : plugh(0,0 xyzzy ;';
like $@->message, qr/Unterminated attribute parameter in attribute list at/;

eval 'sub e4 ($) : plugh + xyzzy ;';
like $@->message, qr/Invalid separator character '[+]' in attribute list at/;

eval 'my main $x : = 0;';
like $@->message, qr/Expected variable after declarator at/;
eval_ok 'my $x : = 0;';
eval_ok 'my $x ;';
eval_ok 'my ($x) : = 0;';
eval_ok 'my ($x) ;';
eval_ok 'my ($x) : ;';
eval_ok 'my ($x,$y) : = 0;';
eval_ok 'my ($x,$y) ;';
eval_ok 'my ($x,$y) : ;';

eval 'my ($x,$y) : plugh;';
like $@->message, qr/^Invalid SCALAR attribute: ["']?plugh["']? at/;

# bug #16080
eval '{my $x : plugh}';
like $@->message, qr/^Invalid SCALAR attribute: ["']?plugh["']? at/;
eval '{my ($x,$y) : plugh(})}';
like $@->message, qr/^Invalid SCALAR attribute: ["']?plugh\(}\)["']? at/;

# More syntax tests from the attributes manpage
eval 'my $x : switch(10,foo(7,3))  :  expensive;';
like $@->message, qr/^Invalid SCALAR attributes: ["']?switch\(10,foo\(7,3\)\) : expensive["']? at/;
eval q/my $x : Ugly('\(") :Bad;/;
like $@->message, qr/^Invalid SCALAR attributes: ["']?Ugly\('\\\("\) : Bad["']? at/;
eval 'my $x : _5x5;';
like $@->message, qr/^Invalid SCALAR attribute: ["']?_5x5["']? at/;
eval 'my $x : locked method;';
like $@->message, qr/^Invalid SCALAR attributes: ["']?locked : method["']? at/;
eval 'my $x : switch(10,foo();';
like $@->message, qr/^Unterminated attribute parameter in attribute list at/;
eval q/my $x : Ugly('(');/;
like $@->message, qr/^Unterminated attribute parameter in attribute list at/;
eval 'my $x : 5x5;';
like $@->message, qr/error/;
eval 'my $x : Y2::north;';
like $@->message, qr/Invalid separator character ':' in attribute list at/;

sub A::MODIFY_SCALAR_ATTRIBUTES { return }
eval 'my A $x : plugh;';
like $@->message, qr/^Expected variable after declarator at/;

eval 'my A $x : plugh plover;';
like $@->{description}, qr/^Expected variable after declarator/;

no warnings 'reserved';
eval 'my A $x : plugh;';
like $@->message, qr/^Expected variable after declarator at/;

eval 'package Cat; my Cat @socks;';
like $@->message, qr/^Expected variable after declarator at/;

sub X::MODIFY_CODE_ATTRIBUTES { die "$_[0]" }
sub X::foo { 1 }
*Y::bar = \&X::foo;
*Y::bar = \&X::foo;	# second time for -w
eval 'package Z; sub Y::bar : foo';
like $@->message, qr/^X at /;

eval 'package Z; sub Y::baz : locked {}';
my @attrs = eval 'attributes::get \&Y::baz';
is "@attrs", "locked";

@attrs = eval 'attributes::get $anon1';
is "@attrs", "locked method";

sub Z::DESTROY { }
sub Z::FETCH_CODE_ATTRIBUTES { return 'Z' }
my $thunk = eval 'bless +sub : method locked { 1 }, "Z"';
is ref($thunk), "Z";

@attrs = eval 'attributes::get $thunk';
is "@attrs", "locked method Z";

# Test attributes on predeclared subroutines:
eval 'package A; sub PS : locked';
@attrs = eval 'attributes::get \&A::PS';
is "@attrs", "locked";

# Test ability to modify existing sub's (or XSUB's) attributes.
eval 'package A; sub X { $_[0] } sub X : locked';
@attrs = eval 'attributes::get \&A::X';
is "@attrs", "locked";

# Above not with just 'pure' built-in attributes.
sub Z::MODIFY_CODE_ATTRIBUTES { (); }
eval 'package Z; sub L { $_[0] } sub L : Z locked';
@attrs = eval 'attributes::get \&Z::L';
is "@attrs", "locked Z";

# Begin testing attributes that tie

{
    package Ttie;
    sub DESTROY {}
    sub TIESCALAR { my $x = $_[1]; bless \$x, $_[0]; }
    sub FETCH { ${$_[0]} }
    sub STORE {
	::pass;
	${$_[0]} = $_[1]*2;
    }
    package Tloop;
    sub MODIFY_SCALAR_ATTRIBUTES { tie ${$_[1]}, 'Ttie', -1; (); }
}

eval_ok '
    package Tloop;
    for my $i (0..2) {
	my $x : TieLoop = $i;
	$x != $i*2 and ::is $x, $i*2;
    }
';

# bug #15898
eval 'our ${""} : foo = 1';
like $@->message, qr/Can't declare scalar dereference in "our"/;
eval 'my $$foo : bar = 1';
like $@->message, qr/Can't declare scalar dereference in "my"/;


my @code = qw(locked method);
my @other = qw(shared unique);
my %valid;
$valid{CODE} = {map {$_ => 1} @code};
$valid{SCALAR} = {map {$_ => 1} @other};
$valid{ARRAY} = $valid{HASH} = $valid{SCALAR};

our ($scalar, @array, %hash);
foreach my $value (\&foo, \$scalar, \@array, \%hash) {
    my $type = ref $value;
    foreach my $negate ('', '-') {
	foreach my $attr (@code, @other) {
	    my $attribute = $negate . $attr;
	    eval "use attributes __PACKAGE__, \$value, '$attribute'";
	    if ($valid{$type}{$attr}) {
		if ($attribute eq '-shared') {
		    like $@->message, qr/^A variable may not be unshared/;
		} else {
		    is( $@, '', "$type attribute $attribute");
		}
	    } else {
		like $@->message, qr/^Invalid $type attribute: $attribute/,
		    "Bogus $type attribute $attribute should fail";
	    }
	}
    }
}
