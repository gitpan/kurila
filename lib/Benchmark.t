#!./perl -w

use warnings;

our ($foo, $bar, $baz, $ballast);
use Test::More tests => 182;

use Benchmark < qw(:all);

my $delta = 0.4;

# Some timing ballast
sub fib {
  my $n = shift;
  return $n if $n +< 2;
  fib($n-1) + fib($n-2);
}
$ballast = 15;

my $All_Pattern =
    qr/(\d+) +wallclock secs? +\( *(-?\d+\.\d\d) +usr +(-?\d+\.\d\d) +sys +\+ +(-?\d+\.\d\d) +cusr +(-?\d+\.\d\d) +csys += +(-?\d+\.\d\d) +CPU\)/;
my $Noc_Pattern =
    qr/(\d+) +wallclock secs? +\( *(-?\d+\.\d\d) +usr +\+ +(-?\d+\.\d\d) +sys += +(-?\d+\.\d\d) +CPU\)/;
my $Nop_Pattern =
    qr/(\d+) +wallclock secs? +\( *(-?\d+\.\d\d) +cusr +\+ +(-?\d+\.\d\d) +csys += +\d+\.\d\d +CPU\)/;
# Please don't trust the matching parenthises to be useful in this :-)
my $Default_Pattern = qr/$All_Pattern|$Noc_Pattern/;

my $t0 = Benchmark->new();
isa_ok ($t0, 'Benchmark', "Ensure we can create a benchmark object");

# We use the benchmark object once we've done some work:

isa_ok(timeit(5, sub {++$foo}), 'Benchmark', "timeit CODEREF");
is ($foo, 5, "benchmarked code was run 5 times");

our $bar;
isa_ok(timeit(5, '++ our $bar'), 'Benchmark', "timeit eval");
is ($bar, 5, "benchmarked code was run 5 times");

# is coderef called with spurious arguments?
timeit( 1, sub { $foo = @_[?0] });
is ($foo, undef, "benchmarked code called without arguments");


print $^STDOUT, "# Burning CPU to benchmark things will take time...\n";



# We need to do something fairly slow in the coderef.
# Same coderef. Same place in memory.
my $coderef = sub {$baz += fib($ballast)};

# The default is three.
$baz = 0;
my $threesecs = countit(0, $coderef);
isa_ok($threesecs, 'Benchmark', "countit 0, CODEREF");
isnt ($baz, 0, "benchmarked code was run");
my $in_threesecs = $threesecs->iters;
print $^STDOUT, "# $in_threesecs iterations\n";
ok ($in_threesecs +> 0, "iters returned positive iterations");

my $estimate = int (100 * $in_threesecs / 3) / 100;
print $^STDOUT, "# from the 3 second run estimate $estimate iterations in 1 second...\n";
$baz = 0;
my $onesec = countit(1, $coderef);
isa_ok($onesec, 'Benchmark', "countit 1, CODEREF");
isnt ($baz, 0, "benchmarked code was run");
my $in_onesec = $onesec->iters;
print $^STDOUT, "# $in_onesec iterations\n";
ok ($in_onesec +> 0, "iters returned positive iterations");

do {
  my $difference = $in_onesec - $estimate;
  my $actual = abs ($difference / $in_onesec);
  ok ($actual +< $delta, "is $in_onesec within $delta of estimate ($estimate)");
  print $^STDOUT, "# $in_onesec is between " . ($delta / 2) .
    " and $delta of estimate. Not that safe.\n" if $actual +> $delta/2;
};

# I found that the eval'ed version was 3 times faster than the coderef.
# (now it has a different ballast value)
$baz = 0;
my $again = countit(1, '$main::baz += fib($main::ballast)');
isa_ok($onesec, 'Benchmark', "countit 1, eval");
isnt ($baz, 0, "benchmarked code was run");
my $in_again = $again->iters;
print $^STDOUT, "# $in_again iterations\n";
ok ($in_again +> 0, "iters returned positive iterations");


