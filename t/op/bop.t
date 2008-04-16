#!./perl

#
# test the bit operators '&', '|', '^', '~', '<<', and '>>'
#

BEGIN {
    require "./test.pl";
    require Config;
}

# Tests don't have names yet.
# If you find tests are failing, please try adding names to tests to track
# down where the failure is, and supply your new names as a patch.
# (Just-in-time test naming)
plan tests => 135;

our ($Aoz, $Aaz, $Axz, $foo, $bar, $zap, $neg1, $neg7, $x, $y);

# numerics
ok ((0xdead ^&^ 0xbeef) == 0x9ead);
ok ((0xdead ^|^ 0xbeef) == 0xfeef);
ok ((0xdead ^^^ 0xbeef) == 0x6042);
ok ((^~^0xdead ^&^ 0xbeef) == 0x2042);

# shifts
ok ((257 << 7) == 32896);
ok ((33023 >> 7) == 257);

# signed vs. unsigned
ok ((^~^0 +> 0 && do { use integer; ^~^0 } == -1));

my $bits = 0;
for (my $i = ^~^0; $i; $i >>= 1) { ++$bits; }
my $cusp = 1 << ($bits - 1);


ok (($cusp ^&^ -1) +> 0 && do { use integer; $cusp ^&^ -1 } +< 0);
ok (($cusp ^|^ 1) +> 0 && do { use integer; $cusp ^|^ 1 } +< 0);
ok (($cusp ^^^ 1) +> 0 && do { use integer; $cusp ^^^ 1 } +< 0);
ok ((1 << ($bits - 1)) == $cusp &&
    do { use integer; 1 << ($bits - 1) } == -$cusp);
ok (($cusp >> 1) == ($cusp / 2) &&
    do { use integer; abs($cusp >> 1) } == ($cusp / 2));

$Aaz = chr(ord("A") ^&^ ord("z"));
$Aoz = chr(ord("A") ^|^ ord("z"));
$Axz = chr(ord("A") ^^^ ord("z"));

# short strings
is (("AAAAA" ^&^ "zzzzz"), ($Aaz x 5));
is (("AAAAA" ^|^ "zzzzz"), ($Aoz x 5));
is (("AAAAA" ^^^ "zzzzz"), ($Axz x 5));

# long strings
$foo = "A" x 150;
$bar = "z" x 75;
$zap = "A" x 75;
# & truncates
is (($foo ^&^ $bar), ($Aaz x 75 ));
# | does not truncate
is (($foo ^|^ $bar), ($Aoz x 75 . $zap));
# ^ does not truncate
is (($foo ^^^ $bar), ($Axz x 75 . $zap));

# everything using bytes
is (sprintf("\%vd", utf8::chr(0x321)), '204.161');
is (sprintf("\%vd", utf8::chr(0xfff) ^&^ utf8::chr(0x321)), '192.161');
is (sprintf("\%vd", utf8::chr(0xfff) ^|^ utf8::chr(0x321)), '236.191.191');
is (sprintf("\%vd", utf8::chr(0xfff) ^^^ utf8::chr(0x321)), '44.30.191');

#
# UTF8 ~ behaviour: ~ always works on bytes
#

is ^~^"\x[0100]", "\x[FEFF]";

# Tests to see if you really can do casts negative floats to unsigned properly
$neg1 = -1.0;
ok (^~^ $neg1 == 0);
$neg7 = -7.0;
ok (^~^ $neg7 == 6);


# double magic tests

sub TIESCALAR { bless { value => @_[1], orig => @_[1] } }
sub STORE { @_[0]{store}++; @_[0]{value} = @_[1] }
sub FETCH { @_[0]{fetch}++; @_[0]{value} }
sub stores { tied(@_[0])->{value} = tied(@_[0])->{orig};
             delete(tied(@_[0])->{store}) || 0 }
sub fetches { delete(tied(@_[0])->{fetch}) || 0 }

# numeric double magic tests

tie $x, "main", 1;
tie $y, "main", 3;

is(($x ^|^ $y), 3);
is(fetches($x), 1);
is(fetches($y), 1);
is(stores($x), 0);
is(stores($y), 0);

is(($x ^&^ $y), 1);
is(fetches($x), 1);
is(fetches($y), 1);
is(stores($x), 0);
is(stores($y), 0);

