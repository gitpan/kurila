package Sys::Syslog;

use warnings::register;
use Fcntl < qw(O_WRONLY);
use File::Basename;
use POSIX < qw(strftime setlocale LC_TIME);
use Socket ':all';
require Exporter;

do {
    our $VERSION = '0.24';
    our @ISA = qw(Exporter);

    our %EXPORT_TAGS = %(
        standard => \qw(openlog syslog closelog setlogmask),
        extended => \qw(setlogsock),
        macros => \@(
            # levels
            < qw(
                LOG_ALERT LOG_CRIT LOG_DEBUG LOG_EMERG LOG_ERR 
                LOG_INFO LOG_NOTICE LOG_WARNING
            ),

            # standard facilities
            < qw(
                LOG_AUTH LOG_AUTHPRIV LOG_CRON LOG_DAEMON LOG_FTP LOG_KERN
                LOG_LOCAL0 LOG_LOCAL1 LOG_LOCAL2 LOG_LOCAL3 LOG_LOCAL4
                LOG_LOCAL5 LOG_LOCAL6 LOG_LOCAL7 LOG_LPR LOG_MAIL LOG_NEWS
                LOG_SYSLOG LOG_USER LOG_UUCP
            ),
            # Mac OS X specific facilities
            < qw( LOG_INSTALL LOG_LAUNCHD LOG_NETINFO LOG_RAS LOG_REMOTEAUTH ),
            # modern BSD specific facilities
            < qw( LOG_CONSOLE LOG_NTP LOG_SECURITY ),
            # IRIX specific facilities
            < qw( LOG_AUDIT LOG_LFMT ),

            # options
            < qw(
                LOG_CONS LOG_PID LOG_NDELAY LOG_NOWAIT LOG_ODELAY LOG_PERROR 
            ),

            # others macros
            < qw(
                LOG_FACMASK LOG_NFACILITIES LOG_PRIMASK 
                LOG_MASK LOG_UPTO
            ),
        ),
    );

    our @EXPORT = @{%EXPORT_TAGS{?standard}};

    our @EXPORT_OK = @(
        < @{%EXPORT_TAGS{?extended}}, 
        < @{%EXPORT_TAGS{?macros}}, 
    );

    try {
        require XSLoader;
        XSLoader::load('Sys::Syslog', $VERSION);
        1
    } or do {
        require DynaLoader;
        push @ISA, 'DynaLoader';
        Sys::Syslog->bootstrap($VERSION);
    };
};


# 
# Public variables
# 
our ($host);             # host to send syslog messages to (see notes at end)

# 
# Global variables
# 
our ($facility);
my $connected = 0;              # flag to indicate if we're connected or not
my $syslog_send;                # coderef of the function used to send messages
my $syslog_path = undef;        # syslog path for "stream" and "unix" mechanisms
my $syslog_xobj = undef;        # if defined, holds the external object used to send messages
my $transmit_ok = 0;            # flag to indicate if the last message was transmited
my $sock_timeout  = 0;          # socket timeout, see below
my $current_proto = undef;      # current mechanism used to transmit messages
my $ident = '';                 # identifiant prepended to each message
$facility = '';                 # current facility
my $maskpri = LOG_UPTO(LOG_DEBUG());     # current log mask

my $syslogfh;

my %options = %(
    ndelay  => 0, 
    nofatal => 0, 
    nowait  => 0, 
    pid     => 0, 
);

# Default is now to first use the native mechanism, so Perl programs 
# behave like other normal Unix programs, then try other mechanisms.
my @connectMethods = qw(native tcp udp unix pipe stream console);
if ($^OS_NAME =~ m/^(freebsd|linux)$/) {
    @connectMethods = grep { $_ ne 'udp' }, @connectMethods;
}

# use EventLog on Win32
my $is_Win32 = $^OS_NAME =~ m/Win32/i;
eval "use Sys::Syslog::Win32";

if (not $^EVAL_ERROR) {
    unshift @connectMethods, 'eventlog';
} elsif ($is_Win32) {
    warn $^EVAL_ERROR;
}

my @defaultMethods = @connectMethods;
my @fallbackMethods = @( () );

$sock_timeout = 0.25 if $^OS_NAME =~ m/darwin/;

# coderef for a nicer handling of errors
my $err_sub = %options{?nofatal} ?? \&warnings::warnif !! sub { die shift; };


sub openlog(?$ident, ?my $logopt, ?$facility) {

    # default values
    $ident    ||= basename($^PROGRAM_NAME) || getlogin() || getpwuid($^UID) || 'syslog';
    $logopt   ||= '';
    $facility ||= LOG_USER();

    for my $opt (split m/\b/, $logopt) {
        %options{+$opt} = 1 if exists %options{$opt}
    }

    $err_sub = %options{?nofatal} ?? \&warnings::warnif !! sub { die shift; };
    return 1 unless %options{?ndelay};
    connect_log();
}

sub closelog {
    $facility = $ident = '';
    disconnect_log();
} 

sub setlogmask {
    my $oldmask = $maskpri;
    $maskpri = shift unless @_[0] == 0;
    $oldmask;
}
 
