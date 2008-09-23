#!/usr/bin/perl -w
use strict;

require 'regen_lib.pl';

my $kw = safer_open("keywords.h-new");
select $kw;

print <<EOM;
/* -*- buffer-read-only: t -*-
 *
 *    keywords.h
 *
 *    Copyright (C) 1994, 1995, 1996, 1997, 1999, 2000, 2001, 2002, 2005,
 *    2006, 2007, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * !!!!!!!   DO NOT EDIT THIS FILE   !!!!!!!
 *  This file is built by keywords.pl from its data.  Any changes made here
 *  will be lost!
 */
EOM

# Read & print data.

my $keynum = 0;
while ( ~< *DATA) {
    chop;
    next unless $_;
    next if m/^#/;
    my ($keyword) = < split;
    print &tab(5, "#define KEY_$keyword"), $keynum++, "\n";
}

print $kw "\n/* ex: set ro: */\n";

safer_close($kw);

rename_if_different("keywords.h-new", "keywords.h");

###########################################################################
sub tab {
    my ($l, $t) = < @_;
    $t .= "\t" x ($l - (length($t) + 1) / 8);
    $t;
}
###########################################################################
__END__

NULL
__FILE__
__LINE__
__PACKAGE__
__DATA__
__END__
BEGIN
UNITCHECK
CORE
DESTROY
END
INIT
CHECK
abs
accept
alarm
and
atan2
bind
binmode
bless
caller
chdir
chmod
chomp
chop
chown
chr
chroot
close
closedir
cmp
connect
continue
cos
crypt
defined
delete
die
do
dump
each
else
elsif
endgrent
endhostent
endnetent
endprotoent
endpwent
endservent
eof
eq
eval
exec
exists
exit
exp
fcntl
fileno
flock
for
foreach
fork
getc
getgrent
getgrgid
getgrnam
gethostbyaddr
gethostbyname
gethostent
getlogin
getnetbyaddr
getnetbyname
getnetent
getpeername
getpgrp
getppid
getpriority
getprotobyname
getprotobynumber
getprotoent
getpwent
getpwnam
getpwuid
getservbyname
getservbyport
getservent
getsockname
getsockopt
glob
gmtime
goto
grep
hex
if
index
int
ioctl
join
keys
kill
last
lc
lcfirst
length
link
listen
local
localtime
lock
log
lstat
nelems
nkeys
m
map
mkdir
msgctl
msgget
msgrcv
msgsnd
my
ne
next
no
not
oct
open
opendir
or
ord
our
pack
package
pipe
pop
pos
print
printf
prototype
push
q
qq
qr
quotemeta
qw
qx
rand
read
readdir
readline
readlink
readpipe
recv
redo
ref
rename
require
return
reverse
rewinddir
rindex
rmdir
s
scalar
seek
seekdir
select
semctl
semget
semop
send
setgrent
sethostent
setnetent
setpgrp
setpriority
setprotoent
setpwent
setservent
setsockopt
shift
shmctl
shmget
shmread
shmwrite
shutdown
sin
sleep
socket
socketpair
sort
splice
split
sprintf
sqrt
srand
stat
state
study
sub
substr
symlink
syscall
sysopen
sysread
sysseek
system
syswrite
tell
telldir
tie
tied
time
times
tr
truncate
try
uc
ucfirst
umask
undef
unless
unlink
unpack
unshift
untie
until
use
utime
values
vec
wait
waitpid
wantarray
warn
while
write
x
xor
y
