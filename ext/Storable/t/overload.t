#!./perl
#
#  Copyright (c) 1995-2000, Raphael Manfredi
#  
#  You may redistribute only under the same terms as Perl 5, as specified
#  in the README file that comes with the distribution.
#  

sub BEGIN {
    if (%ENV{PERL_CORE}){
	chdir('t') if -d 't';
	@INC = ('.', '../lib', '../ext/Storable/t');
    } else {
	unshift @INC, 't';
    }
    require Config; Config->import;
    if (%ENV{PERL_CORE} and %Config{'extensions'} !~ m/\bStorable\b/) {
        print "1..0 # Skip: Storable was not built\n";
        exit 0;
    }
    require 'st-dump.pl';
}

sub ok;

use Storable qw(freeze thaw);

print "1..16\n";

package OVERLOADED;

use overload
	'""' => sub { @_[0][0] };

package main;

$a = bless [77], 'OVERLOADED';

$b = thaw freeze $a;
ok 1, ref $b eq 'OVERLOADED';
ok 2, "$b" eq "77";

$c = thaw freeze \$a;
ok 3, ref $c eq 'REF';
ok 4, ref $$c eq 'OVERLOADED';
ok 5, "$$c" eq "77";

$d = thaw freeze [$a, $a];
ok 6, "$d->[0]" eq "77";
$d->[0][0]++;
ok 7, "$d->[1]" eq "78";

package REF_TO_OVER;

sub make {
	my $self = bless {}, shift;
	my ($over) = @_;
	$self->{over} = $over;
	return $self;
}

package OVER;

use overload
	'+'		=> \&plus,
	'""'	=> sub { ref @_[0] };

sub plus {
	return 314;
}

sub make {
	my $self = bless {}, shift;
	my $ref = REF_TO_OVER->make($self);
	$self->{ref} = $ref;
	return $self;
}

package main;

$a = OVER->make();
$b = thaw freeze $a;

ok 8, ref $b eq 'OVER';
ok 9, $a + $a == 314;
ok 10, ref $b->{ref} eq 'REF_TO_OVER';
ok 11, "$b->{ref}->{over}" eq "$b";
ok 12, $b + $b == 314;

# nfreeze data generated by make_overload.pl
my $f = '';
if (ord ('A') == 193) { # EBCDIC.
    $f = unpack 'u', q{7!084$0S(P>)MUN7%V=/6P<0*!**5EJ8`};
}else {
    $f = unpack 'u', q{7!084$0Q(05-?3U9%4DQ/040*!'-N;W<`};
}

# see note at the end of do_retrieve in Storable.xs about why this test has to
# use a reference to an overloaded reference, rather than just a reference.
my $t = eval {thaw $f};
print "# $@" if $@;
ok 13, $@ eq "";
ok 14, ref ($t) eq 'REF';
ok 15, ref ($$t) eq 'HAS_OVERLOAD';
ok 16, $$$t eq 'snow';
1;
