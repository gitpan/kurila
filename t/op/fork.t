#!./perl

# tests for both real and emulated fork()

BEGIN {
    our %Config;
    require Config; Config->import;
    unless (%Config{'d_fork'} or %Config{'d_pseudofork'}) {
	print "1..0 # Skip: no fork\n";
	exit 0;
    }
    %ENV{PERL5LIB} = "../lib";
}

if ($^O eq 'mpeix') {
    print "1..0 # Skip: fork/status problems on MPE/iX\n";
    exit 0;
}

$|=1;

our (@prgs, $tmpfile, $CAT, $status, $i);

undef $/;
@prgs = @( split "\n########\n", ~< *DATA );
print "1..", scalar nelems @prgs, "\n";

$tmpfile = "forktmp000";
1 while -f ++$tmpfile;
END { close TEST; unlink $tmpfile if $tmpfile; }

$CAT = (($^O eq 'MSWin32') ? '.\perl -e "print ~< *ARGV"' : (($^O eq 'NetWare') ? 'perl -e "print ~< *ARGV"' : 'cat'));

for (< @prgs){
    my $switch;
    if (s/^\s*(-\w.*)//){
	$switch = $1;
    }
    my($prog,$expected) = split(m/\nEXPECT\n/, $_);
    $expected =~ s/\n+$//;
    # results can be in any order, so sort 'em
    my @expected = @( sort split m/\n/, $expected );
    open TEST, ">", "$tmpfile" or die "Cannot open $tmpfile: $!";
    print TEST $prog, "\n";
    close TEST or die "Cannot close $tmpfile: $!";
    my $results;
    if ($^O eq 'MSWin32') {
      $results = `.\\perl -I../lib $switch $tmpfile 2>&1`;
    }
    elsif ($^O eq 'NetWare') {
      $results = `perl -I../lib $switch $tmpfile 2>&1`;
    }
    else {
      $results = `./perl $switch $tmpfile 2>&1`;
    }
    $status = $?;
    $results =~ s/\n+$//;
    $results =~ s/at\s+forktmp\d+\s+line/at - line/g;
    $results =~ s/of\s+forktmp\d+\s+aborted/of - aborted/g;
# bison says 'parse error' instead of 'syntax error',
# various yaccs may or may not capitalize 'syntax'.
    $results =~ s/^(syntax|parse) error/syntax error/mig;
    $results =~ s/^\n*Process terminated by SIG\w+\n?//mg
	if $^O eq 'os2';
    my @results = @( sort split m/\n/, $results );
    if ( "{join ' ', <@results}" ne "{join ' ', <@expected}" ) {
	print STDERR "PROG: $switch\n$prog\n";
	print STDERR "EXPECTED:\n$expected\n";
	print STDERR "GOT:\n$results\n";
	print "not ";
    }
    print "ok ", ++$i, "\n";
}

__END__
$| = 1;
if (my $cid = fork) {
    sleep 1;
    if (my $result = (kill 9, $cid)) {
	print "ok 2\n";
    }
    else {
	print "not ok 2 $result\n";
    }
    sleep 1 if $^O eq 'MSWin32';	# avoid WinNT race bug
}
else {
    print "ok 1\n";
    sleep 10;
}
EXPECT
ok 1
ok 2
########
$| = 1;
if (my $cid = fork) {
    sleep 1;
    print "not " unless kill 'INT', $cid;
    print "ok 2\n";
}
else {
    # XXX On Windows the default signal handler kills the
    # XXX whole process, not just the thread (pseudo-process)
    %SIG{INT} = sub { exit };
    print "ok 1\n";
    sleep 5;
    die;
}
EXPECT
ok 1
ok 2
########
$| = 1;
our $i;
sub forkit {
    print "iteration $i start\n";
    my $x = fork;
    if (defined $x) {
	if ($x) {
	    print "iteration $i parent\n";
	}
	else {
	    print "iteration $i child\n";
	}
    }
    else {
	print "pid $$ failed to fork\n";
    }
}
while ($i++ +< 3) { do { forkit(); }; }
EXPECT
iteration 1 start
iteration 1 parent
iteration 1 child
iteration 2 start
iteration 2 parent
iteration 2 child
iteration 2 start
iteration 2 parent
iteration 2 child
iteration 3 start
iteration 3 parent
iteration 3 child
iteration 3 start
iteration 3 parent
iteration 3 child
iteration 3 start
iteration 3 parent
iteration 3 child
iteration 3 start
iteration 3 parent
iteration 3 child
########
$| = 1;
fork()
 ? (print("parent\n"),sleep(1))
 : (print("child\n"),exit) ;
