#!/usr/bin/perl -w

# Test ExtUtils::Install.

BEGIN {
    if( env::var('PERL_CORE') ) {
        $^INCLUDE_PATH = @('../../lib', '../lib', 'lib');
    }
    else {
        unshift $^INCLUDE_PATH, 't/lib';
    }
}

use File::Path;
use File::Spec;
use env;

use Test::More tests => 69;

use MakeMaker::Test::Setup::BFD;

BEGIN { use_ok('ExtUtils::Install') }

# Check exports.
foreach my $func (qw(install uninstall pm_to_blib install_default)) {
    can_ok(__PACKAGE__, $func);
}


ok( setup_recurs(), 'setup' );
END {
    ok( chdir File::Spec->updir );
    ok( teardown_recurs(), 'teardown' );
}
# ensure the env doesnt pollute our tests
local env::var('EU_INSTALL_ALWAYS_COPY') = undef;
local env::var('EU_ALWAYS_COPY') = undef;

chdir 'Big-Dummy';

my $stdout = '';
close $^STDOUT;
open $^STDOUT, '>>', \$stdout or die;
pm_to_blib( \%( 'lib/Big/Dummy.pm' => 'blib/lib/Big/Dummy.pm' ),
            'blib/lib/auto'
          );
END { rmtree 'blib' }

ok( -d 'blib/lib',              'pm_to_blib created blib dir' );
ok( -r 'blib/lib/Big/Dummy.pm', '  copied .pm file' );
ok( -r 'blib/lib/auto',         '  created autosplit dir' );
is( $stdout, "symlink lib/Big/Dummy.pm blib/lib/Big/Dummy.pm\n" );
$stdout = '';

pm_to_blib( \%( 'lib/Big/Dummy.pm' => 'blib/lib/Big/Dummy.pm' ),
            'blib/lib/auto'
          );
ok( -d 'blib/lib',              'second run, blib dir still there' );
ok( -r 'blib/lib/Big/Dummy.pm', '  .pm file still there' );
ok( -r 'blib/lib/auto',         '  autosplit still there' );
is( $stdout, "Skip blib/lib/Big/Dummy.pm (unchanged)\n" );
$stdout = '';

install( \@(
    from_to=>\%( 'blib/lib' => 'install-test/lib/perl',
           read   => 'install-test/packlist',
           write  => 'install-test/packlist'
         ),
    dry_run=>1));
ok( ! -d 'install-test/lib/perl',        'install made dir (dry run)');
ok( ! -r 'install-test/lib/perl/Big/Dummy.pm',
                                         '  .pm file installed (dry run)');
ok( ! -r 'install-test/packlist',        '  packlist exists (dry run)');

install(\@( from_to=> \%( 'blib/lib' => 'install-test/lib/perl',
           read   => 'install-test/packlist',
           write  => 'install-test/packlist'
         ) ));
ok( -d 'install-test/lib/perl',                 'install made dir' );
ok( -r 'install-test/lib/perl/Big/Dummy.pm',    '  .pm file installed' );
ok(!-r 'install-test/lib/perl/Big/Dummy.SKIP',  '  ignored .SKIP file' );
ok( -r 'install-test/packlist',                 '  packlist exists' );

open(my $packlist, '<', 'install-test/packlist' ) or die;
my %packlist = %( < @+: map { chomp;  @($_ => 1) }, @( ~< $packlist) );
close $packlist or die;

# On case-insensitive filesystems (ie. VMS), the keys of the packlist might
# be lowercase. :(
my $native_dummy = File::Spec->catfile( <qw(install-test lib perl Big Dummy.pm));
is( nkeys %packlist, 1 );
is( lc((keys %packlist)[0]), lc $native_dummy, 'packlist written' );


# Test UNINST=1 preserving same versions in other dirs.
install(\@(from_to=> \%( 'blib/lib' => 'install-test/other_lib/perl',
           read   => 'install-test/packlist',
           write  => 'install-test/packlist'
         ),uninstall_shadows=>1));
ok( -d 'install-test/other_lib/perl',        'install made other dir' );
ok( -r 'install-test/other_lib/perl/Big/Dummy.pm', '  .pm file installed' );
ok( -r 'install-test/packlist',              '  packlist exists' );
ok( -r 'install-test/lib/perl/Big/Dummy.pm', '  UNINST=1 preserved same' );


chmod 0644, 'blib/lib/Big/Dummy.pm' or die $^OS_ERROR;
open(my $dummy, ">>", "blib/lib/Big/Dummy.pm") or die $^OS_ERROR;
print $dummy, "Extra stuff\n";
close $dummy;


# Test UNINST=0 does not remove other versions in other dirs.
do {
  ok( -r 'install-test/lib/perl/Big/Dummy.pm', 'different install exists' );

  local $^INCLUDE_PATH = @('install-test/lib/perl');
  local env::var('PERL5LIB' ) = '';
  install(\@(from_to=> \%( 'blib/lib' => 'install-test/other_lib/perl',
           read   => 'install-test/packlist',
           write  => 'install-test/packlist'
         )));
  ok( -d 'install-test/other_lib/perl',        'install made other dir' );
  ok( -r 'install-test/other_lib/perl/Big/Dummy.pm', '  .pm file installed' );
  ok( -r 'install-test/packlist',              '  packlist exists' );
  ok( -r 'install-test/lib/perl/Big/Dummy.pm',
                                             '  UNINST=0 left different' );
};

