#!/usr/bin/perl -w

# Wherein we ensure the INST_* and INSTALL* variables are set correctly
# in a default Makefile.PL run
#
# Essentially, this test is a Makefile.PL.

BEGIN {
    if( env::var('PERL_CORE') ) {
        chdir 't' if -d 't';
        $^INCLUDE_PATH = @('../lib', 'lib');
    }
    else {
        unshift $^INCLUDE_PATH, 't/lib';
    }
}

use Test::More tests => 26;
use MakeMaker::Test::Utils;
use MakeMaker::Test::Setup::BFD;
use ExtUtils::MakeMaker;
use File::Spec;
use Config;

chdir 't';

perl_lib;

$^OUTPUT_AUTOFLUSH = 1;

my $Makefile = makefile_name;
my $Curdir = File::Spec->curdir;
my $Updir  = File::Spec->updir;

ok( setup_recurs(), 'setup' );
END {
    ok( chdir File::Spec->updir );
    ok( teardown_recurs(), 'teardown' );
}

ok( chdir 'Big-Dummy', "chdir'd to Big-Dummy" ) ||
  diag("chdir failed: $^OS_ERROR");

my $stdout = '';
close $^STDOUT;
open $^STDOUT, '>>', \$stdout or die;
my $mm = WriteMakefile(
    NAME          => 'Big::Dummy',
    VERSION_FROM  => 'lib/Big/Dummy.pm',
    PREREQ_PM     => \%(),
    PERL_CORE     => env::var('PERL_CORE'),
);
like( $stdout, qr{
                        Writing\ $Makefile\ for\ Big::Liar\n
                        Big::Liar's\ vars\n
                        INST_LIB\ =\ \S+\n
                        INST_ARCHLIB\ =\ \S+\n
                        Writing\ $Makefile\ for\ Big::Dummy\n
}x );
$stdout = '';

isa_ok( $mm, 'ExtUtils::MakeMaker' );

is( $mm->{NAME}, 'Big::Dummy',  'NAME' );
is( $mm->{VERSION}, 0.01,            'VERSION' );

my $config_prefix = config_value("installprefixexp") || config_value("installprefix") ||
                    config_value("prefixexp")        || config_value("prefix");
is( $mm->{PERLPREFIX}, $config_prefix,   'PERLPREFIX' );

is( $mm->{PERL_CORE}, env::var('PERL_CORE'), 'PERL_CORE' );

my($perl_src, $mm_perl_src);
if( env::var('PERL_CORE') ) {
    $perl_src = File::Spec->catdir($Updir, $Updir);
    $perl_src = File::Spec->canonpath($perl_src);
    $mm_perl_src = File::Spec->canonpath($mm->{PERL_SRC});
}
else {
    $mm_perl_src = $mm->{PERL_SRC};
}

is( $mm_perl_src, $perl_src,     'PERL_SRC' );


# PERM_*
is( $mm->{PERM_RW},  644,    'PERM_RW' );
is( $mm->{PERM_RWX}, 755,    'PERM_RWX' );


# INST_*
is( $mm->{INST_ARCHLIB}, 
    $mm->{PERL_CORE} ?? $mm->{PERL_ARCHLIB}
                     !! File::Spec->catdir($Curdir, 'blib', 'arch'),
                                     'INST_ARCHLIB');
is( $mm->{INST_BIN},     File::Spec->catdir($Curdir, 'blib', 'bin'),
                                     'INST_BIN' );

is( nkeys %{$mm->{CHILDREN}}, 1 );
my@($child_pack) =  keys %{$mm->{CHILDREN}};
my $c_mm = $mm->{CHILDREN}->{$child_pack};
is( $c_mm->{INST_ARCHLIB}, 
    $c_mm->{PERL_CORE} ?? $c_mm->{PERL_ARCHLIB}
                       !! File::Spec->catdir($Updir, 'blib', 'arch'),
                                     'CHILD INST_ARCHLIB');
is( $c_mm->{INST_BIN},     File::Spec->catdir($Updir, 'blib', 'bin'),
                                     'CHILD INST_BIN' );


my $inst_lib = File::Spec->catdir($Curdir, 'blib', 'lib');
is( $mm->{INST_LIB}, 
    $mm->{PERL_CORE} ?? $mm->{PERL_LIB} !! $inst_lib,     'INST_LIB' );


# INSTALL*
is( $mm->{INSTALLDIRS}, 'site',     'INSTALLDIRS' );



# Make sure the INSTALL*MAN*DIR variables work.  We forgot them
# at one point.
$mm = WriteMakefile(
    NAME          => 'Big::Dummy',
    VERSION_FROM  => 'lib/Big/Dummy.pm',
    PERL_CORE     => env::var('PERL_CORE'),
    INSTALLMAN1DIR       => 'none',
    INSTALLSITEMAN3DIR   => 'none',
    INSTALLVENDORMAN1DIR => 'none',
    INST_MAN1DIR         => 'none',
);
like( $stdout, qr{
                        Writing\ $Makefile\ for\ Big::Liar\n
                        Big::Liar's\ vars\n
                        INST_LIB\ =\ \S+\n
                        INST_ARCHLIB\ =\ \S+\n
                        Writing\ $Makefile\ for\ Big::Dummy\n
}x );
$stdout = '';

isa_ok( $mm, 'ExtUtils::MakeMaker' );

is  ( $mm->{INSTALLMAN1DIR},        'none' );
is  ( $mm->{INSTALLSITEMAN3DIR},    'none' );
is  ( $mm->{INSTALLVENDORMAN1DIR},  'none' );
is  ( $mm->{INST_MAN1DIR},          'none' );
