#!/usr/bin/perl -w
# $Id: basic.t,v 1.11 2006-09-16 20:25:25 eagle Exp $
#
# basic.t -- Basic tests for podlators.
#
# Copyright 2001, 2002, 2004, 2006 by Russ Allbery <rra@stanford.edu>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.

use TestInit;

BEGIN {
    $^OUTPUT_AUTOFLUSH = 1;
    print $^STDOUT, "1..11\n";
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
    if (env::var('PERL_CORE')) {
        require File::Spec;
        my $updir = File::Spec->updir;
        my $dir = File::Spec->catdir ($updir, 'lib', 'Pod', 't');
        return File::Spec->catfile ($dir, $file);
    } else {
        return $file;
    }
}

print $^STDOUT, "ok 1\n";

# Hard-code a few values to try to get reproducible results.
env::var('COLUMNS' ) = 80;
env::var('TERM' ) = 'xterm';
env::var('TERMCAP' ) = 'xterm:co=80:do=^J:md=\E[1m:us=\E[4m:me=\E[m';

# Map of translators to file extensions to find the formatted output to
# compare against.
my %translators = %('Pod::Man'              => 'man',
                   'Pod::Text'             => 'txt',
                   'Pod::Text::Color'      => 'clr',
                   'Pod::Text::Overstrike' => 'ovr',
                   'Pod::Text::Termcap'    => 'cap');

# Set default options to match those of pod2man and pod2text.
our %options = %(sentence => 0);

my $n = 2;
for my $translator (sort keys %translators) {
    if ($translator eq 'Pod::Text::Color') {
        try { require Term::ANSIColor };
        if ($^EVAL_ERROR) {
            print $^STDOUT, "ok $n # skip\n";
            $n++;
            print $^STDOUT, "ok $n # skip\n";
            $n++;
            next;
        }
        require Pod::Text::Color;
    }
    my $parser = $translator->new (< %options);
    print ($^STDOUT, ($parser && ref ($parser) eq $translator) ?? "ok $n\n" !! "not ok $n\n");
    $n++;

    # For Pod::Man, strip out the autogenerated header up to the .TH title
    # line.  That means that we don't check those things; oh well.  The header
    # changes with each version change or touch of the input file.
    open (my $out, ">", 'out.tmp') or die "Cannot create out.tmp: $^OS_ERROR\n";
    $parser->parse_from_file ( source_path ('basic.pod'), $out);
    close $out;
    if ($translator eq 'Pod::Man') {
        open (my $tmp, "<", 'out.tmp') or die "Cannot open out.tmp: $^OS_ERROR\n";
        open (my $output_fh, ">", "out.%translators{?$translator}")
            or die "Cannot create out.%translators{?$translator}: $^OS_ERROR\n";
        local $_ = undef;
        while ( ~< $tmp) { last if m/^\.nh/ }
        print $output_fh, $_ while ~< $tmp;
        close $output_fh;
        close $tmp;
        unlink 'out.tmp';
    } else {
        rename ('out.tmp', "out.%translators{?$translator}")
            or die "Cannot rename out.tmp: $^OS_ERROR\n";
    }
    do {
        local $^INPUT_RECORD_SEPARATOR = undef;
        open (my $master_fh, "<", source_path ("basic.%translators{?$translator}"))
            or die "Cannot open basic.%translators{?$translator}: $^OS_ERROR\n";
        open (my $output_fh, "<", "out.%translators{?$translator}")
            or die "Cannot open out.%translators{?$translator}: $^OS_ERROR\n";
        my $master = ~< $master_fh;
        my $output = ~< $output_fh;
        close $master_fh;
        close $output_fh;

        # OS/390 is EBCDIC, which uses a different character for ESC
        # apparently.  Try to convert so that the test still works.
        if ($^OS_NAME eq 'os390' && $_ eq 'Pod::Text::Termcap') {
            $output =~ s/\033/\047/g;
        }

        if ($master eq $output) {
            print $^STDOUT, "ok $n\n";
            unlink "out.%translators{?$translator}";
        } else {
            print $^STDOUT, "not ok $n\n";
            print $^STDOUT, "# Non-matching output left in out.%translators{?$translator}\n";
        }
    };
    $n++;
}