sub setlogsock {
    my $setsock = shift;
    $syslog_path = shift;
    disconnect_log() if $connected;
    $transmit_ok = 0;
    @fallbackMethods = @( () );
    @connectMethods = @defaultMethods;

    if (ref $setsock eq 'ARRAY') {
	@connectMethods = @$setsock;

    } elsif (lc $setsock eq 'stream') {
	if (not defined $syslog_path) {
	    my @try = qw(/dev/log /dev/conslog);

            if (length _PATH_LOG()) {        # Undefined _PATH_LOG is "".
		unshift @try, _PATH_LOG();
            }

	    for my $try ( @try) {
		if (-w $try) {
		    $syslog_path = $try;
		    last;
		}
	    }

            if (not defined $syslog_path) {
                warnings::warnif "stream passed to setlogsock, but could not find any device";
                return undef
            }
        }

	if (not -w $syslog_path) {
            warnings::warnif "stream passed to setlogsock, but $syslog_path is not writable";
	    return undef;
	} else {
            @connectMethods = qw(stream);
	}

    } elsif (lc $setsock eq 'unix') {
        if (length _PATH_LOG() || (defined $syslog_path && -w $syslog_path)) {
	    $syslog_path = _PATH_LOG() unless defined $syslog_path;
            @connectMethods = qw(unix);
        } else {
            warnings::warnif 'unix passed to setlogsock, but path not available';
	    return undef;
        }

    } elsif (lc $setsock eq 'pipe') {
        for my $path (@: $syslog_path, _PATH_LOG(), "/dev/log") {
            next unless defined $path and length $path and -p $path and -w _;
            $syslog_path = $path;
            last
        }

        if (not $syslog_path) {
            warnings::warnif "pipe passed to setlogsock, but path not available";
            return undef
        }

        @connectMethods = qw(pipe);

    } elsif (lc $setsock eq 'native') {
        @connectMethods = qw(native);

    } elsif (lc $setsock eq 'eventlog') {
        if (eval "use Win32::EventLog; 1") {
            @connectMethods = qw(eventlog);
        } else {
            warnings::warnif "eventlog passed to setlogsock, but operating system isn't Win32-compatible";
            return undef;
        }

    } elsif (lc $setsock eq 'tcp') {
	if (getservbyname('syslog', 'tcp') || getservbyname('syslogng', 'tcp')) {
            @connectMethods = qw(tcp);
	} else {
            warnings::warnif "tcp passed to setlogsock, but tcp service unavailable";
	    return undef;
	}

    } elsif (lc $setsock eq 'udp') {
	if (getservbyname('syslog', 'udp')) {
            @connectMethods = qw(udp);
	} else {
            warnings::warnif "udp passed to setlogsock, but udp service unavailable";
	    return undef;
	}

    } elsif (lc $setsock eq 'inet') {
	@connectMethods = @( 'tcp', 'udp' );

    } elsif (lc $setsock eq 'console') {
	@connectMethods = qw(console);

    } else {
        die "Invalid argument passed to setlogsock; must be 'stream', 'pipe', "
          . "'unix', 'native', 'eventlog', 'tcp', 'udp' or 'inet'";
    }

    return 1;
}

sub syslog {
    my $priority = shift;
    my $mask = shift;
    my ($message, $buf);
    my (@words, $num, $numpri, $numfac, $sum);
    my $failed = undef;
    my $fail_time = undef;
    my $error = $^OS_ERROR;

    # if $ident is undefined, it means openlog() wasn't previously called
    # so do it now in order to have sensible defaults
    openlog() unless $ident;

    local $facility = $facility;    # may need to change temporarily.

    die "syslog: expecting argument \$priority" unless defined $priority;
    die "syslog: expecting argument \$format"   unless defined $mask;

    @words = split(m/\W+/, $priority, 2);    # Allow "level" or "level|facility".
    undef $numpri;
    undef $numfac;

    foreach ( @words) {
	$num = xlate($_);		    # Translate word to number.
	if ($num +< 0) {
	    die "syslog: invalid level/facility: $_"
	}
	elsif ($num +<= LOG_PRIMASK()) {
	    die "syslog: too many levels given: $_" if defined $numpri;
	    $numpri = $num;
	    return 0 unless LOG_MASK($numpri) ^&^ $maskpri;
	}
	else {
	    die "syslog: too many facilities given: $_" if defined $numfac;
	    $facility = $_;
	    $numfac = $num;
	}
    }

    die "syslog: level must be given" unless defined $numpri;

    if (not defined $numfac) {  # Facility not specified in this call.
	$facility = 'user' unless $facility;
	$numfac = xlate($facility);
    }

    connect_log() unless $connected;

    if ($mask =~ m/\%m/) {
        # escape percent signs for sprintf()
        $error =~ s/%/\%\%/g if @_;
        # replace %m with $error, if preceded by an even number of percent signs
        $mask =~ s/(?<!\%)((?:\%\%)*)\%m/$1$error/g;
    }

    $mask .= "\n" unless $mask =~ m/\n$/;
    $message = @_ ?? sprintf($mask, < @_) !! $mask;

    # See CPAN-RT#24431. Opened on Apple Radar as bug #4944407 on 2007.01.21
    chomp $message if $^OS_NAME =~ m/darwin/;

    if ($current_proto eq 'native') {
        $buf = $message;

    }
    elsif ($current_proto eq 'eventlog') {
        $buf = $message;
    }
    else {
        my $whoami = $ident;
        $whoami .= "[$^PID]" if %options{?pid};

        $sum = $numpri + $numfac;
        my $oldlocale = setlocale(LC_TIME);
        setlocale(LC_TIME, 'C');
        my $timestamp = strftime "\%b \%e \%T", localtime;
        setlocale(LC_TIME, $oldlocale);
        $buf = "<$sum>$timestamp $whoami: $message\0";
    }

    # it's possible that we'll get an error from sending
    # (e.g. if method is UDP and there is no UDP listener,
    # then we'll get ECONNREFUSED on the send). So what we
    # want to do at this point is to fallback onto a different
    # connection method.
    while (@fallbackMethods || $syslog_send) {
	if ($failed && (time - $fail_time) +> 60) {
	    # it's been a while... maybe things have been fixed
	    @fallbackMethods = @( () );
	    disconnect_log();
	    $transmit_ok = 0; # make it look like a fresh attempt
	    connect_log();
        }

	if ($connected && !connection_ok()) {
	    # Something was OK, but has now broken. Remember coz we'll
	    # want to go back to what used to be OK.
	    $failed = $current_proto unless $failed;
	    $fail_time = time;
	    disconnect_log();
	}

	connect_log() unless $connected;
	$failed = undef if ($current_proto && $failed && $current_proto eq $failed);

	if ($syslog_send) {
            if ($syslog_send->($buf, $numpri, $numfac)) {
		$transmit_ok++;
		return 1;
	    }
	    # typically doesn't happen, since errors are rare from write().
	    disconnect_log();
	}
    }
    # could not send, could not fallback onto a working
    # connection method. Lose.
    return 0;
}

