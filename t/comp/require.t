#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '.';
    push @INC, '../lib';
}

# don't make this lexical
our $i = 1;

my $Is_EBCDIC = (ord('A') == 193) ? 1 : 0;
my $Is_UTF8   = (${^OPEN} || "") =~ /:utf8/;
my $total_tests = 43;
if ($Is_EBCDIC || $Is_UTF8) { $total_tests -= 3; }
print "1..$total_tests\n";

sub do_require {
    %INC = ();
    write_file('bleah.pm',@_);
    eval { require "bleah.pm" };
    my @a; # magic guard for scope violations (must be first lexical in file)
}

sub write_file {
    my $f = shift;
    open(REQ,">$f") or die "Can't write '$f': $!";
    binmode REQ;
    use bytes;
    print REQ @_;
    close REQ or die "Could not close $f: $!";
}

eval {require 5.005};
print "# $@\nnot " if $@;
print "ok ",$i++,"\n";

eval { require 5.005 };
print "# $@\nnot " if $@;
print "ok ",$i++,"\n";

eval { require 5.005; };
print "# $@\nnot " if $@;
print "ok ",$i++,"\n";

eval {
    require 5.005
};
print "# $@\nnot " if $@;
print "ok ",$i++,"\n";

# new style version numbers

eval { require v5.5.630; };
print "# $@\nnot " if $@;
print "ok ",$i++,"\n";

eval { require 10.0.2; };
print "# $@\nnot " unless $@ =~ /^Perl v10\.0\.2 required/;
print "ok ",$i++,"\n";

eval q{ use v5.5.630; };
print "# $@\nnot " if $@;
print "ok ",$i++,"\n";

eval q{ use 10.0.2; };
print "# $@\nnot " unless $@ =~ /^Perl v10\.0\.2 required/;
print "ok ",$i++,"\n";

my $ver = 5.005_63;
eval { require $ver; };
print "# $@\nnot " if $@;
print "ok ",$i++,"\n";

# check inaccurate fp
$ver = 10.2;
eval { require $ver; };
print "# $@\nnot " unless $@ =~ /^Perl v10\.200.0 required/;
print "ok ",$i++,"\n";

$ver = 10.000_02;
eval { require $ver; };
print "# $@\nnot " unless $@ =~ /^Perl v10\.0\.20 required/;
print "ok ",$i++,"\n";

print "not " unless 5.5.1 gt v5.5;
print "ok ",$i++,"\n";

{
    use utf8;
    print "not " unless v5.5.640 eq "\x{5}\x{5}\x{280}";
    print "ok ",$i++,"\n";

    print "not " unless v7.15 eq "\x{7}\x{f}";
    print "ok ",$i++,"\n";

    print "not "
      unless v1.20.300.4000.50000.600000 eq "\x{1}\x{14}\x{12c}\x{fa0}\x{c350}\x{927c0}";
    print "ok ",$i++,"\n";
}

# interaction with pod (see the eof)
write_file('bleah.pm', "print 'ok $i\n'; 1;\n");
require "bleah.pm";
$i++;

# run-time failure in require
do_require "0;\n";
print "# $@\nnot " unless $@ =~ /did not return a true/;
print "ok ",$i++,"\n";

print "not " if exists $INC{'bleah.pm'};
print "ok ",$i++,"\n";

my $flag_file = 'bleah.flg';
# run-time error in require
for my $expected_compile (1,0) {
    write_file($flag_file, 1);
    print "not " unless -e $flag_file;
    print "ok ",$i++,"\n";
    write_file('bleah.pm', "unlink '$flag_file' or die; \$a=0; \$b=1/\$a; 1;\n");
    print "# $@\nnot " if eval { require 'bleah.pm' };
    print "ok ",$i++,"\n";
    print "not " unless -e $flag_file xor $expected_compile;
    print "ok ",$i++,"\n";
    print "not " unless exists $INC{'bleah.pm'};
    print "ok ",$i++,"\n";
}

# compile-time failure in require
do_require "1)\n";
# bison says 'parse error' instead of 'syntax error',
# various yaccs may or may not capitalize 'syntax'.
print "# $@\nnot " unless $@ =~ /(syntax|parse) error/mi;
print "ok ",$i++,"\n";

# previous failure cached in %INC
print "not " unless exists $INC{'bleah.pm'};
print "ok ",$i++,"\n";
write_file($flag_file, 1);
write_file('bleah.pm', "unlink '$flag_file'; 1");
print "# $@\nnot " if eval { require 'bleah.pm' };
print "ok ",$i++,"\n";
print "# $@\nnot " unless $@ =~ /Compilation failed/i;
print "ok ",$i++,"\n";
print "not " unless -e $flag_file;
print "ok ",$i++,"\n";
print "not " unless exists $INC{'bleah.pm'};
print "ok ",$i++,"\n";

# successful require
do_require "1";
print "# $@\nnot " if $@;
print "ok ",$i++,"\n";

# do FILE shouldn't see any outside lexicals
my $x = "ok $i\n";
write_file("bleah.do", <<EOT);
no strict 'vars';
\$x = "not ok $i\\n";
EOT
do "bleah.do" or die $@;
dofile();
sub dofile { do "bleah.do" or die $@; };
print $x;

# Test that scalar context is forced for require

write_file('bleah.pm', <<'**BLEAH**'
print "not " if !defined wantarray || wantarray ne '';
print "ok $i - require() context\n";
1;
**BLEAH**
);
our ($foo, @foo);
                              delete $INC{"bleah.pm"}; ++$::i;
$foo = eval q{require bleah}; delete $INC{"bleah.pm"}; ++$::i;
@foo = eval q{require bleah}; delete $INC{"bleah.pm"}; ++$::i;
       eval q{require bleah}; delete $INC{"bleah.pm"}; ++$::i;
       eval q{$_=$_+2;require bleah}; delete $INC{"bleah.pm"}; ++$::i;
$foo = eval  {require bleah}; delete $INC{"bleah.pm"}; ++$::i;
@foo = eval  {require bleah}; delete $INC{"bleah.pm"}; ++$::i;
       eval  {require bleah};

# Test for fix of RT #24404 : "require $scalar" may load a directory
my $r = "threads";
eval { require $r };
$i++;
if($@ =~ /Can't locate threads in \@INC/) {
    print "ok $i\n";
} else {
    print "not ok $i\n";
}


write_file('bleah.pm', qq(die "This is an expected error";\n));
delete $INC{"bleah.pm"}; ++$::i;
eval { CORE::require bleah; };
if ($@ =~ /^This is an expected error/) {
    print "ok $i\n";
} else {
    print "not ok $i\n";
}

END {
    1 while unlink 'bleah.pm';
    1 while unlink 'bleah.do';
    1 while unlink 'bleah.flg';
}

# ***interaction with pod (don't put any thing after here)***

=pod