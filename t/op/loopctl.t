#!./perl

# We have the following types of loop:
#
# 1a)  while(A) {B}
# 1b)  B while A;
#
# 2a)  until(A) {B}
# 2b)  B until A;
#
# 3a)  for(@A)  {B}
# 3b)  B for A;
#
# 4a)  for (A;B;C) {D}
#
# 5a)  { A }        # a bare block is a loop which runs once
#
# Loops of type (b) don't allow for next/last/redo style
#  control, so we ignore them here. Type (a) loops can
#  all be labelled, so there are ten possibilities (each
#  of 5 types, labelled/unlabelled). We therefore need
#  thirty tests to try the three control statements against
#  the ten types of loop. For the first four types it's useful
#  to distinguish the case where next re-iterates from the case
#  where it leaves the loop. That makes 38.
# All these tests rely on "last LABEL"
#  so if they've *all* failed, maybe you broke that...
#
# These tests are followed by an extra test of nested loops.
# Feel free to add more here.
#
#  -- .robin. <robin@kitsite.com>  2001-03-13
require "./test.pl";
plan( tests => 33 );

my $ok;

TEST1: do {

  $ok = 0;

  my $x = 1;
  my $first_time = 1;
  while($x--) {
    if (!$first_time) {
      $ok = 1;
      last TEST1;
    }
    $ok = 0;
    $first_time = 0;
    redo;
    last TEST1;
  }
  continue {
    $ok = 0;
    last TEST1;
  }
  $ok = 0;
};
cmp_ok($ok,'==',1,'no label on while()');