my $t1 = Benchmark->new();
isa_ok ($t1, 'Benchmark', "Create another benchmark object now we're finished");

my $diff = timediff ($t1, $t0);
isa_ok ($diff, 'Benchmark', "Get the time difference");
isa_ok (timesum ($t0, $t1), 'Benchmark', "check timesum");

my $default = timestr ($diff);
isnt ($default, '', 'timestr ($diff)');
my $auto = timestr ($diff, 'auto');
is ($auto, $default, 'timestr ($diff, "auto") matches timestr ($diff)');

do {
    my $all = timestr ($diff, 'all');
    like ($all, $All_Pattern, 'timestr ($diff, "all")');
    print $^STDOUT, "# $all\n";

    my @($wallclock, $usr, $sys, $cusr, $csys, $cpu) = @: $all =~ $All_Pattern;

    is (timestr ($diff, 'none'), '', "none supresses output");

    my $noc = timestr ($diff, 'noc');
    like ($noc, qr/$wallclock +wallclock secs? +\( *$usr +usr +\+ +$sys +sys += +$cpu +CPU\)/, 'timestr ($diff, "noc")');

    my $nop = timestr ($diff, 'nop');
    like ($nop, qr/$wallclock +wallclock secs? +\( *$cusr +cusr +\+ +$csys +csys += +\d+\.\d\d +CPU\)/, 'timestr ($diff, "nop")');

    if ($auto eq $noc) {
        pass ('"auto" is "noc"');
    } else {
        is ($auto, $all, q|"auto" isn't "noc", so should be eq to "all"|);
    }

    like (timestr ($diff, 'all', 'E'), 
          qr/(\d+) +wallclock secs? +\( *\d\.\d+E[-+]?\d\d\d? +usr +\d\.\d+E[-+]?\d\d\d? +sys +\+ +\d\.\d+E[-+]?\d\d\d? +cusr +\d\.\d+E[-+]?\d\d\d? +csys += +\d\.\d+E[-+]?\d\d\d? +CPU\)/, 'timestr ($diff, "all", "E") [sprintf format of "E"]');
};

my $out = "";
open my $out_fh, '>>', \$out or die;

sub do_with_out($sub) {
    open my $real_stdout, ">&", $^STDOUT;
    close $^STDOUT;
    $^STDOUT = *$out_fh{IO};

    my $res = $sub->();

    $^STDOUT = *$real_stdout{IO};
    return $res;
}

my $iterations = 3;

$foo = 0;
my $got = do_with_out( { timethis($iterations, sub {++$foo}) } );
isa_ok($got, 'Benchmark', "timethis CODEREF");
is ($foo, $iterations, "benchmarked code was run $iterations times");
$got = $out;
like ($got, qr/^timethis $iterations/, 'default title');
like ($got, $Default_Pattern, 'default format is all or noc');
$out = "";
$bar = 0;
$got = do_with_out({ timethis($iterations, '++$main::bar') });
isa_ok($got, 'Benchmark', "timethis eval");
is ($bar, $iterations, "benchmarked code was run $iterations times");

$got = $out;
like ($got, qr/^timethis $iterations/, 'default title');
like ($got, $Default_Pattern, 'default format is all or noc');

my $title = 'lies, damn lies and benchmarks';
$foo = 0;
$out = "";
$got = do_with_out({ timethis($iterations, sub {++$foo}, $title) });
isa_ok($got, 'Benchmark', "timethis with title");
is ($foo, $iterations, "benchmarked code was run $iterations times");

$got = $out;
like ($got, qr/^$title:/, 'specify title');
like ($got, $Default_Pattern, 'default format is all or noc');

# default is auto, which is all or noc. nop can never match the default
$foo = 0;
$out = "";
$got = do_with_out({ timethis($iterations, sub {++$foo}, $title, 'nop') });
isa_ok($got, 'Benchmark', "timethis with format");
is ($foo, $iterations, "benchmarked code was run $iterations times");

$got = $out;
like ($got, qr/^$title:/, 'specify title');
like ($got, $Nop_Pattern, 'specify format as nop');

do {
    $foo = 0;
    $out = "";
    my $start = time;
    $got = do_with_out({ timethis(-2, sub {$foo+= fib($ballast)}, $title, 'none') });
    my $end = time;
    isa_ok($got, 'Benchmark',
           "timethis, at least 2 seconds with format 'none'");
    ok ($foo +> 0, "benchmarked code was run");
    ok ($end - $start +> 1, "benchmarked code ran for over 1 second");

    $got = $out;
    # Remove any warnings about having too few iterations.
    $got =~ s/\(warning:[^\)]+\)//gs;
    $got =~ s/^[ \t\n]+//s; # Remove all the whitespace from the beginning

    is ($got, '', "format 'none' should suppress output");
};

