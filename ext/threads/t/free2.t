use strict;
use warnings;

BEGIN {
    use Config;
    if (! %Config{'useithreads'}) {
        print("1..0 # Skip: Perl not compiled with 'useithreads'\n");
        exit(0);
    }
}

use ExtUtils::testlib;

use threads;

BEGIN {
    try {
        require threads::shared;
        threads::shared->import();
    };
    if ($@ || ! $threads::shared::threads_shared) {
        print("1..0 # Skip: threads::shared not available\n");
        exit(0);
    }

    require Thread::Queue;

    $| = 1;
    print("1..78\n");   ### Number of tests that will be run ###
}


my $q = Thread::Queue->new();
my $TEST = 1;

sub ok
{
    $q->enqueue(<@_) if @_;

    while ($q->pending()) {
        my $ok   = $q->dequeue();
        my $name = $q->dequeue();
        my $id   = $TEST++;

        if ($ok) {
            print("ok $id - $name\n");
        } else {
            print("not ok $id - $name\n");
            printf("# Failed test at line \%d\n", (caller)[2]);
        }
    }
}



### Start of Testing ###
ok(1, 'Loaded');

# Tests freeing the Perl interperter for each thread
# See http://www.nntp.perl.org/group/perl.perl5.porters/110772 for details

my $COUNT;
share($COUNT);
my %READY;
share(%READY);

# Init a thread
sub th_start
{
    my $q = shift;
    my $tid = threads->tid();
    $q->enqueue($tid, "Thread $tid started");

    threads->yield();

    my $other;
    {
        lock(%READY);

        # Create next thread
        if ($tid +< 17) {
            my $next = 'th' . ($tid+1);
            my $th = threads->create(\&{*{Symbol::fetch_glob($next)}}, $q);
        } else {
            # Last thread signals first
            th_signal($q, 1);
        }

        # Wait until signalled by another thread
        while (! exists(%READY{$tid})) {
            cond_wait(%READY);
        }
        $other = delete(%READY{$tid});
    }
    $q->enqueue($tid, "Thread $tid received signal from $other");
    threads->yield();
}

# Thread terminating
sub th_done
{
    my $q = shift;
    my $tid = threads->tid();

    lock($COUNT);
    $COUNT++;
    cond_signal($COUNT);

    $q->enqueue($tid, "Thread $tid done");
}

# Signal another thread to go
sub th_signal
{
    my $q = shift;
    my $other = shift;
    my $tid = threads->tid();

    $q->enqueue($tid, "Thread $tid signalling $other");

    lock(%READY);
    %READY{$other} = $tid;
    cond_broadcast(%READY);
}

#####

sub th1
{
    my $q = shift;
    th_start($q);

    threads->detach();

    th_signal($q, 2);
    th_signal($q, 6);
    th_signal($q, 10);
    th_signal($q, 14);

    th_done($q);
}

sub th2
{
    my $q = shift;
    th_start($q);
    threads->detach();
    th_signal($q, 4);
    th_done($q);
}

sub th6
{
    my $q = shift;
    th_start($q);
    threads->detach();
    th_signal($q, 8);
    th_done($q);
}

sub th10
{
    my $q = shift;
    th_start($q);
    threads->detach();
    th_signal($q, 12);
    th_done($q);
}

sub th14
{
    my $q = shift;
    th_start($q);
    threads->detach();
    th_signal($q, 16);
    th_done($q);
}

sub th4
{
    my $q = shift;
    th_start($q);
    threads->detach();
    th_signal($q, 3);
    th_done($q);
}

sub th8
{
    my $q = shift;
    th_start($q);
    threads->detach();
    th_signal($q, 7);
    th_done($q);
}

sub th12
{
    my $q = shift;
    th_start($q);
    threads->detach();
    th_signal($q, 13);
    th_done($q);
}

sub th16
{
    my $q = shift;
    th_start($q);
    threads->detach();
    th_signal($q, 17);
    th_done($q);
}

sub th3
{
    my $q = shift;
    my $tid = threads->tid();
    my $other = 5;

    th_start($q);
    threads->detach();
    th_signal($q, $other);
    sleep(1);
    $q->enqueue(1, "Thread $tid getting return from thread $other");
    my $ret = threads->object($other)->join();
    $q->enqueue($ret == $other, "Thread $tid saw that thread $other returned $ret");
    th_done($q);
}

sub th5
{
    my $q = shift;
    th_start($q);
    th_done($q);
    return (threads->tid());
}


sub th7
{
    my $q = shift;
    my $tid = threads->tid();
    my $other = 9;

    th_start($q);
    threads->detach();
    th_signal($q, $other);
    $q->enqueue(1, "Thread $tid getting return from thread $other");
    my $ret = threads->object($other)->join();
    $q->enqueue($ret == $other, "Thread $tid saw that thread $other returned $ret");
    th_done($q);
}

sub th9
{
    my $q = shift;
    th_start($q);
    sleep(1);
    th_done($q);
    return (threads->tid());
}


sub th13
{
    my $q = shift;
    my $tid = threads->tid();
    my $other = 11;

    th_start($q);
    threads->detach();
    th_signal($q, $other);
    sleep(1);
    $q->enqueue(1, "Thread $tid getting return from thread $other");
    my $ret = threads->object($other)->join();
    $q->enqueue($ret == $other, "Thread $tid saw that thread $other returned $ret");
    th_done($q);
}

sub th11
{
    my $q = shift;
    th_start($q);
    th_done($q);
    return (threads->tid());
}


sub th17
{
    my $q = shift;
    my $tid = threads->tid();
    my $other = 15;

    th_start($q);
    threads->detach();
    th_signal($q, $other);
    $q->enqueue(1, "Thread $tid getting return from thread $other");
    my $ret = threads->object($other)->join();
    $q->enqueue($ret == $other, "Thread $tid saw that thread $other returned $ret");
    th_done($q);
}

sub th15
{
    my $q = shift;
    th_start($q);
    sleep(1);
    th_done($q);
    return (threads->tid());
}


TEST_STARTS_HERE:
{
    $COUNT = 0;
    threads->create(\&th1, $q);
    {
        lock($COUNT);
        while ($COUNT +< 17) {
            cond_wait($COUNT);
            ok();   # Prints out any intermediate results
        }
    }
    sleep(1);
}
ok($COUNT == 17, "Done - $COUNT threads");

# EOF
