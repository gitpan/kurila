#!./perl

BEGIN {
    require './test.pl';
}

try {my @n = @( getgrgid 0 )};
if ($^EVAL_ERROR and $^EVAL_ERROR->{?description} =~ m/(The \w+ function is unimplemented)/) {
    skip_all "getgrgid unimplemented";
}

our (%Config, $where);
try { require Config; Config->import; };
my $reason;
if (%Config{?'i_grp'} ne 'define') {
	$reason = '%Config{i_grp} not defined';
}
elsif (not -f "/etc/group" ) { # Play safe.
	$reason = 'no /etc/group file';
}

if (not defined $where) {	# Try NIS.
    foreach my $ypcat (qw(/usr/bin/ypcat /bin/ypcat /etc/ypcat)) {
        my $gr;
        if (-x $ypcat &&
            open($gr, "$ypcat group 2>/dev/null |") &&
            defined( ~< $gr)) 
        {
            print $^STDOUT, "# `ypcat group` worked\n";

            # Check to make sure we're really using NIS.
            if( open(my $nssw, "<", "/etc/nsswitch.conf" ) ) {
                my@($group) =  grep { m/^\s*group:/ }, @( ~< $nssw);

                # If there's no group line, assume it default to compat.
                if( !$group || $group !~ m/(nis|compat)/ ) {
                    print $^STDOUT, "# Doesn't look like you're using NIS in ".
                          "/etc/nsswitch.conf\n";
                    last;
                }
            }
            $where = "NIS group - $ypcat";
            undef $reason;
            last;
        }
    }
}

if (not defined $where) {	# Try NetInfo.
    foreach my $nidump (qw(/usr/bin/nidump)) {
        my $gr;
        if (-x $nidump &&
            open($gr, "$nidump group . 2>/dev/null |") &&
            defined( ~< $gr)) 
        {
            $where = "NetInfo group - $nidump";
            undef $reason;
            last;
        }
    }
}

if (not defined $where) {	# Try local.
    my $GR = "/etc/group";
    my $gr_fh;
    if (-f $GR && open($gr_fh, "<", $GR) && defined( ~< $gr_fh)) {
        undef $reason;
        $where = "local $GR";
    }
}

if ($reason) {
    skip_all $reason;
}


# By now the GR filehandle should be open and full of juicy group entries.

plan tests => 3;

# Go through at most this many groups.
# (note that the first entry has been read away by now)
my $max = 25;

my $n   = 0;
my $tst = 1;
my %perfect;
my %seen;

print $^STDOUT, "# where $where\n";

ok( setgrent(), 'setgrent' ) || print $^STDOUT, "# $^OS_ERROR\n";

while ( ~< *GR) {
    chomp;
    # LIMIT -1 so that groups with no users don't fall off
    my @s = split m/:/, $_, -1;
    my @($name_s,$passwd_s,$gid_s,$members_s) =  @s;
    if ((nelems @s)) {
	push @{ %seen{+$name_s} }, iohandle::input_line_number(\*GR);
    } else {
	warn "# Your $where line $(iohandle::input_line_number(\*GR)) is empty.\n";
	next;
    }
    if ($n == $max) {
	local $^INPUT_RECORD_SEPARATOR = undef;
	my $junk = ~< *GR;
	last;
    }
    # In principle we could whine if @s != 4 but do we know enough
    # of group file formats everywhere?
    if ((nelems @s) == 4) {
	$members_s =~ s/\s*,\s*/,/g;
	$members_s =~ s/\s+$//;
	$members_s =~ s/^\s+//;
	my @n = @( getgrgid($gid_s) );
	# 'nogroup' et al.
	next unless (nelems @n);
	my @($name,$passwd,$gid,$members) =  @n;
	# Protect against one-to-many and many-to-one mappings.
	if ($name_s ne $name) {
	    @n = @( getgrnam($name_s) );
	    @($name,$passwd,$gid,$members) =  @n;
	    next if $name_s ne $name;
	}
	# NOTE: group names *CAN* contain whitespace.
	$members =~ s/\s+/,/g;
	# what about different orders of members?
	%perfect{+$name_s}++
	    if $name    eq $name_s    and
# Do not compare passwords: think shadow passwords.
# Not that group passwords are used much but better not assume anything.
               $gid     eq $gid_s     and
               $members eq $members_s;
    }
    $n++;
}

endgrent();

print $^STDOUT, "# max = $max, n = $n, perfect = ", nkeys %perfect, "\n";

if (nkeys %perfect == 0 && $n) {
    $max++;
    print $^STDOUT, <<EOEX;
#
# The failure of op/grent test is not necessarily serious.
# It may fail due to local group administration conventions.
# If you are for example using both NIS and local groups,
# test failure is possible.  Any distributed group scheme
# can cause such failures.
#
# What the grent test is doing is that it compares the $max first
# entries of $where
# with the results of getgrgid() and getgrnam() call.  If it finds no
# matches at all, it suspects something is wrong.
# 
EOEX

    fail();
    print $^STDOUT, "#\t (not necessarily serious: run t/op/grent.t by itself)\n";
} else {
    pass();
}

# Test both the scalar and list contexts.

my @gr1;

setgrent();
for (1..$max) {
    my $gr = scalar getgrent();
    last unless defined $gr;
    push @gr1, $gr;
}
endgrent();

my @gr2;

setgrent();
for (1..$max) {
    my @($gr, ...) = @(getgrent());
    last unless defined $gr;
    push @gr2, $gr;
}
endgrent();

is("$(join ' ',@gr1)", "$(join ' ',@gr2)");
