#!./perl -w

use Test::More;

BEGIN {
    require Config;
    if (Config::config_value('extensions') !~ m/\bSocket\b/ && 
        !(($^OS_NAME eq 'VMS') && Config::config_value('d_socket'))) 
    {
	plan skip_all => "Test uses Socket, Socket not built";
    }
    if ($^OS_NAME eq 'MacOS' || ($^OS_NAME eq 'irix' && Config::config_value('osvers') == 5)) {
	plan skip_all => "Test relies on resolution of localhost, fails on $^OS_NAME ($(Config::config_value('osvers')))";
    }
}

use Test::More tests => 7;

BEGIN { use_ok 'Net::hostent' }

# Remind me to add this to Test::More.
sub DIE {
    print $^STDOUT, "# $(join ' ',@_)\n";
    exit 1;
}

# test basic resolution of localhost <-> 127.0.0.1
use Socket;

my $h = gethost('localhost');
ok(defined $h,  "gethost('localhost')") ||
  DIE("Can't continue without working gethost: $^OS_ERROR");

is( inet_ntoa($h->addr), "127.0.0.1",   'addr from gethost' );

my $i = gethostbyaddr(inet_aton("127.0.0.1"));
ok(defined $i,  "gethostbyaddr('127.0.0.1')") || 
  DIE("Can't continue without working gethostbyaddr: $^OS_ERROR");

is( inet_ntoa($i->addr), "127.0.0.1",   'addr from gethostbyaddr' );

# need to skip the name comparisons on Win32 because windows will
# return the name of the machine instead of "localhost" when resolving
# 127.0.0.1 or even "localhost"

# - VMS returns "LOCALHOST" under tcp/ip services V4.1 ECO 2, possibly others
# - OS/390 returns localhost.YADDA.YADDA

SKIP: do {
    skip "Windows will return the machine name instead of 'localhost'", 2
      if $^OS_NAME eq 'MSWin32' or $^OS_NAME eq 'NetWare' or $^OS_NAME eq 'cygwin';

    print $^STDOUT, "# name = " . $h->name . ", aliases = " . join (",", @{$h->aliases}) . "\n";

    my $in_alias;
    unless ($h->name =~ m/^localhost(?:\..+)?$/i) {
        foreach ( @{$h->aliases}) {
            if (m/^localhost(?:\..+)?$/i) {
                $in_alias = 1;
                last;
            }
        }
	ok( $in_alias );
    } else {
	ok( 1 );
    }
    
    if ($in_alias) {
        # If we found it in the aliases before, expect to find it there again.
        foreach ( @{$h->aliases}) {
            if (m/^localhost(?:\..+)?$/i) {
                # This time, clear the flag if we see "localhost"
                undef $in_alias;
                last;
            }
        }
    } 

    if( $in_alias ) {
        like( $i->name, qr/^localhost(?:\..+)?$/i );
    }
    else {
        ok( !$in_alias );
        print $^STDOUT, "# " . $h->name . " " . join (",", @{$h->aliases}) . "\n";
    }
};
