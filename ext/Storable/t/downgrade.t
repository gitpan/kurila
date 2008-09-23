#!./perl -w
#
#  Copyright 2002, Larry Wall.
#
#  You may redistribute only under the same terms as Perl 5, as specified
#  in the README file that comes with the distribution.
#

# I ought to keep this test easily backwards compatible to 5.004, so no
# qr//;

# This test checks downgrade behaviour on pre-5.8 perls when new 5.8 features
# are encountered.


use Test::More;
use Storable 'thaw';

require utf8;

use strict;
use vars < qw(@RESTRICT_TESTS %R_HASH %U_HASH $UTF8_CROAK $RESTRICTED_CROAK);

@RESTRICT_TESTS = @('Locked hash', 'Locked hash placeholder',
                   'Locked keys', 'Locked keys placeholder',
                  );
%R_HASH = %(perl => 'rules');

{
  # This is cheating. "\xdf" in Latin 1 is beta S, so will match \w if it
  # is stored in utf8, not bytes.
  # "\xdf" is y diaresis in EBCDIC (except for cp875, but so far no-one seems
  # to use that) which has exactly the same properties for \w
  # So the tests happen to pass.
  use utf8;
  my $utf8 = "Schlo\x{df}";

  # \xe5 is V in EBCDIC. That doesn't have the same properties w.r.t. \w as
  # an a circumflex, so we need to be explicit.

  my $a_circumflex = "\xe5"; # a byte.
  %U_HASH = %(< map {$_, $_} @( 'castle', "ch{$a_circumflex}teau", $utf8, chr 0x57CE));
  plan tests => 162;
}

$UTF8_CROAK = "/^Cannot retrieve UTF8 data in non-UTF8 perl/";
$RESTRICTED_CROAK = "/^Cannot retrieve restricted hash/";

my %tests;
{
  local $/ = "\n\nend\n";
  while ( ~< *DATA) {
    next unless m/\S/s;
    unless (m/begin ([0-7]{3}) ([^\n]*)\n(.*)$/s) {
      s/\n.*//s;
      warn "Dodgy data in section starting '$_'";
      next;
    }
    next unless oct $1 == ord 'A'; # Skip ASCII on EBCDIC, and vice versa
    my $data = unpack 'u', $3;
    %tests{$2} = $data;
  }
}

# use Data::Dumper; $Data::Dumper::Useqq = 1; print Dumper \%tests;
sub thaw_hash {
  my ($name, $expected) = < @_;
  my $hash = try {thaw %tests{$name}};
  is ($@, '', "Thawed $name without error?");
  isa_ok ($hash, 'HASH');
  ok (defined $hash && eq_hash($hash, $expected),
      "And it is the hash we expected?");
  is_deeply($hash, $expected);
  $hash;
}

sub thaw_scalar {
  my ($name, $expected, $bug) = < @_;
  my $scalar = try {thaw %tests{$name}};
  is ($@, '', "Thawed $name without error?");
  isa_ok ($scalar, 'SCALAR', "Thawed $name?");
  is ($$scalar, $expected, "And it is the data we expected?");
  $scalar;
}

sub thaw_fail {
  my ($name, $expected) = < @_;
  my $thing = try {thaw %tests{$name}};
  is ($thing, undef, "Thawed $name failed as expected?");
  like ($@->{description}, $expected, "Error as predicted?");
}

sub test_locked_hash {
  my $hash = shift;
  my @keys = keys %$hash;
  my ($key, $value) = each %$hash;
  try {$hash->{$key} = 'x' . $value};
  like( $@->{description}, "/^Modification of a read-only value attempted/",
        'trying to change a locked key' );
  is ($hash->{$key}, $value, "hash should not change?");
  try {$hash->{use} = 'perl'};
  like( $@->{description}, "/^Attempt to access disallowed key 'use' in a restricted hash/",
        'trying to add another key' );
  ok (eq_array(\keys %$hash, \@keys), "Still the same keys?");
}

sub test_restricted_hash {
  my $hash = shift;
  my @keys = keys %$hash;
  my ($key, $value) = each %$hash;
  try {$hash->{$key} = 'x' . $value};
  is( $@, '',
        'trying to change a restricted key' );
  is ($hash->{$key}, 'x' . $value, "hash should change");
  try {$hash->{use} = 'perl'};
  like( $@->{description}, "/^Attempt to access disallowed key 'use' in a restricted hash/",
        'trying to add another key' );
  ok (eq_array(\keys %$hash, \@keys), "Still the same keys?");
}

