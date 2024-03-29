#!./perl

# Tests sprintf, excluding handling of 64-bit integers or long
# doubles (if supported), of machine-specific short and long
# integers, machine-specific floating point exceptions (infinity,
# not-a-number ...), of the effects of locale, and of features
# specific to multi-byte characters (under the utf8 pragma and such).

use warnings;
use version;
use Config;


my @tests = @( () );
my ($i, $template, $data, $result, $comment, $w, $x, $evalData, $n, $p);

my $Is_VMS_VAX = 0;
# We use HW_MODEL since ARCH_NAME was not in VMS V5.*
if ($^OS_NAME eq 'VMS') {
    my $hw_model;
    chomp($hw_model = `write sys\$output f\$getsyi("HW_MODEL")`);
    $Is_VMS_VAX = $hw_model +< 1024 ?? 1 !! 0;
}

# No %Config.
my $Is_Ultrix_VAX = $^OS_NAME eq 'ultrix' && `uname -m` =~ m/^VAX$/;

while ( ~< *DATA) {
    s/^\s*>//; s/<\s*$//;
    @($template, $data, $result, ?$comment) =  split(m/<\s*>/, $_, 4);
    if ($^OS_NAME eq 'os390' || $^OS_NAME eq 's390') { # non-IEEE (s390 is UTS)
        $data   =~ s/([eE])96$/$163/;      # smaller exponents
        $result =~ s/([eE]\+)102$/$169/;   #  "       "
        $data   =~ s/([eE])\-101$/$1-56/;  # larger exponents
        $result =~ s/([eE])\-102$/$1-57/;  #  "       "
    }
    if ($Is_VMS_VAX || $Is_Ultrix_VAX) {
	# VAX DEC C 5.3 at least since there is no
	# ccflags =~ m/float=ieee/ on VAX.
	# AXP is unaffected whether or not it's using ieee.
        $data   =~ s/([eE])96$/$126/;      # smaller exponents
        $result =~ s/([eE]\+)102$/$132/;   #  "       "
        $data   =~ s/([eE])\-101$/$1-24/;  # larger exponents
        $result =~ s/([eE])\-102$/$1-25/;  #  "       "
    }

    $evalData = eval $data;
    die if $^EVAL_ERROR;
    $evalData = ref $evalData ?? $evalData !! \@($evalData);
    push @tests, \@($template, $evalData, $result, $comment, $data);
}

print $^STDOUT, '1..', scalar nelems @tests, "\n";

$^WARN_HOOK = sub {
    if (@_[0]->{?description} =~ m/^Invalid conversion/) {
	$w = ' INVALID';
    } elsif (@_[0]->{?description}=~ m/^Use of uninitialized value/) {
	$w = ' UNINIT';
    } else {
	warn @_[0]->{?description};
    }
};

for my  $i (1 .. nelems(@tests)) {
    @($template, $evalData, $result, $comment, $data) =  @{shift @tests};
    $w = undef;
    $x = sprintf(">$template<", < @$evalData);
    substr($x, -1, 0, $w) if $w;
    # $x may have 3 exponent digits, not 2
    my $y = $x;
    if ($y =~ s/([Ee][-+])0(\d)/$1$2/) {
        # if result is left-adjusted, append extra space
        if ($template =~ m/%\+?\-/ and $result =~ m/ $/) {
	    $y =~ s/<$/ </;
	}
        # if result is zero-filled, add extra zero
	elsif ($template =~ m/%\+?0/ and $result =~ m/^0/) {
	    $y =~ s/^>0/>00/;
	}
        # if result is right-adjusted, prepend extra space
	elsif ($result =~ m/^ /) {
	    $y =~ s/^>/> /;
	}
    }

    my $skip = 0;
    if ($comment =~ s/\s+skip:\s*(.*)//) {
	my $os  = $1;
	my $osv = config_value('osvers');
	# >comment skip: all<
	if ($os =~ m/\ball\b/i) {
	    $skip = 1;
	# >comment skip: VMS hpux:10.20<
	} elsif ($os =~ m/\b$^OS_NAME(?::(\S+))?\b/i) {
	    my $vsn = defined $1 ?? $1 !! "0";
	    # Only compare on the the first pair of digits, as numeric
	    # compares don't like 2.6.10-3mdksmp or 2.6.8-24.10-default
	    s/^(\d+(\.\d+)?).*/$1/ for @( $osv, $vsn);
	    $skip = $vsn ?? ($osv +<= $vsn ?? 1 !! 0) !! 1;
	}
	$skip and $comment =~ s/$/, failure expected on $^OS_NAME $osv/;
    }

    if ($x eq ">$result<") {
        print $^STDOUT, "ok $i\n";
    }
    elsif ($skip) {
	print $^STDOUT, "ok $i # skip $comment\n";
    }
    elsif ($y eq ">$result<")	# Some C libraries always give
    {				# three-digit exponent
		print($^STDOUT, "ok $i # >$result< $x three-digit exponent accepted\n");
    }
	elsif ($result =~ m/[-+]\d{3}$/ &&
		   # Suppress tests with modulo of exponent >= 100 on platforms
		   # which can't handle such magnitudes (or where we can't tell).
		   ((!try {require POSIX}) || # Costly: only do this if we must!
			(length(&POSIX::DBL_MAX( < @_ )) - rindex(&POSIX::DBL_MAX( < @_ ), '+')) == 3))
	{
		print($^STDOUT, "ok $i # >$template< >$data< >$result<",
			  " Suppressed: exponent out of range?\n");
	}
    else {
	$y = ($x eq $y ?? "" !! " => $y");
	print($^STDOUT, "not ok $i >$template< >$data< >$result< $x$y",
	    $comment ?? " # $comment\n" !! "\n");
    }
}

