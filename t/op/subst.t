#!./perl -w

BEGIN {
    require Config; Config->import;
}


require './test.pl';
plan( tests => 117 );

our ($x, $snum, $foo, $t, $r, $s);

$x = 'foo';
$_ = "x";
s/x/\$x/;
ok( $_ eq '$x', ":$_: eq :\$x:" );

$_ = "x";
s/x/$x/;
ok( $_ eq 'foo', ":$_: eq :foo:" );

$_ = "x";
s/x/\$x $x/;
ok( $_ eq '$x foo', ":$_: eq :\$x foo:" );

dies_like(sub { my @a =~ s/aap/noot/ },
          qr/substitute expected a plain value but got ARRAY/);

$b = 'cd';
($a = 'abcdef') =~ s<(b$b(?:)e)>'\n$1';
ok( $1 eq 'bcde' && $a eq "a\nbcdef" );

$a = 'abacada';
ok( ($a =~ s/a/x/g) == 4 && $a eq 'xbxcxdx' );

ok( ($a =~ s/a/y/g) == 0 && $a eq 'xbxcxdx' );

ok( ($a =~ s/b/y/g) == 1 && $a eq 'xyxcxdx' );

ok( ($a !~ s/c/z/g) eq '' && $a eq 'xyxzxdx' );

ok( ($a !~ s/c/z/g) eq '1' && $a eq 'xyxzxdx' );