sub test_placeholder {
  my $hash = shift;
  try {$hash->{rules} = 42};
  is ($@, '', 'No errors');
  is ($hash->{rules}, 42, "New value added");
}

sub test_newkey {
  my $hash = shift;
  try {$hash->{nms} = "http://nms-cgi.sourceforge.net/"};
  is ($@, '', 'No errors');
  is ($hash->{nms}, "http://nms-cgi.sourceforge.net/", "New value added");
}

# $Storable::DEBUGME = 1;
thaw_hash ('Hash with utf8 flag but no utf8 keys', \%R_HASH);

if (eval "use Hash::Util; 1") {
  print "# We have Hash::Util, so test that the restricted hashes in <DATA> are valid\n";
  for $Storable::downgrade_restricted (@(0, 1, undef, "cheese")) {
    my $hash = thaw_hash ('Locked hash', \%R_HASH);
    test_locked_hash ($hash);
    $hash = thaw_hash ('Locked hash placeholder', \%R_HASH);
    test_locked_hash ($hash);
    test_placeholder ($hash);

    $hash = thaw_hash ('Locked keys', \%R_HASH);
    test_restricted_hash ($hash);
    $hash = thaw_hash ('Locked keys placeholder', \%R_HASH);
    test_restricted_hash ($hash);
    test_placeholder ($hash);
  }
} else {
  print "# We don't have Hash::Util, so test that the restricted hashes downgrade\n";
  my $hash = thaw_hash ('Locked hash', \%R_HASH);
  test_newkey ($hash);
  $hash = thaw_hash ('Locked hash placeholder', \%R_HASH);
  test_newkey ($hash);
  $hash = thaw_hash ('Locked keys', \%R_HASH);
  test_newkey ($hash);
  $hash = thaw_hash ('Locked keys placeholder', \%R_HASH);
  test_newkey ($hash);
  local $Storable::downgrade_restricted = 0;
  thaw_fail ('Locked hash', $RESTRICTED_CROAK);
  thaw_fail ('Locked hash placeholder', $RESTRICTED_CROAK);
  thaw_fail ('Locked keys', $RESTRICTED_CROAK);
  thaw_fail ('Locked keys placeholder', $RESTRICTED_CROAK);
}

{
  use utf8;
  print "# We have utf8 scalars, so test that the utf8 scalars in <DATA> are valid\n";
  thaw_scalar ('Short 8 bit utf8 data', "\x{DF}", 1);
  thaw_scalar ('Long 8 bit utf8 data', "\x{DF}" x 256, 1);
  thaw_scalar ('Short 24 bit utf8 data', chr 0xC0FFEE);
  thaw_scalar ('Long 24 bit utf8 data', chr (0xC0FFEE) x 256);
}

print "# We have utf8 hashes, so test that the utf8 hashes in <DATA> are valid\n";
thaw_fail ('Hash with utf8 keys', qr/WASUTF8 flag not supported/ );

__END__
# A whole run of 2.x nfreeze data, uuencoded. The "mode bits" are the octal
# value of 'A', the "file name" is the test name. Use make_downgrade.pl to
# generate these.
begin 101 Locked hash
8!049`0````$*!7)U;&5S!`````1P97)L

end

begin 101 Locked hash placeholder
C!049`0````(*!7)U;&5S!`````1P97)L#A0````%<G5L97,`

end

begin 101 Locked keys
8!049`0````$*!7)U;&5S``````1P97)L

end

begin 101 Locked keys placeholder
C!049`0````(*!7)U;&5S``````1P97)L#A0````%<G5L97,`

end

begin 101 Short 8 bit utf8 data
&!047`L.?

end

begin 101 Short 8 bit utf8 data as bytes
&!04*`L.?

end

begin 101 Long 8 bit utf8 data
M!048```"`,.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?
MPY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#
MG\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?
MPY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#
MG\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?
MPY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#
MG\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?
MPY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#
MG\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?
MPY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#
MG\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?
8PY_#G\.?PY_#G\.?PY_#G\.?PY_#G\.?

end

