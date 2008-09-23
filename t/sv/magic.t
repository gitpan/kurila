#!./perl

BEGIN {
    $| = 1;
    $^WARN_HOOK = sub { die "Dying on warning: ", < @_ };
}

BEGIN {
    require "./test.pl";
}

use warnings;
use Config;

plan tests => 49;


my $Is_MSWin32  = $^O eq 'MSWin32';
my $Is_NetWare  = $^O eq 'NetWare';
my $Is_VMS      = $^O eq 'VMS';
my $Is_Dos      = $^O eq 'dos';
my $Is_os2      = $^O eq 'os2';
my $Is_Cygwin   = $^O eq 'cygwin';
my $Is_MacOS    = $^O eq 'MacOS';
my $Is_MPE      = $^O eq 'mpeix';		
my $Is_miniperl = %ENV{PERL_CORE_MINITEST};
my $Is_BeOS     = $^O eq 'beos';

my $PERL = %ENV{PERL}
    || ($Is_NetWare           ? 'perl'   :
       ($Is_MacOS || $Is_VMS) ? $^X      :
       $Is_MSWin32            ? '.\perl' :
       './perl');

eval '%ENV{"FOO"} = "hi there";';	# check that ENV is inited inside eval
# cmd.exe will echo 'variable=value' but 4nt will echo just the value
# -- Nikola Knezevic
if ($Is_MSWin32)  { ok `set FOO` =~ m/^(?:FOO=)?hi there$/; }
elsif ($Is_MacOS) { ok "1 # skipped", 1; }
elsif ($Is_VMS)   { ok `write sys\$output f\$trnlnm("FOO")` eq "hi there\n"; }
else              { ok `echo \$FOO` eq "hi there\n"; }

unlink 'ajslkdfpqjsjfk';
$! = 0;
open(FOO, "<",'ajslkdfpqjsjfk');
ok $!, $!;
close FOO; # just mention it, squelch used-only-once

if ($Is_MSWin32 || $Is_NetWare || $Is_Dos || $Is_MPE || $Is_MacOS) {
    skip('SIGINT not safe on this platform') for 1..4;
}
else {
  # the next tests are done in a subprocess because sh spits out a
  # newline onto stderr when a child process kills itself with SIGINT.
  # We use a pipe rather than system() because the VMS command buffer
  # would overflow with a command that long.

    open( CMDPIPE, "|-", "$PERL");

    print CMDPIPE <<'END';

    $| = 1;		# command buffering

    %SIG{"INT"} = \&ok3;     kill "INT",$$; sleep 1;
    %SIG{"INT"} = "IGNORE";  kill "INT",$$; sleep 1; print "ok 4\n";
    %SIG{"INT"} = "DEFAULT"; kill "INT",$$; sleep 1; print "not ok 4\n";

    sub ok3 {
	if ((my $x = pop(@_)) eq "INT") {
	    print "ok 3\n";
	}
	else {
	    print "not ok 3 ($x @_)\n";
	}
    }

END

    close CMDPIPE;

    open( CMDPIPE, "|-", "$PERL");
    print CMDPIPE <<'END';

    { package X;
	sub DESTROY {
	    kill "INT",$$;
	}
    }
    sub x {
	my $x=bless \@(), 'X';
	return sub { $x };
    }
    $| = 1;		# command buffering
    %SIG{"INT"} = \&ok5;
    {
	local %SIG{"INT"}=x();
	print ""; # Needed to expose failure in 5.8.0 (why?)
    }
    sleep 1;
    delete %SIG{"INT"};
    kill "INT",$$; sleep 1;
    sub ok5 {
	print "ok 5\n";
    }
END
    close CMDPIPE;
    $? >>= 8 if $^O eq 'VMS'; # POSIX status hiding in 2nd byte
    next_test for 1..3;
    local our $TODO = ($^O eq 'os2' ? ' # TODO: EMX v0.9d_fix4 bug: wrong nibble? ' : '');
    ok($? ^&^ 0xFF);
}

