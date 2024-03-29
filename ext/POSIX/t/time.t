#!perl -w



use Config;
use POSIX;
use Test::More tests => 9;

# go to UTC to avoid DST issues around the world when testing.  SUS3 says that
# null should get you UTC, but some environments want the explicit names.
# Those with a working tzset() should be able to use the TZ below.
env::var('TZ' ) = "UTC0UTC";

SKIP: do {
    # It looks like POSIX.xs claims that only VMS and Mac OS traditional
    # don't have tzset().  Win32 works to call the function, but it doesn't
    # actually do anything.  Cygwin works in some places, but not others.  The
    # other Win32's below are guesses.
    skip "No tzset()", 2
       if $^OS_NAME eq "MacOS" || $^OS_NAME eq "VMS" || $^OS_NAME eq "cygwin" || $^OS_NAME eq "djgpp" ||
          $^OS_NAME eq "MSWin32" || $^OS_NAME eq "dos" || $^OS_NAME eq "interix";
    tzset();
    my @tzname = tzname();
    like(@tzname[0], qr/(GMT|UTC)/i, "tzset() to GMT/UTC");
    SKIP: do {
        skip "Mac OS X/Darwin doesn't handle this", 1 if $^OS_NAME =~ m/darwin/i;
        like(@tzname[1], qr/(GMT|UTC)/i, "The whole year?");
    };
};

# asctime and ctime...Let's stay below INT_MAX for 32-bits and
# positive for some picky systems.

is(asctime(localtime(0)), ctime(0), "asctime() and ctime() at zero");
is(asctime(localtime(12345678)), ctime(12345678), "asctime() and ctime() at 12345678");

# Careful!  strftime() is locale sensative.  Let's take care of that
my $orig_loc = setlocale(LC_TIME, "C") || die "Cannot setlocale() to C:  $^OS_ERROR";
my $jan_16 = 15 * 86400;
is(ctime($jan_16), strftime("\%a \%b \%d \%H:\%M:\%S \%Y\n", localtime($jan_16)),
        "get ctime() equal to strftime()");
setlocale(LC_TIME, $orig_loc) || die "Cannot setlocale() back to orig: $^OS_ERROR";

# clock() seems to have different definitions of what it does between POSIX
# and BSD.  Cygwin, Win32, and Linux lean the BSD way.  So, the tests just
# check the basics.
like(clock(), qr/\d*/, "clock() returns a numeric value");
ok(clock() +>= 0, "...and it returns something >= 0");

SKIP: do {
    skip "No difftime()", 1 if config_value('d_difftime') ne 'define';
    is(difftime(2, 1), 1, "difftime()");
};

SKIP: do {
    skip "No mktime()", 1 if config_value('d_mktime') ne 'define';
    my $time = time();
    is(mktime(localtime($time)), $time, "mktime()");
};
