#!./perl

use Config;

BEGIN {
    unless (-d 'blib') {
	if (%Config{extensions} !~ m/\bList\/Util\b/) {
	    print "1..0 # Skip: List::Util was not built\n";
	    exit 0;
	}
    }
}

use Scalar::Util ();
use Test::More  (grep { m/set_prototype/ } < @Scalar::Util::EXPORT_FAIL)
			? (skip_all => 'set_prototype requires XS version')
			: (tests => 13);

Scalar::Util->import('set_prototype');

sub f { }
is( prototype('f'),	undef,	'no prototype');

my $r = set_prototype(\&f,'$');
is( prototype('f'),	'$',	'set prototype');
is( $r,			\&f,	'return value');

set_prototype(\&f,undef);
is( prototype('f'),	undef,	'remove prototype');

set_prototype(\&f,'');
is( prototype('f'),	'',	'empty prototype');

sub g (@) { }
is( prototype('g'),	'@',	'@ prototype');

set_prototype(\&g,undef);
is( prototype('g'),	undef,	'remove prototype');

sub stub;
is( prototype('stub'),	undef,	'non existing sub');

set_prototype(\&stub,'$$$');
is( prototype('stub'),	'$$$',	'change non existing sub');

sub f_decl ($$$$);
is( prototype('f_decl'),	'$$$$',	'forward declaration');

set_prototype(\&f_decl,'\%');
is( prototype('f_decl'),	'\%',	'change forward declaration');

try { &set_prototype( 'f', '' ); };
print "not " unless 
ok($@->{description} =~ m/^set_prototype: not a reference/,	'not a reference');

try { &set_prototype( \'f', '' ); };
ok($@->{description} =~ m/^set_prototype: not a subroutine reference/,	'not a sub reference');