#!/usr/bin/perl -w
# $Id: man.t,v 1.12 2007-11-29 01:35:54 eagle Exp $
#
# man.t -- Additional specialized tests for Pod::Man.
#
# Copyright 2002, 2003, 2004, 2006, 2007 by Russ Allbery <rra@stanford.edu>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.

use TestInit;

BEGIN {
    $^OUTPUT_AUTOFLUSH = 1;
    print $^STDOUT, "1..43\n";
}

use Pod::Man;
use charnames ':full';
use utf8;

print $^STDOUT, "ok 1\n";

my $parser = Pod::Man->new or die "Cannot create parser\n";
my $n = 2;
while ( ~< *DATA) {
    next until $_ eq "###\n";

    my $input = "";
    while ( ~< *DATA) {
        no warnings 'utf8'; # No invalid unicode warnings.
        last if $_ eq "###\n";
        $input .= $_;
    }
    my $expected = '';
    while ( ~< *DATA) {
        last if $_ eq "###\n";
        $expected .= $_;
    }

    open (my $tmp, ">", 'tmp.pod') or die "Cannot create tmp.pod: $^OS_ERROR\n";

    # We have a test in ISO 8859-1 encoding.  Make sure that nothing strange
    # happens if Perl thinks the world is Unicode.  Wrap this in eval so that
    # older versions of Perl don't croak.
    no warnings 'utf8';
    print $tmp, $input;
    close $tmp;

    test_outtmp($expected, "latin1\nINPUT:\n$input");

    unlink('tmp.pod');

    open (my $tmp2, ">", 'tmp.pod') or die "Cannot create tmp.pod: $^OS_ERROR\n";
    print $tmp2, "\N{BOM}";
    print $tmp2, $input;
    close $tmp2;
    test_outtmp($expected, "UTF-8 BOM\nINPUT:\n$input");

    unlink('tmp.pod');
}

sub test_outtmp {
    my $expected = shift;
    my $msg = shift;
    open (my $out, ">", 'out.tmp') or die "Cannot create out.tmp: $^OS_ERROR\n";
    $parser->parse_from_file ('tmp.pod', $out);
    close $out;
    open ($out, "<", 'out.tmp') or die "Cannot open out.tmp: $^OS_ERROR\n";
    while ( ~< $out) { last if m/^\.nh/ }
    my $output;
    do {
        local $^INPUT_RECORD_SEPARATOR = undef;
        $output = ~< $out;
    };
    close $out;
    if ($output eq $expected) {
        print $^STDOUT, "ok $n\n";
    } else {
        print $^STDOUT, "not ok $n\n";
        print $^STDOUT, "$msg\nEXPECTED:\n$expected\nOUTPUT:\n$output\n";
    }
    $n++;
}

# Below the marker are bits of POD and corresponding expected nroff output.
# This is used to test specific features or problems with Pod::Man.  The input
# and output are separated by lines containing only ###.

__DATA__

###
=head1 NAME

gcc - GNU project C and C++ compiler

=head1 C++ NOTES

