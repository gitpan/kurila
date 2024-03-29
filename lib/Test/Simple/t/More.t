#!perl -w

BEGIN {
    if( env::var('PERL_CORE') ) {
        chdir 't';
        $^INCLUDE_PATH = qw(../lib lib);
    }
}

use lib 't/lib';
use Test::More tests => 49;

# Make sure we don't mess with $@ or $!.  Test at bottom.
my $Err   = "this should not be touched";
my $Errno = 42;
$^EVAL_ERROR = $Err;
$^OS_ERROR = $Errno;

use_ok('Dummy');
is( $Dummy::VERSION, '5.562', 'use_ok() loads a module' );
require_ok('Test::More');


ok( 2 eq 2,             'two is two is two is two' );
is(   "foo", "foo",       'foo is foo' );
isnt( "foo", "bar",     'foo isnt bar');

#'#
like("fooble", '/^foo/',    'foo is like fooble');
like("FooBle", '/foo/i',   'foo is like FooBle');
like("/usr/local/pr0n/", '/^\/usr\/local/',   'regexes with slashes in like' );

unlike("fbar", '/^bar/',    'unlike bar');
unlike("FooBle", '/foo/',   'foo is unlike FooBle');
unlike("/var/local/pr0n/", '/^\/usr\/local/','regexes with slashes in unlike' );

my @foo = qw(foo bar baz);
unlike((nelems @foo), '/foo/');

can_ok('Test::More', < qw(require_ok use_ok ok is isnt like skip can_ok
                        pass fail eq_array eq_hash eq_set));
can_ok(bless(\%(), "Test::More"), < qw(require_ok use_ok ok is isnt like skip 
                                   can_ok pass fail eq_array eq_hash eq_set));


isa_ok(bless(\@(), "Foo"), "Foo");
isa_ok(\@(), 'ARRAY');
isa_ok(\42, 'SCALAR');


# can_ok() & isa_ok should call can() & isa() on the given object, not 
# just class, in case of custom can()
do {
       *Foo::can = sub { @_[0]->[0] };
       *Foo::isa = sub { @_[0]->[0] };
       my $foo = bless(\@(0), 'Foo');
       ok( ! $foo->can('bar') );
       ok( ! $foo->isa('bar') );
       $foo->[0] = 1;
       can_ok( $foo, 'blah');
       isa_ok( $foo, 'blah');
};


pass('pass() passed');

ok( eq_array(\qw(this that whatever), \qw(this that whatever)),
    'eq_array with simple arrays' );
is( (nelems @Test::More::Data_Stack), 0, '@Data_Stack not holding onto things');

ok( eq_hash(\%( foo => 42, bar => 23 ), \%(bar => 23, foo => 42)),
    'eq_hash with simple hashes' );
is( (nelems @Test::More::Data_Stack), 0);

ok( eq_set(\qw(this that whatever), \qw(that whatever this)),
    'eq_set with simple sets' );
is( (nelems @Test::More::Data_Stack), 0);

my @complex_array1 = @(
                      qw(this that whatever),
                      %(foo => 23, bar => 42),
                      "moo",
                      "yarrow",
                      qw(498 10 29),
                     );
my @complex_array2 = @(
                      qw(this that whatever),
                      %(foo => 23, bar => 42),
                      "moo",
                      "yarrow",
                      qw(498 10 29),
                     );

is_deeply( \@complex_array1, \@complex_array2,    'is_deeply with arrays' );
ok( eq_array(\@complex_array1, \@complex_array2),
    'eq_array with complicated arrays' );
ok( eq_set(\@complex_array1, \@complex_array2),
    'eq_set with complicated arrays' );

my @array1 = @( <qw(this that whatever),
              \%(foo => 23, bar => 42) );
my @array2 = @( <qw(this that whatever),
              \%(foo => 24, bar => 42) );

ok( !eq_array(\@array1, \@array2),
    'eq_array with slightly different complicated arrays' );
is( (nelems @Test::More::Data_Stack), 0);

ok( !eq_set(\@array1, \@array2),
    'eq_set with slightly different complicated arrays' );
is( (nelems @Test::More::Data_Stack), 0);

my %hash1 = %( foo => 23,
              bar => \qw(this that whatever),
              har => \%( foo => 24, bar => 42 ),
            );
my %hash2 = %( foo => 23,
              bar => \qw(this that whatever),
              har => \%( foo => 24, bar => 42 ),
            );

is_deeply( \%hash1, \%hash2,    'is_deeply with complicated hashes' );
ok( eq_hash(\%hash1, \%hash2),  'eq_hash with complicated hashes');

%hash1 = %( foo => 23,
           bar => \qw(this that whatever),
           har => \%( foo => 24, bar => 42 ),
         );
%hash2 = %( foo => 23,
           bar => \qw(this tha whatever),
           har => \%( foo => 24, bar => 42 ),
         );

ok( !eq_hash(\%hash1, \%hash2),
    'eq_hash with slightly different complicated hashes' );
is( (nelems @Test::More::Data_Stack), 0);

cmp_ok( Test::Builder->new, '\==', Test::More->builder,    'builder()' );


cmp_ok(42, '==', 42,        'cmp_ok ==');
cmp_ok('foo', 'eq', 'foo',  '       eq');
cmp_ok(42.5, '+<', 42.6,     '       +<');
cmp_ok(0, '||', 1,          '       ||');


# Piers pointed out sometimes people override isa().
do {
    package Wibble;
    sub isa($self, $class) {
        return 1 if $class eq 'Wibblemeister';
    }
    sub new { bless \%() }
};
isa_ok( Wibble->new, 'Wibblemeister' );

my $sub = sub {};
is_deeply( $sub, $sub, 'the same function ref' );

use Symbol;
my $glob = gensym;
is_deeply( $glob, $glob, 'the same glob' );

is_deeply( \%( foo => $sub, bar => \@(1, $glob) ),
           \%( foo => $sub, bar => \@(1, $glob) )
         );