$foo = $bar = $baz = 0;
$out = "";
$got = do_with_out({ timethese($iterations, \%( Foo => sub {++$foo}, Bar => '++$main::bar',
                                Baz => sub {++$baz} )) });
is(ref ($got), 'HASH', "timethese should return a hashref");
isa_ok($got->{?Foo}, 'Benchmark', "Foo value");
isa_ok($got->{?Bar}, 'Benchmark', "Bar value");
isa_ok($got->{?Baz}, 'Benchmark', "Baz value");
eq_set(\keys %$got, \qw(Foo Bar Baz), 'should be exactly three objects');
is ($foo, $iterations, "Foo code was run $iterations times");
is ($bar, $iterations, "Bar code was run $iterations times");
is ($baz, $iterations, "Baz code was run $iterations times");

$got = $out;
# Remove any warnings about having too few iterations.
$got =~ s/\(warning:[^\)]+\)//gs;

like ($got, qr/timing $iterations iterations of\s+Bar\W+Baz\W+Foo\W*?\.\.\./s,
      'check title');
# Remove the title
$got =~ s/.*\.\.\.//s;
like ($got, qr/\bBar\b.*\bBaz\b.*\bFoo\b/s, 'check output is in sorted order');
like ($got, $Default_Pattern, 'should find default format somewhere');


my $code_to_test =  \%( Foo => sub {$foo+=fib($ballast-2)},
                      Bar => sub {$bar+=fib($ballast)});
# Keep these for later.
my $results;
do {
    $foo = $bar = 0;
    $out = "";
    my $start = times;
    $results = do_with_out({ timethese(-0.1, $code_to_test, 'none') });
    my $end = times;

    is(ref ($results), 'HASH', "timethese should return a hashref");
    isa_ok($results->{?Foo}, 'Benchmark', "Foo value");
    isa_ok($results->{?Bar}, 'Benchmark', "Bar value");
    eq_set(\keys %$results, \qw(Foo Bar), 'should be exactly two objects');
    ok ($foo +> 0, "Foo code was run");
    ok ($bar +> 0, "Bar code was run");

    ok (($end - $start) +> 0.1, "benchmarked code ran for over 0.1 seconds");

    $got = $out;
    # Remove any warnings about having too few iterations.
    $got =~ s/\(warning:[^\)]+\)//gs;
    ok ($got !~ m/[^ \t\n]/, "format 'none' should suppress output");
};
my $graph_dissassembly =
    qr!^[ \t]+(\S+)[ \t]+(\w+)[ \t]+(\w+)[ \t]*		# Title line
    \n[ \t]*(\w+)[ \t]+([0-9.]+(?:/s)?)[ \t]+(-+)[ \t]+(-?\d+%)[ \t]*
    \n[ \t]*(\w+)[ \t]+([0-9.]+(?:/s)?)[ \t]+(-?\d+%)[ \t]+(-+)[ \t]*$!xm;

