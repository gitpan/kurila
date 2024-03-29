#!/usr/bin/perl -w

BEGIN {
    if( env::var('PERL_CORE') ) {
        chdir 't';
        $^INCLUDE_PATH = @( '../lib' );
    }
    else {
        unshift $^INCLUDE_PATH, 't/lib';
    }
}
chdir 't';

BEGIN { 
    use Test::More; 

    if( $^OS_NAME =~ m/^VMS|os2|MacOS|MSWin32|cygwin|beos|netware$/i ) {
        plan skip_all => 'Non-Unix platform';
    }
    else {
        plan tests => 107;
    }
}

use ExtUtils::MM_Unix;

use File::Spec;

my $class = 'ExtUtils::MM_Unix';

# only one of the following can be true
# test should be removed if MM_Unix ever stops handling other OS than Unix
my $os =  (%ExtUtils::MM_Unix::Is{?OS2}   || 0)
        + (%ExtUtils::MM_Unix::Is{?Win32} || 0) 
        + (%ExtUtils::MM_Unix::Is{?Dos}   || 0)
        + (%ExtUtils::MM_Unix::Is{?VMS}   || 0); 
ok ( $os +<= 1,  'There can be only one (or none)');

cmp_ok ($ExtUtils::MM_Unix::VERSION, '+>=', '1.12606', 'Should be at least version 1.12606');

# when the following calls like canonpath, catdir etc are replaced by
# File::Spec calls, the test's become a bit pointless

foreach ( qw( xx/ ./xx/ xx/././xx xx///xx) )
  {
  is ($class->canonpath($_), File::Spec->canonpath($_), "canonpath $_");
  }

is ($class->catdir('xx','xx'), File::Spec->catdir('xx','xx'),
     'catdir(xx, xx) => xx/xx');
is ($class->catfile('xx','xx','yy'), File::Spec->catfile('xx','xx','yy'),
     'catfile(xx, xx) => xx/xx');

is ($class->file_name_is_absolute('Bombdadil'), 
    File::Spec->file_name_is_absolute('Bombdadil'),
     'file_name_is_absolute()');

is_deeply($class->path(), File::Spec->path(), 'path() same as File::Spec->path()');

foreach (qw/updir curdir rootdir/)
  {
  is ($class->?$_(), File::Spec->?$_(), $_ );
  }

foreach ( qw /
  c_o
  clean
  const_cccmd
  const_config
  const_loadlibs
  constants
  depend
  dist
  dist_basics
  dist_ci
  dist_core
  distdir
  dist_test
  dlsyms
  dynamic
  dynamic_bs
  dynamic_lib
  exescan
  extliblist
  find_perl
  fixin
  force
  guess_name
  init_dirscan
  init_main
  init_others
  install
  installbin
  linkext
  lsdir
  macro
  makeaperl
  makefile
  manifypods
  needs_linking
  pasthru
  perldepend
  pm_to_blib
  ppd
  prefixify
  processPL
  quote_paren
  realclean
  static
  static_lib
  staticmake
  subdir_x
  subdirs
  test
  test_via_harness
  test_via_script
  tool_xsubpp
  tools_other
  top_targets
  writedoc
  xs_c
  xs_cpp
  xs_o
  / )
  {
      can_ok($class, $_);
  }

###############################################################################
# some more detailed tests for the methods above

ok ( $class->dist_basics(), 'distclean :: realclean distcheck');

###############################################################################
# has_link_code tests

my $t = bless \%( NAME => "Foo" ), $class;
$t->{+HAS_LINK_CODE} = 1; 
is ($t->has_link_code(),1,'has_link_code'); is ($t->{HAS_LINK_CODE},1);

$t->{+HAS_LINK_CODE} = 0;
is ($t->has_link_code(),0); is ($t->{HAS_LINK_CODE},0);

delete $t->{HAS_LINK_CODE}; delete $t->{OBJECT};
is ($t->has_link_code(),0); is ($t->{HAS_LINK_CODE},0);

delete $t->{HAS_LINK_CODE}; $t->{+OBJECT} = 1;
is ($t->has_link_code(),1); is ($t->{HAS_LINK_CODE},1);

delete $t->{HAS_LINK_CODE}; delete $t->{OBJECT}; $t->{+MYEXTLIB} = 1;
is ($t->has_link_code(),1); is ($t->{HAS_LINK_CODE},1);

delete $t->{HAS_LINK_CODE}; delete $t->{MYEXTLIB}; $t->{+C} = \@( 'Gloin' );
is ($t->has_link_code(),1); is ($t->{HAS_LINK_CODE},1);

###############################################################################
# libscan

is ($t->libscan('foo/RCS/bar'),     '', 'libscan on RCS');
is ($t->libscan('CVS/bar/car'),     '', 'libscan on CVS');
is ($t->libscan('SCCS'),            '', 'libscan on SCCS');
is ($t->libscan('.svn/something'),  '', 'libscan on Subversion');
is ($t->libscan('foo/b~r'),         'foo/b~r',    'libscan on file with ~');
is ($t->libscan('foo/RCS.pm'),      'foo/RCS.pm', 'libscan on file with RCS');

is ($t->libscan('Fatty'), 'Fatty', 'libscan on something not a VC file' );

###############################################################################
# maybe_command

open(my $fh, ">", "command"); print $fh, "foo"; close $fh;
SKIP: do {
    skip ("no separate execute mode", 1) if ($^OS_NAME eq "vos");
    ok (!$t->maybe_command('command') ,"non executable file isn't a command");
};

chmod 0755, "command";
ok ($t->maybe_command('command'),        "executable file is a command");
unlink "command";


###############################################################################
# perl_script (on unix any ordinary, readable file)

my $self_name = env::var('PERL_CORE') ?? '../lib/ExtUtils/t/MM_Unix.t' 
                                 !! 'MM_Unix.t';
is ($t->perl_script($self_name),$self_name, 'we pass as a perl_script()');

###############################################################################
# perm_rw perm_rwx

$t->init_PERM;
is ($t->perm_rw(),'644', 'perm_rw() is 644');
is ($t->perm_rwx(),'755', 'perm_rwx() is 755');

###############################################################################
# post_constants, postamble, post_initialize

foreach (qw/ post_constants postamble post_initialize/)
  {
  is ($t->?$_(),'', "$_() is an empty string");
  }

###############################################################################
# replace_manpage_separator 

is ($t->replace_manpage_separator('Foo/Bar'),'Foo::Bar','manpage_separator'); 

###############################################################################

$t->init_linker;
foreach (qw/ EXPORT_LIST PERL_ARCHIVE PERL_ARCHIVE_AFTER /)
{
    ok( exists $t->{$_}, "$_ was defined" );
    is( $t->{$_}, '', "$_ is empty on Unix"); 
}


do {
    $t->{+CCFLAGS} = '-DMY_THING';
    $t->{+LIBPERL_A} = 'libperl.a';
    $t->{+LIB_EXT}   = '.a';
    local $t->{+NEEDS_LINKING} = 1;
    $t->cflags();

    # Brief bug where CCFLAGS was being blown away
    is( $t->{CCFLAGS}, '-DMY_THING',    'cflags retains CCFLAGS' );
};