sub _syslog_send_console($buf) {
    chop($buf); # delete the NUL from the end
    # The console print is a method which could block
    # so we do it in a child process and always return success
    # to the caller.
    if (my $pid = fork) {

	if (%options{?nowait}) {
	    return 1;
	} else {
	    if (waitpid($pid, 0) +>= 0) {
	    	return  $^CHILD_ERROR >> 8;
	    } else {
		# it's possible that the caller has other
		# plans for SIGCHLD, so let's not interfere
		return 1;
	    }
	}
    } else {
        if (open(my $consfh, ">", "/dev/console")) {
	    my $ret = print $consfh, $buf . "\r";  # XXX: should this be \x0A ?
	    exit $ret if defined $pid;
	    close $consfh;
	}
	exit if defined $pid;
    }
}

sub _syslog_send_stream($buf) {
    # XXX: this only works if the OS stream implementation makes a write 
    # look like a putmsg() with simple header. For instance it works on 
    # Solaris 8 but not Solaris 7.
    # To be correct, it should use a STREAMS API, but perl doesn't have one.
    return syswrite($syslogfh, $buf, length($buf));
}

sub _syslog_send_pipe($buf) {
    return print $syslogfh, $buf;
}

sub _syslog_send_socket($buf, ...) {
    return syswrite($syslogfh, $buf, length($buf));
    #return send($syslogfh, $buf, 0);
}

sub _syslog_send_native($buf, $numpri, ...) {
    syslog_xs($numpri, $buf);
    return 1;
}


# xlate()
# -----
# private function to translate names to numeric values
# 
sub xlate {
    my@($name) =  @_;
    return $name+0 if $name =~ m/^\s*\d+\s*$/;
    $name = uc $name;
    $name = "LOG_$name" unless $name =~ m/^LOG_/;
    $name = "Sys::Syslog::$name";
    # Can't have just try { &$name } || -1 because some LOG_XXX may be zero.
    my $value = try { &{*{Symbol::fetch_glob($name)}} ( < @_ )};
    defined $value ?? $value !! -1;
}


# connect_log()
# -----------
# This function acts as a kind of front-end: it tries to connect to 
# a syslog service using the selected methods, trying each one in the 
# selected order. 
# 
sub connect_log {
    @fallbackMethods = @connectMethods unless @fallbackMethods;

    if ($transmit_ok && $current_proto) {
        # Retry what we were on, because it has worked in the past.
	unshift(@fallbackMethods, $current_proto);
    }

    $connected = 0;
    my @errs = @( () );
    my $proto = undef;

    while ($proto = shift @fallbackMethods) {
	my $fn = "connect_$proto";
	$connected = &{*{Symbol::fetch_glob($fn)}}(\@errs) if defined &{*{Symbol::fetch_glob($fn)}};
	last if $connected;
    }

    $transmit_ok = 0;
    if ($connected) {
	$current_proto = $proto;
        iohandle::output_autoflush($syslogfh, 1);
    } else {
	@fallbackMethods = @( () );
        $err_sub->(join "\n\t- ", @( "no connection to syslog available", < @errs));
        return undef;
    }
}

sub connect_tcp($errs) {

    my $tcp = getprotobyname('tcp');
    if (!defined $tcp) {
	push @$errs, "getprotobyname failed for tcp";
	return 0;
    }

    my $syslog = getservbyname('syslog', 'tcp');
    $syslog = getservbyname('syslogng', 'tcp') unless defined $syslog;
    if (!defined $syslog) {
	push @$errs, "getservbyname failed for syslog/tcp and syslogng/tcp";
	return 0;
    }

    my $addr;
    if (defined $host) {
        $addr = inet_aton($host);
        if (!$addr) {
	    push @$errs, "can't lookup $host";
	    return 0;
	}
    } else {
        $addr = INADDR_LOOPBACK;
    }
    $addr = sockaddr_in($syslog, $addr);

    if (!socket($syslogfh, AF_INET, SOCK_STREAM, $tcp)) {
	push @$errs, "tcp socket: $^OS_ERROR";
	return 0;
    }

    setsockopt($syslogfh, SOL_SOCKET, SO_KEEPALIVE, 1);
    if (try { IPPROTO_TCP() }) {
        # These constants don't exist in 5.005. They were added in 1999
        setsockopt($syslogfh, IPPROTO_TCP(), TCP_NODELAY(), 1);
    }
    if (!connect($syslogfh, $addr)) {
	push @$errs, "tcp connect: $^OS_ERROR";
	return 0;
    }

    $syslog_send = \&_syslog_send_socket;

    return 1;
}

