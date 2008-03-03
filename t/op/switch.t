#!./perl

use strict;
use warnings;

use Test::More tests => 103;

# The behaviour of the feature pragma should be tested by lib/switch.t
# using the tests in t/lib/switch/*. This file tests the behaviour of
# the switch ops themselves.
              

use feature 'switch';
no warnings "numeric";

eval { continue };
like($@->{description}, qr/^Can't "continue" outside/, "continue outside");

eval { break };
like($@->{description}, qr/^Can't "break" outside/, "break outside");

# Scoping rules

{
    my $x = "foo";
    given(my $x = "bar") {
	is($x, "bar", "given scope starts");
    }
    is($x, "foo", "given scope ends");
}

sub be_true {1}

given(my $x = "foo") {
    when(be_true(my $x = "bar")) {
	is($x, "bar", "given scope starts");
    }
    is($x, "foo", "given scope ends");
}

$_ = "outside";
given("inside") { check_outside1() }
sub check_outside1 { is($_, "outside", "\$_ lexically scoped") }

{
    my $_ = "outside";
    given("inside") { check_outside2() }
    sub check_outside2 {
	is($_, "outside", "\$_ lexically scoped (lexical \$_)")
    }
}

# Basic string/numeric comparisons and control flow

{    
    my $ok;
    given(3) {
	when(2) { $ok = 'two'; }
	when(3) { $ok = 'three'; }
	when(4) { $ok = 'four'; }
	default { $ok = 'd'; }
    }
    is($ok, 'three', "numeric comparison");
}

{    
    my $ok;
    use integer;
    given(3.14159265) {
	when(2) { $ok = 'two'; }
	when(3) { $ok = 'three'; }
	when(4) { $ok = 'four'; }
	default { $ok = 'd'; }
    }
    is($ok, 'three', "integer comparison");
}

{    
    my ($ok1, $ok2);
    given(3) {
	when(3.1)   { $ok1 = 'n'; }
	when(3.0)   { $ok1 = 'y'; continue }
	when("3.0") { $ok2 = 'y'; }
	default     { $ok2 = 'n'; }
    }
    is($ok1, 'y', "more numeric (pt. 1)");
    is($ok2, 'y', "more numeric (pt. 2)");
}

{
    my $ok;
    given("c") {
	when("b") { $ok = 'B'; }
	when("c") { $ok = 'C'; }
	when("d") { $ok = 'D'; }
	default   { $ok = 'def'; }
    }
    is($ok, 'C', "string comparison");
}

{
    my $ok;
    given("c") {
	when("b") { $ok = 'B'; }
	when("c") { $ok = 'C'; continue }
	when("c") { $ok = 'CC'; }
	default   { $ok = 'D'; }
    }
    is($ok, 'CC', "simple continue");
}

# Definedness
{
    my $ok = 1;
    given (0) { when(undef) {$ok = 0} }
    is($ok, 1, "Given(0) when(undef)");
}
{
    my $undef;
    my $ok = 1;
    given (0) { when($undef) {$ok = 0} }
    is($ok, 1, 'Given(0) when($undef)');
}
{
    my $undef;
    my $ok = 0;
    given (0) { when($undef++) {$ok = 1} }
    is($ok, 1, "Given(0) when($undef++)");
}
{
    my $ok = 1;
    given (undef) { when(0) {$ok = 0} }
    is($ok, 1, "Given(undef) when(0)");
}
{
    my $undef;
    my $ok = 1;
    given ($undef) { when(0) {$ok = 0} }
    is($ok, 1, 'Given($undef) when(0)');
}
########
{
    my $ok = 1;
    given ("") { when(undef) {$ok = 0} }
    is($ok, 1, 'Given("") when(undef)');
}
{
    my $undef;
    my $ok = 1;
    given ("") { when($undef) {$ok = 0} }
    is($ok, 1, 'Given("") when($undef)');
}
{
    my $ok = 1;
    given (undef) { when("") {$ok = 0} }
    is($ok, 1, 'Given(undef) when("")');
}
{
    my $undef;
    my $ok = 1;
    given ($undef) { when("") {$ok = 0} }
    is($ok, 1, 'Given($undef) when("")');
}
########
{
    my $ok = 0;
    given (undef) { when(undef) {$ok = 1} }
    is($ok, 1, "Given(undef) when(undef)");
}
{
    my $undef;
    my $ok = 0;
    given (undef) { when($undef) {$ok = 1} }
    is($ok, 1, 'Given(undef) when($undef)');
}
{
    my $undef;
    my $ok = 0;
    given ($undef) { when(undef) {$ok = 1} }
    is($ok, 1, 'Given($undef) when(undef)');
}
{
    my $undef;
    my $ok = 0;
    given ($undef) { when($undef) {$ok = 1} }
    is($ok, 1, 'Given($undef) when($undef)');
}


# Regular expressions
{
    my ($ok1, $ok2);
    given("Hello, world!") {
	when(m/lo/)
	    { $ok1 = 'y'; continue}
	when(m/no/)
	    { $ok1 = 'n'; continue}
	when(m/^(Hello,|Goodbye cruel) world[!.?]/)
	    { $ok2 = 'Y'; continue}
	when(m/^(Hello cruel|Goodbye,) world[!.?]/)
	    { $ok2 = 'n'; continue}
    }
    is($ok1, 'y', "regex 1");
    is($ok2, 'Y', "regex 2");
}

# Comparisons
{
    my $test = "explicit numeric comparison (<)";
    my $twenty_five = 25;
    my $ok;
    given($twenty_five) {
	when ($_ +< 10) { $ok = "ten" }
	when ($_ +< 20) { $ok = "twenty" }
	when ($_ +< 30) { $ok = "thirty" }
	when ($_ +< 40) { $ok = "forty" }
	default        { $ok = "default" }
    }
    is($ok, "thirty", $test);
}

{
    use integer;
    my $test = "explicit numeric comparison (integer <)";
    my $twenty_five = 25;
    my $ok;
    given($twenty_five) {
	when ($_ +< 10) { $ok = "ten" }
	when ($_ +< 20) { $ok = "twenty" }
	when ($_ +< 30) { $ok = "thirty" }
	when ($_ +< 40) { $ok = "forty" }
	default        { $ok = "default" }
    }
    is($ok, "thirty", $test);
}

{
    my $test = "explicit numeric comparison (<=)";
    my $twenty_five = 25;
    my $ok;
    given($twenty_five) {
	when ($_ +<= 10) { $ok = "ten" }
	when ($_ +<= 20) { $ok = "twenty" }
	when ($_ +<= 30) { $ok = "thirty" }
	when ($_ +<= 40) { $ok = "forty" }
	default         { $ok = "default" }
    }
    is($ok, "thirty", $test);
}

{
    use integer;
    my $test = "explicit numeric comparison (integer <=)";
    my $twenty_five = 25;
    my $ok;
    given($twenty_five) {
	when ($_ +<= 10) { $ok = "ten" }
	when ($_ +<= 20) { $ok = "twenty" }
	when ($_ +<= 30) { $ok = "thirty" }
	when ($_ +<= 40) { $ok = "forty" }
	default         { $ok = "default" }
    }
    is($ok, "thirty", $test);
}


{
    my $test = "explicit numeric comparison (>)";
    my $twenty_five = 25;
    my $ok;
    given($twenty_five) {
	when ($_ +> 40) { $ok = "forty" }
	when ($_ +> 30) { $ok = "thirty" }
	when ($_ +> 20) { $ok = "twenty" }
	when ($_ +> 10) { $ok = "ten" }
	default        { $ok = "default" }
    }
    is($ok, "twenty", $test);
}

{
    my $test = "explicit numeric comparison (>=)";
    my $twenty_five = 25;
    my $ok;
    given($twenty_five) {
	when ($_ +>= 40) { $ok = "forty" }
	when ($_ +>= 30) { $ok = "thirty" }
	when ($_ +>= 20) { $ok = "twenty" }
	when ($_ +>= 10) { $ok = "ten" }
	default         { $ok = "default" }
    }
    is($ok, "twenty", $test);
}

{
    use integer;
    my $test = "explicit numeric comparison (integer >)";
    my $twenty_five = 25;
    my $ok;
    given($twenty_five) {
	when ($_ +> 40) { $ok = "forty" }
	when ($_ +> 30) { $ok = "thirty" }
	when ($_ +> 20) { $ok = "twenty" }
	when ($_ +> 10) { $ok = "ten" }
	default        { $ok = "default" }
    }
    is($ok, "twenty", $test);
}

{
    use integer;
    my $test = "explicit numeric comparison (integer >=)";
    my $twenty_five = 25;
    my $ok;
    given($twenty_five) {
	when ($_ +>= 40) { $ok = "forty" }
	when ($_ +>= 30) { $ok = "thirty" }
	when ($_ +>= 20) { $ok = "twenty" }
	when ($_ +>= 10) { $ok = "ten" }
	default         { $ok = "default" }
    }
    is($ok, "twenty", $test);
}

# Optimized-away comparisons
{
    my $ok;
    given(23) {
	when (2 + 2 == 4) { $ok = 'y'; continue }
	when (2 + 2 == 5) { $ok = 'n' }
    }
    is($ok, 'y', "Optimized-away comparison");
}

# File tests
#  (How to be both thorough and portable? Pinch a few ideas
#  from t/op/filetest.t. We err on the side of portability for
#  the time being.)

{
    my ($ok_d, $ok_f, $ok_r);
    given("op") {
	when(-d)  {$ok_d = 1; continue}
	when(!-f) {$ok_f = 1; continue}
	when(-r)  {$ok_r = 1; continue}
    }
    ok($ok_d, "Filetest -d");
    ok($ok_f, "Filetest -f");
    ok($ok_r, "Filetest -r");
}

# Sub and method calls
sub bar {"bar"}
{
    my $ok = 0;
    given("foo") {
	when(bar()) {$ok = 1}
    }
    ok($ok, "Sub call acts as boolean")
}

{
    my $ok = 0;
    given("foo") {
	when(main->bar()) {$ok = 1}
    }
    ok($ok, "Class-method call acts as boolean")
}

{
    my $ok = 0;
    my $obj = bless [];
    given("foo") {
	when($obj->bar()) {$ok = 1}
    }
    ok($ok, "Object-method call acts as boolean")
}

# Other things that should not be smart matched
{
    my $ok = 0;
    given(12) {
        when( m/(\d+)/ and ( 1 +<= $1 and $1 +<= 12 ) ) {
            $ok = 1;
        }
    }
    ok($ok, "bool not smartmatches");
}

{
    my $ok = 0;
    given(0) {
	when(eof(DATA)) {
	    $ok = 1;
	}
    }
    ok($ok, "eof() not smartmatched");
}

{
    my $ok = 0;
    my %foo = ("bar", 0);
    given(0) {
	when(exists $foo{bar}) {
	    $ok = 1;
	}
    }
    ok($ok, "exists() not smartmatched");
}

{
    my $ok = 0;
    given(0) {
	when(defined $ok) {
	    $ok = 1;
	}
    }
    ok($ok, "defined() not smartmatched");
}

{
    my $ok = 1;
    given("foo") {
	when((1 == 1) && "bar") {
	    $ok = 0;
	}
	when((1 == 1) && $_ eq "foo") {
	    $ok = 2;
	}
    }
    is($ok, 2, "((1 == 1) && \"bar\") not smartmatched");
}

{
    my $ok = 0;
    given("foo") {
	when((1 == $ok) || "foo") {
	    $ok = 1;
	}
    }
    ok($ok, '((1 == $ok) || "foo") smartmatched');
}


# Make sure we aren't invoking the get-magic more than once

{ # A helper class to count the number of accesses.
    package FetchCounter;
    sub TIESCALAR {
	my ($class) = @_;
	bless {value => undef, count => 0}, $class;
    }
    sub STORE {
        my ($self, $val) = @_;
        $self->{count} = 0;
        $self->{value} = $val;
    }
    sub FETCH {
	my ($self) = @_;
	# Avoid pre/post increment here
	$self->{count} = 1 + $self->{count};
	$self->{value};
    }
    sub count {
	my ($self) = @_;
	$self->{count};
    }
}

my $f = tie my $v, "FetchCounter";

{   my $test_name = "Only one FETCH (in given)";
    my $ok;
    given($v = 23) {
    	when(undef) {}
    	when(sub{0}->()) {}
	when(21) {}
	when("22") {}
	when(23) {$ok = 1}
	when(m/24/) {$ok = 0}
    }
    is($ok, 1, "precheck: $test_name");
    is($f->count(), 1, $test_name);
}

{   my $test_name = "Only one FETCH (numeric when)";
    my $ok;
    $v = 23;
    is($f->count(), 0, "Sanity check: $test_name");
    given(23) {
    	when(undef) {}
    	when(sub{0}->()) {}
	when(21) {}
	when("22") {}
	when($v) {$ok = 1}
	when(m/24/) {$ok = 0}
    }
    is($ok, 1, "precheck: $test_name");
    is($f->count(), 1, $test_name);
}

{   my $test_name = "Only one FETCH (string when)";
    my $ok;
    $v = "23";
    is($f->count(), 0, "Sanity check: $test_name");
    given("23") {
    	when(undef) {}
    	when(sub{0}->()) {}
	when("21") {}
	when("22") {}
	when($v) {$ok = 1}
	when(m/24/) {$ok = 0}
    }
    is($ok, 1, "precheck: $test_name");
    is($f->count(), 1, $test_name);
}

{   my $test_name = "Only one FETCH (undef)";
    my $ok;
    $v = undef;
    is($f->count(), 0, "Sanity check: $test_name");
    given(my $undef) {
    	when(sub{0}->()) {}
	when("21")  {}
	when("22")  {}
    	when($v)    {$ok = 1}
	when(undef) {$ok = 0}
    }
    is($ok, 1, "precheck: $test_name");
    is($f->count(), 1, $test_name);
}

# Loop topicalizer
{
    my $first = 1;
    for (1, "two") {
	when ("two") {
	    is($first, 0, "Loop: second");
	    eval {break};
	    like($@->{description}, qr/^Can't "break" in a loop topicalizer/,
	    	q{Can't "break" in a loop topicalizer});
	}
	when (1) {
	    is($first, 1, "Loop: first");
	    $first = 0;
	    # Implicit break is okay
	}
    }
}

{
    my $first = 1;
    for $_ (1, "two") {
	when ("two") {
	    is($first, 0, "Explicit \$_: second");
	    eval {break};
	    like($@->{description}, qr/^Can't "break" in a loop topicalizer/,
	    	q{Can't "break" in a loop topicalizer});
	}
	when (1) {
	    is($first, 1, "Explicit \$_: first");
	    $first = 0;
	    # Implicit break is okay
	}
    }
}

{
    my $first = 1;
    my $_;
    for (1, "two") {
	when ("two") {
	    is($first, 0, "Implicitly lexical loop: second");
	    eval {break};
	    like($@->{description}, qr/^Can't "break" in a loop topicalizer/,
	    	q{Can't "break" in a loop topicalizer});
	}
	when (1) {
	    is($first, 1, "Implicitly lexical loop: first");
	    $first = 0;
	    # Implicit break is okay
	}
    }
}

{
    my $first = 1;
    my $_;
    for $_ (1, "two") {
	when ("two") {
	    is($first, 0, "Implicitly lexical, explicit \$_: second");
	    eval {break};
	    like($@->{description}, qr/^Can't "break" in a loop topicalizer/,
	    	q{Can't "break" in a loop topicalizer});
	}
	when (1) {
	    is($first, 1, "Implicitly lexical, explicit \$_: first");
	    $first = 0;
	    # Implicit break is okay
	}
    }
}

{
    my $first = 1;
    for my $_ (1, "two") {
	when ("two") {
	    is($first, 0, "Lexical loop: second");
	    eval {break};
	    like($@->{description}, qr/^Can't "break" in a loop topicalizer/,
	    	q{Can't "break" in a loop topicalizer});
	}
	when (1) {
	    is($first, 1, "Lecical loop: first");
	    $first = 0;
	    # Implicit break is okay
	}
    }
}


# Code references
{
    no warnings "redefine";
    my $called_foo = 0;
    sub foo {$called_foo = 1}
    my $called_bar = 0;
    sub bar {$called_bar = 1}
    my ($matched_foo, $matched_bar) = (0, 0);
    given(\&foo) {
	when(\&bar) {$matched_bar = 1}
	when(\&foo) {$matched_foo = 1}
    }
    is($called_foo, 0,  "Code ref comparison: foo not called");
    is($called_bar, 0,  "Code ref comparison: bar not called");
    is($matched_bar, 0, "Code ref didn't match different one");
    is($matched_foo, 1, "Code ref did match itself");
}

sub contains_x {
    my $x = shift;
    return ($x =~ m/x/);
}
{
    my ($ok1, $ok2) = (0,0);
    given("foxy!") {
	when(contains_x($_))
	    { $ok1 = 1; continue }
	when(\&contains_x)
	    { $ok2 = 1; continue }
    }
    is($ok1, 1, "Calling sub directly (true)");
    is($ok2, 1, "Calling sub indirectly (true)");

    given("foggy") {
	when(contains_x($_))
	    { $ok1 = 2; continue }
	when(\&contains_x)
	    { $ok2 = 2; continue }
    }
    is($ok1, 1, "Calling sub directly (false)");
    is($ok2, 1, "Calling sub indirectly (false)");
}

# Test overloading
{ package OverloadTest;

    use overload '""' => sub{"string value of obj"};

    use overload "~~" => sub {
        my ($self, $other, $reversed) = @_;
        if ($reversed) {
	    $self->{left}  = $other;
	    $self->{right} = $self;
	    $self->{reversed} = 1;
        } else {
	    $self->{left}  = $self;
	    $self->{right} = $other;
	    $self->{reversed} = 0;
        }
	$self->{called} = 1;
	return $self->{retval};
    };
    
    sub new {
	my ($pkg, $retval) = @_;
	bless {
	    called => 0,
	    retval => $retval,
	}, $pkg;
    }
}

{
    my $test = "Overloaded obj in given (true)";
    my $obj = OverloadTest->new(1);
    my $matched;
    given($obj) {
	when ("other arg") {$matched = 1}
	default {$matched = 0}
    }
    
    is($obj->{called},  1, "$test: called");
    ok($matched, "$test: matched");
    is($obj->{left}, "string value of obj", "$test: left");
    is($obj->{right}, "other arg", "$test: right");
    ok(!$obj->{reversed}, "$test: not reversed");
}

{
    my $test = "Overloaded obj in given (false)";
    my $obj = OverloadTest->new(0);
    my $matched;
    given($obj) {
	when ("other arg") {$matched = 1}
    }
    
    is($obj->{called},  1, "$test: called");
    ok(!$matched, "$test: not matched");
    is($obj->{left}, "string value of obj", "$test: left");
    is($obj->{right}, "other arg", "$test: right");
    ok(!$obj->{reversed}, "$test: not reversed");
}

{
    my $test = "Overloaded obj in when (true)";
    my $obj = OverloadTest->new(1);
    my $matched;
    given("topic") {
	when ($obj) {$matched = 1}
	default {$matched = 0}
    }
    
    is($obj->{called},  1, "$test: called");
    ok($matched, "$test: matched");
    is($obj->{left}, "topic", "$test: left");
    is($obj->{right}, "string value of obj", "$test: right");
    ok($obj->{reversed}, "$test: reversed");
}

{
    my $test = "Overloaded obj in when (false)";
    my $obj = OverloadTest->new(0);
    my $matched;
    given("topic") {
	when ($obj) {$matched = 1}
	default {$matched = 0}
    }
    
    is($obj->{called}, 1, "$test: called");
    ok(!$matched, "$test: not matched");
    is($obj->{left}, "topic", "$test: left");
    is($obj->{right}, "string value of obj", "$test: right");
    ok($obj->{reversed}, "$test: reversed");
}

# Okay, that'll do for now. The intricacies of the smartmatch
# semantics are tested in t/op/smartmatch.t
__END__
