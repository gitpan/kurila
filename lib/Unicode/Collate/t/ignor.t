BEGIN {
    unless ("A" eq pack('U', 0x41)) {
	print $^STDOUT, "1..0 # Unicode::Collate " .
	    "cannot stringify a Unicode code point\n";
	exit 0;
    }
    if (env::var('PERL_CORE')) {
	chdir('t') if -d 't';
	$^INCLUDE_PATH = @( $^OS_NAME eq 'MacOS' ?? < qw(::lib) !! < qw(../lib) );
    }
}

use Test::More;
BEGIN { plan tests => 41 };

use warnings;
use Unicode::Collate;

ok(1);

my $trad = Unicode::Collate->new(
  table => 'keys.txt',
  normalization => undef,
  ignoreName => qr/HANGUL|HIRAGANA|KATAKANA|BOPOMOFO/,
  level => 3,
  entry => << 'ENTRIES',
 0063 0068 ; [.0A3F.0020.0002.0063] \% "ch" in traditional Spanish
 0043 0068 ; [.0A3F.0020.0007.0043] # "Ch" in traditional Spanish
 0043 0048 ; [.0A3F.0020.0008.0043] # "CH" in traditional Spanish
ENTRIES
);
# 0063  ; [.0A3D.0020.0002.0063] # LATIN SMALL LETTER C
# 0064  ; [.0A49.0020.0002.0064] # LATIN SMALL LETTER D

##### 2..3

is(
  join(':', $trad->sort( < qw/ acha aca ada acia acka / ) ),
  join(':',              qw/ aca acia acka acha ada / ),
);

is(
  join(':', $trad->sort( < qw/ ACHA ACA ADA ACIA ACKA / ) ),
  join(':',              qw/ ACA ACIA ACKA ACHA ADA / ),
);

##### 4..7

is($trad->cmp("ocho", "oc\cAho"), 1); # UCA v14
is($trad->cmp("ocho", "oc\0\cA\0\cBho"), 1);  # UCA v14
ok($trad->eq("-", ""));
is($trad->cmp("ocho", "oc-ho"), 1);

##### 8..11

$trad->change(UCA_Version => 9);

ok($trad->eq("ocho", "oc\cAho")); # UCA v9
ok($trad->eq("ocho", "oc\0\cA\0\cBho")); # UCA v9
ok($trad->eq("-", ""));
is($trad->cmp("ocho", "oc-ho"), 1);

##### 12..15

$trad->change(UCA_Version => 8);

is($trad->cmp("ocho", "oc\cAho"), 1);
is($trad->cmp("ocho", "oc\0\cA\0\cBho"), 1);
ok($trad->eq("-", ""));
is($trad->cmp("ocho", "oc-ho"), 1);


##### 16..19

$trad->change(UCA_Version => 9);

my $hiragana = "\x{3042}\x{3044}";
my $katakana = "\x{30A2}\x{30A4}";

# HIRAGANA and KATAKANA are ignorable via ignoreName
ok($trad->eq($hiragana, ""));
ok($trad->eq("", $katakana));
ok($trad->eq($hiragana, $katakana));
ok($trad->eq($katakana, $hiragana));


##### 20..31

# According to Conformance Test (UCA_Version == 9 or 11),
# a L3-ignorable is treated as a completely ignorable.

