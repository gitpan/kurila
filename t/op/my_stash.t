#!./perl

package Foo;

use Test;

plan tests => 7;

use constant MyClass => 'Foo::Bar::Biz::Baz';

{
    package Foo::Bar::Biz::Baz;
    1;
}

for (qw(Foo Foo:: MyClass __PACKAGE__)) {
    eval "sub \{ my $_ \$obj = shift; \}";
    ok $@->{description} =~ m/Expected variable after declarator/;
}

use constant NoClass => 'Nope::Foo::Bar::Biz::Baz';

for (qw(Nope Nope:: NoClass)) {
    eval "sub \{ my $_ \$obj = shift; \}";
    ok $@;
#    print $@ if $@;
}
