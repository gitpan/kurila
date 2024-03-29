#!./perl -w
#
#  Copyright 2002, Larry Wall.
#
#  You may redistribute only under the same terms as Perl 5, as specified
#  in the README file that comes with the distribution.
#

# I'm trying to keep this test easily backwards compatible to 5.004, so no
# qr//;

# This test tries to craft malicious data to test out as many different
# error traps in Storable as possible
# It also acts as a test for read_header

use Config;

BEGIN {
    if (env::var('PERL_CORE')){
	chdir('t') if -d 't';
	$^INCLUDE_PATH = @('.', '../lib', '../ext/Storable/t');
    } else {
	# This lets us distribute Test::More in t/
	unshift $^INCLUDE_PATH, 't';
    }
}

our ($file_magic_str, $other_magic, $network_magic, $byteorder,
     $major, $minor, $minor_write, $fancy);

$byteorder = config_value('byteorder');

$file_magic_str = 'pst0';
$other_magic = 7 + length $byteorder;
$network_magic = 2;
$major = 2;
$minor = 7;
$minor_write = 7;

use Test::More;

# utf8 flags have been removed. There are 2 * 2 * 2 tests per byte in the body and header
# common to normal and network order serialised objects (hence the 8)
# There are only 2 * 2 tests per byte in the parts of the header not present
# for network order, and 2 tests per byte on the 'pst0' "magic number" only
# present in files, but not in things store()ed to memory
$fancy = 0;

plan tests => 372 + length ($byteorder) * 4 + $fancy * 8;

use Storable < qw (store retrieve freeze thaw nstore nfreeze);
require 'testlib.pl';
our $file;

# There is no UTF8 flag anymore
use bytes;
my %hash = %(perl => 'rules');

sub test_hash {
  my $clone = shift;
  is (ref $clone, "HASH", "Get hash back");
  is (nkeys %$clone, 1, "with 1 key");
  is ((keys %$clone)[0], "perl", "which is correct");
  is ($clone->{perl}, "rules");
}

sub test_header {
  my @($header, $isfile, $isnetorder) = @_;
  is ( ! ! $header->{?file}, ! ! $isfile, "is file");
  is ($header->{major}, $major, "major number");
  is ($header->{minor}, $minor_write, "minor number");
  is ( ! ! $header->{netorder}, ! ! $isnetorder, "is network order");
  if ($isnetorder) {
    # Network order header has no sizes
  } else {
    is ($header->{byteorder}, $byteorder, "byte order");
    is ($header->{intsize}, config_value('intsize'), "int size");
    is ($header->{longsize}, config_value('longsize'), "long size");
 SKIP: do {
	skip ("No \$Config\{prtsize\} on this perl version ($^PERL_VERSION)", 1)
	    unless defined config_value('ptrsize');
	is ($header->{ptrsize}, config_value('ptrsize'), "long size");
    };
    is ($header->{nvsize}, config_value('nvsize') || config_value('doublesize') || 8,
        "nv size"); # 5.00405 doesn't even have doublesize in config.
  }
}

sub test_truncated {
  my @($data, $sub, $magic_len, $what) = @_;
  for my $i (0 .. length ($data) - 1) {
    my $short = substr $data, 0, $i;

    # local $Storable::DEBUGME = 1;
    my $clone = &$sub($short);
    is (defined ($clone), '', "truncated $what to $i should fail");
    if ($i +< $magic_len) {
      like ($^EVAL_ERROR && $^EVAL_ERROR->{description}, "/^Magic number checking on storable $what failed/",
          "Should croak with magic number warning");
    } else {
      is ($^EVAL_ERROR, "", "Should not set \$\@");
    }
  }
}

sub test_corrupt {
  my @($data, $sub, $what, $name) = @_;

  my $clone = &$sub($data);
  is (defined ($clone), '', "$name $what should fail");
  like ($^EVAL_ERROR->{description}, $what, $name);
}