EXPECT
parent
child
########
$| = 1;
fork()
 ? (print("parent\n"),exit)
 : (print("child\n"),sleep(1)) ;
EXPECT
parent
child
########
$| = 1;
my @a = @(1..3);
for (<@a) {
    if (fork) {
	print "parent $_\n";
	$_ = "[$_]";
    }
    else {
	print "child $_\n";
	$_ = "-$_-";
    }
}
print "{join ' ', <@a}\n";
EXPECT
parent 1
child 1
parent 2
child 2
parent 2
child 2
parent 3
child 3
parent 3
child 3
parent 3
child 3
parent 3
child 3
[1] [2] [3]
-1- [2] [3]
[1] -2- [3]
[1] [2] -3-
-1- -2- [3]
-1- [2] -3-
[1] -2- -3-
-1- -2- -3-
########
$| = 1;
foreach my $c (1,2,3) {
    if (fork) {
	print "parent $c\n";
    }
    else {
	print "child $c\n";
	exit;
    }
}
while (wait() != -1) { print "waited\n" }
EXPECT
child 1
child 2
child 3
parent 1
parent 2
parent 3
waited
waited
waited
########
use Config;
$| = 1;
$\ = "\n";
fork()
 ? print(%Config{osname} eq $^O)
 : print(%Config{osname} eq $^O) ;
EXPECT
1
1
########
$| = 1;
$\ = "\n";
fork()
 ? do { require Config; print(%Config::Config{osname} eq $^O); }
 : do { require Config; print(%Config::Config{osname} eq $^O); }
EXPECT
1
1
########
$| = 1;
use Cwd;
my $cwd = cwd(); # Make sure we load Win32.pm while "../lib" still works.
$\ = "\n";
my $dir;
if (fork) {
    $dir = "f$$.tst";
    mkdir $dir, 0755;
    chdir $dir;
    print cwd() =~ m/\Q$dir/i ? "ok 1 parent" : "not ok 1 parent";
    chdir "..";
    rmdir $dir;
}
else {
    sleep 2;
    $dir = "f$$.tst";
    mkdir $dir, 0755;
    chdir $dir;
    print cwd() =~ m/\Q$dir/i ? "ok 1 child" : "not ok 1 child";
    chdir "..";
    rmdir $dir;
}
EXPECT
ok 1 parent
ok 1 child
########
$| = 1;
$\ = "\n";
my $getenv;
if ($^O eq 'MSWin32' || $^O eq 'NetWare') {
    $getenv = qq[$^X -e "print \%ENV\{TST\}"];
}
else {
    $getenv = qq[$^X -e 'print \%ENV\{TST\}'];
}
%ENV{TST} = 'foo';
if (fork) {
    sleep 1;
    print "parent before: " . `$getenv`;
    %ENV{TST} = 'bar';
    print "parent after: " . `$getenv`;
}
else {
    print "child before: " . `$getenv`;
    %ENV{TST} = 'baz';
    print "child after: " . `$getenv`;
}
EXPECT
child before: foo
child after: baz
parent before: foo
parent after: bar
########
$| = 1;
$\ = "\n";
if (my $pid = fork) {
    waitpid($pid,0);
    print "parent got $?"
}
else {
    exit(42);
}
EXPECT
parent got 10752
########
$| = 1;
$\ = "\n";
my $echo = 'echo';
if (my $pid = fork) {
    waitpid($pid,0);
    print "parent got $?"
}
else {
    exec("$echo foo");
}
EXPECT
foo
parent got 0
########
if (fork) {
    die "parent died";
}
else {
    sleep 1; die "child died";
}
EXPECT
parent died at - line 2.
child died at - line 5.
########
if (my $pid = fork) {
    try { die "parent died" };
    print $@->message;
}
else {
    sleep 1; try { die "child died" };
    print $@->message;
}
EXPECT
parent died at - line 2.
    (eval) called at - line 6.