sub check_graph_consistency(	$ratetext, $slowc, $fastc,
        $slowr, $slowratet, $slowslow, $slowfastt,
        $fastr, $fastratet, $fastslowt, $fastfast) {
    my $all_passed = 1;
    $all_passed
      ^&^= is ($slowc, $slowr, "left col tag should be top row tag");
    $all_passed
      ^&^= is ($fastc, $fastr, "right col tag should be bottom row tag");
    $all_passed ^&^=
      like ($slowslow, qr/^-+/, "should be dash for comparing slow with slow");
    $all_passed
      ^&^= is ($slowslow, $fastfast, "slow v slow should be same as fast v fast");
    my $slowrate = $slowratet;
    my $fastrate = $fastratet;
    my ($slow_is_rate, $fast_is_rate);
    unless ($slow_is_rate = $slowrate =~ s!/s!!) {
        # Slow is expressed as iters per second.
        $slowrate = 1/$slowrate if $slowrate;
    }
    unless ($fast_is_rate = $fastrate =~ s!/s!!) {
        # Fast is expressed as iters per second.
        $fastrate = 1/$fastrate if $fastrate;
    }
    if ($ratetext =~ m/rate/i) {
        $all_passed
          ^&^= ok ($slow_is_rate, "slow should be expressed as a rate");
        $all_passed
          ^&^= ok ($fast_is_rate, "fast should be expressed as a rate");
    } else {
        $all_passed ^&^=
          ok (!$slow_is_rate, "slow should be expressed as a iters per second");
        $all_passed ^&^=
          ok (!$fast_is_rate, "fast should be expressed as a iters per second");
    }

    (my $slowfast = $slowfastt) =~ s!%!!;
    (my $fastslow = $fastslowt) =~ s!%!!;
    if ($slowrate +< $fastrate) {
        pass ("slow rate is less than fast rate");
        unless (ok ($slowfast +<= 0 && $slowfast +>= -100,
                    "slowfast should be less than or equal to zero, and >= -100")) {
          print $^STDERR, "# slowfast $slowfast\n";
          $all_passed = 0;
        }
        unless (ok ($fastslow +> 0, "fastslow should be > 0")) {
          print $^STDERR, "# fastslow $fastslow\n";
          $all_passed = 0;
        }
    } else {
        $all_passed
          ^&^= is ($slowrate, $fastrate,
                 "slow rate isn't less than fast rate, so should be the same");
	# In OpenBSD the $slowfast is sometimes a really, really, really
	# small number less than zero, and this gets stringified as -0.
        $all_passed
          ^&^= like ($slowfast, qr/^-?0$/, "slowfast should be zero");
        $all_passed
          ^&^= like ($fastslow, qr/^-?0$/, "fastslow should be zero");
    }
    return $all_passed;
}

sub check_graph_vs_output($chart, $got) {
    my @(	$ratetext, $slowc, $fastc,
        $slowr, $slowratet, $slowslow, $slowfastt,
        $fastr, $fastratet, $fastslowt, $fastfast)
        = @: $got =~ $graph_dissassembly;
    my $all_passed
      = check_graph_consistency (        $ratetext, $slowc, $fastc,
                                 $slowr, $slowratet, $slowslow, $slowfastt,
                                 $fastr, $fastratet, $fastslowt, $fastfast);
    $all_passed
      &&= is_deeply ($chart, \@(\@('', $ratetext, $slowc, $fastc),
                             \@($slowr, $slowratet, $slowslow, $slowfastt),
                             \@($fastr, $fastratet, $fastslowt, $fastfast)),
                    "check the chart layout matches the formatted output");
    unless ($all_passed) {
      print $^STDERR, "# Something went wrong there. I got this chart:\n";
      print $^STDERR, "# $_\n" foreach split m/\n/, $got;
    }
}

sub check_graph($title, $row1, $row2) {
    is (nelems @$title, 4, "Four entries in title row");
    is (nelems @$row1, 4, "Four entries in first row");
    is (nelems @$row2, 4, "Four entries in second row");
    is (shift @$title, '', "First entry of output graph should be ''");
    check_graph_consistency (<@$title, <@$row1, <@$row2);
}