sub test_things {
  my @($contents, $sub, $what, ?$isnetwork) = @_;
  my $isfile = $what eq 'file';
  my $file_magic = $isfile ?? length $file_magic_str !! 0;

  my $header = Storable::read_magic ($contents);
  test_header ($header, $isfile, $isnetwork);

  # Test that if we re-write it, everything still works:
  my $clone = &$sub ($contents);

  is ($^EVAL_ERROR, "", "There should be no error");

  test_hash ($clone);

  # Now lets check the short version:
  test_truncated ($contents, $sub, $file_magic
                  + ($isnetwork ?? $network_magic !! $other_magic), $what);

  my $copy;
  if ($isfile) {
    $copy = $contents;
    substr ($copy, 0, 4, 'iron');
    test_corrupt ($copy, $sub, "/^File is not a perl storable/",
                  "magic number");
  }

  $copy = $contents;
  # Needs to be more than 1, as we're already coding a spread of 1 minor version
  # number on writes (2.5, 2.4). May increase to 2 if we figure we can do 2.3
  # on 5.005_03 (No utf8).
  # 4 allows for a small safety margin
  # (Joke:
  # Question: What is the value of pi?
  # Mathematician answers "It's pi, isn't it"
  # Physicist answers "3.1, within experimental error"
  # Engineer answers "Well, allowing for a small safety margin,   18"
  # )
  my $minor4 = $header->{minor} + 4;
  substr ($copy, $file_magic + 1, 1, chr $minor4);
  do {
    # Now by default newer minor version numbers are not a pain.
    $clone = &$sub($copy);
    is ($^EVAL_ERROR, "", "by default no error on higher minor");
    test_hash ($clone);

    local $Storable::accept_future_minor = 0;
    test_corrupt ($copy, $sub,
                  "/^Storable binary image v$header->{major}\.$minor4 more recent than I am \\(v$header->{major}\.$minor\\)/",
                  "higher minor");
  };

  $copy = $contents;
  my $major1 = $header->{major} + 1;
  substr ($copy, $file_magic, 1, chr 2*$major1);
  test_corrupt ($copy, $sub,
                "/^Storable binary image v$major1\.$header->{minor} more recent than I am \\(v$header->{major}\.$minor\\)/",
                "higher major");

  # Continue messing with the previous copy
  my $minor1 = $header->{minor} - 1;
  substr ($copy, $file_magic + 1, 1, chr $minor1);
  test_corrupt ($copy, $sub,
                "/^Storable binary image v$major1\.$minor1 more recent than I am \\(v$header->{major}\.$minor\\)/",
              "higher major, lower minor");

  my $where;
  if (!$isnetwork) {
    # All these are omitted from the network order header.
    # I'm not sure if it's correct to omit the byte size stuff.
    $copy = $contents;
    substr ($copy, $file_magic + 3, length $header->{byteorder}, join '', reverse split m//, $header->{byteorder});

    test_corrupt ($copy, $sub, "/^Byte order is not compatible/",
                  "byte order");
    $where = $file_magic + 3 + length $header->{byteorder};
    foreach (@(\@('intsize', "Integer"),
             \@('longsize', "Long integer"),
             \@('ptrsize', "Pointer"),
             \@('nvsize', "Double"))) {
      my @($key, $name) = @$_;
      $copy = $contents;
      substr ($copy, $where++, 1, chr 0);
      test_corrupt ($copy, $sub, "/^$name size is not compatible/",
                    "$name size");
    }
  } else {
    $where = $file_magic + $network_magic;
  }

  # Just the header and a tag 255. As 28 is currently the highest tag, this
  # is "unexpected"
  $copy = substr ($contents, 0, $where) . chr 255;

  test_corrupt ($copy, $sub,
                "/^Corrupted storable $what \\(binary v$header->{major}.$header->{minor}\\)/",
                "bogus tag");

  # Now drop the minor version number
  substr ($copy, $file_magic + 1, 1, chr $minor1);

  test_corrupt ($copy, $sub,
                "/^Corrupted storable $what \\(binary v$header->{major}.$minor1\\)/",
                "bogus tag, minor less 1");
  # Now increase the minor version number
  substr ($copy, $file_magic + 1, 1, chr $minor4);

  # local $Storable::DEBUGME = 1;
  # This is the delayed croak
  test_corrupt ($copy, $sub,
                "/^Storable binary image v$header->{major}.$minor4 contains data of type 255. This Storable is v$header->{major}.$minor and can only handle data types up to 28/",
                "bogus tag, minor plus 4");
  # And check again that this croak is not delayed:
  do {
    # local $Storable::DEBUGME = 1;
    local $Storable::accept_future_minor = 0;
    test_corrupt ($copy, $sub,
                  "/^Storable binary image v$header->{major}\.$minor4 more recent than I am \\(v$header->{major}\.$minor\\)/",
                  "higher minor");
  };
}

ok (defined store(\%hash, $file));

my $expected = 20 + bytes::length ($file_magic_str) + $other_magic + $fancy;
my $length = -s $file;

die "Don't seem to have written file '$file' as I can't get its length: $^OS_ERROR"
  unless defined $file;

die "Expected file to be $expected bytes (sizeof long is $(config_value('longsize'))) but it is $length"
  unless $length == $expected;

# Read the contents into memory:
my $contents = slurp ($file);

# Test the original direct from disk
my $clone = retrieve $file;
test_hash ($clone);

# Then test it.
test_things($contents, \&store_and_retrieve, 'file');

# And now try almost everything again with a Storable string
my $stored = freeze \%hash;
test_things($stored, \&freeze_and_thaw, 'string');

# Network order.
unlink $file or die "Can't unlink '$file': $^OS_ERROR";

ok (defined nstore(\%hash, $file));

$expected = 20 + length ($file_magic_str) + $network_magic + $fancy;
$length = -s $file;

die "Don't seem to have written file '$file' as I can't get its length: $^OS_ERROR"
  unless defined $file;

die "Expected file to be $expected bytes (sizeof long is $(config_value('longsize'))) but it is $length"
  unless $length == $expected;

# Read the contents into memory:
$contents = slurp ($file);

# Test the original direct from disk
$clone = retrieve $file;
test_hash ($clone);

# Then test it.
test_things($contents, \&store_and_retrieve, 'file', 1);

# And now try almost everything again with a Storable string
$stored = nfreeze \%hash;
test_things($stored, \&freeze_and_thaw, 'string', 1);

# Test that the bug fixed by #20587 doesn't affect us under some older
# Perl. AMS 20030901
do {
    chop(my $a = chr(0xDF).chr(256));
    my %a = %(chr(0xDF) => 1);
    %a{$a}++;
    freeze \%a;
    # If we were built with -DDEBUGGING, the assert() should have killed
    # us, which will probably alert the user that something went wrong.
    ok(1);
};

# Unusual in that the empty string is stored with an SX_LSCALAR marker
my $hash = store_and_retrieve("pst0\5\6\3\0\0\0\1\1\0\0\0\0\0\0\0\5empty");
ok(!$^EVAL_ERROR, "no exception");
is(ref($hash), "HASH", "got a hash");
is($hash->{empty}, "", "got empty element");