sub connect_udp($errs) {

    my $udp = getprotobyname('udp');
    if (!defined $udp) {
	push @$errs, "getprotobyname failed for udp";
	return 0;
    }

    my $syslog = getservbyname('syslog', 'udp');
    if (!defined $syslog) {
	push @$errs, "getservbyname failed for syslog/udp";
	return 0;
    }

    my $addr;
    if (defined $host) {
        $addr = inet_aton($host);
        if (!$addr) {
	    push @$errs, "can't lookup $host";
	    return 0;
	}
    } else {
        $addr = INADDR_LOOPBACK;
    }
    $addr = sockaddr_in($syslog, $addr);

    if (!socket($syslogfh, AF_INET, SOCK_DGRAM, $udp)) {
	push @$errs, "udp socket: $^OS_ERROR";
	return 0;
    }
    if (!connect($syslogfh, $addr)) {
	push @$errs, "udp connect: $^OS_ERROR";
	return 0;
    }

    # We want to check that the UDP connect worked. However the only
    # way to do that is to send a message and see if an ICMP is returned
    _syslog_send_socket("");
    if (!connection_ok()) {
	push @$errs, "udp connect: nobody listening";
	return 0;
    }

    $syslog_send = \&_syslog_send_socket;

    return 1;
}

sub connect_stream($errs) {
    # might want syslog_path to be variable based on syslog.h (if only
    # it were in there!)
    $syslog_path = '/dev/conslog' unless defined $syslog_path; 
    if (!-w $syslog_path) {
	push @$errs, "stream $syslog_path is not writable";
	return 0;
    }
    if (!sysopen($syslogfh, $syslog_path, 0400, O_WRONLY)) {
	push @$errs, "stream can't open $syslog_path: $^OS_ERROR";
	return 0;
    }
    $syslog_send = \&_syslog_send_stream;
    return 1;
}

sub connect_pipe($errs) {

    $syslog_path ||= _PATH_LOG() || "/dev/log";

    if (not -w $syslog_path) {
        push @$errs, "$syslog_path is not writable";
        return 0;
    }

    if (not open($syslogfh, ">", "$syslog_path")) {
        push @$errs, "can't write to $syslog_path: $^OS_ERROR";
        return 0;
    }

    $syslog_send = \&_syslog_send_pipe;

    return 1;
}

sub connect_unix($errs) {

    $syslog_path ||= _PATH_LOG() if length _PATH_LOG();

    if (not defined $syslog_path) {
        push @$errs, "_PATH_LOG not available in syslog.h and no user-supplied socket path";
	return 0;
    }

    if (! -S $syslog_path) {
        push @$errs, "$syslog_path is not a socket";
	return 0;
    }

    my $addr = pack_sockaddr_un($syslog_path);
    if (!$addr) {
	push @$errs, "can't locate $syslog_path";
	return 0;
    }
    if (!socket($syslogfh, AF_UNIX, SOCK_STREAM, 0)) {
        push @$errs, "unix stream socket: $^OS_ERROR";
	return 0;
    }

    if (!connect($syslogfh, $addr)) {
        if (!socket($syslogfh, AF_UNIX, SOCK_DGRAM, 0)) {
	    push @$errs, "unix dgram socket: $^OS_ERROR";
	    return 0;
	}
        if (!connect($syslogfh, $addr)) {
	    push @$errs, "unix dgram connect: $^OS_ERROR";
	    return 0;
	}
    }

    $syslog_send = \&_syslog_send_socket;

    return 1;
}

sub connect_native($errs) {
    my $logopt = 0;

    # reconstruct the numeric equivalent of the options
    for my $opt (keys %options) {
        $logopt += xlate($opt) if %options{?$opt}
    }

    try { openlog_xs($ident, $logopt, xlate($facility)) };
    if ($^EVAL_ERROR) {
        push @$errs, $^EVAL_ERROR->message;
        return 0;
    }

    $syslog_send = \&_syslog_send_native;

    return 1;
}

sub connect_eventlog($errs) {

    $syslog_xobj = Sys::Syslog::Win32::_install();
    $syslog_send = \&Sys::Syslog::Win32::_syslog_send;

    return 1;
}

sub connect_console($errs) {
    if (!-w '/dev/console') {
	push @$errs, "console is not writable";
	return 0;
    }
    $syslog_send = \&_syslog_send_console;
    return 1;
}

# To test if the connection is still good, we need to check if any
# errors are present on the connection. The errors will not be raised
# by a write. Instead, sockets are made readable and the next read
# would cause the error to be returned. Unfortunately the syslog 
# 'protocol' never provides anything for us to read. But with 
# judicious use of select(), we can see if it would be readable...
sub connection_ok {
    return 1 if defined $current_proto and (
        $current_proto eq 'native' or $current_proto eq 'console'
        or $current_proto eq 'eventlog'
    );

    my $rin = '';
    vec($rin, fileno($syslogfh), 1, 1);
    my $ret = select $rin, undef, $rin, $sock_timeout;
    return $ret ?? 0 !! 1;
}

