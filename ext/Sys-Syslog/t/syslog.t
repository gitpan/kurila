#!perl

use Config;
use File::Spec;
use Test::More;

# we enable all Perl warnings, but we don't "use warnings 'all'" because 
# we want to disable the warnings generated by Sys::Syslog
no warnings;
use warnings < qw(closure deprecated exiting glob io misc numeric once overflow
                pack portable recursion redefine regexp severe signal substr
                syntax taint uninitialized unpack untie utf8 void);

my $is_Win32  = $^OS_NAME =~ m/win32/i;
my $is_Cygwin = $^OS_NAME =~ m/cygwin/i;

my $tests;
plan tests => $tests;

# any remaining warning should be severly punished
BEGIN { eval "use Test::NoWarnings"; $tests = $^EVAL_ERROR ?? 0 !! 1; }

BEGIN { $tests += 1 }
# ok, now loads them
eval 'use Socket';
use_ok('Sys::Syslog', ':standard', ':extended', ':macros');

BEGIN { $tests += 1 }
# check that the documented functions are correctly provided
can_ok( 'Sys::Syslog' => < qw(openlog syslog syslog setlogmask setlogsock closelog) );


BEGIN { $tests += 1 }
# check the diagnostics
# setlogsock()
try { setlogsock() };
like( $^EVAL_ERROR->{?description}, qr/^Invalid argument passed to setlogsock/, 
    "calling setlogsock() with no argument" );

BEGIN { $tests += 3 }
# syslog()
try { syslog() };
like( $^EVAL_ERROR->{?description}, qr/^syslog: expecting argument \$priority/, 
    "calling syslog() with no argument" );

try { syslog(undef) };
like( $^EVAL_ERROR->{?description}, qr/^syslog: expecting argument \$priority/, 
    "calling syslog() with one undef argument" );

try { syslog('') };
like( $^EVAL_ERROR->{?description}, qr/^syslog: expecting argument \$format/, 
    "calling syslog() with one empty argument" );


my $test_string = "uid $^UID is testing Perl $^PERL_VERSION syslog(3) capabilities";
my $r = 0;

BEGIN { $tests += 8 }
# try to open a syslog using a Unix or stream socket
SKIP: do {
    skip "can't connect to Unix socket: _PATH_LOG unavailable", 8
      unless -e Sys::Syslog::_PATH_LOG();

    # The only known $^O eq 'svr4' that needs this is NCR MP-RAS,
    # but assuming 'stream' in SVR4 is probably not that bad.
    my $sock_type = $^OS_NAME =~ m/^(solaris|irix|svr4|powerux)$/ ?? 'stream' !! 'unix';

    try { setlogsock($sock_type) };
    is( $^EVAL_ERROR, '', "setlogsock() called with '$sock_type'" );
    TODO: do {
        local $TODO = "minor bug";
        ok( $r, "setlogsock() should return true: '$r'" );
    };

    # open syslog with a "local0" facility
    SKIP: do {
        # openlog()
        $r = try { openlog('perl', 'ndelay', 'local0') } || 0;
        skip "can't connect to syslog", 6 if $^EVAL_ERROR and $^EVAL_ERROR->{?description} =~ m/^no connection to syslog available/;
        is( $^EVAL_ERROR, '', "openlog() called with facility 'local0'" );
        ok( $r, "openlog() should return true" );

        # syslog()
        $r = try { syslog('info', "$test_string by connecting to a $sock_type socket") } || 0;
        is( $^EVAL_ERROR, '', "syslog() called with level 'info'" );
        ok( $r, "syslog() should return true: '$r'" );

        # closelog()
        $r = try { closelog() } || 0;
        is( $^EVAL_ERROR, '', "closelog()" );
        ok( $r, "closelog() should return true: '$r'" );
    };
};


