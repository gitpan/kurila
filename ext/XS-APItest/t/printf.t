use TestInit;
use Config;

use Test::More tests => 11;

BEGIN { use_ok('XS::APItest') };

#########################

my $ldok = have_long_double();

# first some IO redirection
ok open(my $oldout, ">&", $^STDOUT), "saving STDOUT";
ok open($^STDOUT, '>', "foo.out"),"redirecting STDOUT";

# Allow for it to be removed
END { unlink "foo.out"; };

iohandle::output_autoflush($^STDOUT, 1); # make unbuffered

# Run the printf tests
print_double(5);
print_int(3);
print_long(4);
print_float(4);
print_long_double() if $ldok;  # val=7 hardwired

print_flush();

# Now redirect STDOUT and read from the file
ok open($^STDOUT, ">&", $oldout), "restore STDOUT";
ok open(my $foo, "<", "foo.out"), "open foo.out";
#print "# Test output by reading from file\n";
# now test the output
my @output = map { chomp; $_ }, @: ~< $foo;
close $foo;
ok (nelems @output) +>= 4, "captured at least four output lines";

is(@output[0], "5.000", "print_double");
is(@output[1], "3", "print_int");
is(@output[2], "4", "print_long");
is(@output[3], "4.000", "print_float");

SKIP: do {
   skip "No long doubles", 1 unless $ldok;
   is(@output[4], "7.000", "print_long_double");
};

