#!./perl

BEGIN { require "./test.pl" }

our ($xref, $runme, @a, %h, $aref, $chopit, @chopar, $posstr,
     $cstr, $nn, $n, @INPUT, @simple_input, $ord, $href, $zzz1,
    $zzz2, $op, $commentt, $expectop, $skip, $integer,
    $comment, $operator, $variable);

our ($undefed, @z, @x, @aaa, $toself, $direct);

$| = 1;
umask 0;
$xref = \ "";
$runme = $^X;
@a = (1..5);
%h = (1..6);
$aref = \@a;
$href = \%h;
open OP, '-|', qq{$runme -le "print 'aaa Ok ok' for 1..100"};
$chopit = 'aaaaaa';
@chopar = (113 .. 119);
$posstr = '123456';
$cstr = 'aBcD.eF';
pos $posstr = 3;
$nn = $n = 2;
sub subb {"in s"}

@INPUT = ~< *DATA;
@simple_input = grep m/^\s*\w+\s*\$\w+\s*[#\n]/, @INPUT;

plan 11 + @INPUT + @simple_input;

sub wrn {"@_"}

# Check correct optimization of ucfirst etc
my $a = "AB";
my $b = ucfirst(lc("$a"));
ok $b eq 'Ab';

# Check correct destruction of objects:
my $dc = 0;
sub A::DESTROY {$dc += 1}
$a=8;
my $b;
{ my $c = 6; $b = bless \$c, "A"}

ok $dc == 0;

$b = $a+5;

ok $dc == 1;

my $xxx = 'b';
$xxx = 'c' . ($xxx || 'e');
ok $xxx eq 'cb';

{				# Check calling STORE
  my $sc = 0;
  sub B::TIESCALAR {bless \@(11), 'B'}
  sub B::FETCH { -(shift->[0]) }
  sub B::STORE { $sc++; my $o = shift; $o->[0] = 17 + shift }

  my $m;
  tie $m, 'B';
  $m = 100;

  ok $sc == 1;

  my $t = 11;
  $m = $t + 89;
  
  ok $sc == 2;
  ok $m == -117;

  $m += $t;

  ok $sc == 3;
  ok $m == 89;

}

# Chains of assignments

my ($l1, $l2, $l3, $l4);
my $zzzz = 12;
$zzz1 = $l1 = $l2 = $zzz2 = $l3 = $l4 = 1 + $zzzz;

ok( $zzz1 == 13 and $zzz2 == 13 and $l1 == 13,
    "$zzz1 = $l1 = $l2 = $zzz2 = $l3 = $l4 = 13" );

for (@INPUT) {
 SKIP: {
    ($op, undef, $comment) = m/^([^\#]+)(\#\s+(.*))?/;
    $comment = $op unless defined $comment;
    chomp;
    $op = "$op==$op" unless $op =~ m/==/;
    ($op, $expectop) = $op =~ m/(.*)==(.*)/;
  
    if ($op =~ m/^'\?\?\?'/ or $comment =~ m/skip\(.*\Q$^O\E.*\)/i) {
      skip("$comment", 1);
    }
    $integer = ($comment =~ m/^i_/) ? "use integer" : '' ;
  
    eval <<EOE . <<'EOE';
    local \$^WARN_HOOK = \\&wrn;
    my \$a = 'fake';
    $integer;
    \$a = $op;
    \$b = $expectop;
EOE
    is($a, $b, $comment);
EOE
    if ($@) {
      if ($@->{description} =~ m/is unimplemented/) {
        skip("$comment: unimplemented", 1);
      } else {
        fail("error: {$@->message}");
      }
    }
  }
}

for (@simple_input) {
 SKIP:
 {
  ($op, undef, $comment) = m/^([^\#]+)(\#\s+(.*))?/;
  $comment = $op unless defined $comment;
  chomp;
  ($operator, $variable) = m/^\s*(\w+)\s*\$(\w+)/ or warn "misprocessed '$_'\n";
  eval <<EOE;
  local \$^WARN_HOOK = \\&wrn;
  my \$$variable = "Ac# Ca\\nxxx";
  \$$variable = $operator \$$variable;
  \$toself = \$$variable;
  \$direct = $operator "Ac# Ca\\nxxx";
  ok( \$toself eq \$direct,
     "\\\$$variable = $operator \\\$$variable");
EOE
  if ($@) {
    if ($@->{description} =~ m/is unimplemented/) {
      skip("skipping $comment: unimplemented", 1);
    } elsif ($@->{description} =~ m/Can't (modify|take log of 0)/) {
      skip("skipping $comment: syntax not good for selfassign", 1);
    } else {
      fail("error: {$@->message}");
    }
  }
 }
}

eval {
    sub PVBM () { 'foo' }
    index 'foo', PVBM;
    my $x = PVBM;

    my $str = 'foo';
    my $pvlv = \substr $str, 0, 1;
    $x = $pvlv;

    1;
};
die if $@;
ok 1;

__END__
ref $xref			# ref
ref $cstr			# ref nonref
`$runme -e "print qq[1\\n]"`				# backtick skip(MSWin32)
`$undefed`			# backtick undef skip(MSWin32)
~< *OP				# readline
'faked'				# rcatline
(@z = (1 .. 3))			# aassign
chop $chopit			# chop
(chop (@x=@chopar))		# schop
chomp $chopit			# chomp
(chop (@x=@chopar))		# schomp
pos $posstr			# pos
pos $chopit			# pos returns undef
$nn++==2			# postinc
$nn++==3			# i_postinc
$nn--==4			# postdec
$nn--==3			# i_postdec
$n ** $n			# pow
$n * $n				# multiply
$n * $n				# i_multiply
$n / $n				# divide
$n / $n				# i_divide
$n % $n				# modulo
$n % $n				# i_modulo
$n x $n				# repeat
$n + $n				# add
$n + $n				# i_add
$n - $n				# subtract
$n - $n				# i_subtract
$n . $n				# concat
$n . $a=='2fake'		# concat with self
"3$a"=='3fake'			# concat with self in stringify
"$n"				# stringify
$n << $n			# left_shift
$n >> $n			# right_shift
$n <+> $n			# ncmp
$n <+> $n			# i_ncmp
$n cmp $n			# scmp
$n ^&^ $n				# bit_and
$n ^^^ $n				# bit_xor
$n ^|^ $n				# bit_or
-$n				# negate
-$n				# i_negate
^~^$n				# complement
atan2 $n,$n			# atan2
sin $n				# sin
cos $n				# cos
'???'				# rand
exp $n				# exp
log $n				# log
sqrt $n				# sqrt
int $n				# int
hex $n				# hex
oct $n				# oct
abs $n				# abs
length $posstr			# length
substr $posstr, 2, 2		# substr
vec("abc",2,8)			# vec
index $posstr, 2		# index
rindex $posstr, 2		# rindex
sprintf "%i%i", $n, $n		# sprintf
ord $n				# ord
chr $n				# chr
crypt $n, $n			# crypt
ucfirst ($cstr . "a")		# ucfirst padtmp
ucfirst $cstr			# ucfirst
lcfirst $cstr			# lcfirst
uc $cstr			# uc
lc $cstr			# lc
quotemeta $cstr			# quotemeta
@$aref				# rv2av
(each %h) % 2 == 1		# each
values %h			# values
keys %h				# keys
%$href				# rv2hv
pack "C2", $n,$n		# pack
split m/a/, "abad"		# split
join "a"; @a			# join
push @a,3==6			# push
unshift @aaa			# unshift
reverse	@a			# reverse
reverse	$cstr			# reverse - scal
grep $_, 1,0,2,0,3		# grepwhile
map "x$_", 1,0,2,0,3		# mapwhile
subb()				# entersub
caller				# caller
'???'                           # warn
'faked'				# die
open BLAH, "<", "non-existent"	# open
fileno STDERR			# fileno
umask 0				# umask
select STDOUT			# sselect
select undef,undef,undef,0	# select
getc OP				# getc
'???'				# read
'???'				# sysread
'???'				# syswrite
'???'				# send
'???'				# recv
'???'				# tell
'???'				# fcntl
'???'				# ioctl
'???'				# flock
'???'				# accept
'???'				# shutdown
'???'				# ftsize
'???'				# ftmtime
'???'				# ftatime
'???'				# ftctime
chdir 'non-existent'		# chdir
'???'				# chown
'???'				# chroot
unlink 'non-existent'		# unlink
chmod 'non-existent'		# chmod
utime 'non-existent'		# utime
rename 'non-existent', 'non-existent1'	# rename
link 'non-existent', 'non-existent1' # link
'???'				# symlink
readlink 'non-existent', 'non-existent1' # readlink
'???'				# mkdir
'???'				# rmdir
'???'				# telldir
'???'				# fork
'???'				# wait
'???'				# waitpid
system "$runme -e 0"		# system skip(VMS)
'???'				# exec
'???'				# kill
getppid				# getppid
getpgrp				# getpgrp
'???'				# setpgrp
getpriority $$, $$		# getpriority
'???'				# setpriority
time				# time
localtime $^T			# localtime
gmtime $^T			# gmtime
'???'				# sleep: can randomly fail
'???'				# alarm
'???'				# shmget
'???'				# shmctl
'???'				# shmread
'???'				# shmwrite
'???'				# msgget
'???'				# msgctl
'???'				# msgsnd
'???'				# msgrcv
'???'				# semget
'???'				# semctl
'???'				# semop
'???'				# getlogin
'???'				# syscall
