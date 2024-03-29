#!perl -w

use TestInit;
use Config;

BEGIN {
    print $^STDOUT, "1..0 # TODO Skip: Fix for kurila\n";
    exit 0;
}

use utf8;
use Tie::Hash;
use Test::More 'no_plan';

BEGIN {use_ok('XS::APItest')};

my $utf8_for_258 = chr 258;

my @testkeys = @('N', chr 198, chr 256);
my @keys = @(< @testkeys, $utf8_for_258);

main_tests (\@keys, \@testkeys, '');

main_tests (\@keys, \@testkeys, ' [utf8 hash]');

do {
  my %h = %(a=>'cheat');
  tie %h, 'Tie::StdHash';
  # is bug 36327 fixed?
  my $result = undef;

  is (XS::APItest::Hash::store(\%h, chr 258,  1), $result);
    
  ok (exists %h{$utf8_for_258},
      "hv_store does insert a key with the raw utf8 on a tied hash");
};

do {
    my $strtab = strtab();
    is (ref $strtab, 'HASH', "The shared string table quacks like a hash");
    my $wibble = "\0";
    try {
	$strtab->{$wibble}++;
    };
    my $prefix = "Cannot modify shared string table in hv_";
    my $what = $prefix . 'fetch';
    like ($@->{description}, qr/^$what/,$what);
    try {
	XS::APItest::Hash::store($strtab, 'Boom!',  1)
    };
    $what = $prefix . 'store';
    like ($@->{description}, qr/^$what/, $what);
    if (0) {
	A::B->method();
    }
    # DESTROY should be in there.
    try {
	delete $strtab->{DESTROY};
    };
    $what = $prefix . 'delete';
    like ($@->{description}, qr/^$what/, $what);
    # I can't work out how to get to the code that flips the wasutf8 flag on
    # the hash key without some ikcy XS
};

do {
    is_deeply(\@( <&XS::APItest::Hash::test_hv_free_ent), \@(2,2,1,1),
	      "hv_free_ent frees the value immediately");
    is_deeply(\@( <&XS::APItest::Hash::test_hv_delayfree_ent), \@(2,2,2,1),
	      "hv_delayfree_ent keeps the value around until FREETMPS");
};

foreach my $in ("", "N", "a\0b") {
    my $got = XS::APItest::Hash::test_share_unshare_pvn($in);
    is ($got, $in, "test_share_unshare_pvn");
}

do {
    foreach (\@(\&XS::APItest::Hash::rot13_hash, \&rot13, "rot 13"),
	     \@(\&XS::APItest::Hash::bitflip_hash, \&bitflip, "bitflip"),
	    ) {
	my ($setup, $mapping, $name) = < @$_;
	my %hash;
	my %placebo = %(a => 1, p => 2, i => 4, e => 8);
	$setup->(\%hash);
	%hash{a}++; %hash{[qw(p i e)]} = (2, 4, 8);

	test_U_hash(\%hash, \%placebo, \@(f => 9, g => 10, h => 11), $mapping,
		    $name);
    }
	    my (%hash, %placebo);
	    XS::APItest::Hash::bitflip_hash(\%hash);
	    foreach my $new (\@("7", 65, 67, 80),
			     \@("8", 163, 171, 215),
			     \@("U", 2603, 2604, 2604),
			    ) {
		foreach my $code (78, 240, 256, 1336) {
		    my $key = chr $code;
		    %hash{$key} = %placebo{$key} = $code;
		}
		my $name = 'bitflip ' . shift @$new;
		my @new_kv;
		foreach my $code (< @$new) {
		    my $key = chr $code;
		    push @new_kv, $key, $_;
		}

		test_U_hash(\%hash, \%placebo, \@new_kv, \&bitflip, $name);
	    }
};

################################   The End   ################################

