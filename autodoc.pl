#!/usr/bin/perl -w

                # we build the new one

BEGIN {
  push @INC, 'lib';
  require 'regen_lib.pl';
}

use strict;

#
# See database of global and static function prototypes in embed.fnc
# This is used to generate prototype headers under various configurations,
# export symbols lists for different platforms, and macros to provide an
# implicit interpreter context argument.
#

open IN, "<", "embed.fnc" or die $!;

# walk table providing an array of components in each line to
# subroutine, printing the result
sub walk_table (&@) {
    my $function = shift;
    my $filename = shift || '-';
    my $leader = shift;
    my $trailer = shift;
    my $F;
    local *F;
    if (ref $filename) {	# filehandle
	$F = $filename;
    }
    else {
	safer_unlink $filename;
	$F = safer_open($filename);
	binmode F;
	$F = \*F;
    }
    print $F $leader if $leader;
    seek IN, 0, 0;		# so we may restart
    while ( ~< *IN) {
	chomp;
	next if m/^:/;
	while (s|\\\s*$||) {
	    $_ .= ~< *IN;
	    chomp;
	}
	s/\s+$//;
	my @args;
	if (m/^\s*(#|$)/) {
	    @args = @( $_ );
	}
	else {
	    @args = @( split m/\s*\|\s*/, $_ );
	}
	s/\b(NN|NULLOK)\b\s+//g for < @args;
	print $F < $function->(< @args);
    }
    print $F $trailer if $trailer;
    unless (ref $filename) {
	close $F or die "Error closing $filename: $!";
    }
}

my %apidocs;
my %gutsdocs;
my %docfuncs;
my %seenfuncs;

my $curheader = "Unknown section";

sub autodoc ($$) { # parse a file and extract documentation info
    my($fh,$file) = < @_;
    my($in, $doc, $line);
FUNC:
    while (defined($in = ~< $fh)) {
        if ($in=~ m/^=head1 (.*)/) {
            $curheader = $1;
            next FUNC;
        }
	$line++;
	if ($in =~ m/^=for\s+apidoc\s+(.*?)\s*\n/) {
	    my $proto = $1;
	    $proto = "||$proto" unless $proto =~ m/\|/;
	    my($flags, $ret, $name, < @args) = split m/\|/, $proto;
	    my $docs = "";
DOC:
	    while (defined($doc = ~< $fh)) {
		$line++;
		last DOC if $doc =~ m/^=\w+/;
		if ($doc =~ m:^\*/$:) {
		    warn "=cut missing? $file:$line:$doc";;
		    last DOC;
		}
		$docs .= $doc;
	    }
	    $docs = "\n$docs" if $docs and $docs !~ m/^\n/;
	    if ($flags =~ m/m/) {
		if ($flags =~ m/A/) {
		    %apidocs{$curheader}{$name} = \@($flags, $docs, $ret, $file, < @args);
		}
		else {
		    %gutsdocs{$curheader}{$name} = \@($flags, $docs, $ret, $file, < @args);
		}
	    }
	    else {
		%docfuncs{$name} = \@($flags, $docs, $ret, $file, $curheader, < @args);
	    }
	    if (defined $doc) {
		if ($doc =~ m/^=(?:for|head)/) {
		    $in = $doc;
		    redo FUNC;
		}
	    } else {
		warn "$file:$line:$in";
	    }
	}
    }
}

sub docout ($$$) { # output the docs for one function
    my($fh, $name, $docref) = < @_;
    my($flags, $docs, $ret, $file, < @args) = < @$docref;
    $name =~ s/\s*$//;

    $docs .= "NOTE: this function is experimental and may change or be
removed without notice.\n\n" if $flags =~ m/x/;
    $docs .= "NOTE: the perl_ form of this function is deprecated.\n\n"
	if $flags =~ m/p/;

    print $fh "=item $name\nX<$name>\n$docs";

    if ($flags =~ m/U/) { # no usage
	# nothing
    } elsif ($flags =~ m/s/) { # semicolon ("dTHR;")
	print $fh "\t\t$name;\n\n";
    } elsif ($flags =~ m/n/) { # no args
	print $fh "\t$ret\t$name\n\n";
    } else { # full usage
	print $fh "\t$ret\t$name";
	print $fh "(" . join(", ", < @args) . ")";
	print $fh "\n\n";
    }
    print $fh "=for hackers\nFound in file $file\n\n";
}

sub readonly_header (*) {
    my $fh = shift;
    print $fh <<"_EOH_";
-*- buffer-read-only: t -*-

!!!!!!!   DO NOT EDIT THIS FILE   !!!!!!!
This file is built by $0 extracting documentation from the C source
files.

_EOH_
}

sub readonly_footer (*) {
    my $fh = shift;
    print $fh <<'_EOF_';
=cut

 ex: set ro:
_EOF_
}

my $file;
# glob() picks up docs from extra .c or .h files that may be in unclean
# development trees.
my $MANIFEST = do {
  local ($/, *FH);
  open FH, "<", "MANIFEST" or die "Can't open MANIFEST: $!";
  ~< *FH;
};

for $file (($MANIFEST =~ m/^(\S+\.c)\t/gm), ($MANIFEST =~ m/^(\S+\.h)\t/gm)) {
    open F, "<", $file or die "Cannot open $file for docs: $!\n";
    $curheader = "Functions in file $file\n";
    autodoc(\*F,$file);
    close F or die "Error closing $file: $!\n";
}

safer_unlink "pod/perlapi.pod";
my $doc = safer_open("pod/perlapi.pod");

walk_table {	# load documented functions into appropriate hash
    if ((nelems @_) +> 1) {
	my($flags, $retval, $func, < @args) = < @_;
	return "" unless $flags =~ m/d/;
	$func =~ s/\t//g; $flags =~ s/p//; # clean up fields from embed.pl
	$retval =~ s/\t//;
	my $docref = delete %docfuncs{$func};
	%seenfuncs{$func} = 1;
	if ($docref and nelems @$docref) {
	    if ($flags =~ m/A/) {
		$docref->[0].="x" if $flags =~ m/M/;
		%apidocs{$docref->[4]}{$func} =
		    \@($docref->[0] . 'A', $docref->[1], $retval, $docref->[3],
			< @args);
	    } else {
		%gutsdocs{$docref->[4]}{$func} =
		    \@($docref->[0], $docref->[1], $retval, $docref->[3], < @args);
	    }
	}
	else {
	    warn "no docs for $func\n" unless %seenfuncs{$func};
	}
    }
    return "";
} $doc;

for (sort keys %docfuncs) {
    # Have you used a full for apidoc or just a func name?
    # Have you used Ap instead of Am in the for apidoc?
    warn "Unable to place $_!\n";
}

readonly_header($doc);

print $doc <<'_EOB_';
=head1 NAME

perlapi - autogenerated documentation for the perl public API

=head1 DESCRIPTION
X<Perl API> X<API> X<api>

This file contains the documentation of the perl public API generated by
embed.pl, specifically a listing of functions, macros, flags, and variables
that may be used by extension writers.  The interfaces of any functions that
are not listed here are subject to change without notice.  For this reason,
blindly using functions listed in proto.h is to be avoided when writing
extensions.

Note that all Perl API global variables must be referenced with the C<PL_>
prefix.  Some macros are provided for compatibility with the older,
unadorned names, but this support may be disabled in a future release.

The listing is alphabetical, case insensitive.

_EOB_

my $key;
# case insensitive sort, with fallback for determinacy
for $key (sort { uc($a) cmp uc($b) || $a cmp $b } keys %apidocs) {
    my $section = %apidocs{$key}; 
    print $doc "\n=head1 $key\n\n=over 8\n\n";
    # Again, fallback for determinacy
    for my $key (sort { uc($a) cmp uc($b) || $a cmp $b } keys %$section) {
        docout($doc, $key, $section->{$key});
    }
    print $doc "\n=back\n";
}

print $doc <<'_EOE_';

=head1 AUTHORS

Until May 1997, this document was maintained by Jeff Okamoto
<okamoto@corp.hp.com>.  It is now maintained as part of Perl itself.

With lots of help and suggestions from Dean Roehrich, Malcolm Beattie,
Andreas Koenig, Paul Hudson, Ilya Zakharevich, Paul Marquess, Neil
Bowers, Matthew Green, Tim Bunce, Spider Boardman, Ulrich Pfeifer,
Stephen McCamant, and Gurusamy Sarathy.

API Listing originally by Dean Roehrich <roehrich@cray.com>.

Updated to be autogenerated from comments in the source by Benjamin Stuhl.

=head1 SEE ALSO

perlguts(1), perlxs(1), perlxstut(1), perlintern(1)

_EOE_

readonly_footer($doc);

safer_close($doc);

safer_unlink "pod/perlintern.pod";
my $guts = safer_open("pod/perlintern.pod");
readonly_header($guts);
print $guts <<'END';
=head1 NAME

perlintern - autogenerated documentation of purely B<internal>
		 Perl functions

=head1 DESCRIPTION
X<internal Perl functions> X<interpreter functions>

This file is the autogenerated documentation of functions in the
Perl interpreter that are documented using Perl's internal documentation
format but are not marked as part of the Perl API. In other words,
B<they are not for use in extensions>!

END

for $key (sort { uc($a) cmp uc($b); } keys %gutsdocs) {
    my $section = %gutsdocs{$key}; 
    print $guts "\n=head1 $key\n\n=over 8\n\n";
    for my $key (sort { uc($a) cmp uc($b); } keys %$section) {
        docout($guts, $key, $section->{$key});
    }
    print $guts "\n=back\n";
}

print $guts <<'END';

=head1 AUTHORS

The autodocumentation system was originally added to the Perl core by
Benjamin Stuhl. Documentation is by whoever was kind enough to
document their functions.

=head1 SEE ALSO

perlguts(1), perlapi(1)

END
readonly_footer($guts);

safer_close($guts);