sub disconnect_log {
    $connected = 0;
    $syslog_send = undef;

    if (defined $current_proto and $current_proto eq 'native') {
        closelog_xs();
        return 1;
    }
    elsif (defined $current_proto and $current_proto eq 'eventlog') {
        $syslog_xobj->Close();
        return 1;
    }

    return close $syslogfh;
}

1;

__END__

=head1 NAME

Sys::Syslog - Perl interface to the UNIX syslog(3) calls

=head1 VERSION

Version 0.24

=head1 SYNOPSIS

    use Sys::Syslog;                          # all except setlogsock(), or:
    use Sys::Syslog qw(:DEFAULT setlogsock);  # default set, plus setlogsock()
    use Sys::Syslog qw(:standard :macros);    # standard functions, plus macros

    openlog $ident, $logopt, $facility;       # don't forget this
    syslog $priority, $format, @args;
    $oldmask = setlogmask $mask_priority;
    closelog;


=head1 DESCRIPTION

C<Sys::Syslog> is an interface to the UNIX C<syslog(3)> program.
Call C<syslog()> with a string priority and a list of C<printf()> args
just like C<syslog(3)>.

You can find a kind of FAQ in L<"THE RULES OF SYS::SYSLOG">.  Please read 
it before coding, and again before asking questions. 


=head1 EXPORTS

C<Sys::Syslog> exports the following C<Exporter> tags: 

=over 4

=item *

C<:standard> exports the standard C<syslog(3)> functions: 

    openlog closelog setlogmask syslog

=item *

C<:extended> exports the Perl specific functions for C<syslog(3)>: 

    setlogsock

=item *

C<:macros> exports the symbols corresponding to most of your C<syslog(3)> 
macros and the C<LOG_UPTO()> and C<LOG_MASK()> functions. 
See L<"CONSTANTS"> for the supported constants and their meaning. 

=back

By default, C<Sys::Syslog> exports the symbols from the C<:standard> tag. 


=head1 FUNCTIONS

=over 4

=item B<openlog($ident, $logopt, $facility)>

Opens the syslog.
C<$ident> is prepended to every message.  C<$logopt> contains zero or
more of the options detailed below.  C<$facility> specifies the part 
of the system to report about, for example C<LOG_USER> or C<LOG_LOCAL0>:
see L<"Facilities"> for a list of well-known facilities, and your 
C<syslog(3)> documentation for the facilities available in your system. 
Check L<"SEE ALSO"> for useful links. Facility can be given as a string 
or a numeric macro. 

This function will croak if it can't connect to the syslog daemon.

Note that C<openlog()> now takes three arguments, just like C<openlog(3)>.

B<You should use C<openlog()> before calling C<syslog()>.>

B<Options>

=over 4

=item *

C<cons> - This option is ignored, since the failover mechanism will drop 
down to the console automatically if all other media fail.

=item *

C<ndelay> - Open the connection immediately (normally, the connection is
opened when the first message is logged).

=item *

C<nofatal> - When set to true, C<openlog()> and C<syslog()> will only 
emit warnings instead of dying if the connection to the syslog can't 
be established. 

=item *

C<nowait> - Don't wait for child processes that may have been created 
while logging the message.  (The GNU C library does not create a child
process, so this option has no effect on Linux.)

=item *

C<pid> - Include PID with each message.

=back

B<Examples>

Open the syslog with options C<ndelay> and C<pid>, and with facility C<LOCAL0>: 

    openlog($name, "ndelay,pid", "local0");

Same thing, but this time using the macro corresponding to C<LOCAL0>: 

    openlog($name, "ndelay,pid", LOG_LOCAL0);


=item B<syslog($priority, $message)>

=item B<syslog($priority, $format, @args)>

If C<$priority> permits, logs C<$message> or C<sprintf($format, @args)>
with the addition that C<%m> in $message or C<$format> is replaced with
C<"$!"> (the latest error message). 

C<$priority> can specify a level, or a level and a facility.  Levels and 
facilities can be given as strings or as macros.  When using the C<eventlog>
mechanism, priorities C<DEBUG> and C<INFO> are mapped to event type 
C<informational>, C<NOTICE> and C<WARNIN> to C<warning> and C<ERR> to 
C<EMERG> to C<error>.

If you didn't use C<openlog()> before using C<syslog()>, C<syslog()> will 
try to guess the C<$ident> by extracting the shortest prefix of 
C<$format> that ends in a C<":">.

B<Examples>

    syslog("info", $message);           # informational level
    syslog(LOG_INFO, $message);         # informational level

    syslog("info|local0", $message);        # information level, Local0 facility
    syslog(LOG_INFO|LOG_LOCAL0, $message);  # information level, Local0 facility

=over 4

=item B<Note>

C<Sys::Syslog> version v0.07 and older passed the C<$message> as the 
formatting string to C<sprintf()> even when no formatting arguments
were provided.  If the code calling C<syslog()> might execute with 
older versions of this module, make sure to call the function as
C<syslog($priority, "%s", $message)> instead of C<syslog($priority,
$message)>.  This protects against hostile formatting sequences that
might show up if $message contains tainted data.

=back


=item B<setlogmask($mask_priority)>

Sets the log mask for the current process to C<$mask_priority> and 
returns the old mask.  If the mask argument is 0, the current log mask 
is not modified.  See L<"Levels"> for the list of available levels. 
You can use the C<LOG_UPTO()> function to allow all levels up to a 
given priority (but it only accept the numeric macros as arguments).