sub test_U_hash {
    my ($hash, $placebo, $new, $mapping, $message) = < @_;
    my @hitlist = @( keys %$placebo );
    print "# $message\n";

    my @keys = @( sort keys %$hash );
    is (join(' ', @keys), join(' ', sort( $mapping->(keys %$placebo))),
	"uvar magic called exactly once on store");

    is (keys %$hash, keys %$placebo);

    my $victim = shift @hitlist;
    is (delete $hash->{$victim}, delete $placebo->{$victim});

    is (keys %$hash, keys %$placebo);
    @keys = @( sort keys %$hash );
    is (join(' ', @keys), join(' ', sort( <$mapping->(keys %$placebo))));

    $victim = shift @hitlist;
    is (XS::APItest::Hash::delete_ent ($hash, $victim,
				       XS::APItest::HV_DISABLE_UVAR_XKEY),
	undef, "Deleting a known key with conversion disabled fails (ent)");
    is (keys %$hash, keys %$placebo);

    is (XS::APItest::Hash::delete_ent ($hash, $victim, 0),
	delete $placebo->{$victim},
	"Deleting a known key with conversion enabled works (ent)");
    is (keys %$hash, keys %$placebo);
    @keys = @( sort keys %$hash );
    is (join(' ', @keys), join(' ', sort( <$mapping->(keys %$placebo))));

    $victim = shift @hitlist;
    is (XS::APItest::Hash::delete ($hash, $victim,
				   XS::APItest::HV_DISABLE_UVAR_XKEY),
	undef, "Deleting a known key with conversion disabled fails");
    is (keys %$hash, keys %$placebo);

    is (XS::APItest::Hash::delete ($hash, $victim, 0),
	delete $placebo->{$victim},
	"Deleting a known key with conversion enabled works");
    is (keys %$hash, keys %$placebo);
    @keys = @( sort keys %$hash );
    is (join(' ', @keys), join(' ', sort( <$mapping->(keys %$placebo))));

    my ($k, $v) = splice @$new, 0, 2;
    $hash->{$k} = $v;
    $placebo->{$k} = $v;
    is (keys %$hash, keys %$placebo);
    @keys = @( sort keys %$hash );
    is (join(' ', @keys), join(' ', sort( <$mapping->(keys %$placebo))));

    ($k, $v) = splice @$new, 0, 2;
    is (XS::APItest::Hash::store_ent($hash, $k, $v), $v, "store_ent");
    $placebo->{$k} = $v;
    is (keys %$hash, keys %$placebo);
    @keys = @( sort keys %$hash );
    is (join(' ', @keys), join(' ', sort( <$mapping->(keys %$placebo))));

    ($k, $v) = splice @$new, 0, 2;
    is (XS::APItest::Hash::store($hash, $k, $v), $v, "store");
    $placebo->{$k} = $v;
    is (keys %$hash, keys %$placebo);
    @keys = @( sort keys %$hash );
    is (join(' ', @keys), join(' ', sort( <$mapping->(keys %$placebo))));

    @hitlist = @( keys %$placebo );
    $victim = shift @hitlist;
    is (XS::APItest::Hash::fetch_ent($hash, $victim), $placebo->{$victim},
	"fetch_ent");
    is (XS::APItest::Hash::fetch_ent($hash, < $mapping->($victim)), undef,
	"fetch_ent (missing)");

    $victim = shift @hitlist;
    is (XS::APItest::Hash::fetch($hash, $victim), $placebo->{$victim},
	"fetch");
    is (XS::APItest::Hash::fetch($hash, < $mapping->($victim)), undef,
	"fetch (missing)");

    $victim = shift @hitlist;
    ok (XS::APItest::Hash::exists_ent($hash, $victim), "exists_ent");
    ok (!XS::APItest::Hash::exists_ent($hash, < $mapping->($victim)),
	"exists_ent (missing)");

    $victim = shift @hitlist;
    die "Need a victim" unless defined $victim;
    ok (XS::APItest::Hash::exists($hash, $victim), "exists");
    ok (!XS::APItest::Hash::exists($hash, < $mapping->($victim)),
	"exists (missing)");

    is (XS::APItest::Hash::common(\%(hv => $hash, keysv => $victim)),
	$placebo->{$victim}, "common (fetch)");
    is (XS::APItest::Hash::common(\%(hv => $hash, keypv => $victim)),
	$placebo->{$victim}, "common (fetch pv)");
    is (XS::APItest::Hash::common(\%(hv => $hash, keysv => $victim,
				   action => XS::APItest::HV_DISABLE_UVAR_XKEY)),
	undef, "common (fetch) missing");
    is (XS::APItest::Hash::common(\%(hv => $hash, keypv => $victim,
				   action => XS::APItest::HV_DISABLE_UVAR_XKEY)),
	undef, "common (fetch pv) missing");
    is (XS::APItest::Hash::common(\%(hv => $hash, keysv => < $mapping->($victim),
				   action => XS::APItest::HV_DISABLE_UVAR_XKEY)),
	$placebo->{$victim}, "common (fetch) missing mapped");
    is (XS::APItest::Hash::common(\%(hv => $hash, keypv => < $mapping->($victim),
				   action => XS::APItest::HV_DISABLE_UVAR_XKEY)),
	$placebo->{$victim}, "common (fetch pv) missing mapped");
}

sub main_tests {
  my ($keys, $testkeys, $description) = < @_;
  foreach my $key (@$testkeys) {
    my $lckey = ($key eq chr 198) ? chr 230 : lc $key;
    my $unikey = $key;
    utf8::encode $unikey;

    main_test_inner ($key, $lckey, $unikey, $keys, $description);

    main_test_inner ($key, $lckey, $unikey, $keys,
		     $description . ' \@(key utf8 on)');
  }
}

sub main_test_inner {
  my ($key, $lckey, $unikey, $keys, $description) = < @_;
  perform_test (\&test_present, $key, $keys, $description);
  perform_test (\&test_fetch_present, $key, $keys, $description);
  perform_test (\&test_delete_present, $key, $keys, $description);

  perform_test (\&test_store, $key, $keys, $description, \@(a=>'cheat'));
  perform_test (\&test_store, $key, $keys, $description, \@());

  perform_test (\&test_absent, $lckey, $keys, $description);
  perform_test (\&test_fetch_absent, $lckey, $keys, $description);
  perform_test (\&test_delete_absent, $lckey, $keys, $description);

  return if $unikey eq $key;

  perform_test (\&test_absent, $unikey, $keys, $description);
  perform_test (\&test_fetch_absent, $unikey, $keys, $description);
  perform_test (\&test_delete_absent, $unikey, $keys, $description);
}

