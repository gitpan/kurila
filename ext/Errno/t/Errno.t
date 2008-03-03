#!./perl -w

BEGIN {
    unless(grep m/blib/, @INC) {
	chdir 't' if -d 't';
	if ($^O eq 'MacOS') { 
	    @INC = qw(: ::lib ::macos:lib); 
	} else { 
	    @INC = '../lib'; 
	}
    }
}

use Test::More tests => 10;

BEGIN {
    use_ok("Errno");
}

BAIL_OUT("No errno's are exported") unless @Errno::EXPORT_OK;

my $err = $Errno::EXPORT_OK[0];
my $num = &{*{Symbol::fetch_glob("Errno::$err")}};

is($num, &{*{Symbol::fetch_glob("Errno::$err")}});

$! = $num;
ok(exists $!{$err});

$! = 0;
ok(! $!{$err});

ok(join(",",sort keys(%!)) eq join(",",sort @Errno::EXPORT_OK));

eval { exists $!{[]} };
ok(! $@);

eval {$!{$err} = "qunckkk" };
like($@->{description}, qr/^ERRNO hash is read only!/);

eval {delete $!{$err}};
like($@->{description}, qr/^ERRNO hash is read only!/);

# The following tests are in trouble if some OS picks errno values
# through Acme::MetaSyntactic::batman
is($!{EFLRBBB}, "");
ok(! exists($!{EFLRBBB}));