# In each of the following lines, there are three required fields:
# printf template, data to be formatted (as a Perl expression), and
# expected result of formatting.  An optional fourth field can contain
# a comment.  Each field is delimited by a starting '>' and a
# finishing '<'; any whitespace outside these start and end marks is
# not part of the field.  If formatting requires more than one data
# item (for example, if variable field widths are used), the Perl data
# expression should return a reference to an array having the requisite
# number of elements.  Even so, subterfuge is sometimes required: see
# tests for %n and %p.
#
# Tests that are expected to fail on a certain OS can be marked as such
# by trailing the comment with a skip: section. Skips are tags separated
# bu space consisting of a $^O optionally trailed with :osvers. In the
# latter case, all os-levels below that are expected to fail. A special
# tag 'all' is allowed for todo tests that should fail on any system
#
# >%G<   >1234567e96<  >1.23457E+102<   >exponent too big skip: os390<
# >%.0g< >-0.0<        >-0<             >No minus skip: MSWin32 VMS hpux:10.20<
# >%d<   >4<           >1<              >4 != 1 skip: all<
#
# The following tests are not currently run, for the reasons stated:

=pod

=begin problematic

>%.0f<      >1.5<         >2<   >Standard vague: no rounding rules<
>%.0f<      >2.5<         >2<   >Standard vague: no rounding rules<

=end problematic

=cut