TEST2: do {

  $ok = 0;

  my $x = 2;
  my $first_time = 1;
  my $been_in_continue = 0;
  while($x--) {
    if (!$first_time) {
      $ok = $been_in_continue;
      last TEST2;
    }
    $ok = 0;
    $first_time = 0;
    next;
    last TEST2;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = 0;
};
cmp_ok($ok,'==',1,'no label on while() successful next');

TEST3: do {

  $ok = 0;

  my $x = 1;
  my $first_time = 1;
  my $been_in_loop = 0;
  my $been_in_continue = 0;
  while($x--) {
    $been_in_loop = 1;
    if (!$first_time) {
      $ok = 0;
      last TEST3;
    }
    $ok = 0;
    $first_time = 0;
    next;
    last TEST3;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = $been_in_loop && $been_in_continue;
};
cmp_ok($ok,'==',1,'no label on while() unsuccessful next');

TEST4: do {

  $ok = 0;

  my $x = 1;
  my $first_time = 1;
  while($x++) {
    if (!$first_time) {
      $ok = 0;
      last TEST4;
    }
    $ok = 0;
    $first_time = 0;
    last;
    last TEST4;
  }
  continue {
    $ok = 0;
    last TEST4;
  }
  $ok = 1;
};
cmp_ok($ok,'==',1,'no label on while() last');

TEST5: do {

  $ok = 0;

  my $x = 0;
  my $first_time = 1;
  until($x++) {
    if (!$first_time) {
      $ok = 1;
      last TEST5;
    }
    $ok = 0;
    $first_time = 0;
    redo;
    last TEST5;
  }
  continue {
    $ok = 0;
    last TEST5;
  }
  $ok = 0;
};
cmp_ok($ok,'==',1,'no label on until()');

TEST6: do {

  $ok = 0;

  my $x = 0;
  my $first_time = 1;
  my $been_in_continue = 0;
  until($x++ +>= 2) {
    if (!$first_time) {
      $ok = $been_in_continue;
      last TEST6;
    }
    $ok = 0;
    $first_time = 0;
    next;
    last TEST6;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = 0;
};
cmp_ok($ok,'==',1,'no label on until() successful next');

TEST7: do {

  $ok = 0;

  my $x = 0;
  my $first_time = 1;
  my $been_in_loop = 0;
  my $been_in_continue = 0;
  until($x++) {
    $been_in_loop = 1;
    if (!$first_time) {
      $ok = 0;
      last TEST7;
    }
    $ok = 0;
    $first_time = 0;
    next;
    last TEST7;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = $been_in_loop && $been_in_continue;
};
cmp_ok($ok,'==',1,'no label on until() unsuccessful next');

TEST8: do {

  $ok = 0;

  my $x = 0;
  my $first_time = 1;
  until($x++ == 10) {
    if (!$first_time) {
      $ok = 0;
      last TEST8;
    }
    $ok = 0;
    $first_time = 0;
    last;
    last TEST8;
  }
  continue {
    $ok = 0;
    last TEST8;
  }
  $ok = 1;
};
cmp_ok($ok,'==',1,'no label on until() last');

TEST9: do {

  $ok = 0;

  my $first_time = 1;
  for(@(1)) {
    if (!$first_time) {
      $ok = 1;
      last TEST9;
    }
    $ok = 0;
    $first_time = 0;
    redo;
    last TEST9;
  }
  continue {
    $ok = 0;
    last TEST9;
  }
  $ok = 0;
};
cmp_ok($ok,'==',1,'no label on for(@array)');

TEST10: do {

  $ok = 0;

  my $first_time = 1;
  my $been_in_continue = 0;
  for(@(1,2)) {
    if (!$first_time) {
      $ok = $been_in_continue;
      last TEST10;
    }
    $ok = 0;
    $first_time = 0;
    next;
    last TEST10;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = 0;
};
cmp_ok($ok,'==',1,'no label on for(@array) successful next');

TEST11: do {

  $ok = 0;

  my $first_time = 1;
  my $been_in_loop = 0;
  my $been_in_continue = 0;
  for(@(1)) {
    $been_in_loop = 1;
    if (!$first_time) {
      $ok = 0;
      last TEST11;
    }
    $ok = 0;
    $first_time = 0;
    next;
    last TEST11;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = $been_in_loop && $been_in_continue;
};
cmp_ok($ok,'==',1,'no label on for(@array) unsuccessful next');

TEST12: do {

  $ok = 0;

  my $first_time = 1;
  for(1..10) {
    if (!$first_time) {
      $ok = 0;
      last TEST12;
    }
    $ok = 0;
    $first_time = 0;
    last;
    last TEST12;
  }
  continue {
    $ok=0;
    last TEST12;
  }
  $ok = 1;
};
cmp_ok($ok,'==',1,'no label on for(@array) last');

### Now do it all again with labels

TEST20: do {

  $ok = 0;

  my $x = 1;
  my $first_time = 1;
  LABEL20: while($x--) {
    if (!$first_time) {
      $ok = 1;
      last TEST20;
    }
    $ok = 0;
    $first_time = 0;
    redo LABEL20;
    last TEST20;
  }
  continue {
    $ok = 0;
    last TEST20;
  }
  $ok = 0;
};
cmp_ok($ok,'==',1,'label on while()');

TEST21: do {

  $ok = 0;

  my $x = 2;
  my $first_time = 1;
  my $been_in_continue = 0;
  LABEL21: while($x--) {
    if (!$first_time) {
      $ok = $been_in_continue;
      last TEST21;
    }
    $ok = 0;
    $first_time = 0;
    next LABEL21;
    last TEST21;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = 0;
};
cmp_ok($ok,'==',1,'label on while() successful next');

TEST22: do {

  $ok = 0;

  my $x = 1;
  my $first_time = 1;
  my $been_in_loop = 0;
  my $been_in_continue = 0;
  LABEL22: while($x--) {
    $been_in_loop = 1;
    if (!$first_time) {
      $ok = 0;
      last TEST22;
    }
    $ok = 0;
    $first_time = 0;
    next LABEL22;
    last TEST22;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = $been_in_loop && $been_in_continue;
};
cmp_ok($ok,'==',1,'label on while() unsuccessful next');

TEST23: do {

  $ok = 0;

  my $x = 1;
  my $first_time = 1;
  LABEL23: while($x++) {
    if (!$first_time) {
      $ok = 0;
      last TEST23;
    }
    $ok = 0;
    $first_time = 0;
    last LABEL23;
    last TEST23;
  }
  continue {
    $ok = 0;
    last TEST23;
  }
  $ok = 1;
};
cmp_ok($ok,'==',1,'label on while() last');

TEST24: do {

  $ok = 0;

  my $x = 0;
  my $first_time = 1;
  LABEL24: until($x++) {
    if (!$first_time) {
      $ok = 1;
      last TEST24;
    }
    $ok = 0;
    $first_time = 0;
    redo LABEL24;
    last TEST24;
  }
  continue {
    $ok = 0;
    last TEST24;
  }
  $ok = 0;
};
cmp_ok($ok,'==',1,'label on until()');

TEST25: do {

  $ok = 0;

  my $x = 0;
  my $first_time = 1;
  my $been_in_continue = 0;
  LABEL25: until($x++ +>= 2) {
    if (!$first_time) {
      $ok = $been_in_continue;
      last TEST25;
    }
    $ok = 0;
    $first_time = 0;
    next LABEL25;
    last TEST25;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = 0;
};
cmp_ok($ok,'==',1,'label on until() successful next');

TEST26: do {

  $ok = 0;

  my $x = 0;
  my $first_time = 1;
  my $been_in_loop = 0;
  my $been_in_continue = 0;
  LABEL26: until($x++) {
    $been_in_loop = 1;
    if (!$first_time) {
      $ok = 0;
      last TEST26;
    }
    $ok = 0;
    $first_time = 0;
    next LABEL26;
    last TEST26;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = $been_in_loop && $been_in_continue;
};
cmp_ok($ok,'==',1,'label on until() unsuccessful next');

TEST27: do {

  $ok = 0;

  my $x = 0;
  my $first_time = 1;
  LABEL27: until($x++ == 10) {
    if (!$first_time) {
      $ok = 0;
      last TEST27;
    }
    $ok = 0;
    $first_time = 0;
    last LABEL27;
    last TEST27;
  }
  continue {
    $ok = 0;
    last TEST8;
  }
  $ok = 1;
};
cmp_ok($ok,'==',1,'label on until() last');

TEST28: do {

  $ok = 0;

  my $first_time = 1;
  LABEL28: for(@(1)) {
    if (!$first_time) {
      $ok = 1;
      last TEST28;
    }
    $ok = 0;
    $first_time = 0;
    redo LABEL28;
    last TEST28;
  }
  continue {
    $ok = 0;
    last TEST28;
  }
  $ok = 0;
};
cmp_ok($ok,'==',1,'label on for(@array)');

TEST29: do {

  $ok = 0;

  my $first_time = 1;
  my $been_in_continue = 0;
  LABEL29: for(@(1,2)) {
    if (!$first_time) {
      $ok = $been_in_continue;
      last TEST29;
    }
    $ok = 0;
    $first_time = 0;
    next LABEL29;
    last TEST29;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = 0;
};
cmp_ok($ok,'==',1,'label on for(@array) successful next');

TEST30: do {

  $ok = 0;

  my $first_time = 1;
  my $been_in_loop = 0;
  my $been_in_continue = 0;
  LABEL30: for(@(1)) {
    $been_in_loop = 1;
    if (!$first_time) {
      $ok = 0;
      last TEST30;
    }
    $ok = 0;
    $first_time = 0;
    next LABEL30;
    last TEST30;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = $been_in_loop && $been_in_continue;
};
cmp_ok($ok,'==',1,'label on for(@array) unsuccessful next');

TEST31: do {

  $ok = 0;

  my $first_time = 1;
  LABEL31: for(1..10) {
    if (!$first_time) {
      $ok = 0;
      last TEST31;
    }
    $ok = 0;
    $first_time = 0;
    last LABEL31;
    last TEST31;
  }
  continue {
    $ok=0;
    last TEST31;
  }
  $ok = 1;
};
cmp_ok($ok,'==',1,'label on for(@array) last');

TEST36: do {

  $ok = 0;
  my $first_time = 1;

  LABEL36: do {
    if (!$first_time) {
      $ok = 1;
      last TEST36;
    }
    $ok = 0;
    $first_time=0;

    redo LABEL36;
    last TEST36;
  };
  $ok = 0;
};
cmp_ok($ok,'==',1,'label on bare block');

TEST38: do {

  $ok = 0;
  LABEL38: do {
    last LABEL38;
    last TEST38;
  };
  $ok = 1;
};
cmp_ok($ok,'==',1,'label on bare block last');

TEST39: do {
    $ok = 0;
    my @($x, $y, $z) = @(1,1,1);
    one39: while ($x--) {
      $ok = 0;
      two39: while ($y--) {
        $ok = 0;
        three39: while ($z--) {
           next two39;
        }
        continue {
          $ok = 0;
          last TEST39;
        }
      }
      continue {
        $ok = 1;
        last TEST39;
      }
      $ok = 0;
    }
};
cmp_ok($ok,'==',1,'nested constructs');

sub test_last_label { last TEST40 }

TEST40: do {
    $ok = 1;
    test_last_label();
    $ok = 0;
};
cmp_ok($ok,'==',1,'dynamically scoped label');

sub test_last { last }

TEST41: do {
    $ok = 1;
    test_last();
    $ok = 0;
};
cmp_ok($ok,'==',1,'dynamically scoped');


# [perl #27206] Memory leak in continue loop
# Ensure that the temporary object is freed each time round the loop,
# rather then all 10 of them all being freed right at the end

do {
    my $n=10; my $late_free = 0;
    sub X::DESTROY { $late_free++ if $n +< 0 };
  LOOP:
    do {
	($n-- && bless \%(), 'X') && redo;
    };
    cmp_ok($late_free,'==',0,"bug 27206: redo memory leak");
};

# ensure that redo doesn't clear a lexical declared in the condition

do {
    my $i = 1;
    while (my $x = $i) {
	$i++;
	redo if $i == 2;
	cmp_ok($x,'==',1,"while/redo lexical life");
	last;
    }
    $i = 1;
    until (! (my $x = $i)) {
	$i++;
	redo if $i == 2;
	cmp_ok($x,'==',1,"until/redo lexical life");
	last;
    }
};

TODO: do {
    todo_skip("modification of readonly value", 1);
    our @a37725;
    @a37725[3] = 1; # use package var
    our $i = 2;
    for my $x (@(reverse < @a37725)) {
	$x = $i++;
    }
    cmp_ok("$(join ' ',@a37725)",'eq',"5 4 3 2",'bug 27725: reverse with empty slots bug');
};