child died at - line 6.
    (eval) called at - line 2.
########
my $pid;
if (eval q{$pid = fork}) {
    eval q{ die "parent died" };
    print $@->message;
}
else {
    sleep 1; eval q{ die "child died" };
    print $@->message;
}
EXPECT
parent died at (eval 2) line 1.
    (eval) called at - line 3.
child died at (eval 2) line 1.
    (eval) called at - line 7.
########
BEGIN {
    $| = 1;
    fork and exit;
    print "inner\n";
}
# XXX In emulated fork(), the child will not execute anything after
# the BEGIN block, due to difficulties in recreating the parse stacks
# and restarting yyparse() midstream in the child.  This can potentially
# be overcome by treating what's after the BEGIN{} as a brand new parse.
#print "outer\n"
EXPECT
inner
########
sub pipe_to_fork ($$) {
    my $parent = shift;
    my $child = shift;
    pipe($child, $parent) or die;
    my $pid = fork();
    die "fork() failed: $!" unless defined $pid;
    close($pid ? $child : $parent);
    $pid;
}

my ($parent, $child);

open $parent, "<", '';
open $child, "<", '';
if (pipe_to_fork($parent, $child)) {
    # parent
    print $parent "pipe_to_fork\n";
    close $parent;
}
else {
    # child
    while (~< $child) { print; }
    close $child;
    exit;
}

sub pipe_from_fork ($$) {
    my $parent = shift;
    my $child = shift;
    pipe($parent, $child) or die;
    my $pid = fork();
    die "fork() failed: $!" unless defined $pid;
    close($pid ? $child : $parent);
    $pid;
}

if (pipe_from_fork($parent, $child)) {
    # parent
    while (~< $parent) { print; }
    close $parent;
}
else {
    # child
    print $child "pipe_from_fork\n";
    close $child;
    exit;
}
EXPECT
pipe_from_fork
pipe_to_fork
########
$|=1;
if (my $pid = fork()) {
    print "forked first kid\n";
    print "waitpid() returned ok\n" if waitpid($pid,0) == $pid;
}
else {
    print "first child\n";
    exit(0);
}
if (my $pid = fork()) {
    print "forked second kid\n";
    print "wait() returned ok\n" if wait() == $pid;
}
else {
    print "second child\n";
    exit(0);
}
EXPECT
forked first kid
first child
waitpid() returned ok
forked second kid
second child
wait() returned ok
########
pipe(RDR,WTR) or die $!;
my $pid = fork;
die "fork: $!" if !defined $pid;
if ($pid == 0) {
    my $rand_child = rand;
    close RDR;
    print WTR $rand_child, "\n";
    close WTR;
} else {
    my $rand_parent = rand;
    close WTR;
    chomp(my $rand_child  = ~< *RDR);
    close RDR;
    print $rand_child ne $rand_parent, "\n";
}
EXPECT
1
########
# [perl #39145] Perl_dounwind() crashing with Win32's fork() emulation
sub { @_ = @(3); fork ? die "1" : die "1" }->(2);
EXPECT
1 at - line 2.
    main::__ANON__ called at - line 2.
1 at - line 2.
    main::__ANON__ called at - line 2.