BEGIN { $tests += 20 * 8 }
# try to open a syslog using all the available connection methods
my @passed = @( () );
for my $sock_type (qw(native eventlog unix pipe stream inet tcp udp)) {
    SKIP: do {
        skip "the 'stream' mechanism because a previous mechanism with similar interface succeeded", 20 
            if $sock_type eq 'stream' and grep {m/pipe|unix/}, @passed;

        # setlogsock() called with an arrayref
        $r = try { setlogsock(\@($sock_type)) } || 0; die $^EVAL_ERROR->message if $^EVAL_ERROR;
        skip "can't use '$sock_type' socket", 20 unless $r;
        is( $^EVAL_ERROR, '', "[$sock_type] setlogsock() called with ['$sock_type']" );
        ok( $r, "[$sock_type] setlogsock() should return true: '$r'" );

        # setlogsock() called with a single argument
        $r = try { setlogsock($sock_type) } || 0; die $^EVAL_ERROR->message if $^EVAL_ERROR;
        skip "can't use '$sock_type' socket", 18 unless $r;
        is( $^EVAL_ERROR, '', "[$sock_type] setlogsock() called with '$sock_type'" );
        ok( $r, "[$sock_type] setlogsock() should return true: '$r'" );

        # openlog() without option NDELAY
        $r = try { openlog('perl', '', 'local0') } || 0;
        diag $^EVAL_ERROR->message if $^EVAL_ERROR;
        skip "can't connect to syslog", 16 if $^EVAL_ERROR and $^EVAL_ERROR->{?description} =~ m/^no connection to syslog available/;
        is( $^EVAL_ERROR, '', "[$sock_type] openlog() called with facility 'local0' and without option 'ndelay'" );
        ok( $r, "[$sock_type] openlog() should return true: $(dump::view($r))" );

        # openlog() with the option NDELAY
        $r = try { openlog('perl', 'ndelay', 'local0') } || 0;
        skip "can't connect to syslog", 14 if $^EVAL_ERROR and $^EVAL_ERROR->{?description} =~ m/^no connection to syslog available/;
        is( $^EVAL_ERROR, '', "[$sock_type] openlog() called with facility 'local0' with option 'ndelay'" );
        ok( $r, "[$sock_type] openlog() should return true: $(dump::view($r))" );

        # syslog() with negative level, should fail
        $r = try { syslog(-1, "$test_string by connecting to a $sock_type socket") } || 0;
        like( $^EVAL_ERROR->{?description}, '/^syslog: invalid level\/facility: /', "[$sock_type] syslog() called with level -1" );
        ok( !$r, "[$sock_type] syslog() should return false: $(dump::view($r))" );

        # syslog() with levels "info" and "notice" (as a strings), should fail
        $r = try { syslog('info,notice', "$test_string by connecting to a $sock_type socket") } || 0;
        like( $^EVAL_ERROR->{?description}, '/^syslog: too many levels given: notice/', "[$sock_type] syslog() called with level 'info,notice'" );
        ok( !$r, "[$sock_type] syslog() should return false: $(dump::view($r))" );

        # syslog() with facilities "local0" and "local1" (as a strings), should fail
        $r = try { syslog('local0,local1', "$test_string by connecting to a $sock_type socket") } || 0;
        like( $^EVAL_ERROR->{?description}, '/^syslog: too many facilities given: local1/', "[$sock_type] syslog() called with level 'local0,local1'" );
        ok( !$r, "[$sock_type] syslog() should return false: $(dump::view($r))" );

        # syslog() with level "info" (as a string), should pass
        $r = try { syslog('info', "$test_string by connecting to a $sock_type socket") } || 0;
        is( $^EVAL_ERROR, '', "[$sock_type] syslog() called with level 'info' (string)" );
        ok( $r, "[$sock_type] syslog() should return true: $(dump::view($r))" );

        # syslog() with level "info" (as a macro), should pass
        do { local $^OS_ERROR = 1;
          $r = try { syslog(LOG_INFO(), "$test_string by connecting to a $sock_type socket, setting a fake errno: \%m") } || 0;
        };
        is( $^EVAL_ERROR, '', "[$sock_type] syslog() called with level 'info' (macro)" );
        ok( $r, "[$sock_type] syslog() should return true: $(dump::view($r))" );

        push @passed, $sock_type;

        SKIP: do {
            skip "skipping closelog() tests for 'console'", 2 if $sock_type eq 'console';
            # closelog()
            $r = try { closelog() } || 0;
            is( $^EVAL_ERROR, '', "[$sock_type] closelog()" );
            ok( $r, "[$sock_type] closelog() should return true: '$r'" );
        };
    };
}


