
# as of 2.09 on win32 Storable w/threads dies with "free to wrong
# pool" since it uses the same context for different threads. since
# win32 perl implementation allocates a different memory pool for each
# thread using the a memory pool from one thread to allocate memory
# for another thread makes win32 perl very unhappy
#
# but the problem exists everywhere, not only on win32 perl , it's
# just hard to catch it deterministically - since the same context is
# used if two or more threads happen to change the state of the
# context in the middle of the operation, and those operations aren't
# atomic per thread, bad things including data loss and corrupted data
# can happen.
#
# this has been solved in 2.10 by adding a Storable::CLONE which calls
# Storable::init_perinterp() to create a new context for each new
# thread when it starts

sub BEGIN {
    if (%ENV{PERL_CORE}){
	chdir('t') if -d 't';
	@INC = ('.', '../lib');
    } else {
	unshift @INC, 't';
    }
    require Config; Config->import;
    if (%ENV{PERL_CORE} and %Config{'extensions'} !~ m/\bStorable\b/) {
        print "1..0 # Skip: Storable was not built\n";
        exit 0;
    }
    unless (%Config{'useithreads'} and eval { require threads; 1 }) {
        print "1..0 # Skip: no threads\n";
        exit 0;
    }
    # - is \W, so can't use \b at start. Negative look ahead and look behind
    # works at start/end of string, or where preceded/followed by spaces
}

use Test::More;

use strict;

use threads;
use Storable qw(nfreeze);

plan tests => 2;

threads->new(\&sub1);

$_->join() for threads->list();

ok 1;

sub sub1 {
    nfreeze {};
    ok 1;
}
