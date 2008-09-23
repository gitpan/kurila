#!./perl
#
#  Copyright (c) 2002 Slaven Rezic
#
#  You may redistribute only under the same terms as Perl 5, as specified
#  in the README file that comes with the distribution.
#

use strict;
use Test::More;
use B::Deparse v0.61;
use File::Spec v0.8;

plan skip_all => "Fix B::Deparse to produce valid code";
plan tests => 59;

use Storable < qw(retrieve store nstore freeze nfreeze thaw dclone);
use Safe;

#$Storable::DEBUGME = 1;

use vars < qw($freezed $thawed @obj @res $blessed_code);

$blessed_code = bless sub { "blessed" }, "Some::Package";
{ package Another::Package; sub foo { __PACKAGE__ } }

{
    no strict; # to make the life for Safe->reval easier
    sub code { "JAPH" }
}

local *FOO;

@obj =
    @(\@(\&code,                   # code reference
      sub { 6*7 },
      $blessed_code,            # blessed code reference
      \&Another::Package::foo,  # code in another package
      sub ($$;$) { 0 },         # prototypes
      sub { print "test\n" },
      \&Test::ok,               # large scalar
     ),

     \%("a" => sub { "srt" }, "b" => \&code),

     sub { ord("a")-ord("7") },

     \&code,

     \&dclone,                 # XS function

     sub { open FOO, "/" },
    );

$Storable::Deparse = 1;
$Storable::Eval    = 1;

######################################################################
# Test freeze & thaw

$freezed = freeze @obj[0];
$thawed  = thaw $freezed;

ok($thawed->[0]->(), "JAPH");
ok($thawed->[1]->(), 42);
ok($thawed->[2]->(), "blessed");
ok($thawed->[3]->(), "Another::Package");
ok(prototype($thawed->[4]), prototype(@obj[0]->[4]));

######################################################################

$freezed = freeze @obj[1];
$thawed  = thaw $freezed;

ok($thawed->{"a"}->(), "srt");
ok($thawed->{"b"}->(), "JAPH");

######################################################################

$freezed = freeze @obj[2];
$thawed  = thaw $freezed;

ok($thawed->(), 42);

######################################################################

$freezed = freeze @obj[3];
$thawed  = thaw $freezed;

ok($thawed->(), "JAPH");

######################################################################

try { $freezed = freeze @obj[4] };
ok($@, qr/The result of B::Deparse::coderef2text was empty/);

######################################################################
# Test dclone

my $new_sub = dclone(@obj[2]);
ok($new_sub->(), @obj[2]->());

######################################################################
# Test retrieve & store

store @obj[0], 'store';
$thawed = retrieve 'store';

ok($thawed->[0]->(), "JAPH");
ok($thawed->[1]->(), 42);
ok($thawed->[2]->(), "blessed");
ok($thawed->[3]->(), "Another::Package");
ok(prototype($thawed->[4]), prototype(@obj[0]->[4]));

######################################################################

nstore @obj[0], 'store';
$thawed = retrieve 'store';
unlink 'store';

ok($thawed->[0]->(), "JAPH");
ok($thawed->[1]->(), 42);
ok($thawed->[2]->(), "blessed");
ok($thawed->[3]->(), "Another::Package");
ok(prototype($thawed->[4]), prototype(@obj[0]->[4]));

######################################################################
# Security with
#   $Storable::Eval
#   $Storable::Deparse

