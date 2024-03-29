#!/usr/bin/perl -w

# Test Test::Builder->reset;

BEGIN {
    if( env::var('PERL_CORE') ) {
        chdir 't';
        $^INCLUDE_PATH = @('../lib', 'lib');
    }
    else {
        unshift $^INCLUDE_PATH, 't/lib';
    }
}
chdir 't';


use Test::Builder;
my $tb = Test::Builder->new;

my %Original_Output;
%Original_Output{+$_} = $tb->?$_ for qw(output failure_output todo_output);


$tb->plan(tests => 14);
$tb->level(0);

# Alter the state of Test::Builder as much as possible.
$tb->ok(1, "Running a test to alter TB's state");

my $tmpfile = 'foo.tmp';

$tb->output($tmpfile);
$tb->failure_output($tmpfile);
$tb->todo_output($tmpfile);
END { 1 while unlink $tmpfile }

# This won't print since we just sent output off to oblivion.
$tb->ok(0, "And a failure for fun");

$Test::Builder::Level = 3;

$tb->exported_to('Foofer');

$tb->use_numbers(0);
$tb->no_header(1);
$tb->no_ending(1);


# Now reset it.
$tb->reset;

my $test_num = 2;   # since we already printed 1
# Utility testing functions.
sub ok($test, $name) {
    my $ok = '';
    $ok .= "not " unless $test;
    $ok .= "ok $test_num";
    $ok .= " - $name" if defined $name;
    $ok .= "\n";
    print $^STDOUT, $ok;
    $test_num++;

    return $test;
}


ok( !defined $tb->exported_to,          'exported_to' );
ok( $tb->expected_tests == 0,           'expected_tests' );
ok( $tb->level          == 1,           'level' );
ok( $tb->use_numbers    == 1,           'use_numbers' );
ok( $tb->no_header      == 0,           'no_header' );
ok( $tb->no_ending      == 0,           'no_ending' );
ok( fileno $tb->output         == fileno %Original_Output{?output},    
                                        'output' );
ok( fileno $tb->failure_output == fileno %Original_Output{?failure_output},    
                                        'failure_output' );
ok( fileno $tb->todo_output    == fileno %Original_Output{?todo_output},
                                        'todo_output' );
ok( $tb->current_test   == 0,           'current_test' );
ok( (nelems $tb->summary)        == 0,           'summary' );
ok( (nelems $tb->details)        == 0,           'details' );

$tb->no_ending(1);
$tb->no_header(1);
$tb->plan(tests => 14);
$tb->current_test = 13;
$tb->level(0);
$tb->ok(1, 'final test to make sure output was reset');