is(($x ^^^ $y), 2);
is(fetches($x), 1);
is(fetches($y), 1);
is(stores($x), 0);
is(stores($y), 0);

is(($x ^|^= $y), 3);
is(fetches($x), 2);
is(fetches($y), 1);
is(stores($x), 1);
is(stores($y), 0);

is(($x ^&^= $y), 1);
is(fetches($x), 2);
is(fetches($y), 1);
is(stores($x), 1);
is(stores($y), 0);

is(($x ^^^= $y), 2);
is(fetches($x), 2);
is(fetches($y), 1);
is(stores($x), 1);
is(stores($y), 0);

is(^~^^~^$y, 3);
is(fetches($y), 1);
is(stores($y), 0);

{ use integer;

is(($x ^|^ $y), 3);
is(fetches($x), 1);
is(fetches($y), 1);
is(stores($x), 0);
is(stores($y), 0);

is(($x ^&^ $y), 1);
is(fetches($x), 1);
is(fetches($y), 1);
is(stores($x), 0);
is(stores($y), 0);

is(($x ^^^ $y), 2);
is(fetches($x), 1);
is(fetches($y), 1);
is(stores($x), 0);
is(stores($y), 0);

is(($x ^|^= $y), 3);
is(fetches($x), 2);
is(fetches($y), 1);
is(stores($x), 1);
is(stores($y), 0);

is(($x ^&^= $y), 1);
is(fetches($x), 2);
is(fetches($y), 1);
is(stores($x), 1);
is(stores($y), 0);

is(($x ^^^= $y), 2);
is(fetches($x), 2);
is(fetches($y), 1);
is(stores($x), 1);
is(stores($y), 0);

is(^~^$y, -4);
is(fetches($y), 1);
is(stores($y), 0);

} # end of use integer;

# stringwise double magic tests

tie $x, "main", "a";
tie $y, "main", "c";

is(($x ^|^ $y), ("a" ^|^ "c"));
is(fetches($x), 1);
is(fetches($y), 1);
is(stores($x), 0);
is(stores($y), 0);

is(($x ^&^ $y), ("a" ^&^ "c"));
is(fetches($x), 1);
is(fetches($y), 1);
is(stores($x), 0);
is(stores($y), 0);

is(($x ^^^ $y), ("a" ^^^ "c"));
is(fetches($x), 1);
is(fetches($y), 1);
is(stores($x), 0);
is(stores($y), 0);

is(($x ^|^= $y), ("a" ^|^ "c"));
is(fetches($x), 2);
is(fetches($y), 1);
is(stores($x), 1);
is(stores($y), 0);

is(($x ^&^= $y), ("a" ^&^ "c"));
is(fetches($x), 2);
is(fetches($y), 1);
is(stores($x), 1);
is(stores($y), 0);

is(($x ^^^= $y), ("a" ^^^ "c"));
is(fetches($x), 2);
is(fetches($y), 1);
is(stores($x), 1);
is(stores($y), 0);

is(^~^^~^$y, "c");
is(fetches($y), 1);
is(stores($y), 0);

# [perl #37616] Bug in &= (string) and/or m//
{
    $a = "aa";
    $a ^&^= "a";
    ok($a =~ m/a+$/, 'ASCII "a" is NUL-terminated');

    use utf8;
    $b = "bb\x{100}";
    $b ^&^= "b";
    ok($b =~ m/b+$/, 'Unicode "b" is NUL-terminated');
}

{
    $a = "\x[0101]" x 0x101;
    $b = "\x[FF]" x 0x100;

    my $c = $a ^|^ $b;
    is($c, "\x[FF]" x 0x100 . "\x[0101]" x 0x81);
    is( ($a ^|^ $b), ($b ^|^ $a) );
    $c = $a; $c ^|^= $b;
    is( $c, ($a ^|^ $b) );

    $c = $a ^&^ $b;
    is($c, "\x[01]" x 0x100);
    is( ($a ^&^ $b), ($b ^&^ $a) );
    $c = $a; $c ^^^= $b;
    is( $c, ($a ^^^ $b) );

    $c = $a ^^^ $b;
    is($c, "\x[FE]" x 0x100 . "\x[0101]" x 0x81);
    is( ($a ^^^ $b), ($b ^^^ $a) );
    $c = $a; $c ^^^= $b;
    is( $c, ($a ^^^ $b) );
}