begin 101 Short 24 bit utf8 data
)!047!?BPC[^N

end

begin 101 Short 24 bit utf8 data as bytes
)!04*!?BPC[^N

end

begin 101 Long 24 bit utf8 data
M!048```%`/BPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
MOZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N^+"/
;OZ[XL(^_KOBPC[^N^+"/OZ[XL(^_KOBPC[^N

end

begin 101 Hash with utf8 flag but no utf8 keys
8!049``````$*!7)U;&5S``````1P97)L

end

begin 101 Hash with utf8 keys
M!049``````0*!F-A<W1L90`````&8V%S=&QE"@=C:.5T96%U``````=C:.5T
D96%U%P/EGXX!`````^6?CA<'4V-H;&_#GP(````&4V-H;&_?

end

begin 101 Locked hash with utf8 keys
M!049`0````0*!F-A<W1L900````&8V%S=&QE"@=C:.5T96%U!`````=C:.5T
D96%U%P/EGXX%`````^6?CA<'4V-H;&_#GP8````&4V-H;&_?

end

begin 101 Hash with utf8 keys for pre 5.6
M!049``````0*!F-A<W1L90`````&8V%S=&QE"@=C:.5T96%U``````=C:.5T
D96%U"@/EGXX``````^6?C@H'4V-H;&_#GP(````&4V-H;&_?

end

begin 101 Hash with utf8 keys for 5.6
M!049``````0*!F-A<W1L90`````&8V%S=&QE"@=C:.5T96%U``````=C:.5T
D96%U%P/EGXX``````^6?CA<'4V-H;&_#GP(````&4V-H;&_?

end

begin 301 Locked hash
8!049`0````$*!9FDDX6B!`````27A9F3

end

begin 301 Locked hash placeholder
C!049`0````(.%`````69I).%H@H%F:23A:($````!)>%F9,`

end

begin 301 Locked keys
8!049`0````$*!9FDDX6B``````27A9F3

end

begin 301 Locked keys placeholder
C!049`0````(.%`````69I).%H@H%F:23A:(`````!)>%F9,`

end

begin 301 Short 8 bit utf8 data
&!047`HMS

end

begin 301 Short 8 bit utf8 data as bytes
&!04*`HMS

end

begin 301 Long 8 bit utf8 data
M!048```"`(MSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMS
MBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+
M<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMS
MBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+
M<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMS
MBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+
M<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMS
MBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+
M<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMS
MBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+
M<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMS
8BW.+<XMSBW.+<XMSBW.+<XMSBW.+<XMS

end

begin 301 Short 24 bit utf8 data
*!047!OM30G-S50``

end

begin 301 Short 24 bit utf8 data as bytes
*!04*!OM30G-S50``

end

begin 301 Long 24 bit utf8 data
M!048```&`/M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M3
M0G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S
M5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M3
M0G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S
M5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M3
M0G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S
M5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M3
M0G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S
M5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M3
M0G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S
M5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M3
M0G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S
M5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M3
M0G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S
M5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M3
M0G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S
M5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M3
M0G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S
M5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M3
M0G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S
M5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M3
M0G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S
M5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M3
M0G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S
M5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M3
M0G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S
M5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M3
M0G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S
M5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M3
M0G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S
M5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M3
M0G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S
M5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M3
M0G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S5?M30G-S
-5?M30G-S5?M30G-S50``

end

begin 301 Hash with utf8 flag but no utf8 keys
8!049``````$*!9FDDX6B``````27A9F3

end

begin 301 Hash with utf8 keys
M!049``````0*!X.(1Z.%@:0`````!X.(1Z.%@:0*!H.!HJ.3A0`````&@X&B
FHY.%%P3<9')5`0````3<9')5%P?B@XB3EHMS`@````;B@XB3EM\`

end

begin 301 Locked hash with utf8 keys
M!049`0````0*!X.(1Z.%@:0$````!X.(1Z.%@:0*!H.!HJ.3A00````&@X&B
FHY.%%P3<9')5!0````3<9')5%P?B@XB3EHMS!@````;B@XB3EM\`

end

begin 301 Hash with utf8 keys for pre 5.6
M!049``````0*!H.!HJ.3A0`````&@X&BHY.%"@B#B(M&HX6!I``````'@XA'
GHX6!I`H'XH.(DY:+<P(````&XH.(DY;?"@3<9')5``````3<9')5

end

begin 301 Hash with utf8 keys for 5.6
M!049``````0*!H.!HJ.3A0`````&@X&BHY.%"@>#B$>CA8&D``````>#B$>C
FA8&D%P?B@XB3EHMS`@````;B@XB3EM\7!-QD<E4`````!-QD<E4`

end
