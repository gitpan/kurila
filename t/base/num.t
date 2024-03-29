#!./perl

print $^STDOUT, "1..50\n";

# First test whether the number stringification works okay.
# (Testing with == would exercize the IV/NV part, not the PV.)

$a = 1; "$a";
print $^STDOUT, $a eq "1"       ?? "ok 1\n"  !! "not ok 1 # $a\n";

$a = -1; "$a";
print $^STDOUT, $a eq "-1"      ?? "ok 2\n"  !! "not ok 2 # $a\n";

$a = 1.; "$a";
print $^STDOUT, $a eq "1"       ?? "ok 3\n"  !! "not ok 3 # $a\n";

$a = -1.; "$a";
print $^STDOUT, $a eq "-1"      ?? "ok 4\n"  !! "not ok 4 # $a\n";

$a = 0.1; "$a";
print $^STDOUT, $a eq "0.1"     ?? "ok 5\n"  !! "not ok 5 # $a\n";

$a = -0.1; "$a";
print $^STDOUT, $a eq "-0.1"    ?? "ok 6\n"  !! "not ok 6 # $a\n";

$a = .1; "$a";
print $^STDOUT, $a eq "0.1"     ?? "ok 7\n"  !! "not ok 7 # $a\n";

$a = -.1; "$a";
print $^STDOUT, $a eq "-0.1"    ?? "ok 8\n"  !! "not ok 8 # $a\n";

$a = 10.01; "$a";
print $^STDOUT, $a eq "10.01"   ?? "ok 9\n"  !! "not ok 9 # $a\n";

$a = 1e3; "$a";
print $^STDOUT, $a eq "1000"    ?? "ok 10\n" !! "not ok 10 # $a\n";

$a = 10.01e3; "$a";
print $^STDOUT, $a eq "10010"   ?? "ok 11\n"  !! "not ok 11 # $a\n";

$a = 0b100; "$a";
print $^STDOUT, $a eq "4"       ?? "ok 12\n"  !! "not ok 12 # $a\n";

$a = 0100; "$a";
print $^STDOUT, $a eq "64"      ?? "ok 13\n"  !! "not ok 13 # $a\n";

$a = 0x100; "$a";
print $^STDOUT, $a eq "256"     ?? "ok 14\n" !! "not ok 14 # $a\n";

$a = 1000; "$a";
print $^STDOUT, $a eq "1000"    ?? "ok 15\n" !! "not ok 15 # $a\n";

# Okay, now test the numerics.
# We may be assuming too much, given the painfully well-known floating
# point sloppiness, but the following are still quite reasonable
# assumptions which if not working would confuse people quite badly.

$a = 1; "$a"; # Keep the stringification as a potential troublemaker.
print $^STDOUT, $a + 1 == 2     ?? "ok 16\n" !! "not ok 16 #" . $a + 1 . "\n";
# Don't know how useful printing the stringification of $a + 1 really is.

$a = -1; "$a";
print $^STDOUT, $a + 1 == 0     ?? "ok 17\n" !! "not ok 17 #" . $a + 1 . "\n";

$a = 1.; "$a";
print $^STDOUT, $a + 1 == 2     ?? "ok 18\n" !! "not ok 18 #" . $a + 1 . "\n";

$a = -1.; "$a";
print $^STDOUT, $a + 1 == 0     ?? "ok 19\n" !! "not ok 19 #" . $a + 1 . "\n";

sub ok($a, $b, $c) {
    abs($a - $b) +<= $c;
}

$a = 0.1; "$a";
print $^STDOUT, ok($a + 1,  1.1,  0.05)   ?? "ok 20\n" !! "not ok 20 #" . $a + 1 . "\n";

$a = -0.1; "$a";
print $^STDOUT, ok($a + 1,  0.9,  0.05)   ?? "ok 21\n" !! "not ok 21 #" . $a + 1 . "\n";

$a = .1; "$a";
print $^STDOUT, ok($a + 1,  1.1,  0.005)  ?? "ok 22\n" !! "not ok 22 #" . $a + 1 . "\n";

$a = -.1; "$a";
print $^STDOUT, ok($a + 1,  0.9,  0.05)   ?? "ok 23\n" !! "not ok 23 #" . $a + 1 . "\n";

$a = 10.01; "$a";
print $^STDOUT, ok($a + 1, 11.01, 0.005) ?? "ok 24\n" !! "not ok 24 #" . $a + 1 . "\n";

$a = 1e3; "$a";
print $^STDOUT, $a + 1 == 1001  ?? "ok 25\n" !! "not ok 25 #" . $a + 1 . "\n";

$a = 10.01e3; "$a";
print $^STDOUT, $a + 1 == 10011 ?? "ok 26\n" !! "not ok 26 #" . $a + 1 . "\n";

$a = 0b100; "$a";
print $^STDOUT, $a + 1 == 0b101 ?? "ok 27\n" !! "not ok 27 #" . $a + 1 . "\n";

$a = 0100; "$a";
print $^STDOUT, $a + 1 == 0101  ?? "ok 28\n" !! "not ok 28 #" . $a + 1 . "\n";