do {
    $out = "";
    my $start = times;
    my $chart = do_with_out({ cmpthese( -0.1, \%( a => "++ our \$i", b => "our \$i; \$i = sqrt( \$i++)" ), "auto" )  });
    my $end = times;
    ok (($end - $start) +> 0.05, "benchmarked code ran for over 0.05 seconds");

    $got = $out;
    # Remove any warnings about having too few iterations.
    $got =~ s/\(warning:[^\)]+\)//gs;

    like ($got, qr/running\W+a\W+b.*?for at least 0\.1 CPU second/s,
          'check title');
    # Remove the title
    $got =~ s/.*\.\.\.//s;
    like ($got, $Default_Pattern, 'should find default format somewhere');
    like ($got, $graph_dissassembly, "Should find the output graph somewhere");
    check_graph_vs_output ($chart, $got);
};

# Not giving auto should suppress timethese results.
do {
    $out = "";
    my $start = times;
    my $chart = do_with_out({ cmpthese( -0.1, \%( a => "++ our \$i", b => "our \$i; \$i = sqrt(\$i++)" ) )  });
    my $end = times;
    ok (($end - $start) +> 0.05, "benchmarked code ran for over 0.05 seconds");

    $got = $out;
    # Remove any warnings about having too few iterations.
    $got =~ s/\(warning:[^\)]+\)//gs;

    unlike ($got, qr/running\W+a\W+b.*?for at least 0\.1 CPU second/s,
          'should not have title');
    # Remove the title
    $got =~ s/.*\.\.\.//s;
    unlike ($got, $Default_Pattern, 'should not find default format somewhere');
    like ($got, $graph_dissassembly, "Should find the output graph somewhere");
    check_graph_vs_output ($chart, $got);
};

do {
    $foo = $bar = 0;
    $out = "";
    my $chart = do_with_out({ cmpthese( 10, $code_to_test, 'nop' ) });
    ok ($foo +> 0, "Foo code was run");
    ok ($bar +> 0, "Bar code was run");

    $got = $out;
    # Remove any warnings about having too few iterations.
    $got =~ s/\(warning:[^\)]+\)//gs;
    like ($got, qr/timing 10 iterations of\s+Bar\W+Foo\W*?\.\.\./s,
      'check title');
    # Remove the title
    $got =~ s/.*\.\.\.//s;
    like ($got, $Nop_Pattern, 'specify format as nop');
    like ($got, $graph_dissassembly, "Should find the output graph somewhere");
    check_graph_vs_output ($chart, $got);
};

do {
    $foo = $bar = 0;
    $out = "";
    my $chart = do_with_out({ cmpthese( 10, $code_to_test, 'none' ) });
    ok ($foo +> 0, "Foo code was run");
    ok ($bar +> 0, "Bar code was run");

    $got = $out;
    # Remove any warnings about having too few iterations.
    $got =~ s/\(warning:[^\)]+\)//gs;
    $got =~ s/^[ \t\n]+//s; # Remove all the whitespace from the beginning
    is ($got, '', "format 'none' should suppress output");
    is (ref $chart, 'ARRAY', "output should be an array ref");
    # Some of these will go bang if the preceding test fails. There will be
    # a big clue as to why, from the previous test's diagnostic
    is (ref $chart->[0], 'ARRAY', "output should be an array of arrays");
    check_graph (<@$chart);
};

do {
    $foo = $bar = 0;
    $out = "";
    my $chart = do_with_out({ cmpthese( $results ) });
    is ($foo, 0, "Foo code was not run");
    is ($bar, 0, "Bar code was not run");

    $got = $out;
    ok ($got !~ m/\.\.\./s, 'check that there is no title');
    like ($got, $graph_dissassembly, "Should find the output graph somewhere");
    check_graph_vs_output ($chart, $got);
};

