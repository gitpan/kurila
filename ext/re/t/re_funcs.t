#!./perl

use warnings;

use Test::More; # test count at bottom of file
use re < qw(regmust);
do {
    my $qr=qr/here .* there/x;
    my @($anchored,$floating)=  regmust($qr);
    is($anchored,'here',"Regmust anchored - qr//");
    is($floating,'there',"Regmust floating - qr//");
    my $foo='blah';
    @(?$anchored,?$floating)= regmust($foo) || @();
    is($anchored,undef,"Regmust anchored - non ref");
    is($floating,undef,"Regmust anchored - non ref");
    my $bar=\@('blah');
    @(?$anchored,?$floating)= regmust($foo) || @();
    is($anchored,undef,"Regmust anchored - ref");
    is($floating,undef,"Regmust anchored - ref");
};
# New tests above this line, don't forget to update the test count below!
use Test::More tests => 6;
# No tests here!
