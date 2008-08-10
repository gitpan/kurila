#!./perl

BEGIN {
    require './test.pl';
}

plan(tests => 5);

open(TRY, ">",'Comp.try') || (die "Can't open temp file.");

my $x = 'now is the time
for all good men
to come to.


!

';

my $y = 'now is the time' . "\n" .
'for all good men' . "\n" .
'to come to.' . "\n\n\n!\n\n";

is($x, $y,  'test data is sane');

print TRY $x;
close TRY or die "Could not close: $!";

open(TRY, "<",'Comp.try') || (die "Can't reopen temp file.");
my $count = 0;
my $z = '';
while ( ~< *TRY) {
    $z .= $_;
    $count = $count + 1;
}

is($z, $y,  'basic multiline reading');

is($count, 7,   '    line count');

my $out = (($^O eq 'MSWin32') || $^O eq 'NetWare' || $^O eq 'VMS') ? `type Comp.try`
    : ($^O eq 'MacOS') ? `catenate Comp.try`
    : `cat Comp.try`;

like($out, qr/.*\n.*\n.*\n$/);

close(TRY) || (die "Can't close temp file.");
unlink 'Comp.try' || `/bin/rm -f Comp.try`;

is($out, $y);
