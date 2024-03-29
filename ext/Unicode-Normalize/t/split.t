
BEGIN {
    if (env::var('PERL_CORE')) {
        chdir('t') if -d 't';
        $^INCLUDE_PATH = @( $^OS_NAME eq 'MacOS' ?? < qw(::lib) !! < qw(../lib) );
    }
}

#########################

use Test::More;

use warnings;
BEGIN { plan tests => 14 };
use Unicode::Normalize < qw(:all);
ok(1); # If we made it this far, we're ok.

sub _pack_U   { Unicode::Normalize::pack_U(< @_) }
sub _unpack_U { Unicode::Normalize::unpack_U(< @_) }

#########################

our $proc;    # before the last starter
our $unproc;  # the last starter and after
# If string has no starter, entire string is set to $unproc.

# When you have $normalized string and $unnormalized string following,
# a simple concatenation
#   C<$concat = $normalized . normalize($form, $unnormalized)>
# is wrong. Instead of it, like this:
#
#       ($processed, $unprocessed) = splitOnLastStarter($normalized);
#       $concat = $processed . normalize($form, $unprocessed.$unnormalized);

@($proc, $unproc) = splitOnLastStarter("");
is($proc,   "");
is($unproc, "");

@($proc, $unproc) = splitOnLastStarter("A");
is($proc,   "");
is($unproc, "A");

@($proc, $unproc) = splitOnLastStarter(_pack_U(0x41, 0x300, 0x327, 0x42));
is($proc,   _pack_U(0x41, 0x300, 0x327));
is($unproc, "B");

@($proc, $unproc) = splitOnLastStarter(_pack_U(0x4E00, 0x41, 0x301));
is($proc,   _pack_U(0x4E00));
is($unproc, _pack_U(0x41, 0x301));

@($proc, $unproc) = splitOnLastStarter(_pack_U(0x302, 0x301, 0x300));
is($proc,   "");
is($unproc, _pack_U(0x302, 0x301, 0x300));

our $ka_grave = _pack_U(0x41, 0, 0x42, 0x304B, 0x300);
our $dakuten  = _pack_U(0x3099);
our $ga_grave = _pack_U(0x41, 0, 0x42, 0x304C, 0x300);

our @($p, $u) = splitOnLastStarter($ka_grave);
our $concat = $p . NFC($u.$dakuten);

ok(NFC($ka_grave.$dakuten) eq $ga_grave);
ok(NFC($ka_grave).NFC($dakuten) ne $ga_grave);
ok($concat eq $ga_grave);

