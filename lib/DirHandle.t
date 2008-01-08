#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    our %Config;
    require Config; Config->import;
    if (not $Config{'d_readdir'}) {
	print "1..0\n";
	exit 0;
    }
}

use DirHandle;
require './test.pl';

plan(5);

$dot = DirHandle->new($^O eq 'MacOS' ? ':' : '.');

ok(defined($dot));

@a = sort glob("*");
do { $first = $dot->read } while defined($first) && $first =~ m/^\./;
ok(+(grep { $_ eq $first } @a));

@b = sort($first, (grep {m/^[^.]/} $dot->read));
ok(+(join("\0", @a) eq join("\0", @b)));

$dot->rewind;
@c = sort grep {m/^[^.]/} $dot->read;
cmp_ok(+(join("\0", @b), 'eq', join("\0", @c)));

$dot->close;
$dot->rewind;
ok(!defined($dot->read));