B<Examples>

Only log errors: 

    setlogmask( LOG_MASK(LOG_ERR) );

Log everything except informational messages: 

    setlogmask( ~(LOG_MASK(LOG_INFO)) );

Log critical messages, errors and warnings: 

    setlogmask( LOG_MASK(LOG_CRIT) | LOG_MASK(LOG_ERR) | LOG_MASK(LOG_WARNING) );

Log all messages up to debug: 

    setlogmask( LOG_UPTO(LOG_DEBUG) );


=item B<setlogsock($sock_type)>

=item B<setlogsock($sock_type, $stream_location)> (added in Perl 5.004_02)

Sets the socket type to be used for the next call to
C<openlog()> or C<syslog()> and returns true on success,
C<undef> on failure. The available mechanisms are: 

=over

=item *

C<"native"> - use the native C functions from your C<syslog(3)> library
(added in C<Sys::Syslog> 0.15).

=item *

C<"eventlog"> - send messages to the Win32 events logger (Win32 only; 
added in C<Sys::Syslog> 0.19).

=item *

C<"tcp"> - connect to a TCP socket, on the C<syslog/tcp> or C<syslogng/tcp> 
service. 

=item *

C<"udp"> - connect to a UDP socket, on the C<syslog/udp> service.

=item *

C<"inet"> - connect to an INET socket, either TCP or UDP, tried in that order. 

=item *

C<"unix"> - connect to a UNIX domain socket (in some systems a character 
special device).  The name of that socket is the second parameter or, if 
you omit the second parameter, the value returned by the C<_PATH_LOG> macro 
(if your system defines it), or F</dev/log> or F</dev/conslog>, whatever is 
writable.  

=item *

C<"stream"> - connect to the stream indicated by the pathname provided as 
the optional second parameter, or, if omitted, to F</dev/conslog>. 
For example Solaris and IRIX system may prefer C<"stream"> instead of C<"unix">. 

=item *

C<"pipe"> - connect to the named pipe indicated by the pathname provided as 
the optional second parameter, or, if omitted, to the value returned by 
the C<_PATH_LOG> macro (if your system defines it), or F</dev/log>
(added in C<Sys::Syslog> 0.21).

=item *

C<"console"> - send messages directly to the console, as for the C<"cons"> 
option of C<openlog()>.

=back

A reference to an array can also be passed as the first parameter.
When this calling method is used, the array should contain a list of
mechanisms which are attempted in order.

The default is to try C<native>, C<tcp>, C<udp>, C<unix>, C<stream>, C<console>.
Under Win32 systems, C<eventlog> will be added as the first mechanism to try 
if C<Win32::EventLog> is available.

Giving an invalid value for C<$sock_type> will C<croak>.

B<Examples>

Select the UDP socket mechanism: 

    setlogsock("udp");

Select the native, UDP socket then UNIX domain socket mechanisms: 

    setlogsock(["native", "udp", "unix"]);

=over

=item B<Note>

Now that the "native" mechanism is supported by C<Sys::Syslog> and selected 
by default, the use of the C<setlogsock()> function is discouraged because 
other mechanisms are less portable across operating systems.  Authors of 
modules and programs that use this function, especially its cargo-cult form 
C<setlogsock("unix")>, are advised to remove any occurence of it unless they 
specifically want to use a given mechanism (like TCP or UDP to connect to 
a remote host).

=back

=item B<closelog()>

Closes the log file and returns true on success.

=back


=head1 THE RULES OF SYS::SYSLOG

I<The First Rule of Sys::Syslog is:>
You do not call C<setlogsock>.

I<The Second Rule of Sys::Syslog is:>
You B<do not> call C<setlogsock>.

I<The Third Rule of Sys::Syslog is:>
The program crashes, C<die>s, calls C<closelog>, the log is over.

I<The Fourth Rule of Sys::Syslog is:>
One facility, one priority.

I<The Fifth Rule of Sys::Syslog is:>
One log at a time.

I<The Sixth Rule of Sys::Syslog is:>
No C<syslog> before C<openlog>.

I<The Seventh Rule of Sys::Syslog is:>
Logs will go on as long as they have to. 

I<The Eighth, and Final Rule of Sys::Syslog is:>
If this is your first use of Sys::Syslog, you must read the doc.


=head1 EXAMPLES

An example:

    openlog($program, 'cons,pid', 'user');
    syslog('info', '%s', 'this is another test');
    syslog('mail|warning', 'this is a better test: %d', time);
    closelog();

    syslog('debug', 'this is the last test');

Another example:

    openlog("$program $$", 'ndelay', 'user');
    syslog('notice', 'fooprogram: this is really done');

Example of use of C<%m>:

    $! = 55;
    syslog('info', 'problem was %m');   # %m == $! in syslog(3)

Log to UDP port on C<$remotehost> instead of logging locally:

    setlogsock('udp');
    $Sys::Syslog::host = $remotehost;
    openlog($program, 'ndelay', 'user');
    syslog('info', 'something happened over here');


=head1 CONSTANTS

=head2 Facilities

=over 4

=item *

C<LOG_AUDIT> - audit daemon (IRIX); falls back to C<LOG_AUTH>

=item *

C<LOG_AUTH> - security/authorization messages

=item *

C<LOG_AUTHPRIV> - security/authorization messages (private)

=item *