sub perform_test {
  my ($test_sub, $key, $keys, $message, < @other) = < @_;
  my $printable = join ',', map {ord} split m//, $key;

  my (%hash, %tiehash);
  tie %tiehash, 'Tie::StdHash';

  < %hash{[@$keys]} = < @$keys;
  < %tiehash{[@$keys]} = < @$keys;

  &$test_sub (\%hash, $key, $printable, $message, < @other);
  &$test_sub (\%tiehash, $key, $printable, "$message tie", < @other);
}

sub test_present {
  my ($hash, $key, $printable, $message) = < @_;

  ok (exists $hash->{$key}, "hv_exists_ent present$message $printable");
  ok (XS::APItest::Hash::exists ($hash, $key),
      "hv_exists present$message $printable");
}

sub test_absent {
  my ($hash, $key, $printable, $message) = < @_;

  ok (!exists $hash->{$key}, "hv_exists_ent absent$message $printable");

  ok (!XS::APItest::Hash::exists ($hash, $key),
      "hv_exists absent$message $printable");
}

sub test_delete_present {
  my ($hash, $key, $printable, $message) = < @_;

  my $copy = \%();
  my $class = tied %$hash;
  if (defined $class) {
    tie %$copy, ref $class;
  }
  $copy = \%(< %$hash);
  ok (brute_force_exists ($copy, $key),
      "hv_delete_ent present$message $printable");
  is (delete $copy->{$key}, $key, "hv_delete_ent present$message $printable");
  ok (!brute_force_exists ($copy, $key),
      "hv_delete_ent present$message $printable");
  $copy = \%(< %$hash);
  ok (brute_force_exists ($copy, $key),
      "hv_delete present$message $printable");
  is (XS::APItest::Hash::delete ($copy, $key), $key,
      "hv_delete present$message $printable");
  ok (!brute_force_exists ($copy, $key),
      "hv_delete present$message $printable");
}

sub test_delete_absent {
  my ($hash, $key, $printable, $message) = < @_;

  my $copy = \%();
  my $class = tied %$hash;
  if (defined $class) {
    tie %$copy, ref $class;
  }
  $copy = \%(< %$hash);
  is (delete $copy->{$key}, undef, "hv_delete_ent absent$message $printable");
  $copy = \%(< %$hash);
  is (XS::APItest::Hash::delete ($copy, $key), undef,
      "hv_delete absent$message $printable");
}

sub test_store {
  my ($hash, $key, $printable, $message, $defaults) = < @_;
  my $HV_STORE_IS_CRAZY = 1;

  # We are cheating - hv_store returns NULL for a store into an empty
  # tied hash. This isn't helpful here.

  my $class = tied %$hash;

  # It's important to do this with nice new hashes created each time round
  # the loop, rather than hashes in the pad, which get recycled, and may have
  # xhv_array non-NULL
  my $h1 = \%(< @$defaults);
  my $h2 = \%(< @$defaults);
  if (defined $class) {
    tie %$h1, ref $class;
    tie %$h2, ref $class;
    # bug 36327 is fixed
    $HV_STORE_IS_CRAZY = undef;
  }
  is (XS::APItest::Hash::store_ent($h1, $key, 1), $HV_STORE_IS_CRAZY,
      "hv_store_ent$message $printable");
  ok (brute_force_exists ($h1, $key), "hv_store_ent$message $printable");
  is (XS::APItest::Hash::store($h2, $key,  1), $HV_STORE_IS_CRAZY,
      "hv_store$message $printable");
  ok (brute_force_exists ($h2, $key), "hv_store$message $printable");
}

sub test_fetch_present {
  my ($hash, $key, $printable, $message) = < @_;

  is ($hash->{$key}, $key, "hv_fetch_ent present$message $printable");
  is (XS::APItest::Hash::fetch ($hash, $key), $key,
      "hv_fetch present$message $printable");
}

sub test_fetch_absent {
  my ($hash, $key, $printable, $message) = < @_;

  is ($hash->{$key}, undef, "hv_fetch_ent absent$message $printable");
  is (XS::APItest::Hash::fetch ($hash, $key), undef,
      "hv_fetch absent$message $printable");
}

sub brute_force_exists {
  my ($hash, $key) = < @_;
  foreach (keys %$hash) {
    return 1 if $key eq $_;
  }
  return 0;
}

sub rot13 {
    my @results = @( map {my $a = $_; $a =~ s/([A-Z])/$( chr((ord($1) + 13 - ord('A')) % 26 + ord('A')) )/g;
                       $a =~ s/([a-z])/$( chr((ord($1) + 13 - ord('a')) % 26 + ord('a')) )/g;
                       $a} < @_ );
    @results;
}

sub bitflip {
    use bytes;
    my @results = @( map {join '', map {chr(32 ^^^ ord $_)} split '', $_} < @_ );
    @results;
}
