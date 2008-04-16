#!/usr/bin/perl -w
# $Id: basic.t,v 1.11 2006-09-16 20:25:25 eagle Exp $
#
# basic.t -- Basic tests for podlators.
#
# Copyright 2001, 2002, 2004, 2006 by Russ Allbery <rra@stanford.edu>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.

BEGIN {
    chdir 't' if -d 't';
    if (%ENV{PERL_CORE}) {
        @INC = '../lib';
    } else {
        unshift (@INC, '../blib/lib');
    }
    unshift (@INC, '../blib/lib');
    $| = 1;
    print "1..11\n";
}

END {
    print "not ok 1\n" unless $loaded;
}

use bytes;

use Pod::Man;
use Pod::Text;
use Pod::Text::Overstrike;
use Pod::Text::Termcap;

# Find the path to the test source files.  This requires some fiddling when
# these tests are run as part of Perl core.
sub source_path {
    my $file = shift;
    if (%ENV{PERL_CORE}) {
        require File::Spec;
        my $updir = File::Spec->updir;
        my $dir = File::Spec->catdir ($updir, 'lib', 'Pod', 't');
        return File::Spec->catfile ($dir, $file);
    } else {
        return $file;
    }
}

$loaded = 1;
print "ok 1\n";

# Hard-code a few values to try to get reproducible results.
%ENV{COLUMNS} = 80;
%ENV{TERM} = 'xterm';
%ENV{TERMCAP} = 'xterm:co=80:do=^J:md=\E[1m:us=\E[4m:me=\E[m';

# Map of translators to file extensions to find the formatted output to
# compare against.
my %translators = ('Pod::Man'              => 'man',
                   'Pod::Text'             => 'txt',
                   'Pod::Text::Color'      => 'clr',
                   'Pod::Text::Overstrike' => 'ovr',
                   'Pod::Text::Termcap'    => 'cap');

# Set default options to match those of pod2man and pod2text.
%options = (sentence => 0);

my $n = 2;
for (sort keys %translators) {
    if ($_ eq 'Pod::Text::Color') {
        eval { require Term::ANSIColor };
        if ($@) {
            print "ok $n # skip\n";
            $n++;
            print "ok $n # skip\n";
            $n++;
            next;
        }
        require Pod::Text::Color;
    }
    my $parser = $_->new (%options);
    print (($parser && ref ($parser) eq $_) ? "ok $n\n" : "not ok $n\n");
    $n++;

    # For Pod::Man, strip out the autogenerated header up to the .TH title
    # line.  That means that we don't check those things; oh well.  The header
    # changes with each version change or touch of the input file.
    open (OUT, ">", 'out.tmp') or die "Cannot create out.tmp: $!\n";
    $parser->parse_from_file (source_path ('basic.pod'), \*OUT);
    close OUT;
    if ($_ eq 'Pod::Man') {
        open (TMP, "<", 'out.tmp') or die "Cannot open out.tmp: $!\n";
        open (OUTPUT, ">", "out.%translators{$_}")
            or die "Cannot create out.%translators{$_}: $!\n";
        local $_;
        while ( ~< *TMP) { last if m/^\.nh/ }
        print OUTPUT while ~< *TMP;
        close OUTPUT;
        close TMP;
        unlink 'out.tmp';
    } else {
        rename ('out.tmp', "out.%translators{$_}")
            or die "Cannot rename out.tmp: $!\n";
    }
    {
        local $/;
        open (MASTER, "<", source_path ("basic.%translators{$_}"))
            or die "Cannot open basic.%translators{$_}: $!\n";
        open (OUTPUT, "<", "out.%translators{$_}")
            or die "Cannot open out.%translators{$_}: $!\n";
        my $master = ~< *MASTER;
        my $output = ~< *OUTPUT;
        close MASTER;
        close OUTPUT;

        # OS/390 is EBCDIC, which uses a different character for ESC
        # apparently.  Try to convert so that the test still works.
        if ($^O eq 'os390' && $_ eq 'Pod::Text::Termcap') {
            $output =~ tr/\033/\047/;
        }

        if ($master eq $output) {
            print "ok $n\n";
            unlink "out.%translators{$_}";
        } else {
            print "not ok $n\n";
            print "# Non-matching output left in out.%translators{$_}\n";
        }
    }
    $n++;
}