dies_like( sub { %SIG{TERM} = 'foo' },
    qr/signal handler should be a code reference or .../ );

# can we slice ENV?
my @val1 = %ENV{[keys(%ENV)]};
my @val2 = values(%ENV);
ok join(':', @val1) eq join(':', @val2);
ok( (nelems @val1) +> 1 );

# regex vars

# $"
my @a = qw(foo bar baz);
ok "{join ' ',@a}" eq "foo bar baz", "{join ' ',@a}";
{
    local $" = ',';
    ok qq|{join $",@a}| eq "foo,bar,baz", "{join ' ',@a}";
}

# $?, $@, $$
if ($Is_MacOS) {
    skip('$? + system are broken on MacPerl') for 1..2;
}
else {
    system qq[$PERL "-I../lib" -e "use vmsish qw(hushed); exit(0)"];
    ok $? == 0, '$?';
    system qq[$PERL "-I../lib" -e "use vmsish qw(hushed); exit(1)"];
    ok $? != 0, '$?';
}

try { die "foo\n" };
ok $@->{description} eq "foo\n", '$@';

ok $$ +> 0, $$;
try { $$++ };
ok $@->{description} =~ m/^Modification of a read-only value attempted/;

our ($wd, $script);

# $^X and $0
{
    if ($^O eq 'qnx') {
	chomp($wd = `/usr/bin/fullpath -t`);
    }
    elsif($Is_Cygwin || %Config{'d_procselfexe'}) {
       # Cygwin turns the symlink into the real file
       chomp($wd = `pwd`);
       $wd =~ s#/t$##;
       if ($Is_Cygwin) {
	   $wd = Cygwin::win_to_posix_path( <Cygwin::posix_to_win_path($wd, 1));
       }
    }
    elsif($Is_os2) {
       $wd = Cwd::sys_cwd();
    }
    elsif($Is_MacOS) {
       $wd = ':';
    }
    else {
	$wd = '.';
    }
    my $perl = ($Is_MacOS || $Is_VMS) ? $^X : "$wd/perl";
    my $headmaybe = '';
    my $middlemaybe = '';
    my $tailmaybe = '';
    $script = "$wd/show-shebang";
    if ($Is_MSWin32) {
	chomp($wd = `cd`);
	$wd =~ s|\\|/|g;
	$perl = "$wd/perl.exe";
	$script = "$wd/show-shebang.bat";
	$headmaybe = <<EOH ;
\@rem ='
\@echo off
$perl -x \%0
goto endofperl
\@rem ';
EOH
	$tailmaybe = <<EOT ;

__END__
:endofperl
EOT
    }
    elsif ($Is_os2) {
      $script = "./show-shebang";
    }
    elsif ($Is_MacOS) {
      $script = ":show-shebang";
    }
    elsif ($Is_VMS) {
      $script = "[]show-shebang";
    }
    elsif ($Is_Cygwin) {
      $middlemaybe = <<'EOX'
$^X = Cygwin::win_to_posix_path(Cygwin::posix_to_win_path($^X, 1));
$0 = Cygwin::win_to_posix_path(Cygwin::posix_to_win_path($0, 1));
EOX
    }
    if ($^O eq 'os390' or $^O eq 'posix-bc' or $^O eq 'vmesa') {  # no shebang
	$headmaybe = <<EOH ;
    eval 'exec ./perl -S \$0 \$\{1+"\$\@"\}'
        if 0;
EOH
    }
    my $s1 = "\$^X is $perl, \$0 is $script\n";
    ok open(SCRIPT, ">", "$script"), '$!';
    ok print(SCRIPT $headmaybe . <<EOB . $middlemaybe . <<'EOF' . $tailmaybe), '$!';
#!$wd/perl
EOB
print "\$^X is $^X, \$0 is $0\n";
EOF
    ok close(SCRIPT), '$!';
    ok chmod(0755, $script), '$!';
    $_ = ($Is_MacOS || $Is_VMS) ? `$perl $script` : `$script`;
    s/\.exe//i if $Is_Dos or $Is_Cygwin or $Is_os2;
    s{./$script}{$script} if $Is_BeOS; # revert BeOS execvp() side-effect
    s{\bminiperl\b}{perl}; # so that test doesn't fail with miniperl
    s{is perl}{is $perl}; # for systems where $^X is only a basename
    s{\\}{/}g;
    ok((($Is_MSWin32 || $Is_os2) ? uc($_) eq uc($s1) : $_ eq $s1), ' :$_:!=:$s1:');
    $_ = `$perl $script`;
    s/\.exe//i if $Is_Dos or $Is_os2 or $Is_Cygwin;
    s{./$perl}{$perl} if $Is_BeOS; # revert BeOS execvp() side-effect
    s{\\}{/}g;
    ok(($Is_MSWin32 || $Is_os2) ? uc($_) eq uc($s1) : $_ eq $s1) or diag " :$_:!=:$s1: after `$perl $script`";
    ok unlink($script) or diag $!;
}

# $], $^O, $^T
ok $^O;
ok $^T +> 850000000, $^T;

# Test change 25062 is working
my $orig_osname = $^O;
{
local $^I = '.bak';
ok($^O eq $orig_osname, 'Assigning $^I does not clobber $^O');
}
$^O = $orig_osname;

if ($Is_VMS || $Is_Dos || $Is_MacOS) {
    skip("\%ENV manipulations fail or aren't safe on $^O") for 1..4;
}
else {
	if (%ENV{PERL_VALGRIND}) {
	    skip("clearing \%ENV is not safe when running under valgrind");
	} else {
	    my $PATH = %ENV{PATH};
	    my $PDL = %ENV{PERL_DESTRUCT_LEVEL} || 0;
	    %ENV{foo} = "bar";
	    %ENV = %( () );
	    %ENV{PATH} = $PATH;
	    %ENV{PERL_DESTRUCT_LEVEL} = $PDL || 0;
	    ok ($Is_MSWin32 ? (`set foo 2>NUL` eq "")
			    : (`echo \$foo` eq "\n") );
	}

	%ENV{__NoNeSuCh} = "foo";
	$0 = "bar";
# cmd.exe will echo 'variable=value' but 4nt will echo just the value
# -- Nikola Knezevic
       ok ($Is_MSWin32 ? (`set __NoNeSuCh` =~ m/^(?:__NoNeSuCh=)?foo$/)
			    : (`echo \$__NoNeSuCh` eq "foo\n") );
	if ($^O =~ m/^(linux|freebsd)$/ &&
	    open CMDLINE, '<', "/proc/$$/cmdline") {
	    chomp(my $line = scalar ~< *CMDLINE);
	    my $me = (split m/\0/, $line)[0];
	    ok($me eq $0, 'altering $0 is effective (testing with /proc/)');
	    close CMDLINE;
            # perlbug #22811
            my $mydollarzero = sub {
              my($arg) = shift;
              $0 = $arg if defined $arg;
	      # In FreeBSD the ps -o command= will cause
	      # an empty header line, grab only the last line.
              my $ps = @(`ps -o command= -p $$`)[-1];
              return if $?;
              chomp $ps;
              printf "# 0[\%s]ps[\%s]\n", $0, $ps;
              $ps;
            };
            my $ps = $mydollarzero->("x");
            ok(!$ps  # we allow that something goes wrong with the ps command
	       # In Linux 2.4 we would get an exact match ($ps eq 'x') but
	       # in Linux 2.2 there seems to be something funny going on:
	       # it seems as if the original length of the argv[] would
	       # be stored in the proc struct and then used by ps(1),
	       # no matter what characters we use to pad the argv[].
	       # (And if we use \0:s, they are shown as spaces.)  Sigh.
               || $ps =~ m/^x\s*$/
	       # FreeBSD cannot get rid of both the leading "perl :"
	       # and the trailing " (perl)": some FreeBSD versions
	       # can get rid of the first one.
	       || ($^O eq 'freebsd' && $ps =~ m/^(?:perl: )?x(?: \(perl\))?$/),
		       'altering $0 is effective (testing with `ps`)');
	} else {
	    skip("\$0 check only on Linux and FreeBSD") for @( 0, 1);
	}
}

{
    my $ok = 1;
    my $warn = '';
    local $^WARN_HOOK = sub { $ok = 0; $warn = join '', @_; };
    $! = undef;
    ok($ok, $warn, $Is_VMS ? "'\$!=undef' does throw a warning" : '');
}

SKIP: {
    # test case-insignificance of %ENV (these tests must be enabled only
    # when perl is compiled with -DENV_IS_CASELESS)
    skip('no caseless %ENV support', 4) unless $Is_MSWin32 || $Is_NetWare;
    %ENV = %( () );
    %ENV{'Foo'} = 'bar';
    %ENV{'fOo'} = 'baz';
    ok (scalar(keys(%ENV)) == 1);
    ok exists(%ENV{'FOo'});
    ok (delete(%ENV{'foO'}) eq 'baz');
    ok (scalar(keys(%ENV)) == 0);
}

if ($Is_miniperl) {
    skip ("miniperl can't rely on loading \%Errno") for 1..2;
} else {
   no warnings 'void';

# Make sure Errno hasn't been prematurely autoloaded

   ok ! %{Symbol::stash("Errno")};

# Test auto-loading of Errno when %! is used

   ok scalar eval q{
      %!;
      defined %Errno::;
   }, $@ && $@->message;
}

if ($Is_miniperl) {
    skip ("miniperl can't rely on loading \%Errno");
} else {
    # Make sure that Errno loading doesn't clobber $!

    undef %{Symbol::stash("Errno")};
    delete %INC{"Errno.pm"};

    open(FOO, "<", "nonesuch"); # Generate ENOENT
    ok %{*{Symbol::fetch_glob("!")}}{ENOENT};
}

ok $^S == 0 && defined $^S;
try { ok $^S == 1 };
eval " BEGIN \{ ok ! defined \$^S \} ";
ok $^S == 0 && defined $^S;

ok $^TAINT == 0;
try { $^TAINT = 1 };
ok $^TAINT == 0;

# Tests for the magic get of $\
{
    my $ok = 0;
    # [perl #19330]
    {
	local $\ = undef;
	$\++; $\++;
	$ok = $\ eq 2;
    }
    ok $ok;
    $ok = 0;
    {
	local $\ = "a\0b";
	$ok = "a$\b" eq "aa\0bb";
    }
    ok $ok;
}

# Test for bug [perl #36434]
if (!$Is_VMS) {
    our @ISA;
    local @ISA;
    local %ENV;
    # This used to be __PACKAGE__, but that causes recursive
    #  inheritance, which is detected earlier now and broke
    #  this test
    try { push @ISA, __FILE__ };
    ok( $@ eq '', 'Push a constant on a magic array');
    $@ and print "# $@";
    try { %ENV = %(PATH => __PACKAGE__) };
    ok( $@ eq '', 'Assign a constant to a magic hash');
    $@ and print "# $@";
    try { my %h = %( < qw(A B) ); %ENV = %(PATH => (keys %h)[0]) };
    ok( $@ eq '', 'Assign a shared key to a magic hash');
    $@ and print "# $@";
}
else {
# Can not do this test on VMS, EPOC, and SYMBIAN according to comments
# in mg.c/Perl_magic_clear_all_env()
#
    skip(q|Can't make assignment to \%ENV on this system|) for 1..3;
}
