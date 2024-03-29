BEGIN {
    require "./test.pl";
}

plan tests => 4784;

use utf8;

sub MyUniClass {
  <<END;
0030	004F
END
}

sub Other::Class {
  <<END;
0040	005F
END
}

sub A::B::Intersection {
  <<END;
+main::MyUniClass
&Other::Class
END
}

sub test_regexp($str, $blk) {
  # test that given string consists of N-1 chars matching $qr1, and 1
  # char matching $qr2

  # constructing these objects here makes the last test loop go much faster
  my $qr1 = qr/(\p{$blk}+)/;
  if ($str =~ $qr1) {
    is($1, substr($str, 0, -1));		# all except last char
  }
  else {
    fail('first N-1 chars did not match');
  }

  my $qr2 = qr/(\P{$blk}+)/;
  if ($str =~ $qr2) {
    is($1, substr($str, -1));			# only last char
  }
  else {
    fail('last char did not match');
  }
}


my $str;

$str = join "", map { chr($_) }, 0x20 .. 0x6F;

# make sure it finds built-in class
is(@($str =~ m/(\p{Letter}+)/)[0], 'ABCDEFGHIJKLMNOPQRSTUVWXYZ');
is(@($str =~ m/(\p{l}+)/)[0], 'ABCDEFGHIJKLMNOPQRSTUVWXYZ');

# make sure it finds user-defined class
is(@($str =~ m/(\p{main::MyUniClass}+)/)[0], '0123456789:;<=>?@ABCDEFGHIJKLMNO');

# make sure it finds class in other package
is(@($str =~ m/(\p{Other::Class}+)/)[0], '@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_');

# make sure it finds class in other OTHER package
is(@($str =~ m/(\p{A::B::Intersection}+)/)[0], '@ABCDEFGHIJKLMNO');

# all of these should look in lib/unicore/bc/AL.pl
$str = "\x{070D}\x{070E}\x{070F}\x{0710}\x{0711}";
is(@($str =~ m/(\P{BidiClass: ArabicLetter}+)/)[0], "\x{070E}\x{070F}");
is(@($str =~ m/(\P{BidiClass: AL}+)/)[0], "\x{070E}\x{070F}");
is(@($str =~ m/(\P{BC :ArabicLetter}+)/)[0], "\x{070E}\x{070F}");
is(@($str =~ m/(\P{bc=AL}+)/)[0], "\x{070E}\x{070F}");

# make sure InGreek works
$str = "[\x{038B}\x{038C}\x{038D}]";

is(@($str =~ m/(\p{InGreek}+)/)[0], "\x{038B}\x{038C}\x{038D}");
is(@($str =~ m/(\p{Script:InGreek}+)/)[0], "\x{038B}\x{038C}\x{038D}");
is(@($str =~ m/(\p{Script=InGreek}+)/)[0], "\x{038B}\x{038C}\x{038D}");
is(@($str =~ m/(\p{sc:InGreek}+)/)[0], "\x{038B}\x{038C}\x{038D}");
is(@($str =~ m/(\p{sc=InGreek}+)/)[0], "\x{038B}\x{038C}\x{038D}");

use File::Spec;
my $updir = 'File::Spec'->updir;

# the %utf8::... hashes are already in existence
# because utf8_pva.pl was run by utf8_heavy.pl

*utf8::PropertyAlias = *utf8::PropertyAlias; # thwart a warning

no warnings 'utf8'; # we do not want warnings about surrogates etc

sub char_range {
    my @($h1, $h2) = @_;

    my $str;

    $str = join "", map { chr $_ }, $h1 .. (($h2 || $h1) + 1);

    return $str;
}

# non-General Category and non-Script
while (my @(?$abbrev, ?$files) =@( each %utf8::PVA_abbr_map)) {
  my $prop_name = %utf8::PropertyAlias{?$abbrev};
  next unless $prop_name;
  next if $abbrev eq "gc_sc";

  for (sort keys %$files) {
    my $filename = 'File::Spec'->catfile(
      $updir => lib => unicore => lib => $abbrev => "$files->{?$_}.pl"
    );

    next unless -e $filename;
    my @($h1, $h2) =  map { hex }, (split(m/\t/, (do $filename), 3))[[0..1]];

    my $str = char_range($h1, $h2);

    for my $p (@($prop_name, $abbrev)) {
      for my $c (@($files->{?$_}, $_)) {
        is($str =~ m/(\p{$p: $c}+)/ && $1, substr($str, 0, -1), "$filename - $p - $c");
        is($str =~ m/(\P{$p= $c}+)/ && $1, substr($str, -1));
      }
    }
  }
}

