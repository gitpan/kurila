#!perl -w

BEGIN {
    require Config; Config->import;
    if (%Config{'extensions'} !~ m/\bOpcode\b/
	&& %Config{'extensions'} !~ m/\bPOSIX\b/
	&& %Config{'osname'} ne 'VMS')
    {
	print "1..0\n";
	exit 0;
    }
}

use strict;
use warnings;
use POSIX qw(ceil);
use Test::More tests => 2;
use Safe;

my $safe = Safe->new();
$safe->deny('add');

my $masksize = ceil( Opcode::opcodes / 8 );
# Attempt to change the opmask from within the safe compartment
$safe->reval( qq{\@_[1] = qq/\0/ x } . $masksize );

# Check that it didn't work
$safe->reval( q{$x + $y} );
like( $@->{description}, qr/^'?addition \(\+\)'? trapped by operation mask/,
	    'opmask still in place with reval' );

my $safe2 = Safe->new();
$safe2->deny('add');

open my $fh, ">", 'nasty.pl' or die "Can't write nasty.pl: $!\n";
print $fh <<EOF;
\@_[1] = "\0" x $masksize;
EOF
close $fh;
$safe2->rdo('nasty.pl');
$safe2->reval( q{$x + $y} );
like( $@->{description}, qr/^'?addition \(\+\)'? trapped by operation mask/,
	    'opmask still in place with rdo' );
END { unlink 'nasty.pl' }
