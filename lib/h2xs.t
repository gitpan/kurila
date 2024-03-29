#!./perl -w

# Some quick tests to see if h2xs actually runs and creates files as 
# expected.  File contents include date stamps and/or usernames
# hence are not checked.  File existence is checked with -e though.
# This test depends on File::Path::rmtree() to clean up with.
#  - pvhp
#
# We are now checking that the correct use $version; is present in
# Makefile.PL and $module.pm

use File::Path;  # for cleaning up with rmtree()
use Test::More skip_all => "fix test for kurila";
use File::Spec;
use File::Find;
use ExtUtils::Manifest;
# Don't want its diagnostics getting in the way of ours.
$ExtUtils::Manifest::Quiet=1;
my $up = File::Spec->updir();

my $extracted_program = '../utils/h2xs'; # unix, nt, ...
if ($^O eq 'VMS') { $extracted_program = '[-.utils]h2xs.com'; }
if ($^O eq 'MacOS') { $extracted_program = '::utils:h2xs'; }
if (!(-e $extracted_program)) {
    print "1..0 # Skip: $extracted_program was not built\n";
    exit 0;
}
# You might also wish to bail out if your perl platform does not
# do `$^X -e 'warn "Writing h2xst"' 2>&1`; duplicity.

# ok on unix, nt, VMS, ...
my $dupe = '2>&1';
# ok on unix, nt, The extra \" are for VMS
my $lib = '"-I../lib" "-I../../lib"';
# The >&1 would create a file named &1 on MPW (STDERR && STDOUT are
# already merged).
if ($^O eq 'MacOS') {
    $dupe = '';
    # -x overcomes MPW $Config{startperl} anomaly
    $lib = '-x -I::lib: -I:::lib:';
}
# $name should differ from system header file names and must
# not already be found in the t/ subdirectory for perl.
my $name = 'h2xst';
my $header = "$name.h";
require kurila;
my $thisversion = $kurila::VERSION->stringify;

# If this test has failed previously a copy may be left.
rmtree($name);

my @tests = @(
"-f -n $name", $^V, <<"EOXSFILES",
Writing $name/ppport.h
Writing $name/lib/$name.pm
Writing $name/$name.xs
Writing $name/fallback/const-c.inc
Writing $name/fallback/const-xs.inc
Writing $name/Makefile.PL
Writing $name/README
Writing $name/t/$name.t
Writing $name/Changes
Writing $name/MANIFEST
EOXSFILES

"\"-X\" -f -n $name", $^V, <<"EONOXSFILES",
Writing $name/lib/$name.pm
Writing $name/Makefile.PL
Writing $name/README
Writing $name/t/$name.t
Writing $name/Changes
Writing $name/MANIFEST
EONOXSFILES

"-f -n $name -b $thisversion $header", $^V, <<"EOXSFILES",
Writing $name/ppport.h
Writing $name/lib/$name.pm
Writing $name/$name.xs
Writing $name/fallback/const-c.inc
Writing $name/fallback/const-xs.inc
Writing $name/Makefile.PL
Writing $name/README
Writing $name/t/$name.t
Writing $name/Changes
Writing $name/MANIFEST
EOXSFILES
);

my $total_tests = 3; # opening, closing and deleting the header file.
my $i = nelems(@tests) - 1;
while ( $i +> 0 ) {
  # 1 test for running it, 1 test for the expected result, and 1 for each file
  # plus 1 to open and 1 to check for the use in lib/$name.pm and Makefile.PL
  # And 1 more for our check for "bonus" files, 2 more for ExtUtil::Manifest.
  # use the () to force list context and hence count the number of matches.
  $total_tests += 9 + (() = @tests[$i] =~ m/(Writing)/sg);
  $i -= 3;
}

plan tests => $total_tests;

ok (open (HEADER, ">", "$header"), "open '$header'");
print HEADER <<HEADER or die $!;
#define Camel 2
#define Dromedary 1
HEADER
ok (close (HEADER), "close '$header'");

while (my ($args, $version, $expectation) = splice @tests, 0, 3) {
  # h2xs warns about what it is writing hence the (possibly unportable)
  # 2>&1 dupe:
  # does it run?
  my $prog = "$^X $lib $extracted_program $args $dupe";
  my $result = `$prog`;
  cmp_ok ($?, "==", 0, "running $prog ");

  # accomodate MPW # comment character prependage
  if ($^O eq 'MacOS') {
    $result =~ s/#\s*//gs;
  }

  #print "# expectation is >$expectation<\n";
  #print "# result is >$result<\n";
  # Was the output the list of files that were expected?
  is ($result, $expectation, "running $prog");

  my (%got);
  find (sub {%got{$File::Find::name}++ unless -d $_}, $name);

  foreach ($expectation =~ m/Writing\s+(\S+)/gm) {
    if ($^O eq 'MacOS') {
      $_ = ':' . join(':',split(m/\//,$_));
      $_ =~ s/$name:t:1.t/$name:t\/1.t/; # is this an h2xs bug?
    }
    if ($^O eq 'VMS') {
      $_ .= '.' unless $_ =~ m/\./;
      $_ = lc($_) unless exists %got{$_};
    }
    ok (-e $_, "check for $_") and delete %got{$_};
  }
  my @extra = keys %got;
  unless (ok (!@extra, "Are any extra files present?")) {
    print "# These files are unexpectedly present:\n";
    print "# $_\n" foreach sort @extra;
  }

  chdir ($name) or die "chdir $name failed: $!";
  # Aargh. Something wants to load a bit of regexp. And we have to chdir
  # for ExtUtils::Manifest. Caught between a rock and a hard place, so this
  # seems the least evil thing to do:
  push $^INCLUDE_PATH, "../../lib";
  my ($missing, $extra) = ExtUtils::Manifest::fullcheck();
  is_deeply ($missing, \@(), "No files in the MANIFEST should be missing");
  is_deeply ($extra, \@(),   "and all files present should be in the MANIFEST");
  pop $^INCLUDE_PATH;
  chdir ($up) or die "chdir $up failed: $!";
 
  foreach my $leaf (File::Spec->catfile('lib', "$name.pm"), 'Makefile.PL') {
    my $file = File::Spec->catfile($name, $leaf);
    if (ok (open (FILE, "<", $file), "open $file")) {
        require kurila;
      my $match = qr/use kurila v$kurila::VERSION;/;
      my $found;
      while ( ~< *FILE) {
        last if $found = m/$match/;
      }
      ok ($found, "looking for /$match/ in $file");
      close FILE or die "close $file: $!";
    }
  }
  # clean up
  rmtree($name);
}

cmp_ok (unlink ($header), "==", 1, "unlink '$header'") or die "\$! is $!";
