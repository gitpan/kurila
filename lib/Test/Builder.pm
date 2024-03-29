package Test::Builder;

use kurila;


our $VERSION = '0.78_01';
$VERSION = try { $VERSION }; # make the alpha version come out as a number

# Make Test::Builder thread-safe for ithreads.
BEGIN {
    use Config;
    *share = sub { return @_[0] };
    *lock  = sub { 0 };
}


=head1 NAME

Test::Builder - Backend for building test libraries

=head1 SYNOPSIS

  package My::Test::Module;
  use base 'Test::Builder::Module';

  my $CLASS = __PACKAGE__;

  sub ok {
      my($test, $name) = @_;
      my $tb = $CLASS->builder;

      $tb->ok($test, $name);
  }


=head1 DESCRIPTION

Test::Simple and Test::More have proven to be popular testing modules,
but they're not always flexible enough.  Test::Builder provides the a
building block upon which to write your own test libraries I<which can
work together>.

=head2 Construction

=over 4

=item B<new>

  my $Test = Test::Builder->new;

Returns a Test::Builder object representing the current state of the
test.

Since you only run one test per program C<new> always returns the same
Test::Builder object.  No matter how many times you call new(), you're
getting the same object.  This is called a singleton.  This is done so that
multiple modules share such global information as the test counter and
where test output is going.

If you want a completely new Test::Builder object different from the
singleton, use C<create>.

=cut

my $Test = Test::Builder->new;
sub new {
    my@($class) =@( shift);
    $Test ||= $class->create;
    return $Test;
}


=item B<create>

  my $Test = Test::Builder->create;

Ok, so there can be more than one Test::Builder object and this is how
you get it.  You might use this instead of C<new()> if you're testing
a Test::Builder based module, but otherwise you probably want C<new>.

B<NOTE>: the implementation is not complete.  C<level>, for example, is
still shared amongst B<all> Test::Builder objects, even ones created using
this method.  Also, the method name may change in the future.

=cut

sub create {
    my $class = shift;

    my $self = bless \%(), $class;
    $self->reset;

    return $self;
}

=item B<reset>

  $Test->reset;

Reinitializes the Test::Builder singleton to its original state.
Mostly useful for tests run in persistent environments where the same
test might be run multiple times in the same process.

=cut

our ($Level);

sub reset($self) {

    # We leave this a global because it has to be localized and localizing
    # hash keys is just asking for pain.  Also, it was documented.
    $Level = 1;

    $self->{+Have_Plan}    = 0;
    $self->{+No_Plan}      = 0;
    $self->{+Original_Pid} = $^PID;

    share($self->{?Curr_Test});
    $self->{+Curr_Test}    = 0;
    $self->{+Test_Results} = &share(\@());

    $self->{+Exported_To}    = undef;
    $self->{+Expected_Tests} = 0;

    $self->{+Skip_All}   = 0;

    $self->{+Use_Nums}   = 1;

    $self->{+No_Header}  = 0;
    $self->{+No_Ending}  = 0;

    $self->{+TODO}       = undef;

    $self->_dup_stdhandles unless $^COMPILING;

    return;
}

=back

=head2 Setting up tests

These methods are for setting up tests and declaring how many there
are.  You usually only want to call one of these methods.

=over 4

=item B<plan>

  $Test->plan('no_plan');
  $Test->plan( skip_all => $reason );
  $Test->plan( tests => $num_tests );

A convenient way to set up your tests.  Call this and Test::Builder
will print the appropriate headers and take the appropriate actions.

If you call plan(), don't call any of the other methods below.

=cut

sub plan($self, $cmd, ?$arg) {

    local $Level = $Level + 1;

    if( $self->{?Have_Plan} ) {
        die("You tried to plan twice");
    }

    if( $cmd eq 'no_plan' ) {
        $self->no_plan;
    }
    elsif( $cmd eq 'skip_all' ) {
        return $self->skip_all($arg);
    }
    elsif( $cmd eq 'tests' ) {
        if( $arg ) {
            local $Level = $Level + 1;
            return $self->expected_tests($arg);
        }
        elsif( !defined $arg ) {
            die("Got an undefined number of tests");
        }
        elsif( !$arg ) {
            die("You said to run 0 tests");
        }
    }
    else {
        my @args = grep { defined }, @( ($cmd, $arg));
        die("plan() doesn't understand $(join ' ',@args)");
    }

    return 1;
}

=item B<expected_tests>

    my $max = $Test->expected_tests;
    $Test->expected_tests($max);

Gets/sets the # of tests we expect this test to run and prints out
the appropriate headers.

=cut

sub expected_tests {
    my $self = shift;
    my @(?$max) =  @_;

    if( (nelems @_) ) {
        die("Number of tests must be a positive integer.  You gave it '$max'")
          unless $max =~ m/^\+?\d+$/ and $max +> 0;

        $self->{+Expected_Tests} = $max;
        $self->{+Have_Plan}      = 1;

        $self->_print("1..$max\n") unless $self->no_header;
    }
    return $self->{?Expected_Tests};
}


=item B<no_plan>

  $Test->no_plan;

Declares that this test will run an indeterminate # of tests.

=cut