do {
    $foo = $bar = 0;
    $out = "";
    my $chart = do_with_out({ cmpthese( $results, 'none' ) });
    is ($foo, 0, "Foo code was not run");
    is ($bar, 0, "Bar code was not run");

    $got = $out;
    is ($got, '', "'none' should suppress all output");
    is (ref $chart, 'ARRAY', "output should be an array ref");
    # Some of these will go bang if the preceding test fails. There will be
    # a big clue as to why, from the previous test's diagnostic
    is (ref $chart->[0], 'ARRAY', "output should be an array of arrays");
    check_graph (<@$chart);
};

###}my $out = tie *OUT, 'TieOut'; my ($got); ###

do {
    my $debug = "";
    open my $debug_fh, '>>', \$debug or die;
    local $^STDERR = *$debug_fh{IO};

    $bar = 0;
    isa_ok(timeit(5, '++$main::bar'), 'Benchmark', "timeit eval");
    is ($bar, 5, "benchmarked code was run 5 times");
    is ($debug, '', "There was no debug output");

    Benchmark->debug(1);

    $bar = 0;
    $out = "";
    $got = do_with_out({ timeit(5, '++$main::bar') });
    isa_ok($got, 'Benchmark', "timeit eval");
    is ($bar, 5, "benchmarked code was run 5 times");
    is ($out, '', "There was no STDOUT output with debug enabled");
    isnt ($debug, '', "There was STDERR debug output with debug enabled");
    $debug = "";

    Benchmark->debug(0);

    $bar = 0;
    isa_ok(timeit(5, '++$main::bar'), 'Benchmark', "timeit eval");
    is ($bar, 5, "benchmarked code was run 5 times");
    is ($debug, '', "There was no debug output debug disabled");

};

# To check the cache we are poking where we don't belong, inside the namespace.
# The way benchmark is written We can't actually check whehter the cache is
# being used, merely what's become cached.

clearallcache();
my @before_keys =keys %Benchmark::Cache;
$bar = 0;
isa_ok(timeit(5, '++$main::bar'), 'Benchmark', "timeit eval");
is ($bar, 5, "benchmarked code was run 5 times");
my @after5_keys =keys %Benchmark::Cache;
$bar = 0;
isa_ok(timeit(10, '++$main::bar'), 'Benchmark', "timeit eval");
is ($bar, 10, "benchmarked code was run 10 times");
ok (!eq_array (\keys %Benchmark::Cache, \@after5_keys), "10 differs from 5");

clearcache(10);
# Hash key order will be the same if there are the same keys.
is_deeply (\keys %Benchmark::Cache, \@after5_keys,
           "cleared 10, only cached results for 5 should remain");

clearallcache();
is_deeply (\keys %Benchmark::Cache, \@before_keys,
           "back to square 1 when we clear the cache again?");


do {   # Check usage error messages
    my %usage = %Benchmark::_Usage;
    delete %usage{runloop};  # not public, not worrying about it just now

    my @takes_no_args =qw(clearallcache disablecache enablecache);

    my %cmpthese = %('forgot {}' => 'cmpthese( 42, foo => sub { 1 } )',
                     'not result' => 'cmpthese(42)',
                     'array ref'  => 'cmpthese( 42, \@( foo => sub { 1 } ) )',
                    );
    while( my@(?$name, ?$code) =@( each %cmpthese) ) {
        eval $code;
        is( $^EVAL_ERROR->{?description}, %usage{?cmpthese}, "cmpthese usage: $name" );
    }

    my %timethese = %('forgot {}'  => 'timethese( 42, foo => sub { 1 } )',
                       'array ref'  => 'timethese( 42, \@( foo => sub { 1 } ) )',
                      );

    while( my@(?$name, ?$code) =@( each %timethese) ) {
        eval $code;
        is( $^EVAL_ERROR->{?description}, %usage{?timethese}, "timethese usage: $name" );
    }


    foreach my $func ( @takes_no_args) {
        eval "$func(42)";
        is( $^EVAL_ERROR->{?description}, %usage{?$func}, "$func usage: with args" );
    }
};