BEGIN { $tests += 10 }
SKIP: do {
    skip "not testing setlogsock('stream') on Win32", 10 if $is_Win32;
    skip "the 'unix' mechanism works, so the tests will likely fail with the 'stream' mechanism", 10 
        if grep {m/unix/}, @passed;

    skip "not testing setlogsock('stream'): _PATH_LOG unavailable", 10
        unless -e Sys::Syslog::_PATH_LOG();

    # setlogsock() with "stream" and an undef path
    $r = try { setlogsock("stream", undef ) } || '';
    is( $^EVAL_ERROR, '', "setlogsock() called, with 'stream' and an undef path" );
    if ($is_Cygwin) {
        if (-x "/usr/sbin/syslog-ng") {
            ok( $r, "setlogsock() on Cygwin with syslog-ng should return true: '$r'" );
        }
        else {
            ok( !$r, "setlogsock() on Cygwin without syslog-ng should return false: '$r'" );
        }
    }
    else  {
        ok( $r, "setlogsock() should return true: '$r'" );
    }

    # setlogsock() with "stream" and an empty path
    $r = try { setlogsock("stream", '' ) } || '';
    is( $^EVAL_ERROR, '', "setlogsock() called, with 'stream' and an empty path" );
    ok( !$r, "setlogsock() should return false: '$r'" );

    # setlogsock() with "stream" and /dev/null
    $r = try { setlogsock("stream", '/dev/null' ) } || '';
    is( $^EVAL_ERROR, '', "setlogsock() called, with 'stream' and '/dev/null'" );
    ok( $r, "setlogsock() should return true: '$r'" );

    # setlogsock() with "stream" and a non-existing file
    $r = try { setlogsock("stream", 'test.log' ) } || '';
    is( $^EVAL_ERROR, '', "setlogsock() called, with 'stream' and 'test.log' (file does not exist)" );
    ok( !$r, "setlogsock() should return false: '$r'" );

    # setlogsock() with "stream" and a local file
    SKIP: do {
        my $logfile = "test.log";
        open(my $logfh, ">", "$logfile") or skip "can't create file '$logfile': $^OS_ERROR", 2;
        close($logfh);
        $r = try { setlogsock("stream", $logfile ) } || '';
        is( $^EVAL_ERROR, '', "setlogsock() called, with 'stream' and '$logfile' (file exists)" );
        ok( $r, "setlogsock() should return true: '$r'" );
        unlink($logfile);
    };
};


BEGIN { $tests += 3 + 4 * 3 }
# setlogmask()
do {
    my $oldmask = 0;

    $oldmask = try { setlogmask(0) } || 0;
    is( $^EVAL_ERROR, '', "setlogmask() called with a null mask" );
    $r = try { setlogmask(0) } || 0;
    is( $^EVAL_ERROR, '', "setlogmask() called with a null mask (second time)" );
    is( $r, $oldmask, "setlogmask() must return the same mask as previous call");

    my @masks = @(
        LOG_MASK(LOG_ERR()), 
        ^~^LOG_MASK(LOG_INFO()), 
        LOG_MASK(LOG_CRIT()) ^|^ LOG_MASK(LOG_ERR()) ^|^ LOG_MASK(LOG_WARNING()), 
    );

    for my $newmask ( @masks) {
        $r = try { setlogmask($newmask) } || 0;
        is( $^EVAL_ERROR, '', "setlogmask() called with a new mask" );
        is( $r, $oldmask, "setlogmask() must return the same mask as previous call");
        $r = try { setlogmask(0) } || 0;
        is( $^EVAL_ERROR, '', "setlogmask() called with a null mask" );
        is( $r, $newmask, "setlogmask() must return the new mask");
        setlogmask($oldmask);
    }
};