$_ = 'ABACADA';
ok( m/a/i && s///gi && $_ eq 'BCD' );

$_ = '\' x 4;
ok( length($_) == 4 );
$snum = s/\\/\\\\/g;
ok( $_ eq '\' x 8 && $snum == 4 );

$_ = '\/' x 4;
ok( length($_) == 8 );
$snum = s/\//\/\//g;
ok( $_ eq '\//' x 4 && $snum == 4 );
ok( length($_) == 12 );

$_ = 'aaaXXXXbbb';
s/^a//;
ok( $_ eq 'aaXXXXbbb' );

$_ = 'aaaXXXXbbb';
s/a//;
ok( $_ eq 'aaXXXXbbb' );

$_ = 'aaaXXXXbbb';
s/^a/b/;
ok( $_ eq 'baaXXXXbbb' );

$_ = 'aaaXXXXbbb';
s/a/b/;
ok( $_ eq 'baaXXXXbbb' );

$_ = 'aaaXXXXbbb';
s/aa//;
ok( $_ eq 'aXXXXbbb' );

$_ = 'aaaXXXXbbb';
s/aa/b/;
ok( $_ eq 'baXXXXbbb' );

$_ = 'aaaXXXXbbb';
s/b$//;
ok( $_ eq 'aaaXXXXbb' );

$_ = 'aaaXXXXbbb';
s/b//;
ok( $_ eq 'aaaXXXXbb' );

$_ = 'aaaXXXXbbb';
s/bb//;
ok( $_ eq 'aaaXXXXb' );

$_ = 'aaaXXXXbbb';
s/aX/y/;
ok( $_ eq 'aayXXXbbb' );

$_ = 'aaaXXXXbbb';
s/Xb/z/;
ok( $_ eq 'aaaXXXzbb' );

$_ = 'aaaXXXXbbb';
s/aaX.*Xbb//;
ok( $_ eq 'ab' );

$_ = 'aaaXXXXbbb';
s/bb/x/;
ok( $_ eq 'aaaXXXXxb' );

# now for some unoptimized versions of the same.

$_ = 'aaaXXXXbbb';
$x ne $x || s/^a//;
ok( $_ eq 'aaXXXXbbb' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/a//;
ok( $_ eq 'aaXXXXbbb' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/^a/b/;
ok( $_ eq 'baaXXXXbbb' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/a/b/;
ok( $_ eq 'baaXXXXbbb' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/aa//;
ok( $_ eq 'aXXXXbbb' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/aa/b/;
ok( $_ eq 'baXXXXbbb' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/b$//;
ok( $_ eq 'aaaXXXXbb' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/b//;
ok( $_ eq 'aaaXXXXbb' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/bb//;
ok( $_ eq 'aaaXXXXb' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/aX/y/;
ok( $_ eq 'aayXXXbbb' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/Xb/z/;
ok( $_ eq 'aaaXXXzbb' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/aaX.*Xbb//;
ok( $_ eq 'ab' );

$_ = 'aaaXXXXbbb';
$x ne $x || s/bb/x/;
ok( $_ eq 'aaaXXXXxb' );

$_ = 'abc123xyz';
s/(\d+)/$($1*2)/;              # yields 'abc246xyz'
ok( $_ eq 'abc246xyz' );
s/(\d+)/$(sprintf("\%5d",$1))/; # yields 'abc  246xyz'
ok( $_ eq 'abc  246xyz' );
s/(\w)/$($1 x 2)/g;            # yields 'aabbcc  224466xxyyzz'
ok( $_ eq 'aabbcc  224466xxyyzz' );

# test recursive substitutions
# code based on the recursive expansion of makefile variables

my %MK = %(
    AAAAA => '$(B)', B=>'$(C)', C => 'D',			# long->short
    E     => '$(F)', F=>'p $(G) q', G => 'HHHHH',	# short->long
    DIR => '$(UNDEFINEDNAME)/xxx',
);
sub var($var,$level) {
    return "\$($var)" unless exists %MK{$var};
    return exp_vars(%MK{?$var}, $level+1); # can recurse
}
sub exp_vars($str,$level) {
    $str =~ s/\$\((\w+)\)/$(var($1, $level+1))/g; # can recurse
    #warn "exp_vars $level = '$str'\n";
    $str;
}

ok( exp_vars('$(AAAAA)',0)           eq 'D' );
ok( exp_vars('$(E)',0)               eq 'p HHHHH q' );
ok( exp_vars('$(DIR)',0)             eq '$(UNDEFINEDNAME)/xxx' );
ok( exp_vars('foo $(DIR)/yyy bar',0) eq 'foo $(UNDEFINEDNAME)/xxx/yyy bar' );

$_ = "abcd";
s/(..)/$( do { $x = $1; m#.#
} )/g;
ok( $x eq "cd", 'a match nested in the RHS of a substitution' );

# Subst and lookbehind

$_="ccccc";
$snum = s/(?<!x)c/x/g;
ok( $_ eq "xxxxx" && $snum == 5 );

$_="ccccc";
$snum = s/(?<!x)(c)/x/g;
ok( $_ eq "xxxxx" && $snum == 5 );

$_="foobbarfoobbar";
$snum = s/(?<!r)foobbar/foobar/g;
ok( $_ eq "foobarfoobbar" && $snum == 1 );

$_="foobbarfoobbar";
$snum = s/(?<!ar)(foobbar)/foobar/g;
ok( $_ eq "foobarfoobbar" && $snum == 1 );

$_="foobbarfoobbar";
$snum = s/(?<!ar)foobbar/foobar/g;
ok( $_ eq "foobarfoobbar" && $snum == 1 );

eval 's{foo} # this is a comment, not a delimiter
       {bar};';
ok( ! $^EVAL_ERROR, 'parsing of split subst with comment' );
is( $_, "barbarfoobbar" );

$_ = "ab";
ok( s/a/b/ == 1 );

$_ = 'a' x 6;
$snum = s/a(?{})//g;
ok( $_ eq '' && $snum == 6 );

$_ = 'x' x 20; 
$snum = s/(\d*|x)/<$1>/g; 
$foo = '<>' . ('<x><>' x 20) ;
ok( $_ eq $foo && $snum == 41 );

$t = 'aaaaaaaaa'; 

$_ = $t;
pos($_, 6);
$snum = s/\Ga/xx/g;
ok( $_ eq 'aaaaaaxxxxxx' && $snum == 3 );

$_ = $t;
pos($_, 6);
$snum = s/\Ga/x/g;
ok( $_ eq 'aaaaaaxxx' && $snum == 3 );

$_ = $t;
pos($_, 6);
s/\Ga/xx/;
ok( $_ eq 'aaaaaaxxaa' );

$_ = $t;
pos($_, 6);
s/\Ga/x/;
ok( $_ eq 'aaaaaaxaa' );

$_ = $t;
$snum = s/\Ga/xx/g;
ok( $_ eq 'xxxxxxxxxxxxxxxxxx' && $snum == 9 );

$_ = $t;
$snum = s/\Ga/x/g;
ok( $_ eq 'xxxxxxxxx' && $snum == 9 );

$_ = $t;
s/\Ga/xx/;
ok( $_ eq 'xxaaaaaaaa' );

$_ = $t;
s/\Ga/x/;
ok( $_ eq 'xaaaaaaaa' );

$_ = 'aaaa';
$snum = s/\ba/./g;
ok( $_ eq '.aaa' && $snum == 1 );

eval q% ($_ = "x") =~ s/(.)/$("$1 ")/ %;
ok( $_ eq "x " and !length $^EVAL_ERROR );
$x = $x = 'interp';
eval q% ($_ = "x") =~ s/x(($x)*)/$(eval "$1")/ %;
ok( $_ eq '' and !length $^EVAL_ERROR );

$_ = "C:/";
ok( !s/^([a-z]:)/$(uc($1))/ );

$_ = "Charles Bronson";
$snum = s/\B\w//g;
ok( $_ eq "C B" && $snum == 12 );

do {
    use utf8;
    my $s = "H\303\266he";
    my $l = my $r = $s;
    $l =~ s/[^\w]//g;
    $r =~ s/[^\w\.]//g;
    is($l, $r, "use utf8 \\w");
};

use utf8;

my $pv1 = my $pv2  = "Andreas J. K\303\266nig";
$pv1 =~ s/A/\x{100}/;
substr($pv2,0,1, "\x{100}");
is($pv1, $pv2);

SKIP: do {
    skip("EBCDIC", 3) if ord("A") == 193; 

    do {   
	# Gregor Chrupala <gregor.chrupala@star-group.net>
	use utf8;
	$a = 'Espa&ntilde;a';
	$a =~ s/&ntilde;/ñ/;
	like($a, qr/ñ/, "use utf8 RHS");
    };

    do {
	use utf8;
	$a = 'España España';
	$a =~ s/ñ/&ntilde;/;
	like($a, qr/ñ/, "use utf8 LHS");
    };

    do {
	use utf8;
	$a = 'España';
	$a =~ s/ñ/ñ/;
	like($a, qr/ñ/, "use utf8 LHS and RHS");
    };
};

do {
    # SADAHIRO Tomoyuki <bqw10602@nifty.com>

    use utf8;

    $a = "\x{100}\x{101}";
    $a =~ s/\x{101}/\x{FF}/;
    like($a, qr/\x{FF}/);
    is(length($a), 2, "SADAHIRO utf8 s///");

    $a = "\x{100}\x{101}";
    $a =~ s/\x{101}/$("\x{FF}")/;
    like($a, qr/\x{FF}/);
    is(length($a), 2);

    $a = "\x{100}\x{101}";
    $a =~ s/\x{101}/\x{FF}\x{FF}\x{FF}/;
    like($a, qr/\x{FF}\x{FF}\x{FF}/);
    is(length($a), 4);

    $a = "\x{100}\x{101}";
    $a =~ s/\x{101}/$("\x{FF}\x{FF}\x{FF}")/;
    like($a, qr/\x{FF}\x{FF}\x{FF}/);
    is(length($a), 4);

    $a = "\x{FF}\x{101}";
    $a =~ s/\x{FF}/\x{100}/;
    like($a, qr/\x{100}/);
    is(length($a), 2);

    $a = "\x{FF}\x{101}";
    $a =~ s/\x{FF}/$("\x{100}")/;
    like($a, qr/\x{100}/);
    is(length($a), 2);

    $a = "\x{FF}";
    $a =~ s/\x{FF}/\x{100}/;
    like($a, qr/\x{100}/);
    is(length($a), 1);

    $a = "\x{FF}";
    $a =~ s/\x{FF}/$("\x{100}")/;
    like($a, qr/\x{100}/);
    is(length($a), 1);
};

do {
    # subst with mixed utf8/non-utf8 type
    my@($ua, $ub, $uc, $ud) = @("\x{101}", "\x{102}", "\x{103}", "\x{104}");
    my@($na, $nb) = @("\x{ff}", "\x{fe}");
    my $a = "$ua--$ub";
    my $b;
    ($b = $a) =~ s/--/$na/;
    is($b, "$ua$na$ub", "s///: replace non-utf8 into utf8");
    ($b = $a) =~ s/--/--$na--/;
    is($b, "$ua--$na--$ub", "s///: replace long non-utf8 into utf8");
    ($b = $a) =~ s/--/$uc/;
    is($b, "$ua$uc$ub", "s///: replace utf8 into utf8");
    ($b = $a) =~ s/--/--$uc--/;
    is($b, "$ua--$uc--$ub", "s///: replace long utf8 into utf8");
    $a = "$na--$nb";
    ($b = $a) =~ s/--/$ua/;
    is($b, "$na$ua$nb", "s///: replace utf8 into non-utf8");
    ($b = $a) =~ s/--/--$ua--/;
    is($b, "$na--$ua--$nb", "s///: replace long utf8 into non-utf8");

    # now with utf8 pattern
    $a = "$ua--$ub";
    ($b = $a) =~ s/-($ud)?-/$na/;
    is($b, "$ua$na$ub", "s///: replace non-utf8 into utf8 (utf8 pattern)");
    ($b = $a) =~ s/-($ud)?-/--$na--/;
    is($b, "$ua--$na--$ub", "s///: replace long non-utf8 into utf8 (utf8 pattern)");
    ($b = $a) =~ s/-($ud)?-/$uc/;
    is($b, "$ua$uc$ub", "s///: replace utf8 into utf8 (utf8 pattern)");
    ($b = $a) =~ s/-($ud)?-/--$uc--/;
    is($b, "$ua--$uc--$ub", "s///: replace long utf8 into utf8 (utf8 pattern)");
    $a = "$na--$nb";
    ($b = $a) =~ s/-($ud)?-/$ua/;
    is($b, "$na$ua$nb", "s///: replace utf8 into non-utf8 (utf8 pattern)");
    ($b = $a) =~ s/-($ud)?-/--$ua--/;
    is($b, "$na--$ua--$nb", "s///: replace long utf8 into non-utf8 (utf8 pattern)");
    ($b = $a) =~ s/-($ud)?-/$na/;
    is($b, "$na$na$nb", "s///: replace non-utf8 into non-utf8 (utf8 pattern)");
    ($b = $a) =~ s/-($ud)?-/--$na--/;
    is($b, "$na--$na--$nb", "s///: replace long non-utf8 into non-utf8 (utf8 pattern)");
};

$_ = 'aaaa';
$r = 'x';
$s = s/a(?{})/$r/g;
is("<$_> <$s>", "<xxxx> <4>", "[perl #7806]");

$_ = 'aaaa';
$s = s/a(?{})//g;
is("<$_> <$s>", "<> <4>", "[perl #7806]");

# [perl #19048] Coredump in silly replacement
do {
    local $^WARNING = 0;
    $_="abcdef\n";
    s!.!$('')!g;
    is($_, "\n", "[perl #19048]");
};

# [perl #17757] interaction between saw_ampersand and study
do {
    my $f = eval q{ $& };
    $f = "xx";
    study $f;
    $f =~ s/x/y/g;
    is($f, "yy", "[perl #17757]");
};

# [perl #20684] returned a zero count
$_ = "1111";
is(s/(??{1})/$(2)/g, 4, '#20684 s/// with (??{..}) inside');

# [perl #20682] $^N not visible in replacement
$_ = "abc";
m/(a)/; s/(b)|(c)/-$($^LAST_SUBMATCH_RESULT)/g;
is($_,'a-b-c','# TODO #20682 $^N not visible in replacement');

# [perl #22351] perl bug with 'e' substitution modifier
my $name = "chris";
do {
    no warnings 'uninitialized';
    $name =~ s/hr/$('')/;
};
is($name, "cis", q[#22351 bug with 'e' substitution modifier]);


do { # [perl #27940] perlbug: [\x00-\x1f] works, [\c@-\c_] does not
    my $c;

    ($c = "\x20\c@\x30\cA\x40\cZ\x50\c_\x60") =~ s/[\c@-\c_]//g;
    is($c, "\x20\x30\x40\x50\x60", "s/[\\c\@-\\c_]//g");

    ($c = "\x20\x00\x30\x01\x40\x1A\x50\x1F\x60") =~ s/[\x00-\x1f]//g;
    is($c, "\x20\x30\x40\x50\x60", "s/[\\x00-\\x1f]//g");
};
