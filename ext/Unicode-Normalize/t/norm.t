
BEGIN {
    if (env::var('PERL_CORE')) {
        chdir('t') if -d 't';
        $^INCLUDE_PATH = @( $^OS_NAME eq 'MacOS' ?? < qw(::lib) !! < qw(../lib) );
    }
}

#########################

use Test::More;

use warnings;
BEGIN { plan tests => 64 };
use Unicode::Normalize < qw(normalize);
ok(1); # If we made it this far, we're ok.

sub _pack_U   { Unicode::Normalize::pack_U(< @_) }
sub _unpack_U { Unicode::Normalize::unpack_U(< @_) }

#########################

is(normalize('D', ""), "");
is(normalize('C', ""), "");
is(normalize('KD',""), "");
is(normalize('KC',""), "");

is(normalize('D', "A"), "A");
is(normalize('C', "A"), "A");
is(normalize('KD',"A"), "A");
is(normalize('KC',"A"), "A");

is(normalize('NFD', ""), "");
is(normalize('NFC', ""), "");
is(normalize('NFKD',""), "");
is(normalize('NFKC',""), "");

is(normalize('NFD', "A"), "A");
is(normalize('NFC', "A"), "A");
is(normalize('NFKD',"A"), "A");
is(normalize('NFKC',"A"), "A");

# don't modify the source
my $sNFD = "\x{FA19}";
is(normalize('NFD', $sNFD), "\x{795E}");
is($sNFD, "\x{FA19}");

my $sNFC = "\x{FA1B}";
is(normalize('NFC', $sNFC), "\x{798F}");
is($sNFC, "\x{FA1B}");

my $sNFKD = "\x{FA1E}";
is(normalize('NFKD', $sNFKD), "\x{7FBD}");
is($sNFKD, "\x{FA1E}");

my $sNFKC = "\x{FA26}";
is(normalize('NFKC', $sNFKC), "\x{90FD}");
is($sNFKC, "\x{FA26}");

sub hexNFC {
  join " ", map { sprintf("\%04X", $_) },
  _unpack_U normalize 'C', _pack_U < map { hex }, split ' ', shift;
}
sub hexNFD {
  join " ", map { sprintf("\%04X", $_) },
  _unpack_U normalize 'D', _pack_U < map { hex }, split ' ', shift;
}

is(hexNFD("1E14 AC01"), "0045 0304 0300 1100 1161 11A8");
is(hexNFD("AC00 AE00"), "1100 1161 1100 1173 11AF");

is(hexNFC("0061 0315 0300 05AE 05C4 0062"), "00E0 05AE 05C4 0315 0062");
is(hexNFC("00E0 05AE 05C4 0315 0062"),      "00E0 05AE 05C4 0315 0062");
is(hexNFC("0061 05AE 0300 05C4 0315 0062"), "00E0 05AE 05C4 0315 0062");
is(hexNFC("0045 0304 0300 AC00 11A8"), "1E14 AC01");
is(hexNFC("1100 1161 1100 1173 11AF"), "AC00 AE00");
is(hexNFC("1100 0300 1161 1173 11AF"), "1100 0300 1161 1173 11AF");

is(hexNFD("0061 0315 0300 05AE 05C4 0062"), "0061 05AE 0300 05C4 0315 0062");
is(hexNFD("00E0 05AE 05C4 0315 0062"),      "0061 05AE 0300 05C4 0315 0062");
is(hexNFD("0061 05AE 0300 05C4 0315 0062"), "0061 05AE 0300 05C4 0315 0062");
is(hexNFC("0061 05C4 0315 0300 05AE 0062"), "0061 05AE 05C4 0300 0315 0062");
is(hexNFC("0061 05AE 05C4 0300 0315 0062"), "0061 05AE 05C4 0300 0315 0062");
is(hexNFD("0061 05C4 0315 0300 05AE 0062"), "0061 05AE 05C4 0300 0315 0062");
is(hexNFD("0061 05AE 05C4 0300 0315 0062"), "0061 05AE 05C4 0300 0315 0062");
is(hexNFC("0000 0041 0000 0000"), "0000 0041 0000 0000");
is(hexNFD("0000 0041 0000 0000"), "0000 0041 0000 0000");

is(hexNFC("AC00 11A7"), "AC00 11A7");
is(hexNFC("AC00 11A8"), "AC01");
is(hexNFC("AC00 11A9"), "AC02");
is(hexNFC("AC00 11C2"), "AC1B");
is(hexNFC("AC00 11C3"), "AC00 11C3");

# Test Cases from Public Review Issue #29: Normalization Issue
# cf. http://www.unicode.org/review/pr-29.html
is(hexNFC("0B47 0300 0B3E"), "0B47 0300 0B3E");
is(hexNFC("1100 0300 1161"), "1100 0300 1161");

is(hexNFC("0B47 0B3E 0300"), "0B4B 0300");
is(hexNFC("1100 1161 0300"), "AC00 0300");

is(hexNFC("0B47 0300 0B3E 0327"), "0B47 0300 0B3E 0327");
is(hexNFC("1100 0300 1161 0327"), "1100 0300 1161 0327");

is(hexNFC("0300 0041"), "0300 0041");
is(hexNFC("0300 0301 0041"), "0300 0301 0041");
is(hexNFC("0301 0300 0041"), "0301 0300 0041");
is(hexNFC("0000 0300 0000 0301"), "0000 0300 0000 0301");
is(hexNFC("0000 0301 0000 0300"), "0000 0301 0000 0300");

is(hexNFC("0327 0061 0300"), "0327 00E0");
is(hexNFC("0301 0061 0300"), "0301 00E0");
is(hexNFC("0315 0061 0300"), "0315 00E0");
is(hexNFC("0000 0327 0061 0300"), "0000 0327 00E0");
is(hexNFC("0000 0301 0061 0300"), "0000 0301 00E0");
is(hexNFC("0000 0315 0061 0300"), "0000 0315 00E0");
