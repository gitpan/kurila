#  -*- perl -*-
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl File-Temp.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use Test::More tests => 10;
BEGIN { use_ok('File::Temp') };

#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.

# make sure we can create a tmp file...
my $tmp = File::Temp->new;
isa_ok( $tmp, 'File::Temp' );
isa_ok( $tmp, 'IO::Handle' );
isa_ok( $tmp, 'IO::Seekable' );

# make sure the seek method is available...
ok( File::Temp->can('seek'), 'tmp can seek' );

# make sure IO::Handle methods are still there...
ok( File::Temp->can('print'), 'tmp can print' );

# let's see what we're exporting...
my $c = scalar nelems @File::Temp::EXPORT;
my $l = join ' ', @File::Temp::EXPORT;
ok( $c == 9, "really exporting $c: $l" );

ok(defined try { SEEK_SET() }, 'SEEK_SET defined by File::Temp') or diag $^EVAL_ERROR;
ok(defined try { SEEK_END() }, 'SEEK_END defined by File::Temp') or diag $^EVAL_ERROR;
ok(defined try { SEEK_CUR() }, 'SEEK_CUR defined by File::Temp') or diag $^EVAL_ERROR;