# Test UNINST=1 only warning when failing to remove an irrelevent shadow file
do {
  my $tfile='install-test/lib/perl/Big/Dummy.pm';
  local $ExtUtils::Install::Testing = $tfile; 
  local $^INCLUDE_PATH = @('install-test/other_lib/perl','install-test/lib/perl');
  local env::var('PERL5LIB' ) = '';
  ok( -r $tfile, 'different install exists' );
  my @warn;
  local $^WARN_HOOK =sub { push @warn, @_[0]->message; return };
  install(\@(from_to=> \%( 'blib/lib' => 'install-test/other_lib/perl',
                           read   => 'install-test/packlist',
                           write  => 'install-test/packlist'
                         ),
       uninstall_shadows=>1));
  ok(0+nelems @warn,"  we did warn");
  ok( -d 'install-test/other_lib/perl',        'install made other dir' );
  ok( -r 'install-test/other_lib/perl/Big/Dummy.pm', '  .pm file installed' );
  ok( -r 'install-test/packlist',              '  packlist exists' );
  ok( -r $tfile, '  UNINST=1 failed to remove different' );
  
};

# Test UNINST=1 dieing when failing to remove an relevent shadow file
do {
  my $tfile='install-test/lib/perl/Big/Dummy.pm';
  local $ExtUtils::Install::Testing = $tfile;
  local $^INCLUDE_PATH = @('install-test/lib/perl','install-test/other_lib/perl');
  local env::var('PERL5LIB' ) = '';
  ok( -r $tfile, 'different install exists' );
  my @warn;
  local $^WARN_HOOK = sub { push @warn,< @_; return };
  my $ok=try {
    install(\@(from_to=> \%( 'blib/lib' => 'install-test/other_lib/perl',
           read   => 'install-test/packlist',
           write  => 'install-test/packlist'
         ),uninstall_shadows=>1));
    1
  };
  ok(!$ok,'  we did die');
  ok(!nelems @warn,"  we didnt warn");
  ok( -d 'install-test/other_lib/perl',        'install made other dir' );
  ok( -r 'install-test/other_lib/perl/Big/Dummy.pm', '  .pm file installed' );
  ok( -r 'install-test/packlist',              '  packlist exists' );
  ok( -r $tfile,'  UNINST=1 failed to remove different' );
};

# Test UNINST=1 removing other versions in other dirs.
do {
  local $^INCLUDE_PATH = @('install-test/lib/perl');
  local env::var('PERL5LIB' ) = '';
  ok( -r 'install-test/lib/perl/Big/Dummy.pm','different install exists' );
  install(\@(from_to=>\%( 'blib/lib' => 'install-test/other_lib/perl',
           read   => 'install-test/packlist',
           write  => 'install-test/packlist'
         ),uninstall_shadows=>1));
  ok( -d 'install-test/other_lib/perl',        'install made other dir' );
  ok( -r 'install-test/other_lib/perl/Big/Dummy.pm', '  .pm file installed' );
  ok( -r 'install-test/packlist',              '  packlist exists' );
  ok( !-r 'install-test/lib/perl/Big/Dummy.pm',
                                             '  UNINST=1 removed different' );
};

# Test EU_ALWAYS_COPY triggers copy.
do {
  local $^INCLUDE_PATH = @('install-test/lib/perl');
  local env::var('PERL5LIB' ) = '';
  local env::var('EU_INSTALL_ALWAYS_COPY') =1;
  my $tfile='install-test/other_lib/perl/Big/Dummy.pm';
  my $sfile='blib/lib/Big/Dummy.pm';
  ok(-r $tfile,"install file already exists");
  ok(-r $sfile,"source file already exists");
  utime time-600, time-600, $sfile or die "utime '$sfile' failed:$^OS_ERROR";   
  ok( @(stat $tfile)[9]!=@(stat $sfile)[9],'  Times are different');
  install(\@(from_to=>\%( 'blib/lib' => 'install-test/other_lib/perl',
           read   => 'install-test/packlist',
           write  => 'install-test/packlist'
         ),result=>\my %result));
  ok( -d 'install-test/other_lib/perl',        'install made other dir' );
  ok( -r 'install-test/other_lib/perl/Big/Dummy.pm', '  .pm file installed' );
  ok( -r 'install-test/packlist',              '  packlist exists' );
  ok( @(stat $tfile)[9]==@(stat$sfile)[9],'  Times are same');
  ok( !%result{?install_unchanged},'  $result{install_unchanged} should be empty');
};
# Test nothing is copied.
do {
  local $^INCLUDE_PATH = @('install-test/lib/perl');
  local env::var('PERL5LIB' ) = '';
  local env::var('EU_INSTALL_ALWAYS_COPY') =0;
  my $tfile='install-test/other_lib/perl/Big/Dummy.pm';
  my $sfile='blib/lib/Big/Dummy.pm';
  ok(-r $tfile,"install file already exists");
  ok(-r $sfile,"source file already exists");
  utime time-1200, time-1200, $sfile or die "utime '$sfile' failed:$^OS_ERROR";   
  ok( @(stat $tfile)[9]!=@(stat $sfile)[9],'  Times are different');
  install(\@(from_to=>\%( 'blib/lib' => 'install-test/other_lib/perl',
           read   => 'install-test/packlist',
           write  => 'install-test/packlist'
         ),result=>\my %result));
  ok( -d 'install-test/other_lib/perl',        'install made other dir' );
  ok( -r 'install-test/other_lib/perl/Big/Dummy.pm', '  .pm file installed' );
  ok( -r 'install-test/packlist',              '  packlist exists' );
  ok( @(stat $tfile)[9]!=@(stat$sfile)[9],'  Times are different');
  ok( !%result{?install},'  nothing should have been installed');
  ok( %result{install_unchanged},'  install_unchanged should be populated');
};