# template    data          result
__END__
>%6. 6s<    >''<          >%6. 6s INVALID< >(See use of $w in code above)<
>%6 .6s<    >''<          >%6 .6s INVALID<
>%6.6 s<    >''<          >%6.6 s INVALID<
>%A<        >''<          >%A INVALID<
>%B<        >2**32-1<     >11111111111111111111111111111111<
>%+B<       >2**32-1<     >11111111111111111111111111111111<
>%#B<       >2**32-1<     >0B11111111111111111111111111111111<
>%C<        >ord('A')<          >A<
>%D<        >0x7fffffff<  >2147483647<     >Synonym for %ld<
>%E<        >123456.789<  >1.234568E+05<   >Like %e, but using upper-case "E"<
>%F<        >123456.789<  >123456.789000<  >Synonym for %f<
>%G<        >1234567.89<  >1.23457E+06<    >Like %g, but using upper-case "E"<
>%G<        >1234567e96<  >1.23457E+102<
>%G<        >.1234567e-101< >1.23457E-102<
>%G<        >12345.6789<  >12345.7<
>%G<        >1234567e96<  >1.23457E+102<	>exponent too big skip: os390<
>%G<        >.1234567e-101< >1.23457E-102<	>exponent too small skip: os390<
>%H<        >''<          >%H INVALID<
>%I<        >''<          >%I INVALID<
>%J<        >''<          >%J INVALID<
>%K<        >''<          >%K INVALID<
>%L<        >''<          >%L INVALID<
>%M<        >''<          >%M INVALID<
>%N<        >''<          >%N INVALID<
>%O<        >2**32-1<     >37777777777<    >Synonym for %lo<
>%P<        >''<          >%P INVALID<
>%Q<        >''<          >%Q INVALID<
>%R<        >''<          >%R INVALID<
>%S<        >''<          >%S INVALID<
>%T<        >''<          >%T INVALID<
>%U<        >2**32-1<     >4294967295<     >Synonym for %lu<
>%V<        >''<          >%V INVALID<
>%W<        >''<          >%W INVALID<
>%X<        >2**32-1<     >FFFFFFFF<       >Like %x, but with u/c letters<
>%#X<       >2**32-1<     >0XFFFFFFFF<
>%Y<        >''<          >%Y INVALID<
>%Z<        >''<          >%Z INVALID<
>%a<        >''<          >%a INVALID<
>%b<        >2**32-1<     >11111111111111111111111111111111<
>%+b<       >2**32-1<     >11111111111111111111111111111111<
>%#b<       >2**32-1<     >0b11111111111111111111111111111111<
>%34b<      >2**32-1<     >  11111111111111111111111111111111<
>%034b<     >2**32-1<     >0011111111111111111111111111111111<
>%-34b<     >2**32-1<     >11111111111111111111111111111111  <
>%-034b<    >2**32-1<     >11111111111111111111111111111111  <
>%6b<       >12<          >  1100<
>%6.5b<     >12<          > 01100<
>%-6.5b<    >12<          >01100 <
>%+6.5b<    >12<          > 01100<
>% 6.5b<    >12<          > 01100<
>%06.5b<    >12<          > 01100<         >0 flag with precision: no effect<
>%.5b<      >12<          >01100<
>%.0b<      >0<           ><
>%+.0b<     >0<           ><
>% .0b<     >0<           ><
>%-.0b<     >0<           ><
>%#.0b<     >0<           ><
>%#3.0b<    >0<           >   <
>%#3.1b<    >0<           >  0<
>%#3.2b<    >0<           > 00<
>%#3.3b<    >0<           >000<
>%#3.4b<    >0<           >0000<
>%.0b<      >1<           >1<
>%+.0b<     >1<           >1<
>% .0b<     >1<           >1<
>%-.0b<     >1<           >1<
>%#.0b<     >1<           >0b1<
>%#3.0b<    >1<           >0b1<
>%#3.1b<    >1<           >0b1<
>%#3.2b<    >1<           >0b01<
>%#3.3b<    >1<           >0b001<
>%#3.4b<    >1<           >0b0001<
>%c<        >ord('A')<    >A<
>%10c<      >ord('A')<    >         A<
>%#10c<     >ord('A')<    >         A<     ># modifier: no effect<
>%010c<     >ord('A')<    >000000000A<
>%10lc<     >ord('A')<    >         A<     >l modifier: no effect<
>%10hc<     >ord('A')<    >         A<     >h modifier: no effect<
>%10.5c<    >ord('A')<    >         A<     >precision: no effect<
>%-10c<     >ord('A')<    >A         <
>%d<        >123456.789<  >123456<
>%d<        >-123456.789< >-123456<
>%d<        >0<           >0<
>%-d<       >0<           >0<
>%+d<       >0<           >+0<
>% d<       >0<           > 0<
>%0d<       >0<           >0<
>%-3d<      >1<           >1  <
>%+3d<      >1<           > +1<
>% 3d<      >1<           >  1<
>%03d<      >1<           >001<
>%+ 3d<     >1<           > +1<
>% +3d<     >1<           > +1<
>%.0d<      >0<           ><
>%+.0d<     >0<           >+<
>% .0d<     >0<           > <
>%-.0d<     >0<           ><
>%#.0d<     >0<           ><
>%.0d<      >1<           >1<
>%d<        >1<           >1<
>%+d<       >1<           >+1<
>%#3.2d<    >1<           > 01<            ># modifier: no effect<
>%3.2d<     >1<           > 01<
>%03.2d<    >1<           > 01<            >0 flag with precision: no effect<
>%-3.2d<    >1<           >01 <
>%+3.2d<    >1<           >+01<
>% 3.2d<    >1<           > 01<
>%-03.2d<   >1<           >01 <            >zero pad + left just.: no effect<
>%3.*d<     >\@(2,1)<       > 01<
>%3.*d<     >\@(1,1)<       >  1<
>%3.*d<     >\@(0,1)<       >  1<
>%3.*d<     >\@(-1,1)<      >  1<
>%.*d<      >\@(0,0)<       ><
>%-.*d<     >\@(0,0)<       ><
>%+.*d<     >\@(0,0)<       >+<
>% .*d<     >\@(0,0)<       > <
>%0.*d<     >\@(0,0)<       ><
>%.*d<      >\@(-2,0)<      >0<
>%-.*d<     >\@(-2,0)<      >0<
>%+.*d<     >\@(-2,0)<      >+0<
>% .*d<     >\@(-2,0)<      > 0<
>%0.*d<     >\@(-2,0)<      >0<
>%d<        >-1<          >-1<
>%-d<       >-1<          >-1<
>%+d<       >-1<          >-1<
>% d<       >-1<          >-1<
>%-3d<      >-1<          >-1 <
>%+3d<      >-1<          > -1<
>% 3d<      >-1<          > -1<
>%03d<      >-1<          >-01<
>%hd<       >1<           >1<              >More extensive testing of<
>%ld<       >1<           >1<              >length modifiers would be<
>%Vd<       >1<           >1<              >platform-specific<
>%vd<       >chr(1)<      >1<
>%+vd<      >chr(1)<      >+1<
>%#vd<      >chr(1)<      >1<
>%vd<       >"\01\02\03"< >1.2.3<
>%v.3d<     >"\01\02\03"< >001.002.003<
>%0v3d<     >"\01\02\03"< >001.002.003<
>%-v3d<     >"\01\02\03"< >1  .2  .3  <
>%+-v3d<    >"\01\02\03"< >+1 .2  .3  <
>%v4.3d<    >"\01\02\03"< > 001. 002. 003<
>%0v4.3d<   >"\01\02\03"< > 001. 002. 003<
>%0*v2d<    >\@('-', "\0\7\14")< >00-07-12<
>%v.*d<     >\@("\01\02\03", 3)< >001.002.003<
>%0v*d<     >\@("\01\02\03", 3)< >001.002.003<
>%-v*d<     >\@("\01\02\03", 3)< >1  .2  .3  <
>%+-v*d<    >\@("\01\02\03", 3)< >+1 .2  .3  <
>%v*.*d<    >\@("\01\02\03", 4, 3)< > 001. 002. 003<
>%0v*.*d<   >\@("\01\02\03", 4, 3)< > 001. 002. 003<
>%0*v*d<    >\@('-', "\0\7\13", 2)< >00-07-11<
>%e<        >1234.875<    >1.234875e+03<
>%e<        >0.000012345< >1.234500e-05<
>%e<        >1234567E96<  >1.234567e+102<
>%e<        >0<           >0.000000e+00<
>%e<        >.1234567E-101< >1.234567e-102<
>%+e<       >1234.875<    >+1.234875e+03<
>%#e<       >1234.875<    >1.234875e+03<
>%e<        >-1234.875<   >-1.234875e+03<
>%+e<       >-1234.875<   >-1.234875e+03<
>%#e<       >-1234.875<   >-1.234875e+03<
>%.0e<      >1234.875<    >1e+03<
>%#.0e<     >1234.875<    >1.e+03<
>%.0e<      >1.875<       >2e+00<
>%.0e<      >0.875<       >9e-01<
>%.*e<      >\@(0, 1234.875)< >1e+03<
>%.1e<      >1234.875<    >1.2e+03<
>%-12.4e<   >1234.875<    >1.2349e+03  <
>%12.4e<    >1234.875<    >  1.2349e+03<
>%+-12.4e<  >1234.875<    >+1.2349e+03 <
>%+12.4e<   >1234.875<    > +1.2349e+03<
>%+-12.4e<  >-1234.875<   >-1.2349e+03 <
>%+12.4e<   >-1234.875<   > -1.2349e+03<
>%e<        >1234567E96<  >1.234567e+102<	>exponent too big skip: os390<
>%e<        >.1234567E-101< >1.234567e-102<	>exponent too small skip: os390<
>%f<        >1234.875<    >1234.875000<
>%+f<       >1234.875<    >+1234.875000<
>%#f<       >1234.875<    >1234.875000<
>%f<        >-1234.875<   >-1234.875000<
>%+f<       >-1234.875<   >-1234.875000<
>%#f<       >-1234.875<   >-1234.875000<
>%6f<       >1234.875<    >1234.875000<
>%*f<       >\@(6, 1234.875)< >1234.875000<
>%.0f<      >-0.1<        >-0<  >C library bug: no minus skip: VMS<
>%.0f<      >1234.875<    >1235<
>%.1f<      >1234.875<    >1234.9<
>%-8.1f<    >1234.875<    >1234.9  <
>%8.1f<     >1234.875<    >  1234.9<
>%+-8.1f<   >1234.875<    >+1234.9 <
>%+8.1f<    >1234.875<    > +1234.9<
>%+-8.1f<   >-1234.875<   >-1234.9 <
>%+8.1f<    >-1234.875<   > -1234.9<
>%*.*f<     >\@(5, 2, 12.3456)< >12.35<
>%f<        >0<           >0.000000<
>%.0f<      >0<           >0<
>%.0f<      >2**38<       >274877906944<   >Should have exact int'l rep'n<
>%.0f<      >0.1<         >0<
>%.0f<      >0.6<         >1<              >Known to fail with sfio, (irix|nonstop-ux|powerux); -DHAS_LDBL_SPRINTF_BUG may fix<
>%.0f<      >-0.6<        >-1<             >Known to fail with sfio, (irix|nonstop-ux|powerux); -DHAS_LDBL_SPRINTF_BUG may fix<
>%.0f<      >1.6<         >2<
>%.0f<      >-1.6<        >-2<
>%.0f<      >1<           >1<
>%#.0f<     >1<           >1.<
>%.0lf<     >1<           >1<              >'l' should have no effect<
>%.0hf<     >1<           >%.0hf INVALID<  >'h' should be rejected<
>%g<        >12345.6789<  >12345.7<
>%+g<       >12345.6789<  >+12345.7<
>%#g<       >12345.6789<  >12345.7<
>%.0g<      >-0.0<	  >-0<		   >C99 standard mandates minus sign but C89 does not skip: MSWin32 VMS hpux:10.20 openbsd netbsd:1.5 irix darwin<
>%.0g<      >12345.6789<  >1e+04<
>%#.0g<     >12345.6789<  >1.e+04<
>%.2g<      >12345.6789<  >1.2e+04<
>%.*g<      >\@(2, 12345.6789)< >1.2e+04<
>%.9g<      >12345.6789<  >12345.6789<
>%12.9g<    >12345.6789<  >  12345.6789<
>%012.9g<   >12345.6789<  >0012345.6789<
>%-12.9g<   >12345.6789<  >12345.6789  <
>%*.*g<     >\@(-12, 9, 12345.6789)< >12345.6789  <
>%-012.9g<  >12345.6789<  >12345.6789  <
>%g<        >-12345.6789< >-12345.7<
>%+g<       >-12345.6789< >-12345.7<
>%g<        >1234567.89<  >1.23457e+06<
>%+g<       >1234567.89<  >+1.23457e+06<
>%#g<       >1234567.89<  >1.23457e+06<
>%g<        >-1234567.89< >-1.23457e+06<
>%+g<       >-1234567.89< >-1.23457e+06<
>%#g<       >-1234567.89< >-1.23457e+06<
>%g<        >0.00012345<  >0.00012345<
>%g<        >0.000012345< >1.2345e-05<
>%g<        >1234567E96<  >1.23457e+102<
>%g<        >.1234567E-101< >1.23457e-102<
>%g<        >0<           >0<
>%13g<      >1234567.89<  >  1.23457e+06<
>%+13g<     >1234567.89<  > +1.23457e+06<
>%013g<     >1234567.89<  >001.23457e+06<
>%-13g<     >1234567.89<  >1.23457e+06  <
>%g<        >.1234567E-101< >1.23457e-102<	>exponent too small skip: os390<
>%g<        >1234567E96<  >1.23457e+102<	>exponent too big skip: os390<
>%h<        >''<          >%h INVALID<
>%i<        >123456.789<  >123456<         >Synonym for %d<
>%j<        >''<          >%j INVALID<
>%k<        >''<          >%k INVALID<
>%l<        >''<          >%l INVALID<
>%m<        >''<          >%m INVALID<
>%s< >sprintf('%%n%n %d', $n, $n)< >%n 2< >Slight sneakiness to test %n<
>%o<        >2**32-1<     >37777777777<
>%+o<       >2**32-1<     >37777777777<
>%#o<       >2**32-1<     >037777777777<
>%o<        >642<         >1202<          >check smaller octals across platforms<
>%+o<       >642<         >1202<
>% o<       >642<         >1202<
>%#o<       >642<         >01202<
>%4o<       >18<          >  22<
>%4.3o<     >18<          > 022<
>%-4.3o<    >18<          >022 <
>%+4.3o<    >18<          > 022<
>% 4.3o<    >18<          > 022<
>%04.3o<    >18<          > 022<          >0 flag with precision: no effect<
>%4.o<      >36<          >  44<
>%-4.o<     >36<          >44  <
>%+4.o<     >36<          >  44<
>% 4.o<     >36<          >  44<
>%04.o<     >36<          >  44<          >0 flag with precision: no effect<
>%.3o<      >18<          >022<
>%.0o<      >0<           ><
>%+.0o<     >0<           ><
>% .0o<     >0<           ><
>%-.0o<     >0<           ><
>%#.0o<     >0<           >0<
>%#3.0o<    >0<           >  0<
>%#3.1o<    >0<           >  0<
>%#3.2o<    >0<           > 00<
>%#3.3o<    >0<           >000<
>%#3.4o<    >0<           >0000<
>%.0o<      >1<           >1<
>%+.0o<     >1<           >1<
>% .0o<     >1<           >1<
>%-.0o<     >1<           >1<
>%#.0o<     >1<           >01<
>%#3.0o<    >1<           > 01<
>%#3.1o<    >1<           > 01<
>%#3.2o<    >1<           > 01<
>%#3.3o<    >1<           >001<
>%#3.4o<    >1<           >0001<
>%#.5o<     >012345<      >012345<
>%#.5o<     >012<         >00012<
>%#4o<      >17<          > 021<
>%#-4o<     >17<          >021 <
>%-#4o<     >17<          >021 <
>%#+4o<     >17<          > 021<
>%# 4o<     >17<          > 021<
>%#04o<     >17<          >0021<
>%#4.o<     >16<          > 020<
>%#-4.o<    >16<          >020 <
>%-#4.o<    >16<          >020 <
>%#+4.o<    >16<          > 020<
>%# 4.o<    >16<          > 020<
>%#04.o<    >16<          > 020<          >0 flag with precision: no effect<
>%#4.3o<    >18<          > 022<
>%#-4.3o<   >18<          >022 <
>%-#4.3o<   >18<          >022 <
>%#+4.3o<   >18<          > 022<
>%# 4.3o<   >18<          > 022<
>%#04.3o<   >18<          > 022<          >0 flag with precision: no effect<
>%#6.4o<    >18<          >  0022<
>%#-6.4o<   >18<          >0022  <
>%-#6.4o<   >18<          >0022  <
>%#+6.4o<   >18<          >  0022<
>%# 6.4o<   >18<          >  0022<
>%#06.4o<   >18<          >  0022<        >0 flag with precision: no effect<
>%d< >$p=sprintf('%p',$p);$p=~m/^[0-9a-f]+$/< >1< >Coarse hack: hex from %p?<
>%d< >$p=sprintf('%-8p',$p);$p=~m/^[0-9a-f]+\s*$/< >1< >Coarse hack: hex from %p?<
>%#p<       >''<          >%#p INVALID<
>%q<        >''<          >%q INVALID<
>%r<        >''<          >%r INVALID<
>%s<        >'string'<    >string<
>%10s<      >'string'<    >    string<
>%+10s<     >'string'<    >    string<
>%#10s<     >'string'<    >    string<
>%010s<     >'string'<    >0000string<
>%0*s<      >\@(10, 'string')< >0000string<
>%-10s<     >'string'<    >string    <
>%3s<       >'string'<    >string<
>%.3s<      >'string'<    >str<
>%.*s<      >\@(3, 'string')< >str<
>%.*s<      >\@(2, 'string')< >st<
>%.*s<      >\@(1, 'string')< >s<
>%.*s<      >\@(0, 'string')< ><
>%.*s<      >\@(-1,'string')< >string<  >negative precision to be ignored<
>%3.*s<     >\@(3, 'string')< >str<
>%3.*s<     >\@(2, 'string')< > st<
>%3.*s<     >\@(1, 'string')< >  s<
>%3.*s<     >\@(0, 'string')< >   <
>%3.*s<     >\@(-1,'string')< >string<  >negative precision to be ignored<
>%t<        >''<          >%t INVALID<
>%u<        >2**32-1<     >4294967295<
>%+u<       >2**32-1<     >4294967295<
>%#u<       >2**32-1<     >4294967295<
>%12u<      >2**32-1<     >  4294967295<
>%012u<     >2**32-1<     >004294967295<
>%-12u<     >2**32-1<     >4294967295  <
>%-012u<    >2**32-1<     >4294967295  <
>%4u<       >18<          >  18<
>%4.3u<     >18<          > 018<
>%-4.3u<    >18<          >018 <
>%+4.3u<    >18<          > 018<
>% 4.3u<    >18<          > 018<
>%04.3u<    >18<          > 018<         >0 flag with precision: no effect<
>%.3u<      >18<          >018<
>%v<        >''<          >%v INVALID<
>%w<        >''<          >%w INVALID<
>%x<        >2**32-1<     >ffffffff<
>%+x<       >2**32-1<     >ffffffff<
>%#x<       >2**32-1<     >0xffffffff<
>%10x<      >2**32-1<     >  ffffffff<
>%010x<     >2**32-1<     >00ffffffff<
>%-10x<     >2**32-1<     >ffffffff  <
>%-010x<    >2**32-1<     >ffffffff  <
>%0-10x<    >2**32-1<     >ffffffff  <
>%4x<       >18<          >  12<
>%4.3x<     >18<          > 012<
>%-4.3x<    >18<          >012 <
>%+4.3x<    >18<          > 012<
>% 4.3x<    >18<          > 012<
>%04.3x<    >18<          > 012<         >0 flag with precision: no effect<
>%.3x<      >18<          >012<
>%4X<       >28<          >  1C<
>%4.3X<     >28<          > 01C<
>%-4.3X<    >28<          >01C <
>%+4.3X<    >28<          > 01C<
>% 4.3X<    >28<          > 01C<
>%04.3X<    >28<          > 01C<         >0 flag with precision: no effect<
>%.3X<      >28<          >01C<
>%.0x<      >0<           ><
>%+.0x<     >0<           ><
>% .0x<     >0<           ><
>%-.0x<     >0<           ><
>%#.0x<     >0<           ><
>%#3.0x<    >0<           >   <
>%#3.1x<    >0<           >  0<
>%#3.2x<    >0<           > 00<
>%#3.3x<    >0<           >000<
>%#3.4x<    >0<           >0000<
>%.0x<      >1<           >1<
>%+.0x<     >1<           >1<
>% .0x<     >1<           >1<
>%-.0x<     >1<           >1<
>%#.0x<     >1<           >0x1<
>%#3.0x<    >1<           >0x1<
>%#3.1x<    >1<           >0x1<
>%#3.2x<    >1<           >0x01<
>%#3.3x<    >1<           >0x001<
>%#3.4x<    >1<           >0x0001<
>%#.5x<     >0x12345<     >0x12345<
>%#.5x<     >0x12<        >0x00012<
>%#4x<      >28<          >0x1c<
>%#4.3x<    >28<          >0x01c<
>%#-4.3x<   >28<          >0x01c<
>%#+4.3x<   >28<          >0x01c<
>%# 4.3x<   >28<          >0x01c<
>%#04.3x<   >28<          >0x01c<         >0 flag with precision: no effect<
>%#.3x<     >28<          >0x01c<
>%#6.3x<    >28<          > 0x01c<
>%#-6.3x<   >28<          >0x01c <
>%-#6.3x<   >28<          >0x01c <
>%#+6.3x<   >28<          > 0x01c<
>%+#6.3x<   >28<          > 0x01c<
>%# 6.3x<   >28<          > 0x01c<
>% #6.3x<   >28<          > 0x01c<
>%0*x<      >\@(-10, ,2**32-1)< >ffffffff  <
>%vx<       >\@("\01\02\03")< >1.2.3<
>%vx<       >\@("\01\x[14]\x[c4]\x[ac]")< >1.14.c4.ac<
>%.*x<      >\@(0,0)<       ><
>%-.*x<     >\@(0,0)<       ><
>%+.*x<     >\@(0,0)<       ><
>% .*x<     >\@(0,0)<       ><
>%0.*x<     >\@(0,0)<       ><
>%.*x<      >\@(-3,0)<      >0<
>%-.*x<     >\@(-3,0)<      >0<
>%+.*x<     >\@(-3,0)<      >0<
>% .*x<     >\@(-3,0)<      >0<
>%0.*x<     >\@(-3,0)<      >0<
>%#.*x<     >\@(0,0)<       ><
>%#-.*x<    >\@(0,0)<       ><
>%#+.*x<    >\@(0,0)<       ><
>%# .*x<    >\@(0,0)<       ><
>%#0.*x<    >\@(0,0)<       ><
>%#.*x<     >\@(-1,0)<      >0<
>%#-.*x<    >\@(-1,0)<      >0<
>%#+.*x<    >\@(-1,0)<      >0<
>%# .*x<    >\@(-1,0)<      >0<
>%#0.*x<    >\@(-1,0)<      >0<
>%y<        >''<          >%y INVALID<
>%z<        >''<          >%z INVALID<
>%2$d %1$d<	>\@(12, 34)<	>34 12<
>%*2$d<		>\@(12, 3)<	> 12<
>%2$d %d<	>\@(12, 34)<	>34 12<
>%2$d %d %d<	>\@(12, 34)<	>34 12 34<
>%3$d %d %d<	>\@(12, 34, 56)<	>56 12 34<
>%2$*3$d %d<	>\@(12, 34, 3)<	> 34 12<
>%*3$2$d %d<	>\@(12, 34, 3)<	>%*3$2$d 12 INVALID<
>%2$d<		>12<	>0 UNINIT<
>%0$d<		>12<	>%0$d INVALID<
>%1$$d<		>12<	>%1$$d INVALID<
>%1$1$d<	>12<	>%1$1$d INVALID<
>%*2$*2$d<	>\@(12, 3)<	>%*2$*2$d INVALID<
>%*2*2$d<	>\@(12, 3)<	>%*2*2$d INVALID<
>%*2$1d<	>\@(12, 3)<	>%*2$1d INVALID<
>%0v2.2d<	>''<	><
>%vc,%d<	>\@(63, 64, 65)<	>%vc,63 INVALID<
>%v%,%d<	>\@(63, 64, 65)<	>%v%,63 INVALID<
>%vd,%d<	>\@("\x1", 2, 3)<	>1,2<
>%vf,%d<	>\@(1, 2, 3)<	>%vf,1 INVALID<
>%vF,%d<	>\@(1, 2, 3)<	>%vF,1 INVALID<
>%ve,%d<	>\@(1, 2, 3)<	>%ve,1 INVALID<
>%vE,%d<	>\@(1, 2, 3)<	>%vE,1 INVALID<
>%vg,%d<	>\@(1, 2, 3)<	>%vg,1 INVALID<
>%vG,%d<	>\@(1, 2, 3)<	>%vG,1 INVALID<
>%vp<	>''<	>%vp INVALID<
>%vn<	>''<	>%vn INVALID<
>%vs,%d<	>\@(1, 2, 3)<	>%vs,1 INVALID<
>%v_<	>''<	>%v_ INVALID<
>%v#x<	>''<	>%v#x INVALID<
>%v02x<	>"\x66\x6f\x6f\012"<	>66.6f.6f.0a<
>%#v.8b<	>"\141\000\142"<	>0b01100001.00000000.0b01100010<	>perl #39530<
>%#v.0o<	>"\001\000\002\000"<    >01.0.02.0<
>%#v.1o<	>"\001\000\002\000"<    >01.0.02.0<
>%#v.4o<	>"\141\000\142"<	>0141.0000.0142<	>perl #39530<
>%#v.3i<	>"\141\000\142"<	>097.000.098<	>perl #39530<
>%#v.0x<	>"\001\000\002\000"<    >0x1..0x2.<
>%#v.1x<	>"\001\000\002\000"<    >0x1.0.0x2.0<
>%#v.2x<	>"\141\000\142"<	>0x61.00.0x62<	>perl #39530<
>%#v.2X<	>"\141\000\142"<	>0X61.00.0X62<	>perl #39530<
>%#v.8b<	>"\141\017\142"<	>0b01100001.0b00001111.0b01100010<	>perl #39530<
>%#v.4o<	>"\141\017\142"<	>0141.0017.0142<	>perl #39530<
>%#v.3i<	>"\141\017\142"<	>097.015.098<	>perl #39530<
>%#v.2x<	>"\141\017\142"<	>0x61.0x0f.0x62<	>perl #39530<
>%#v.2X<	>"\141\017\142"<	>0X61.0X0F.0X62<	>perl #39530<
>%#*v.8b<	>\@("][", "\141\000\142")<	>0b01100001][00000000][0b01100010<	>perl #39530<
>%#*v.4o<	>\@("][", "\141\000\142")<	>0141][0000][0142<	>perl #39530<
>%#*v.3i<	>\@("][", "\141\000\142")<	>097][000][098<	>perl #39530<
>%#*v.2x<	>\@("][", "\141\000\142")<	>0x61][00][0x62<	>perl #39530<
>%#*v.2X<	>\@("][", "\141\000\142")<	>0X61][00][0X62<	>perl #39530<
>%#*v.8b<	>\@("][", "\141\017\142")<	>0b01100001][0b00001111][0b01100010<	>perl #39530<
>%#*v.4o<	>\@("][", "\141\017\142")<	>0141][0017][0142<	>perl #39530<
>%#*v.3i<	>\@("][", "\141\017\142")<	>097][015][098<	>perl #39530<
>%#*v.2x<	>\@("][", "\141\017\142")<	>0x61][0x0f][0x62<	>perl #39530<
>%#*v.2X<	>\@("][", "\141\017\142")<	>0X61][0X0F][0X62<	>perl #39530<
>%V-%s<		>\@("Hello")<	>%V-Hello INVALID<
>%K %d %d<	>\@(13, 29)<	>%K 13 29 INVALID<
>%*.*K %d<	>\@(13, 29, 76)<	>%*.*K 13 INVALID<
>%4$K %d<	>\@(45, 67)<	>%4$K 45 INVALID<
>%d %K %d<	>\@(23, 45)<	>23 %K 45 INVALID<
>%*v*999\$d %d %d<	>\@(11, 22, 33)<	>%*v*999\$d 11 22 INVALID<
>%#b<		>0<	>0<
>%#o<		>0<	>0<
>%#x<		>0<	>0<
>%2147483647$v2d<	>''<	><
>%*2147483647$v2d<	>''<	> UNINIT<
