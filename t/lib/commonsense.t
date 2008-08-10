#!./perl

BEGIN {
require Config; Config->import;
}

if ((%Config{'extensions'} !~ m/\bFcntl\b/) ){
  print "Bail out! Perl configured without Fcntl module\n";
  exit 0;
}
if ((%Config{'extensions'} !~ m/\bIO\b/) ){
  print "Bail out! Perl configured without IO module\n";
  exit 0;
}
# hey, DOS users do not need this kind of common sense ;-)
if ($^O ne 'dos' && (%Config{'extensions'} !~ m/\bFile-Glob\b/) ){
  print "Bail out! Perl configured without File::Glob module\n";
  exit 0;
}

print "1..1\nok 1\n";