# General Category and Script
for my $p (@('gc', 'sc')) {
  while (my @(?$abbr, ?_) =@( each %{ %utf8::PropValueAlias{$p} })) {
    my $filename = 'File::Spec'->catfile(
      $updir => lib => unicore => lib => gc_sc => "%utf8::PVA_abbr_map{gc_sc}->{?$abbr}.pl"
    );

    next unless -e $filename;
    my @($h1, $h2) =  map { hex }, (split(m/\t/, (do $filename), 3))[[0..1]];

    my $str = char_range($h1, $h2);

    for my $x (@($p, %( gc => 'General Category', sc => 'Script' ){?$p})) {
      for my $y (@($abbr, %utf8::PropValueAlias{$p}->{?$abbr}, %utf8::PVA_abbr_map{gc_sc}->{?$abbr})) {
        is($str =~ m/(\p{$x: $y}+)/ && $1, substr($str, 0, -1));
        is($str =~ m/(\P{$x= $y}+)/ && $1, substr($str, -1));
        SKIP: do {
	  skip("surrogate", 1) if $abbr eq 'cs';
 	  test_regexp ($str, $y);
        };
      }
    }
  }
}

# test extra properties (ASCII_Hex_Digit, Bidi_Control, etc.)
SKIP:
do {
  skip "Can't reliably derive class names from file names", 576 if $^OS_NAME eq 'VMS';

  # On case tolerant filesystems, Cf.pl will cause a -e test for cf.pl to
  # return true. Try to work around this by reading the filenames explicitly
  # to get a case sensitive test.  N.B.  This will fail if filename case is
  # not preserved because you might go looking for a class name of CF or cf
  # when you really want Cf.  Storing case sensitive data in filenames is 
  # simply not portable.

  my %files;

  my $dirname = 'File::Spec'->catdir($updir => lib => unicore => lib => 'gc_sc');
  opendir my $dh, $dirname or die $^OS_ERROR;
   %files{[@(readdir($dh))]} = @();
  closedir $dh;

  for (keys %utf8::PA_reverse) {
    my $leafname = "%utf8::PA_reverse{?$_}.pl";
    next unless exists %files{$leafname};

    my $filename = 'File::Spec'->catfile($dirname, $leafname);

    my @($h1, $h2) =  map { hex }, split(m/\t/, (do $filename), 3)[[0..1]];

    my $str = char_range($h1, $h2);

    for my $x (@('gc', 'General Category')) {
      print $^STDOUT, "# $filename $x $_, %utf8::PA_reverse{?$_}\n";
      for my $y (@($_, %utf8::PA_reverse{?$_})) {
	is($str =~ m/(\p{$x: $y}+)/ && $1, substr($str, 0, -1));
	is($str =~ m/(\P{$x= $y}+)/ && $1, substr($str, -1));
	test_regexp ($str, $y);
      }
    }
  }
};

# test the blocks (InFoobar)
for ( grep { %utf8::Canonical{?$_} =~ m/^In/ }, keys %utf8::Canonical) {
  my $filename = 'File::Spec'->catfile(
    $updir => lib => unicore => lib => gc_sc => "%utf8::Canonical{?$_}.pl"
  );

  next unless -e $filename;

  print $^STDOUT, "# In$_ $filename\n";

  my @($h1, $h2) =  map { hex }, split(m/\t/, (do $filename), 3)[[0..1]];

  my $str = char_range($h1, $h2);

  my $blk = $_;

  SKIP: do {
    skip($blk, 2) if $blk =~ m/surrogates/i;
    test_regexp ($str, $blk);
    $blk =~ s/^In/Block:/;
    test_regexp ($str, $blk);
  };
}