C<LOG_CONSOLE> - C</dev/console> output (FreeBSD); falls back to C<LOG_USER>

=item *

C<LOG_CRON> - clock daemons (B<cron> and B<at>)

=item *

C<LOG_DAEMON> - system daemons without separate facility value

=item *

C<LOG_FTP> - FTP daemon

=item *

C<LOG_KERN> - kernel messages

=item *

C<LOG_INSTALL> - installer subsystem (Mac OS X); falls back to C<LOG_USER>

=item *

C<LOG_LAUNCHD> - launchd - general bootstrap daemon (Mac OS X);
falls back to C<LOG_DAEMON>

=item *

C<LOG_LFMT> - logalert facility; falls back to C<LOG_USER>

=item *

C<LOG_LOCAL0> through C<LOG_LOCAL7> - reserved for local use

=item *

C<LOG_LPR> - line printer subsystem

=item *

C<LOG_MAIL> - mail subsystem

=item *

C<LOG_NETINFO> - NetInfo subsystem (Mac OS X); falls back to C<LOG_DAEMON>

=item *

C<LOG_NEWS> - USENET news subsystem

=item *

C<LOG_NTP> - NTP subsystem (FreeBSD, NetBSD); falls back to C<LOG_DAEMON>

=item *

C<LOG_RAS> - Remote Access Service (VPN / PPP) (Mac OS X);
falls back to C<LOG_AUTH>

=item *

C<LOG_REMOTEAUTH> - remote authentication/authorization (Mac OS X);
falls back to C<LOG_AUTH>

=item *

C<LOG_SECURITY> - security subsystems (firewalling, etc.) (FreeBSD);
falls back to C<LOG_AUTH>

=item *

C<LOG_SYSLOG> - messages generated internally by B<syslogd>

=item *

C<LOG_USER> (default) - generic user-level messages

=item *

C<LOG_UUCP> - UUCP subsystem

=back


=head2 Levels

=over 4

=item *

C<LOG_EMERG> - system is unusable

=item *

C<LOG_ALERT> - action must be taken immediately

=item *

C<LOG_CRIT> - critical conditions

=item *

C<LOG_ERR> - error conditions

=item *

C<LOG_WARNING> - warning conditions

=item *

C<LOG_NOTICE> - normal, but significant, condition

=item *

C<LOG_INFO> - informational message

=item *

C<LOG_DEBUG> - debug-level message

=back


=head1 DIAGNOSTICS

=over

=item C<Invalid argument passed to setlogsock>

B<(F)> You gave C<setlogsock()> an invalid value for C<$sock_type>. 

=item C<eventlog passed to setlogsock, but operating system isn't Win32-compatible>

B<(W)> You asked C<setlogsock()> to use the Win32 event logger but the 
operating system running the program isn't Win32 or does not provides Win32
facilities.

=item C<no connection to syslog available>

B<(F)> C<syslog()> failed to connect to the specified socket.

=item C<stream passed to setlogsock, but %s is not writable>

B<(W)> You asked C<setlogsock()> to use a stream socket, but the given 
path is not writable. 

=item C<stream passed to setlogsock, but could not find any device>

B<(W)> You asked C<setlogsock()> to use a stream socket, but didn't 
provide a path, and C<Sys::Syslog> was unable to find an appropriate one.

=item C<tcp passed to setlogsock, but tcp service unavailable>

B<(W)> You asked C<setlogsock()> to use a TCP socket, but the service 
is not available on the system. 

=item C<syslog: expecting argument %s>

B<(F)> You forgot to give C<syslog()> the indicated argument.

=item C<syslog: invalid level/facility: %s>

B<(F)> You specified an invalid level or facility.

=item C<syslog: too many levels given: %s>

B<(F)> You specified too many levels. 

=item C<syslog: too many facilities given: %s>

B<(F)> You specified too many facilities. 

=item C<syslog: level must be given>

B<(F)> You forgot to specify a level.

=item C<udp passed to setlogsock, but udp service unavailable>

B<(W)> You asked C<setlogsock()> to use a UDP socket, but the service 
is not available on the system. 

=item C<unix passed to setlogsock, but path not available>

B<(W)> You asked C<setlogsock()> to use a UNIX socket, but C<Sys::Syslog> 
was unable to find an appropriate an appropriate device.

=back


=head1 SEE ALSO

=head2 Manual Pages

L<syslog(3)>

SUSv3 issue 6, IEEE Std 1003.1, 2004 edition, 
L<http://www.opengroup.org/onlinepubs/000095399/basedefs/syslog.h.html>

GNU C Library documentation on syslog, 
L<http://www.gnu.org/software/libc/manual/html_node/Syslog.html>

Solaris 10 documentation on syslog, 
L<http://docs.sun.com/app/docs/doc/816-5168/6mbb3hruo?a=view>

IRIX 6.4 documentation on syslog,
L<http://techpubs.sgi.com/library/tpl/cgi-bin/getdoc.cgi?coll=0640&db=man&fname=3c+syslog>

AIX 5L 5.3 documentation on syslog, 
L<http://publib.boulder.ibm.com/infocenter/pseries/v5r3/index.jsp?topic=/com.ibm.aix.basetechref/doc/basetrf2/syslog.htm>

HP-UX 11i documentation on syslog, 
L<http://docs.hp.com/en/B9106-90010/syslog.3C.html>

Tru64 5.1 documentation on syslog, 
L<http://h30097.www3.hp.com/docs/base_doc/DOCUMENTATION/V51_HTML/MAN/MAN3/0193____.HTM>

