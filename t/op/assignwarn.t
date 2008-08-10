#!./perl

#
# Verify which OP= operators warn if their targets are undefined.
# Based on redef.t, contributed by Graham Barr <Graham.Barr@tiuk.ti.com>
#	-- Robin Barker <rmb@cise.npl.co.uk>
#

BEGIN {
    require './test.pl';
}

use strict;
use warnings;

my $warn = "";
$^WARN_HOOK = sub { print $warn; $warn .= @_[0]->{description} . "\n" };

sub uninitialized { $warn =~ s/Use of uninitialized value[^\n]+\n//s; }
sub tiex { }
our $TODO;

print "1..32\n";

# go through all tests once normally and once with tied $x
for my $tie ("") {

{ my $x; tiex $x if $tie; $x ++;     ok ! uninitialized, "postinc$tie"; }
{ my $x; tiex $x if $tie; $x --;     ok ! uninitialized, "postdec$tie"; }
{ my $x; tiex $x if $tie; ++ $x;     ok ! uninitialized, "preinc$tie"; }
{ my $x; tiex $x if $tie; -- $x;     ok ! uninitialized, "predec$tie"; }

{ my $x; tiex $x if $tie; $x **= 1;  ok uninitialized,   "**=$tie"; }

{ local $TODO = $tie && '[perl #17809] pp_add & pp_subtract';
    { my $x; tiex $x if $tie; $x += 1;   ok ! uninitialized, "+=$tie"; }
    { my $x; tiex $x if $tie; $x -= 1;   ok ! uninitialized, "-=$tie"; }
}

{ my $x; tiex $x if $tie; $x .= 1;   ok ! uninitialized, ".=$tie"; }

{ my $x; tiex $x if $tie; $x *= 1;   ok uninitialized,   "*=$tie"; }
{ my $x; tiex $x if $tie; $x /= 1;   ok uninitialized,   "/=$tie"; }
{ my $x; tiex $x if $tie; $x %= 1;   ok uninitialized,   "\%=$tie"; }

{ my $x; tiex $x if $tie; $x x= 1;   ok uninitialized, "x=$tie"; }

{ my $x; tiex $x if $tie; $x ^&^= 1;   ok uninitialized, "&=$tie"; }

{ local $TODO = $tie && '[perl #17809] pp_bit_or & pp_bit_xor';
    { my $x; tiex $x if $tie; $x ^|^= 1;   ok ! uninitialized, "|=$tie"; }
    { my $x; tiex $x if $tie; $x ^^^= 1;   ok ! uninitialized, "^=$tie"; }
}

{ my $x; tiex $x if $tie; $x &&= 1;  ok ! uninitialized, "&&=$tie"; }
{ my $x; tiex $x if $tie; $x ||= 1;  ok ! uninitialized, "||=$tie"; }

{ my $x; tiex $x if $tie; $x <<= 1;  ok uninitialized, "<<=$tie"; }
{ my $x; tiex $x if $tie; $x >>= 1;  ok uninitialized, ">>=$tie"; }

{ my $x; tiex $x if $tie; $x ^&^= "x"; ok uninitialized, "&=$tie, string"; }

{ local $TODO = $tie && '[perl #17809] pp_bit_or & pp_bit_xor';
    { my $x; tiex $x if $tie; $x ^|^= "x"; ok ! uninitialized, "|=$tie, string"; }
    { my $x; tiex $x if $tie; $x ^^^= "x"; ok ! uninitialized, "^=$tie, string"; }
}

{ use integer;

{ local $TODO = $tie && '[perl #17809] pp_i_add & pp_i_subtract';
    { my $x; tiex $x if $tie; $x += 1; ok ! uninitialized, "+=$tie, int"; }
    { my $x; tiex $x if $tie; $x -= 1; ok ! uninitialized, "-=$tie, int"; }
}

{ my $x; tiex $x if $tie; $x *= 1; ok uninitialized, "*=$tie, int"; }
{ my $x; tiex $x if $tie; $x /= 1; ok uninitialized, "/=$tie, int"; }
{ my $x; tiex $x if $tie; $x %= 1; ok uninitialized, "\%=$tie, int"; }

{ my $x; tiex $x if $tie; $x ++;   ok ! uninitialized, "postinc$tie, int"; }
{ my $x; tiex $x if $tie; $x --;   ok ! uninitialized, "postdec$tie, int"; }
{ my $x; tiex $x if $tie; ++ $x;   ok ! uninitialized, "preinc$tie, int"; }
{ my $x; tiex $x if $tie; -- $x;   ok ! uninitialized, "predec$tie, int"; }

} # end of use integer;

} # end of for $tie

is $warn, '', "no spurious warnings";
