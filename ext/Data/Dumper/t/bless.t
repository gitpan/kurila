#!perl

use Test::More v0.60;

# Test::More 0.60 required because:
# - is_deeply(undef, $not_undef); now works. [rt.cpan.org 9441]

BEGIN { plan tests => 1+5*2; }

BEGIN { use_ok('Data::Dumper') };

# RT 39420: Data::Dumper fails to escape bless class name

# test under XS and pure Perl version
foreach $Data::Dumper::Useperl (0, 1) {

#diag("\$Data::Dumper::Useperl = $Data::Dumper::Useperl");

{
my $t = bless( \%(), q{a'b} );
my $dt = Dumper($t);
my $o = <<'PERL';
$VAR1 = bless( \%(), "a'b" );
PERL

is($dt, $o, "package name in bless is escaped if needed (useperl=$Data::Dumper::Useperl)");
is_deeply(scalar eval($dt), $t, "eval reverts dump");
}

{
my $t = bless( \%(), q{a\} );
my $dt = Dumper($t);
my $o = <<'PERL';
$VAR1 = bless( \%(), "a\\" );
PERL

is($dt, $o, "package name in bless is escaped if needed");
is_deeply(scalar eval($dt), $t, "eval reverts dump");
}
SKIP: {
    skip(q/no 're::regexp_pattern'/, 1)
        if ! defined(*re::regexp_pattern{CODE});

my $t = bless( qr//, 'foo');
my $dt = Dumper($t);
my $o = <<'PERL';
$VAR1 = bless( qr/(?-uxism:)/, "foo" );
PERL

is($dt, $o, "We can dump blessed qr//'s properly");

}
}