Stratus VOS 15.1, 
L<http://stratadoc.stratus.com/vos/15.1.1/r502-01/wwhelp/wwhimpl/js/html/wwhelp.htm?context=r502-01&file=ch5r502-01bi.html>

=head2 RFCs

I<RFC 3164 - The BSD syslog Protocol>, L<http://www.faqs.org/rfcs/rfc3164.html>
-- Please note that this is an informational RFC, and therefore does not 
specify a standard of any kind.

I<RFC 3195 - Reliable Delivery for syslog>, L<http://www.faqs.org/rfcs/rfc3195.html>

=head2 Articles

I<Syslogging with Perl>, L<http://lexington.pm.org/meetings/022001.html>

=head2 Event Log

Windows Event Log,
L<http://msdn.microsoft.com/library/default.asp?url=/library/en-us/wes/wes/windows_event_log.asp>


=head1 AUTHORS & ACKNOWLEDGEMENTS

Tom Christiansen E<lt>F<tchrist (at) perl.com>E<gt> and Larry Wall
E<lt>F<larry (at) wall.org>E<gt>.

UNIX domain sockets added by Sean Robinson
E<lt>F<robinson_s (at) sc.maricopa.edu>E<gt> with support from Tim Bunce 
E<lt>F<Tim.Bunce (at) ig.co.uk>E<gt> and the C<perl5-porters> mailing list.

Dependency on F<syslog.ph> replaced with XS code by Tom Hughes
E<lt>F<tom (at) compton.nu>E<gt>.

Code for C<constant()>s regenerated by Nicholas Clark E<lt>F<nick (at) ccl4.org>E<gt>.

Failover to different communication modes by Nick Williams
E<lt>F<Nick.Williams (at) morganstanley.com>E<gt>.

Extracted from core distribution for publishing on the CPAN by 
SE<eacute>bastien Aperghis-Tramoni E<lt>sebastien (at) aperghis.netE<gt>.

XS code for using native C functions borrowed from C<L<Unix::Syslog>>, 
written by Marcus Harnisch E<lt>F<marcus.harnisch (at) gmx.net>E<gt>.

Yves Orton suggested and helped for making C<Sys::Syslog> use the native 
event logger under Win32 systems.

Jerry D. Hedden and Reini Urban provided greatly appreciated help to 
debug and polish C<Sys::Syslog> under Cygwin.


=head1 BUGS

Please report any bugs or feature requests to
C<bug-sys-syslog (at) rt.cpan.org>, or through the web interface at
L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=Sys-Syslog>.
I will be notified, and then you'll automatically be notified of progress on
your bug as I make changes.


=head1 SUPPORT

You can find documentation for this module with the perldoc command.

    perldoc Sys::Syslog

You can also look for information at:

=over 4

=item * AnnoCPAN: Annotated CPAN documentation

L<http://annocpan.org/dist/Sys-Syslog>

=item * CPAN Ratings

L<http://cpanratings.perl.org/d/Sys-Syslog>

=item * RT: CPAN's request tracker

L<http://rt.cpan.org/NoAuth/Bugs.html?Dist=Sys-Syslog>

=item * Search CPAN

L<http://search.cpan.org/dist/Sys-Syslog/>

=item * Kobes' CPAN Search

L<http://cpan.uwinnipeg.ca/dist/Sys-Syslog>

=item * Perl Documentation

L<http://perldoc.perl.org/Sys/Syslog.html>

=back


=head1 LICENSE

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=cut

=begin comment

Notes for the future maintainer (even if it's still me..)
- - - - - - - - - - - - - - - - - - - - - - - - - - - - -

Using Google Code Search, I search who on Earth was relying on $host being 
public. It found 5 hits: 

* First was inside Indigo Star Perl2exe documentation. Just an old version 
of Sys::Syslog. 


* One real hit was inside DalWeathDB, a weather related program. It simply 
does a 

    $Sys::Syslog::host = '127.0.0.1';

- L<http://www.gallistel.net/nparker/weather/code/>


* Two hits were in TPC, a fax server thingy. It does a 

    $Sys::Syslog::host = $TPC::LOGHOST;

but also has this strange piece of code:

    # work around perl5.003 bug
    sub Sys::Syslog::hostname {}

I don't know what bug the author referred to.

- L<http://www.tpc.int/>
- L<ftp://ftp.tpc.int/tpc/server/UNIX/>
- L<ftp://ftp-usa.tpc.int/pub/tpc/server/UNIX/>


* Last hit was in Filefix, which seems to be a FIDOnet mail program (!).
This one does not use $host, but has the following piece of code:

    sub Sys::Syslog::hostname
    {
        use Sys::Hostname;
        return hostname;
    }

I guess this was a more elaborate form of the previous bit, maybe because 
of a bug in Sys::Syslog back then?

- L<ftp://ftp.kiae.su/pub/unix/fido/>


Links
-----
II12021: SYSLOGD HOWTO TCPIPINFO (z/OS, OS/390, MVS)
- L<http://www-1.ibm.com/support/docview.wss?uid=isg1II12021>

Getting the most out of the Event Viewer
- L<http://www.codeproject.com/dotnet/evtvwr.asp?print=true>

Log events to the Windows NT Event Log with JNI
- L<http://www.javaworld.com/javaworld/jw-09-2001/jw-0928-ntmessages.html>

=end comment