{
    local $Storable::Eval = 0;

    for my $i (0 .. 1) {
	$freezed = freeze @obj[$i];
	$@ = "";
	try { $thawed  = thaw $freezed };
	ok($@, qr/Can\'t eval/);
    }
}

{

    local $Storable::Deparse = 0;
    for my $i (0 .. 1) {
	$@ = "";
	try { $freezed = freeze @obj[$i] };
	ok($@, qr/Can\'t store CODE items/);
    }
}

{
    local $Storable::Eval = 0;
    local $Storable::forgive_me = 1;
    for my $i (0 .. 4) {
	$freezed = freeze @obj[0]->[$i];
	$@ = "";
	try { $thawed  = thaw $freezed };
	ok($@, "");
	ok($$thawed, qr/^sub/);
    }
}

{
    local $Storable::Deparse = 0;
    local $Storable::forgive_me = 1;

    my $devnull = File::Spec->devnull;

    open(SAVEERR, ">&", \*STDERR);
    open(STDERR, ">", $devnull) or
	( print SAVEERR "Unable to redirect STDERR: $!\n" and exit(1) );

    try { $freezed = freeze @obj[0]->[0] };

    open(STDERR, ">&", \*SAVEERR);

    ok($@, "");
    ok($freezed ne '');
}

{
    my $safe = Safe->new();
    local $Storable::Eval = sub { $safe->reval(shift) };

    $freezed = freeze @obj[0]->[0];
    $@ = "";
    try { $thawed = thaw $freezed };
    ok($@, "");
    ok($thawed->(), "JAPH");

    $freezed = freeze @obj[0]->[6];
    try { $thawed = thaw $freezed };
    # The "Code sub ..." error message only appears if Log::Agent is installed
    ok($@, qr/(trapped|Code sub)/);

    if (0) {
	# Disable or fix this test if the internal representation of Storable
	# changes.
	skip("no malicious storable file check", 1);
    } else {
	# Construct malicious storable code
	$freezed = nfreeze @obj[0]->[0];
	my $bad_code = ';open FOO, "/badfile"';
	# 5th byte is (short) length of scalar
	my $len = ord(substr($freezed, 4, 1));
	substr($freezed, 4, 1, chr($len+length($bad_code)));
	substr($freezed, -1, 0, $bad_code);
	$@ = "";
	try { $thawed = thaw $freezed };
	ok($@, qr/(trapped|Code sub)/);
    }
}

{
    my $safe = Safe->new();
    # because of opcodes used in "use strict":
    $safe->permit( <qw(:default require caller));
    local $Storable::Eval = sub { $safe->reval(shift) };

    $freezed = freeze @obj[0]->[1];
    $@ = "";
    try { $thawed = thaw $freezed };
    ok($@, "");
    ok($thawed->(), 42);
}

{
    {
	package MySafe;
	sub new { bless \%(), shift }
	sub reval {
	    my $source = @_[1];
	    # Here you can apply some nifty regexpes to ensure the
	    # safeness of the source code.
	    my $coderef = eval $source;
	    $coderef;
	}
    }

    my $safe = MySafe->new();
    local $Storable::Eval = sub { $safe->reval(@_[0]) };

    $freezed = freeze @obj[0];
    try { $thawed  = thaw $freezed };
    ok($@, "");

    if ($@ ne "") {
        ok(0) for @( ( <1..5));
    } else {
	ok($thawed->[0]->(), "JAPH");
	ok($thawed->[1]->(), 42);
	ok($thawed->[2]->(), "blessed");
	ok($thawed->[3]->(), "Another::Package");
	ok(prototype($thawed->[4]), prototype(@obj[0]->[4]));
    }
}

{
    # Check internal "seen" code
    my $short_sub = sub { "short sub" }; # for SX_SCALAR
    # for SX_LSCALAR
    my $long_sub_code = 'sub { "' . "x"x255 . '" }';
    my $long_sub = eval $long_sub_code; die $@ if $@;
    my $sclr = \1;

    local $Storable::Deparse = 1;
    local $Storable::Eval    = 1;

    for my $sub (@($short_sub, $long_sub)) {
	my $res;

	$res = thaw < freeze \@($sub, $sub);
	ok(int($res->[0]), int($res->[1]));

	$res = thaw < freeze \@($sclr, $sub, $sub, $sclr);
	ok(int($res->[0]), int($res->[3]));
	ok(int($res->[1]), int($res->[2]));

	$res = thaw < freeze \@($sub, $sub, $sclr, $sclr);
	ok(int($res->[0]), int($res->[1]));
	ok(int($res->[2]), int($res->[3]));
    }

}