my $L3ignorable = Unicode::Collate->new(
  alternate => 'Non-ignorable',
  level => 3,
  table => undef,
  normalization => undef,
  UCA_Version => 9,
  entry => <<'ENTRIES',
0000  ; [.0000.0000.0000.0000] # [0000] NULL (in 6429)
0001  ; [.0000.0000.0000.0000] # [0001] START OF HEADING (in 6429)
0591  ; [.0000.0000.0000.0591] # HEBREW ACCENT ETNAHTA
1D165 ; [.0000.0000.0000.1D165] # MUSICAL SYMBOL COMBINING STEM
0021  ; [*024B.0020.0002.0021] # EXCLAMATION MARK
09BE  ; [.114E.0020.0002.09BE] # BENGALI VOWEL SIGN AA
09C7  ; [.1157.0020.0002.09C7] # BENGALI VOWEL SIGN E
09CB  ; [.1159.0020.0002.09CB] # BENGALI VOWEL SIGN O
09C7 09BE ; [.1159.0020.0002.09CB] # BENGALI VOWEL SIGN O
1D1B9 ; [*098A.0020.0002.1D1B9] # MUSICAL SYMBOL SEMIBREVIS WHITE
1D1BA ; [*098B.0020.0002.1D1BA] # MUSICAL SYMBOL SEMIBREVIS BLACK
1D1BB ; [*098A.0020.0002.1D1B9][.0000.0000.0000.1D165] # M.S. MINIMA
1D1BC ; [*098B.0020.0002.1D1BA][.0000.0000.0000.1D165] # M.S. MINIMA BLACK
ENTRIES
);

is($L3ignorable->cmp("\cA", "!"), -1);
is($L3ignorable->cmp("\x{591}", "!"), -1);
ok($L3ignorable->eq("\cA", "\x{591}"));
ok($L3ignorable->eq("\x{09C7}\x{09BE}A", "\x{09C7}\cA\x{09BE}A"));
ok($L3ignorable->eq("\x{09C7}\x{09BE}A", "\x{09C7}\x{0591}\x{09BE}A"));
ok($L3ignorable->eq("\x{09C7}\x{09BE}A", "\x{09C7}\x{1D165}\x{09BE}A"));
ok($L3ignorable->eq("\x{09C7}\x{09BE}A", "\x{09CB}A"));
is($L3ignorable->cmp("\x{1D1BB}", "\x{1D1BC}"), -1);
ok($L3ignorable->eq("\x{1D1BB}", "\x{1D1B9}"));
ok($L3ignorable->eq("\x{1D1BC}", "\x{1D1BA}"));
ok($L3ignorable->eq("\x{1D1BB}", "\x{1D1B9}\x{1D165}"));
ok($L3ignorable->eq("\x{1D1BC}", "\x{1D1BA}\x{1D165}"));

##### 32..41

my $c = Unicode::Collate->new(
  table => 'keys.txt',
  normalization => undef,
  level => 1,
  UCA_Version => 14,
  entry => << 'ENTRIES',
034F  ; [.0000.0000.0000.034F] # COMBINING GRAPHEME JOINER
0063 0068 ; [.0A3F.0020.0002.0063] \% "ch" in traditional Spanish
0043 0068 ; [.0A3F.0020.0007.0043] # "Ch" in traditional Spanish
0043 0048 ; [.0A3F.0020.0008.0043] # "CH" in traditional Spanish
ENTRIES
);
# 0063  ; [.0A3D.0020.0002.0063] # LATIN SMALL LETTER C
# 0064  ; [.0A49.0020.0002.0064] # LATIN SMALL LETTER D

is($c->cmp("ocho", "oc\x00\x00ho"), 1);
is($c->cmp("ocho", "oc\cAho"), 1);
is($c->cmp("ocho", "oc\x{034F}ho"), 1);
is($c->cmp("ocio", "oc\x{034F}ho"), 1);
is($c->cmp("ocgo", "oc\x{034F}ho"), -1);
is($c->cmp("oceo", "oc\x{034F}ho"), -1);

ok($c->viewSortKey("ocho"),         "[0B4B 0A3F 0B4B | | |]");
ok($c->viewSortKey("oc\x00\x00ho"), "[0B4B 0A3D 0AB9 0B4B | | |]");
ok($c->viewSortKey("oc\cAho"),      "[0B4B 0A3D 0AB9 0B4B | | |]");
ok($c->viewSortKey("oc\x{034F}ho"), "[0B4B 0A3D 0AB9 0B4B | | |]");