sub no_plan {
    my $self = shift;

    $self->{+No_Plan}   = 1;
    $self->{+Have_Plan} = 1;
}

=item B<has_plan>

  $plan = $Test->has_plan

Find out whether a plan has been defined. $plan is either C<undef> (no plan has been set), C<no_plan> (indeterminate # of tests) or an integer (the number of expected tests).

=cut

sub has_plan {
    my $self = shift;

    return $self->{?Expected_Tests} if $self->{?Expected_Tests};
    return 'no_plan' if $self->{?No_Plan};
    return undef;
};


=item B<skip_all>

  $Test->skip_all;
  $Test->skip_all($reason);

Skips all the tests, using the given $reason.  Exits immediately with 0.

=cut

sub skip_all($self, $reason) {

    my $out = "1..0";
    $out .= " # Skip $reason" if $reason;
    $out .= "\n";

    $self->{+Skip_All} = 1;

    $self->_print($out) unless $self->no_header;
    exit(0);
}


=item B<exported_to>

  my $pack = $Test->exported_to;
  $Test->exported_to($pack);

Tells Test::Builder what package you exported your functions to.

This method isn't terribly useful since modules which share the same
Test::Builder object might get exported to different packages and only
the last one will be honored.

=cut

sub exported_to($self, ?$pack) {

    if( defined $pack ) {
        $self->{+Exported_To} = $pack;
    }
    return $self->{?Exported_To};
}

=back

=head2 Running tests

These actually run the tests, analogous to the functions in Test::More.

They all return true if the test passed, false if the test failed.

$name is always optional.

=over 4

=item B<ok>

  $Test->ok($test, $name);

Your basic test.  Pass if $test is true, fail if $test is false.  Just
like Test::Simple's ok().

=cut

sub ok($self, $test, ?$name) {

    # $test might contain an object which we don't want to accidentally
    # store, so we turn it into a boolean.
    $test = $test ?? 1 !! 0;

    $self->_plan_check;

    lock $self->{?Curr_Test};
    $self->{+Curr_Test}++;

    $self->diag(<<ERR) if defined $name and $name =~ m/^[\d\s]+$/;
    You named your test '$name'.  You shouldn't use numbers for your test names.
    Very confusing.
ERR

    my $todo = $self->todo();
    
    # Capture the value of $TODO for the rest of this ok() call
    # so it can more easily be found by other routines.
    local $self->{+TODO} = $todo;

    my $out;
    my $result = &share(\%());

    unless( $test ) {
        $out .= "not ";
         %$result{[@('ok', 'actual_ok') ]} = @( ( $todo ?? 1 !! 0 ), 0 );
    }
    else { 
        %$result{[@('ok', 'actual_ok') ]} = @( 1, $test );
    }

    $out .= "ok";
    $out .= " $self->{?Curr_Test}" if $self->use_numbers;

    if( defined $name ) {
        $name =~ s|#|\\#|g;     # # in a name can confuse Test::Harness.
        $out   .= " - $name";
        $result->{+name} = $name;
    }
    else {
        $result->{+name} = '';
    }

    if( $todo ) {
        $out   .= " # TODO $todo";
        $result->{+reason} = $todo;
        $result->{+type}   = 'todo';
    }
    else {
        $result->{+reason} = '';
        $result->{+type}   = '';
    }

    $self->{Test_Results}->[+$self->{?Curr_Test}-1] = $result;
    $out .= "\n";

    $self->_print($out);

    unless( $test ) {
        my $msg = $todo ?? "Failed (TODO)" !! "Failed";
        $self->_print_diag("\n") if env::var('HARNESS_ACTIVE');

    my@(_, $file, $line, ...) =  $self->caller;
        if( defined $name ) {
            $self->diag(qq[  $msg test '$name'\n]);
            $self->diag(qq[  at $file line $line.\n]);
        }
        else {
            $self->diag(qq[  $msg test at $file line $line.\n]);
        }
    } 

    return $test ?? 1 !! 0;
}

sub _is_object($self, $thing) {

    return $self->_try(sub { ref $thing && $thing->isa('UNIVERSAL') }) ?? 1 !! 0;
}


# This is a hack to detect a dualvar such as $!
sub _is_dualvar($self, $val) {

    local $^WARNING = 0;
    my $numval = $val+0;
    return 1 if $numval != 0 and $numval ne $val;
}



=item B<is_eq>

  $Test->is_eq($got, $expected, $name);

Like Test::More's is().  Checks if $got eq $expected.  This is the
string version.

=item B<is_num>

  $Test->is_num($got, $expected, $name);

Like Test::More's is().  Checks if $got == $expected.  This is the
numeric version.

=cut

sub is_eq($self, $got, $expect, ?$name) {
    local $Level = $Level + 1;

    if( !defined $got || !defined $expect ) {
        # undef only matches undef and nothing else
        my $test = !defined $got && !defined $expect;

        $self->ok($test, $name);
        $self->_is_diag($got, 'eq', $expect) unless $test;
        return $test;
    }

    if (ref $got && ref $expect) {
        my $test = $got \== $expect;

        $self->ok($test, $name);
        $self->_is_diag($got, '\==', $expect) unless $test;
        return $test;
    }

    return $self->cmp_ok($got, 'eq', $expect, $name);
}

sub is_num($self, $got, $expect, ?$name) {
    local $Level = $Level + 1;

    if( !defined $got || !defined $expect ) {
        # undef only matches undef and nothing else
        my $test = !defined $got && !defined $expect;

        $self->ok($test, $name);
        $self->_is_diag($got, '==', $expect) unless $test;
        return $test;
    }

    return $self->cmp_ok($got, '==', $expect, $name);
}

sub _is_diag($self, $got, $type, $expect) {

    local $Level = $Level + 1;
    return $self->diag(sprintf <<DIAGNOSTIC, dump::view($got), dump::view($expect));
         got: \%s
    expected: \%s
DIAGNOSTIC

}    

=item B<isnt_eq>

  $Test->isnt_eq($got, $dont_expect, $name);

Like Test::More's isnt().  Checks if $got ne $dont_expect.  This is
the string version.

=item B<isnt_num>

  $Test->isnt_num($got, $dont_expect, $name);

Like Test::More's isnt().  Checks if $got ne $dont_expect.  This is
the numeric version.

=cut

sub isnt_eq($self, $got, $dont_expect, ?$name) {
    local $Level = $Level + 1;

    if( !defined $got || !defined $dont_expect ) {
        # undef only matches undef and nothing else
        my $test = defined $got || defined $dont_expect;

        $self->ok($test, $name);
        $self->_cmp_diag($got, 'ne', $dont_expect) unless $test;
        return $test;
    }

    if (ref $got && ref $dont_expect) {
        my $test = $got \!= $dont_expect;

        $self->ok($test, $name);
        $self->_is_diag($got, '\==', $dont_expect) unless $test;
        return $test;
    }

    return $self->cmp_ok($got, 'ne', $dont_expect, $name);
}

sub isnt_num($self, $got, $dont_expect, $name) {
    local $Level = $Level + 1;

    if( !defined $got || !defined $dont_expect ) {
        # undef only matches undef and nothing else
        my $test = defined $got || defined $dont_expect;

        $self->ok($test, $name);
        $self->_cmp_diag($got, '!=', $dont_expect) unless $test;
        return $test;
    }

    return $self->cmp_ok($got, '!=', $dont_expect, $name);
}


=item B<like>

  $Test->like($this, qr/$regex/, $name);
  $Test->like($this, '/$regex/', $name);

Like Test::More's like().  Checks if $this matches the given $regex.

You'll want to avoid qr// if you want your tests to work before 5.005.

=item B<unlike>

  $Test->unlike($this, qr/$regex/, $name);
  $Test->unlike($this, '/$regex/', $name);

Like Test::More's unlike().  Checks if $this B<does not match> the
given $regex.

=cut

sub like($self, $this, $regex, ?$name) {

    local $Level = $Level + 1;
    $self->_regex_ok($this, $regex, '=~', $name);
}

sub unlike($self, $this, $regex, ?$name) {

    local $Level = $Level + 1;
    $self->_regex_ok($this, $regex, '!~', $name);
}


=item B<cmp_ok>

  $Test->cmp_ok($this, $type, $that, $name);

Works just like Test::More's cmp_ok().

    $Test->cmp_ok($big_num, '!=', $other_big_num);

=cut


my %numeric_cmps = %( < @+: map { @($_, 1) }, 
 @(                       ("<",  "<=", ">",  ">=", "==", "!=", "<=>")) );

sub cmp_ok($self, $got, $type, $expect, ?$name) {

    my $test;
    do {
        local($^EVAL_ERROR,$^OS_ERROR);  # isolate eval

        my $code = $self->_caller_context;

        # Yes, it has to look like this or 5.4.5 won't see the #line 
        # directive.
        # Don't ask me, man, I just work here.
        $test = eval "
$code" . "\$got $type \$expect;";

    };
    local $Level = $Level + 1;
    my $ok = $self->ok($test, $name);

    unless( $ok ) {
        if( $type =~ m/^(eq|==)$/ ) {
            $self->_is_diag($got, $type, $expect);
        }
        else {
            $self->_cmp_diag($got, $type, $expect);
        }
    }
    return $ok;
}

sub _cmp_diag($self, $got, $type, $expect) {
    
    $got    = dump::view($got);
    $expect = dump::view($expect);
    
    local $Level = $Level + 1;
    return $self->diag(sprintf <<DIAGNOSTIC, $got, $type, $expect);
    \%s
        \%s
    \%s
DIAGNOSTIC
}


sub _caller_context {
    my $self = shift;

    my @(?$pack, ?$file, ?$line, ...) =  $self->caller(1);

    my $code = '';
    $code .= "#line $line $file\n" if defined $file and defined $line;

    return $code;
}

=back


=head2 Other Testing Methods

These are methods which are used in the course of writing a test but are not themselves tests.

=over 4

=item B<BAIL_OUT>

    $Test->BAIL_OUT($reason);

Indicates to the Test::Harness that things are going so badly all
testing should terminate.  This includes running any additional test
scripts.

It will exit with 255.

=cut

sub BAIL_OUT($self, $reason) {

    $self->{+Bailed_Out} = 1;
    $self->_print("Bail out!  $reason");
    exit 255;
}

=for deprecated
BAIL_OUT() used to be BAILOUT()

=cut

*BAILOUT = \&BAIL_OUT;


=item B<skip>

    $Test->skip;
    $Test->skip($why);

Skips the current test, reporting $why.

=cut

sub skip($self, $why) {
    $why ||= '';

    $self->_plan_check;

    lock($self->{?Curr_Test});
    $self->{+Curr_Test}++;

    $self->{Test_Results}->[+$self->{?Curr_Test}-1] = &share(\%(
        'ok'      => 1,
        actual_ok => 1,
        name      => '',
        type      => 'skip',
        reason    => $why,
    ));

    my $out = "ok";
    $out   .= " $self->{?Curr_Test}" if $self->use_numbers;
    $out   .= " # skip";
    $out   .= " $why"       if length $why;
    $out   .= "\n";

    $self->_print($out);

    return 1;
}


=item B<todo_skip>

  $Test->todo_skip;
  $Test->todo_skip($why);

Like skip(), only it will declare the test as failing and TODO.  Similar
to

    print "not ok $tnum # TODO $why\n";

=cut

sub todo_skip($self, $why) {
    $why ||= '';

    $self->_plan_check;

    lock($self->{?Curr_Test});
    $self->{+Curr_Test}++;

    $self->{Test_Results}->[+$self->{?Curr_Test}-1] = &share(\%(
        'ok'      => 1,
        actual_ok => 0,
        name      => '',
        type      => 'todo_skip',
        reason    => $why,
    ));

    my $out = "not ok";
    $out   .= " $self->{?Curr_Test}" if $self->use_numbers;
    $out   .= " # TODO & SKIP $why\n";

    $self->_print($out);

    return 1;
}


=begin _unimplemented

=item B<skip_rest>

  $Test->skip_rest;
  $Test->skip_rest($reason);

Like skip(), only it skips all the rest of the tests you plan to run
and terminates the test.

If you're running under no_plan, it skips once and terminates the
test.

=end _unimplemented

=back


=head2 Test building utility methods

These methods are useful when writing your own test methods.

=over 4

=item B<maybe_regex>

  $Test->maybe_regex(qr/$regex/);
  $Test->maybe_regex('/$regex/');

Convenience method for building testing functions that take regular
expressions as arguments, but need to work before perl 5.005.

Takes a quoted regular expression produced by qr//, or a string
representing a regular expression.

Returns a Perl value which may be used instead of the corresponding
regular expression, or undef if it's argument is not recognised.

For example, a version of like(), sans the useful diagnostic messages,
could be written as:

  sub laconic_like {
      my ($self, $this, $regex, $name) = @_;
      my $usable_regex = $self->maybe_regex($regex);
      die "expecting regex, found '$regex'\n"
          unless $usable_regex;
      $self->ok($this =~ m/$usable_regex/, $name);
  }

=cut


sub maybe_regex($self, $regex) {
    my $usable_regex = undef;

    return $usable_regex unless defined $regex;

    my($re, $opts);

    # Check for qr/foo/
    if (re::is_regexp($regex))
    {
        $usable_regex = $regex;
    }
    # Check for '/foo/' or 'm,foo,'
    elsif( @(?$re, ?$opts)        = @($regex =~ m{^ /(.*)/ (\w*) $ }sx)           or
           @(?_, ?$re, ?$opts) = @: $regex =~ m,^ m([^\w\s]) (.+) \1 (\w*) $,sx
         )
    {
        $usable_regex = length $opts ?? "(?$opts)$re" !! $re;
    }

    return $usable_regex;
}


sub _is_qr {
    my $regex = shift;
    
    # is_regexp() checks for regexes in a robust manner, say if they're
    # blessed.
    return re::is_regexp($regex) if defined &re::is_regexp;
    return ref $regex eq 'Regexp';
}


sub _regex_ok($self, $this, $regex, $cmp, $name) {

    my $ok = 0;
    my $usable_regex = $self->maybe_regex($regex);
    unless (defined $usable_regex) {
        $ok = $self->ok( 0, $name );
        $self->diag("    '$regex' doesn't look much like a regex to me.");
        return $ok;
    }

    do {
        my $test = $this =~ m/$usable_regex/ ?? 1 !! 0;

        $test = !$test if $cmp eq '!~';

        local $Level = $Level + 1;
        $ok = $self->ok( $test, $name );
    };

    unless( $ok ) {
        $this = dump::view($this);
        my $match = $cmp eq '=~' ?? "doesn't match" !! "matches";

        local $Level = $Level + 1;
        $self->diag(sprintf <<DIAGNOSTIC, $this, $match, $regex);
                  \%s
    \%13s '\%s'
DIAGNOSTIC

    }

    return $ok;
}


# I'm not ready to publish this.  It doesn't deal with array return
# values from the code or context.

=begin private

=item B<_try>

    my $return_from_code          = $Test->try(sub { code });

Works like eval BLOCK except it ensures it has no effect on the rest of the test (ie. $@ is not set) nor is effected by outside interference (ie. $SIG{__DIE__}) and works around some quirks in older Perls.

It is suggested you use this in place of eval BLOCK.

=cut

sub _try($self, $code) {
    
    local $^OS_ERROR = undef;               # eval can mess up $!
    local $^EVAL_ERROR = undef;               # don't set $@ in the test
    my $return = try { $code->() };
    
    return $return;
}

=end private


=item B<is_fh>

    my $is_fh = $Test->is_fh($thing);

Determines if the given $thing can be used as a filehandle.

=cut

sub is_fh {
    my $self = shift;
    my $maybe_fh = shift;
    return 0 unless defined $maybe_fh;

    return 1 if ref $maybe_fh  eq 'GLOB'; # its a glob ref
    return 1 if ref \$maybe_fh eq 'GLOB'; # its a glob

    return try { $maybe_fh->isa("IO::Handle") } ||
           # 5.5.4's tied() and can() doesn't like getting undef
           try { (tied($maybe_fh) || '')->can('TIEHANDLE') };
}


=back


=head2 Test style


=over 4

=item B<level>

    $Test->level($how_high);

How far up the call stack should $Test look when reporting where the
test failed.

Defaults to 1.

Setting L<$Test::Builder::Level> overrides.  This is typically useful
localized:

    sub my_ok {
        my $test = shift;

        local $Test::Builder::Level = $Test::Builder::Level + 1;
        $TB->ok($test);
    }

To be polite to other functions wrapping your own you usually want to increment C<$Level> rather than set it to a constant.

=cut

sub level($self, ?$level) {

    if( defined $level ) {
        $Level = $level;
    }
    return $Level;
}


=item B<use_numbers>

    $Test->use_numbers($on_or_off);

Whether or not the test should output numbers.  That is, this if true:

  ok 1
  ok 2
  ok 3

or this if false

  ok
  ok
  ok

Most useful when you can't depend on the test output order, such as
when threads or forking is involved.

Defaults to on.

=cut

sub use_numbers($self, ?$use_nums) {

    if( defined $use_nums ) {
        $self->{+Use_Nums} = $use_nums;
    }
    return $self->{?Use_Nums};
}


=item B<no_diag>

    $Test->no_diag($no_diag);

If set true no diagnostics will be printed.  This includes calls to
diag().

=item B<no_ending>

    $Test->no_ending($no_ending);

Normally, Test::Builder does some extra diagnostics when the test
ends.  It also changes the exit code as described below.

If this is true, none of that will be done.

=item B<no_header>

    $Test->no_header($no_header);

If set to true, no "1..N" header will be printed.

=cut

foreach my $attribute (qw(No_Header No_Ending No_Diag)) {
    my $method = lc $attribute;

    my $code = sub {
        my@($self, ?$no) =  @_;

        if( defined $no ) {
            $self->{+$attribute} = $no;
        }
        return $self->{?$attribute};
    };

    *{Symbol::fetch_glob(__PACKAGE__.'::'.$method)} = $code;
}


=back

=head2 Output

Controlling where the test output goes.

It's ok for your test to change where STDOUT and STDERR point to,
Test::Builder's default output settings will not be affected.

=over 4

=item B<diag>

    $Test->diag(@msgs);

Prints out the given @msgs.  Like C<print>, arguments are simply
appended together.

Normally, it uses the failure_output() handle, but if this is for a
TODO test, the todo_output() handle is used.

Output will be indented and marked with a # so as not to interfere
with test output.  A newline will be put on the end if there isn't one
already.

We encourage using this rather than calling print directly.

Returns false.  Why?  Because diag() is often used in conjunction with
a failing test (C<ok() || diag()>) it "passes through" the failure.

    return ok(...) || diag(...);

=for blame transfer
Mark Fowler <mark@twoshortplanks.com>

=cut

sub diag($self, @< @msgs) {

    return if $self->no_diag;
    return unless (nelems @msgs);

    # Prevent printing headers when compiling (i.e. -c)
    return if $^COMPILING;

    # Smash args together like print does.
    # Convert undef to 'undef' so its readable.
    my $msg = join '', map { defined($_) ?? $_ !! 'undef' }, @msgs;

    # Escape each line with a #.
    $msg =~ s/^/# /gm;

    # Stick a newline on the end if it needs it.
    $msg .= "\n" unless $msg =~ m/\n\Z/;

    local $Level = $Level + 1;
    $self->_print_diag($msg);

    return 0;
}

=item B<info>

    $Test->info(@msgs);

Prints out the given @msgs.  Like C<print>, arguments are simply
appended together.

=cut

sub info($self, @< @msgs) {

    return unless (nelems @msgs);

    # Prevent printing headers when compiling (i.e. -c)
    return if $^COMPILING;

    # Smash args together like print does.
    # Convert undef to 'undef' so its readable.
    my $msg = join '', map { defined($_) ?? $_ !! 'undef' }, @msgs;

    # Escape each line with a #.
    $msg =~ s/^/# /gm;

    # Stick a newline on the end if it needs it.
    $msg .= "\n" unless $msg =~ m/\n\Z/;

    local $Level = $Level + 1;
    $self->_print($msg);

    return 0;
}

=begin _private

=item B<_print>

    $Test->_print(@msgs);

Prints to the output() filehandle.

=end _private

=cut

sub _print($self, @< @msgs) {

    # Prevent printing headers when only compiling.  Mostly for when
    # tests are deparsed with B::Deparse
    return if $^COMPILING;

    my $msg = join '', @msgs;

    local @($^OUTPUT_RECORD_SEPARATOR, $^OUTPUT_FIELD_SEPARATOR) = @(undef, '');
    my $fh = $self->output;

    # Escape each line after the first with a # so we don't
    # confuse Test::Harness.
    $msg =~ s/\n(.)/\n# $1/sg;

    # Stick a newline on the end if it needs it.
    $msg .= "\n" unless $msg =~ m/\n\Z/;

    print $fh, $msg;
}

=begin private

=item B<_print_diag>

    $Test->_print_diag(@msg);

Like _print, but prints to the current diagnostic filehandle.

=end private

=cut

sub _print_diag {
    my $self = shift;

    local@($^OUTPUT_RECORD_SEPARATOR, $^OUTPUT_FIELD_SEPARATOR) = @(undef, '');
    my $fh = $self->todo ?? $self->todo_output !! $self->failure_output;
    print $fh, < @_;
}    

=item B<output>

    $Test->output($fh);
    $Test->output($file);

Where normal "ok/not ok" test output should go.

Defaults to STDOUT.

=item B<failure_output>

    $Test->failure_output($fh);
    $Test->failure_output($file);

Where diagnostic output on test failures and diag() should go.

Defaults to STDERR.

=item B<todo_output>

    $Test->todo_output($fh);
    $Test->todo_output($file);

Where diagnostics about todo test failures and diag() should go.

Defaults to STDOUT.

=cut

sub output($self, ?$fh) {

    if( defined $fh ) {
        $self->{+Out_FH} = $self->_new_fh($fh);
    }
    return $self->{?Out_FH};
}

sub failure_output($self, ?$fh) {

    if( defined $fh ) {
        $self->{+Fail_FH} = $self->_new_fh($fh);
    }
    return $self->{?Fail_FH};
}

sub todo_output($self, ?$fh) {

    if( defined $fh ) {
        $self->{+Todo_FH} = $self->_new_fh($fh);
    }
    return $self->{?Todo_FH};
}


sub _new_fh {
    my $self = shift;
    my@($file_or_fh) =@( shift);

    my $fh;
    if( $self->is_fh($file_or_fh) ) {
        $fh = $file_or_fh;
    }
    else {
        open $fh, ">", $file_or_fh or
            die("Can't open test output log $file_or_fh: $^OS_ERROR");
        _autoflush($fh);
    }

    return $fh;
}


sub _autoflush($fh) {
    iohandle::output_autoflush($fh, 1);
    return;
}


my($Testout, $Testerr);
sub _dup_stdhandles {
    my $self = shift;

    $self->_open_testhandles;

    # Set everything to unbuffered else plain prints to STDOUT will
    # come out in the wrong order from our own prints.
    _autoflush($Testout);
    _autoflush($^STDOUT);
    _autoflush($Testerr);
    _autoflush($^STDERR);

    $self->output        ($Testout);
    $self->failure_output($Testerr);
    $self->todo_output   ($Testout);
}


my $Opened_Testhandles = 0;
sub _open_testhandles {
    my $self = shift;
    
    return if $Opened_Testhandles;
    
    # We dup STDOUT and STDERR so people can change them in their
    # test suites while still getting normal test output.
    open($Testout, ">&", $^STDOUT) or die "Can't dup STDOUT:  $^OS_ERROR";
    open($Testerr, ">&", $^STDERR) or die "Can't dup STDERR:  $^OS_ERROR";

    $Opened_Testhandles = 1;
}


sub _copy_io_layers($self, $src, $dest) {
    
    $self->_try(sub {
        require PerlIO;
        my @layers = PerlIO::get_layers($src);
        
        binmode $dest, join " ", map { ":$_" }, @layers if (nelems @layers);
    });
}

sub _plan_check {
    my $self = shift;

    unless( $self->{?Have_Plan} ) {
        local $Level = $Level + 2;
        die("You tried to run a test without a plan");
    }
}

=back


=head2 Test Status and Info

=over 4

=item B<current_test>

    my $curr_test = $Test->current_test;
    $Test->current_test($num);

Gets/sets the current test number we're on.  You usually shouldn't
have to set this.

If set forward, the details of the missing tests are filled in as 'unknown'.
if set backward, the details of the intervening tests are deleted.  You
can erase history if you really want to.

=cut

sub current_test($self ?= $num) {

    lock($self->{?Curr_Test});
    if( $^is_assignment ) {
        unless( $self->{?Have_Plan} ) {
            die("Can't change the current test number without a plan!");
        }

        $self->{+Curr_Test} = $num;

        # If the test counter is being pushed forward fill in the details.
        my $test_results = $self->{?Test_Results};
        if( $num +> nelems @$test_results ) {
            my $start = (nelems @$test_results) ?? (nelems @$test_results) !! 0;
            for ($start..$num-1) {
                $test_results->[+$_] = &share(\%(
                    'ok'      => 1, 
                    actual_ok => undef, 
                    reason    => 'incrementing test number', 
                    type      => 'unknown', 
                    name      => undef 
                ));
            }
        }
        # If backward, wipe history.  Its their funeral.
        elsif( $num +< nelems @$test_results ) {
            splice @{$test_results}, $num;
        }
    }
    return $self->{?Curr_Test};
}


=item B<summary>

    my @tests = $Test->summary;

A simple summary of the tests so far.  True for pass, false for fail.
This is a logical pass/fail, so todos are passes.

Of course, test #1 is $tests[0], etc...

=cut

sub summary {
    my@($self) =@( shift);

    return map { $_->{?'ok'} }, @{ $self->{Test_Results} };
}

=item B<details>

    my @tests = $Test->details;

Like summary(), but with a lot more detail.

    $tests[$test_num - 1] = 
            { 'ok'       => is the test considered a pass?
              actual_ok  => did it literally say 'ok'?
              name       => name of the test (if any)
              type       => type of test (if any, see below).
              reason     => reason for the above (if any)
            };

'ok' is true if Test::Harness will consider the test to be a pass.

'actual_ok' is a reflection of whether or not the test literally
printed 'ok' or 'not ok'.  This is for examining the result of 'todo'
tests.  

'name' is the name of the test.

'type' indicates if it was a special test.  Normal tests have a type
of ''.  Type can be one of the following:

    skip        see skip()
    todo        see todo()
    todo_skip   see todo_skip()
    unknown     see below

Sometimes the Test::Builder test counter is incremented without it
printing any test output, for example, when current_test() is changed.
In these cases, Test::Builder doesn't know the result of the test, so
it's type is 'unkown'.  These details for these tests are filled in.
They are considered ok, but the name and actual_ok is left undef.

For example "not ok 23 - hole count # TODO insufficient donuts" would
result in this structure:

    $tests[22] =    # 23 - 1, since arrays start from 0.
      { ok        => 1,   # logically, the test passed since it's todo
        actual_ok => 0,   # in absolute terms, it failed
        name      => 'hole count',
        type      => 'todo',
        reason    => 'insufficient donuts'
      };

=cut

sub details {
    my $self = shift;
    return @{ $self->{?Test_Results} };
}

=item B<todo>

    my $todo_reason = $Test->todo;
    my $todo_reason = $Test->todo($pack);

todo() looks for a $TODO variable in your tests.  If set, all tests
will be considered 'todo' (see Test::More and Test::Harness for
details).  Returns the reason (ie. the value of $TODO) if running as
todo tests, false otherwise.

todo() is about finding the right package to look for $TODO in.  It's
pretty good at guessing the right package to look at.  It first looks for
the caller based on C<$Level + 1>, since C<todo()> is usually called inside
a test function.  As a last resort it will use C<exported_to()>.

Sometimes there is some confusion about where todo() should be looking
for the $TODO variable.  If you want to be sure, tell it explicitly
what $pack to use.

=cut

sub todo($self, ?$package) {

    return $self->{?TODO} if defined $self->{?TODO};

    $package = $package || $self->caller(1)[?0] || $self->exported_to;
    return 0 unless $package;

    return defined ${*{Symbol::fetch_glob($package.'::TODO')}} ?? ${*{Symbol::fetch_glob($package.'::TODO')}}
                                     !! 0;
}

=item B<caller>

    my $package = $Test->caller;
    my($pack, $file, $line) = $Test->caller;
    my($pack, $file, $line) = $Test->caller($height);

Like the normal caller(), except it reports according to your level().

C<$height> will be added to the level().

=cut

sub caller($self, ?$height) {
    $height ||= 0;

    my @caller = @( CORE::caller($self->level + $height + 1) );
    return @caller;
}

=back

=cut

=begin _private

=over 4

=item B<_sanity_check>

  $self->_sanity_check();

Runs a bunch of end of test sanity checks to make sure reality came
through ok.  If anything is wrong it will die with a fairly friendly
error message.

=cut

#'#
sub _sanity_check {
    my $self = shift;

    $self->_whoa($self->{?Curr_Test} +< 0,  'Says here you ran a negative number of tests!');
    $self->_whoa((!$self->{?Have_Plan} and $self->{?Curr_Test}),
          'Somehow your tests ran without a plan!');
    $self->_whoa($self->{?Curr_Test} != nelems @{ $self->{?Test_Results} },
          'Somehow you got a different number of results than tests ran!');
}

=item B<_whoa>

  $self->_whoa($check, $description);

A sanity check, similar to assert().  If the $check is true, something
has gone horribly wrong.  It will die with the given $description and
a note to contact the author.

=cut

sub _whoa($self, $check, $desc) {
    if( $check ) {
        local $Level = $Level + 1;
        die(<<"WHOA");
WHOA!  $desc
This should never happen!  Please contact the author immediately!
WHOA
    }
}

=item B<_my_exit>

  _my_exit($exit_num);

Perl seems to have some trouble with exiting inside an END block.  5.005_03
and 5.6.1 both seem to do odd things.  Instead, this function edits $?
directly.  It should ONLY be called from inside an END block.  It
doesn't actually exit, that's your job.

=cut

sub _my_exit {
    $^CHILD_ERROR ||= @_[0];

    return 1;
}


=back

=end _private

=cut

sub _ending {
    my $self = shift;

    my $real_exit_code = $^CHILD_ERROR;
    $self->_sanity_check();

    # Don't bother with an ending if this is a forked copy.  Only the parent
    # should do the ending.
    if( $self->{?Original_Pid} != $^PID ) {
        return;
    }
    
    # Exit if plan() was never called.  This is so "require Test::Simple" 
    # doesn't puke.
    if( !$self->{?Have_Plan} ) {
        return;
    }

    # Don't do an ending if we bailed out.
    if( $self->{?Bailed_Out} ) {
        return;
    }

    # Figure out if we passed or failed and print helpful messages.
    my $test_results = $self->{?Test_Results};
    if( (nelems @$test_results) ) {
        # The plan?  We have no plan.
        if( $self->{?No_Plan} ) {
            $self->_print("1..$self->{?Curr_Test}\n") unless $self->no_header;
            $self->{+Expected_Tests} = $self->{?Curr_Test};
        }

        # Auto-extended arrays and elements which aren't explicitly
        # filled in with a shared reference will puke under 5.8.0
        # ithreads.  So we have to fill them in by hand. :(
        my $empty_result = &share(\%());
        for my $idx ( 0..$self->{?Expected_Tests}-1 ) {
            $test_results->[+$idx] = $empty_result
              unless defined $test_results->[?$idx];
        }

        my $num_failed = nelems( grep { !$_->{?'ok'} },
            @{$test_results}[[0..$self->{?Curr_Test}-1]] );

        my $num_extra = $self->{?Curr_Test} - $self->{?Expected_Tests};

        if( $num_extra +< 0 ) {
            my $s = $self->{?Expected_Tests} == 1 ?? '' !! 's';
            $self->diag(<<"FAIL");
Looks like you planned $self->{?Expected_Tests} test$s but only ran $self->{?Curr_Test}.
FAIL
        }
        elsif( $num_extra +> 0 ) {
            my $s = $self->{?Expected_Tests} == 1 ?? '' !! 's';
            $self->diag(<<"FAIL");
Looks like you planned $self->{?Expected_Tests} test$s but ran $num_extra extra.
FAIL
        }

        if ( $num_failed ) {
            my $num_tests = $self->{?Curr_Test};
            my $s = $num_failed == 1 ?? '' !! 's';

            my $qualifier = $num_extra == 0 ?? '' !! ' run';

            $self->diag(<<"FAIL");
Looks like you failed $num_failed test$s of $num_tests$qualifier.
FAIL
        }

        if( $real_exit_code ) {
            $self->diag(<<"FAIL");
Looks like your test died just after $self->{?Curr_Test}.
FAIL

            _my_exit( 255 ) && return;
        }

        my $exit_code;
        if( $num_failed ) {
            $exit_code = $num_failed +<= 254 ?? $num_failed !! 254;
        }
        elsif( $num_extra != 0 ) {
            $exit_code = 255;
        }
        else {
            $exit_code = 0;
        }

        _my_exit( $exit_code ) && return;
    }
    elsif ( $self->{?Skip_All} ) {
        _my_exit( 0 ) && return;
    }
    elsif ( $real_exit_code ) {
        $self->diag(<<'FAIL');
Looks like your test died before it could output anything.
FAIL
        _my_exit( 255 ) && return;
    }
    else {
        $self->diag("No tests run!\n");
        _my_exit( 255 ) && return;
    }
}

END {
    $Test->_ending if defined $Test and !$Test->no_ending;
}

=head1 EXIT CODES

If all your tests passed, Test::Builder will exit with zero (which is
normal).  If anything failed it will exit with how many failed.  If
you run less (or more) tests than you planned, the missing (or extras)
will be considered failures.  If no tests were ever run Test::Builder
will throw a warning and exit with 255.  If the test died, even after
having successfully completed all its tests, it will still be
considered a failure and will exit with 255.

So the exit codes are...

    0                   all tests successful
    255                 test died or all passed but wrong # of tests run
    any other number    how many failed (including missing or extras)

If you fail more than 254 tests, it will be reported as 254.


=head1 THREADS

In perl 5.8.1 and later, Test::Builder is thread-safe.  The test
number is shared amongst all threads.  This means if one thread sets
the test number using current_test() they will all be effected.

While versions earlier than 5.8.1 had threads they contain too many
bugs to support.

Test::Builder is only thread-aware if threads.pm is loaded I<before>
Test::Builder.

=head1 EXAMPLES

CPAN can provide the best examples.  Test::Simple, Test::More,
Test::Exception and Test::Differences all use Test::Builder.

=head1 SEE ALSO

Test::Simple, Test::More, Test::Harness

=head1 AUTHORS

Original code by chromatic, maintained by Michael G Schwern
E<lt>schwern@pobox.comE<gt>

=head1 COPYRIGHT

Copyright 2002, 2004 by chromatic E<lt>chromatic@wgz.orgE<gt> and
                        Michael G Schwern E<lt>schwern@pobox.comE<gt>.

This program is free software; you can redistribute it and/or 
modify it under the same terms as Perl itself.

See F<http://www.perl.com/perl/misc/Artistic.html>

=cut

1;