Other mentions of C++.
###
.SH "NAME"
gcc \- GNU project C and C++ compiler
.SH "\*(C+ NOTES"
.IX Header " NOTES"
Other mentions of \*(C+.
###

###
=head1 PERIODS

This C<.> should be quoted.
###
.SH "PERIODS"
.IX Header "PERIODS"
This \f(CW\*(C`.\*(C'\fR should be quoted.
###

###
=over 4

=item *

A bullet.

=item    *

Another bullet.

=item * Also a bullet.

=back
###
.IP "\(bu" 4
A bullet.
.IP "\(bu" 4
Another bullet.
.IP "\(bu" 4
Also a bullet.
###

###
=over 4

=item foo

Not a bullet.

=item *

Also not a bullet.

=back
###
.IP "foo" 4
.IX Item "foo"
Not a bullet.
.IP "*" 4
Also not a bullet.
###

###
=head1 ACCENTS

Beyoncé!  Beyoncé!  Beyoncé!!

    Beyoncé!  Beyoncé!
      Beyoncé!  Beyoncé!
        Beyoncé!  Beyoncé!

Older versions didn't convert Beyoncé in verbatim.
###
.SH "ACCENTS"
.IX Header "ACCENTS"
Beyonce\*'!  Beyonce\*'!  Beyonce\*'!!
.PP
.Vb 3
\&    Beyonce\*'!  Beyonce\*'!
\&      Beyonce\*'!  Beyonce\*'!
\&        Beyonce\*'!  Beyonce\*'!
.Ve
.PP
Older versions didn't convert Beyonce\*' in verbatim.
###

###
=over 4

=item 1. Not a number

=item 2. Spaced right

=back

=over 2

=item 1 Not a number

=item 2 Spaced right

=back
###
.IP "1. Not a number" 4
.IX Item "1. Not a number"
.PD 0
.IP "2. Spaced right" 4
.IX Item "2. Spaced right"
.IP "1 Not a number" 2
.IX Item "1 Not a number"
.IP "2 Spaced right" 2
.IX Item "2 Spaced right"
###

###
=over 4

=item Z<>*

Not bullet.

=back
###
.IP "*" 4
Not bullet.
###

###
=head1 SEQS

"=over ... Z<>=back"

"SE<lt>...E<gt>"

The quotes should be converted in the above to paired quotes.
###
.SH "SEQS"
.IX Header "SEQS"
\&\*(L"=over ... =back\*(R"
.PP
\&\*(L"S<...>\*(R"
.PP
The quotes should be converted in the above to paired quotes.
###

###
=head1 YEN

It cost me E<165>12345! That should be an X.
###
.SH "YEN"
.IX Header "YEN"
It cost me X12345! That should be an X.
###

###
=head1 agrave

Open E<agrave> la shell. Previous versions mapped it wrong.
###
.SH "agrave"
.IX Header "agrave"
Open a\*` la shell. Previous versions mapped it wrong.
###

###
=over

=item First level

Blah blah blah....

=over

=item *

Should be a bullet.

=back

=back
###
.IP "First level" 4
.IX Item "First level"
Blah blah blah....
.RS 4
.IP "\(bu" 4
Should be a bullet.
.RE
.RS 4
.RE
###

###
=over 4

=item 1. Check fonts in @CARP_NOT test.

=back
###
.ie n .IP "1. Check fonts in @CARP_NOT test." 4
.el .IP "1. Check fonts in \f(CW@CARP_NOT\fR test." 4
.IX Item "1. Check fonts in @CARP_NOT test."
###

###
=head1 LINK QUOTING

There should not be double quotes: L<C<< (?>pattern) >>>.
###
.SH "LINK QUOTING"
.IX Header "LINK QUOTING"
There should not be double quotes: \f(CW\*(C`(?>pattern)\*(C'\fR.
###

###
=head1 SE<lt>E<gt> MAGIC

Magic should be applied S<RISC OS> to that.
###
.SH "S<> MAGIC"
.IX Header "S<> MAGIC"
Magic should be applied \s-1RISC\s0\ \s-1OS\s0 to that.
###

###
=head1 MAGIC MONEY

These should be identical.

Bippity boppity boo "The
price is $Z<>100."

Bippity boppity boo "The
price is $100."
###
.SH "MAGIC MONEY"
.IX Header "MAGIC MONEY"
These should be identical.
.PP
Bippity boppity boo \*(L"The
price is \f(CW$100\fR.\*(R"
.PP
Bippity boppity boo \*(L"The
price is \f(CW$100\fR.\*(R"
###

###
=head1 NAME

"Stuff" (no guesswork)

=head2 THINGS

Oboy, is this C++ "fun" yet! (guesswork)
###
.SH "NAME"
"Stuff" (no guesswork)
.Sh "\s-1THINGS\s0"
.IX Subsection "THINGS"
Oboy, is this \*(C+ \*(L"fun\*(R" yet! (guesswork)
###

###
=head1 Newline C Quote Weirdness

Blorp C<'
''>. Yes.
###
.SH "Newline C Quote Weirdness"
.IX Header "Newline C Quote Weirdness"
Blorp \f(CW\*(Aq
\&\*(Aq\*(Aq\fR. Yes.
###

###
=head1 Soft Hypen Testing

sigE<shy>action
manuE<shy>script
JarkE<shy>ko HieE<shy>taE<shy>nieE<shy>mi

And again:

sigE<173>action
manuE<173>script
JarkE<173>ko HieE<173>taE<173>nieE<173>mi

And one more time:

sigE<0x00AD>action
manuE<0x00AD>script
JarkE<0x00AD>ko HieE<0x00AD>taE<0x00AD>nieE<0x00AD>mi
###
.SH "Soft Hypen Testing"
.IX Header "Soft Hypen Testing"
sig\%action
manu\%script
Jark\%ko Hie\%ta\%nie\%mi
.PP
And again:
.PP
sig\%action
manu\%script
Jark\%ko Hie\%ta\%nie\%mi
.PP
And one more time:
.PP
sig\%action
manu\%script
Jark\%ko Hie\%ta\%nie\%mi
###

###
=head1 XE<lt>E<gt> Whitespace

Blorpy L<B<prok>|blap> X<bivav> wugga chachacha.
###
.SH "X<> Whitespace"
.IX Header "X<> Whitespace"
Blorpy \fBprok\fR  wugga chachacha.
.IX Xref "bivav"
###

###
=head1 Hyphen in SE<lt>E<gt>

Don't S<transform even-this hyphen>.  This "one's-fine!", as well.  However,
$-0.13 should have a real hyphen.
###
.SH "Hyphen in S<>"
.IX Header "Hyphen in S<>"
Don't transform\ even-this\ hyphen.  This \*(L"one's-fine!\*(R", as well.  However,
$\-0.13 should have a real hyphen.
###

###
=head1 Quote escaping

Don't escape `this' but do escape C<`this'> (and don't surround it in quotes).
###
.SH "Quote escaping"
.IX Header "Quote escaping"
Don't escape `this' but do escape \f(CW\`this\*(Aq\fR (and don't surround it in quotes).
###