$a = 0x100; "$a";
print $^STDOUT, $a + 1 == 0x101 ?? "ok 29\n" !! "not ok 29 #" . $a + 1 . "\n";

$a = 1000; "$a";
print $^STDOUT, $a + 1 == 1001  ?? "ok 30\n" !! "not ok 30 #" . $a + 1 . "\n";

# back to some basic stringify tests
# we expect NV stringification to work according to C sprintf %.*g rules

if ($^OS_NAME eq 'os2') { # In the long run, fix this.  For 5.8.0, deal.
    $a = 0.01; "$a";
    print $^STDOUT, $a eq "0.01"   || $a eq '1e-02' ?? "ok 31\n" !! "not ok 31 # $a\n";

    $a = 0.001; "$a";
    print $^STDOUT, $a eq "0.001"  || $a eq '1e-03' ?? "ok 32\n" !! "not ok 32 # $a\n";

    $a = 0.0001; "$a";
    print $^STDOUT, $a eq "0.0001" || $a eq '1e-04' ?? "ok 33\n" !! "not ok 33 # $a\n";
} else {
    $a = 0.01; "$a";
    print $^STDOUT, $a eq "0.01"    ?? "ok 31\n" !! "not ok 31 # $a\n";

    $a = 0.001; "$a";
    print $^STDOUT, $a eq "0.001"   ?? "ok 32\n" !! "not ok 32 # $a\n";

    $a = 0.0001; "$a";
    print $^STDOUT, $a eq "0.0001"  ?? "ok 33\n" !! "not ok 33 # $a\n";
}

$a = 0.00009; "$a";
print $^STDOUT, $a eq "9e-05" || $a eq "9e-005" ?? "ok 34\n"  !! "not ok 34 # $a\n";

$a = 1.1; "$a";
print $^STDOUT, $a eq "1.1"     ?? "ok 35\n" !! "not ok 35 # $a\n";

$a = 1.01; "$a";
print $^STDOUT, $a eq "1.01"    ?? "ok 36\n" !! "not ok 36 # $a\n";

$a = 1.001; "$a";
print $^STDOUT, $a eq "1.001"   ?? "ok 37\n" !! "not ok 37 # $a\n";

$a = 1.0001; "$a";
print $^STDOUT, $a eq "1.0001"  ?? "ok 38\n" !! "not ok 38 # $a\n";

$a = 1.00001; "$a";
print $^STDOUT, $a eq "1.00001" ?? "ok 39\n" !! "not ok 39 # $a\n";

$a = 1.000001; "$a";
print $^STDOUT, $a eq "1.000001" ?? "ok 40\n" !! "not ok 40 # $a\n";

$a = 0.; "$a";
print $^STDOUT, $a eq "0"       ?? "ok 41\n" !! "not ok 41 # $a\n";

$a = 100000.; "$a";
print $^STDOUT, $a eq "100000"  ?? "ok 42\n" !! "not ok 42 # $a\n";

$a = -100000.; "$a";
print $^STDOUT, $a eq "-100000" ?? "ok 43\n" !! "not ok 43 # $a\n";

$a = 123.456; "$a";
print $^STDOUT, $a eq "123.456" ?? "ok 44\n" !! "not ok 44 # $a\n";

$a = 1e34; "$a";
unless ($^OS_NAME eq 'posix-bc')
{ print $^STDOUT, $a eq "1e+34" || $a eq "1e+034" ?? "ok 45\n" !! "not ok 45 # $a\n"; }
else
{ print $^STDOUT, "ok 45 # skipped on $^OS_NAME\n"; }

# see bug #15073

$a = 0.00049999999999999999999999999999999999999;
$b = 0.0005000000000000000104;
print $^STDOUT, $a +<= $b ?? "ok 46\n" !! "not ok 46\n";

if ($^OS_NAME eq 'ultrix' || $^OS_NAME eq 'VMS') {
  # Ultrix enters looong nirvana over this. VMS blows up when configured with
  # D_FLOAT (but with G_FLOAT or IEEE works fine).  The test should probably
  # make the number of 0's a function of NV_DIG, but that's not in Config and 
  # we probably don't want to suck Config into a base test anyway.
  print $^STDOUT, "ok 47\n";
} else {
  $a = 0.00000000000000000000000000000000000000000000000000000000000000000001;
  print $^STDOUT, $a +> 0 ?? "ok 47\n" !! "not ok 47\n";
}

$a = 80000.0000000000000000000000000;
print $^STDOUT, $a == 80000.0 ?? "ok 48\n" !! "not ok 48\n";

$a = 1.0000000000000000000000000000000000000000000000000000000000000000000e1;
print $^STDOUT, $a == 10.0 ?? "ok 49\n" !! "not ok 49\n";

# From Math/Trig - number has to be long enough to exceed at least DBL_DIG

$a = 57.295779513082320876798154814169;
print $^STDOUT, ok($a*10,572.95779513082320876798154814169,1e-10) ?? "ok 50\n" !!
  "not ok 50 # $a\n";
