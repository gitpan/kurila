
#!./perl -w
#
#  Copyright (c) 1995-2000, Raphael Manfredi
#  
#  You may redistribute only under the same terms as Perl 5, as specified
#  in the README file that comes with the distribution.
#

sub BEGIN {
    if ($ENV{PERL_CORE}){
	chdir('t') if -d 't';
	@INC = ('.', '../lib', '../ext/Storable/t');
    } else {
	unshift @INC, 't';
    }
    require Config; Config->import;
    if ($ENV{PERL_CORE} and $Config{'extensions'} !~ m/\bStorable\b/) {
        print "1..0 # Skip: Storable was not built\n";
        exit 0;
    }
    require 'st-dump.pl';
}

use strict;
sub ok;

use utf8;

use Storable qw(thaw freeze);

print "1..5\n";

my $x = chr(1234);
ok 1, $x eq ${thaw freeze \$x};

# Long scalar
$x = join '', map {chr $_} (0..1023);
ok 2, $x eq ${thaw freeze \$x};

# Char in the range 127-255 (probably) in utf8
$x = chr (175) . chr (256);
chop $x;
ok 3, $x eq ${thaw freeze \$x};

# Storable needs to cope if a frozen string happens to be internall utf8
# encoded

$x = chr 256;
my $data = freeze \$x;
ok 4, $x eq ${thaw $data};

$data .= chr 256;
chop $data;
ok 5, $x eq ${thaw $data};

